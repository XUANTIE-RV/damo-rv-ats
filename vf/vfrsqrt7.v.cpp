/*
 * Copyright (c) 2026 Alibaba Group.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "../common/v_common.h"

// vfrsqrt7 lookup table: 128 entries
// Index = (normalized_exp[0] * 64) + normalized_sig[MSB-1 : MSB-6]
// Output = 7 MSBs of result significand (after the leading one)
static const uint8_t vfrsqrt7_table[128] = {
	// exp[0] = 0, sig[5:0] = 0..63
	52, 51, 50, 48, 47, 46, 44, 43, 42, 41, 40, 39, 38, 36, 35, 34, 33, 32,
	31, 30, 30, 29, 28, 27, 26, 25, 24, 23, 23, 22, 21, 20, 19, 19, 18, 17,
	16, 16, 15, 14, 14, 13, 12, 12, 11, 10, 10, 9, 9, 8, 7, 7, 6, 6, 5, 4,
	4, 3, 3, 2, 2, 1, 1, 0,
	// exp[0] = 1, sig[5:0] = 0..63
	127, 125, 123, 121, 119, 118, 116, 114, 113, 111, 109, 108, 106, 105,
	103, 102, 100, 99, 97, 96, 95, 93, 92, 91, 90, 88, 87, 86, 85, 84, 83,
	82, 80, 79, 78, 77, 76, 75, 74, 73, 72, 71, 70, 70, 69, 68, 67, 66, 65,
	64, 63, 63, 62, 61, 60, 59, 59, 58, 57, 56, 56, 55, 54, 53
};

const std::vector<InstField> vop_inst_fields = {
	{ 31, 26, 0x13, true, RegClass::NotReg, "funct6" },
	{ 25, 25, 0x00, false, RegClass::NotReg, "vm" },
	{ 24, 20, 0x00, false, RegClass::Vector, "vs2" },
	{ 19, 15, 0x04, true, RegClass::NotReg, "vs1" },
	{ 14, 12, OPFVV, true, RegClass::NotReg, "funct3" },
	{ 11, 7, 0x00, false, RegClass::Vector, "vd" },
	{ 6, 0, opcode_vector, true, RegClass::NotReg, "opcode" }
};

int check_illegal(c_data &cur_data)
{
	int vm_bit = cur_data.map_reg_index["vm"];
	int vs2 = cur_data.map_reg_index["vs2"];
	int vd = cur_data.map_reg_index["vd"];
	ValidationConfig config = { .check_align = true,
				    .check_overlap = true,
				    .check_vm = true };
	int illegal = !(VectorRegValidator::validate(
		VregOperand::one_pow(vd), { VregOperand::one_pow(vs2) }, vm_bit,
		config));
	print_illegal_status(illegal);
	return illegal;
}

// Compute vfrsqrt7 for a single element using the SPEC-defined lookup table
// algorithm. Works generically for any c_check_f<T, U> type.
template <typename Tf> Tf vfrsqrt7_calc(Tf val)
{
	using U = typename Tf::storage_type;
	uint8_t sign_bit = val.sign_bit;
	uint8_t exp_bit = val.exp_bit;
	uint8_t man_bit = val.man_bit;
	uint8_t bits = val.bits;
	U exp_bias = val.exp_bias;

	U sign = val.m_get_sign();
	U exp = val.m_get_exp();
	U man = val.m_get_man();

	U all_ones_exp = ((U)1 << exp_bit) - 1;

	Tf result;

	// Handle special cases per SPEC table
	if (exp == all_ones_exp) {
		if (man != 0) {
			// sNaN or qNaN -> canonical NaN
			result.m_set_exp(all_ones_exp);
			result.m_set_man((U)1 << (man_bit - 1));
			result.m_set_sign(0);
			return result;
		}
		if (sign) {
			// -inf -> canonical NaN (NV exception)
			result.m_set_exp(all_ones_exp);
			result.m_set_man((U)1 << (man_bit - 1));
			result.m_set_sign(0);
			return result;
		}
		// +inf -> +0.0
		result.m_data.idata = 0;
		return result;
	}

	bool is_zero = (exp == 0 && man == 0);

	if (sign && !is_zero) {
		// Negative non-zero (not -0.0) -> canonical NaN (NV exception)
		result.m_set_exp(all_ones_exp);
		result.m_set_man((U)1 << (man_bit - 1));
		result.m_set_sign(0);
		return result;
	}

	if (is_zero) {
		// +0.0 -> +inf, -0.0 -> -inf (DZ exception)
		result.m_set_exp(all_ones_exp);
		result.m_set_man(0);
		result.m_set_sign(sign);
		return result;
	}

	// Normal positive value: compute estimate using lookup table
	// Step 1: Normalize the input
	int normalized_exp;
	U normalized_man;

	if (exp == 0) {
		// Subnormal: count leading zeros in mantissa
		int lz = 0;
		for (int i = man_bit - 1; i >= 0; i--) {
			if (man & ((U)1 << i))
				break;
			lz++;
		}
		// normalized_exp = 0 - lz (which is 1 - (lz + 1) = -(lz))
		// Per SPEC: normalized input exponent = 0 minus the number of
		// leading zeros in the significand
		normalized_exp = 0 - lz;
		// Per SPEC: normalized input significand is given by shifting
		// the input significand left by (1 - normalized_exp),
		// discarding the leading 1 bit
		normalized_man = (man << (1 - normalized_exp)) &
				 (((U)1 << man_bit) - 1);
	} else {
		// Normal
		normalized_exp = (int)exp;
		normalized_man = man;
	}

	// Step 2: Compute lookup table index
	// index = exp[0] * 64 + sig[MSB-1 : MSB-6]
	int exp_lsb = normalized_exp & 1;
	int sig_high6;
	if (man_bit >= 6) {
		sig_high6 = (int)(normalized_man >> (man_bit - 6)) & 0x3F;
	} else {
		sig_high6 = (int)(normalized_man << (6 - man_bit)) & 0x3F;
	}
	int index = exp_lsb * 64 + sig_high6;

	// Step 3: Look up output significand (7 MSBs)
	uint8_t sig_out7 = vfrsqrt7_table[index];

	// Step 4: Compute output exponent
	// out_exp = floor((3*B - 1 - normalized_exp) / 2)
	int out_exp = (3 * (int)exp_bias - 1 - normalized_exp) / 2;

	// Step 5: Assemble result
	// Output sign = input sign (which is 0 for positive values here)
	result.m_set_sign(0);
	result.m_set_exp((U)out_exp);

	// Place sig_out7 in the top 7 bits of the mantissa, rest are zero
	U out_man;
	if (man_bit >= 7) {
		out_man = (U)sig_out7 << (man_bit - 7);
	} else {
		out_man = (U)sig_out7 >> (7 - man_bit);
	}
	result.m_set_man(out_man);

	return result;
}

template <typename Ts2, typename Td> int run_self_result(c_data &cur_data)
{
	int vm_bit = cur_data.map_reg_index["vm"];

	Ts2 *vs2_data = static_cast<Ts2 *>(
		cur_data.map_preinst_typed_value["vs2"].get());

	uint8_t *vm_data;
	if (!vm_bit) {
		vm_data = static_cast<uint8_t *>(
			cur_data.map_preinst_typed_value["vm"].get());
	}

	cur_data.map_selfcheck_typed_value["vd"] =
		c_data::process_from_common<Td>(
			cur_data.map_preinst_reg_value["vd"]);
	Td *selfcheck_data = static_cast<Td *>(
		cur_data.map_selfcheck_typed_value["vd"].get());

	for (int j = 0; j < vector_cfg.len; j++) {
		int row = j / 8;
		int col = j % 8;
		if (j < vector_cfg.vstart ||
		    (!vm_bit && !(vm_data[row] & (1ull << col)))) {
			continue;
		}

		// vfrsqrt7.v: vd[i] = rsqrt7_estimate(vs2[i])
		selfcheck_data[j] = vfrsqrt7_calc<Ts2>(vs2_data[j]);
	}
	return 0;
}

template <typename Ts2, typename Td>
int per_run(int it, c_cfg &cur_cfg, c_data &cur_data)
{
	cur_data.set_value_from_inst(vop_inst_fields, vector_cfg.inst);

	if (random_mode) {
		cur_data.register_type_with_random<Ts2>("vs2", vector_cfg.len);
		cur_data.register_type_with_random<Td>("vd", vector_cfg.len);
		if (!cur_data.map_reg_index["vm"])
			cur_data.register_type_with_random<uint8_t>("vm",
								    vlenb);
		cur_data.set_value_to_cfg(cur_cfg);
		cur_cfg.DESC = cur_data.get_DESC_from_inst(vop_inst_fields,
							   vector_cfg.inst);
		if (record_mode) {
			global_cfg.push_back(cur_cfg);
		}
	} else {
		cur_data.set_value_from_cfg(cur_cfg);
	}

	global_flag_ptr->illegal = check_illegal(cur_data);

	std::vector<uint32_t> insts;

	save_context(insts);
	vzero_all(insts);

	load_multi_vector<Td, Ts2, uint8_t>(
		insts, cur_data, { "vd", "vs2", "vm" },
		{ vector_cfg.lmul, vector_cfg.lmul, lmul_m1 },
		{ vector_cfg.len, vector_cfg.len, vlenb });

	store_multi_preinst_vector<Td, Ts2, uint8_t>(
		insts, cur_data, { "vd", "vs2", "vm" },
		{ vector_cfg.lmul, vector_cfg.lmul, lmul_m1 },
		{ vector_cfg.len, vector_cfg.len, vlenb });

	vsetvli_lmul_sew(insts, vector_cfg.lmul, vector_cfg.sew,
			 vector_cfg.len);
	csrrw(insts, CSR_VSTART, vector_cfg.vstart);
	csrrw(insts, CSR_VXRM, vector_cfg.vxrm);

	insts.push_back(vector_cfg.inst);

	store_multi_afterinst_vector<Td>(insts, cur_data, { "vd" },
					 { vector_cfg.lmul },
					 { vector_cfg.len });

	restore_context(insts);

	run_instruction(insts);

	unpack_multi_after_run<Td, Ts2, uint8_t>(
		cur_data, { "vd", "vs2", "vm" },
		{ vector_cfg.len, vector_cfg.len, vlenb });

	int has_illegal = check_afterinst_illegal();
	if (has_illegal >= 0)
		return has_illegal;

	save_multi_preinst_value_to_common<Td, Ts2, uint8_t>(
		cur_data, { "vd", "vs2", "vm" },
		{ vector_cfg.len, vector_cfg.len, vlenb });

	run_self_result<Ts2, Td>(cur_data);

	int is_error =
		check_multi_error<Td>(cur_data, { "vd" }, { vector_cfg.len });

	if (is_error) {
		DEBUG << std::hex << json(cur_data).dump(4) << std::endl;
		if (early_stop)
			return 1;
	}
	reset_data();
	return 0;
}

int main(int argc, char *argv[])
{
	filename = std::filesystem::path(__FILE__).filename().string();
	datafile = filename + ".data";
	outfile = filename + ".out";

	init_program(argc, argv);
	init_vector_program();

	int it;
	for (it = 0; it < run_times; it++) {
		if (!random_mode && it >= global_cfg.size())
			break;

		print_runtime_iteration(it);
		c_cfg cur_cfg;
		c_data cur_data;

		init_vector_cfg(it, cur_cfg, cur_data, check_illegal,
				vop_inst_fields);

		int has_error = 0;
		switch (vector_cfg.sew) {
		case sew_e8:
			break;
		case sew_e16:
			has_error = per_run<c_check_f<fp16, uint16_t>,
					    c_check_f<fp16, uint16_t> >(
				it, cur_cfg, cur_data);
			break;
		case sew_e32:
			has_error = per_run<c_check_f<float, uint32_t>,
					    c_check_f<float, uint32_t> >(
				it, cur_cfg, cur_data);
			break;
		case sew_e64:
			has_error = per_run<c_check_f<double, uint64_t>,
					    c_check_f<double, uint64_t> >(
				it, cur_cfg, cur_data);
			break;
		default:
			break;
		}

		if (has_error) {
			ERROR << "CSR: " << c_cfg::to_json(cur_cfg).at("CSR")
			      << std::endl;
			ERROR << vector_cfg << std::endl;
		}

		print_runtime_iteration_end();
		if (has_error && early_stop)
			break;
	}

	end_program(it);
	return 0;
}
