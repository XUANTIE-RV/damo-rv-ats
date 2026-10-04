/*
 * Copyright (c) 2026 Alibaba Group.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "../common/v_common.h"

// vfrec7 lookup table: 128 entries
// Index = normalized_sig[MSB -: 7]
// Output = 7 MSBs of normalized output significand (after the leading one)
static const uint8_t vfrec7_table[128] = {
	127, 125, 123, 121, 119, 117, 116, 114, 112, 110, 109, 107, 105,
	104, 102, 100, 99,  97,	 96,  94,  93,	91,  90,  88,  87,  85,
	84,  83,  81,  80,  79,	 77,  76,  75,	74,  72,  71,  70,  69,
	68,  66,  65,  64,  63,	 62,  61,  60,	59,  58,  57,  56,  55,
	54,  53,  52,  51,  50,	 49,  48,  47,	46,  45,  44,  43,  42,
	41,  40,  40,  39,  38,	 37,  36,  35,	35,  34,  33,  32,  31,
	31,  30,  29,  28,  28,	 27,  26,  25,	25,  24,  23,  23,  22,
	21,  21,  20,  19,  19,	 18,  17,  17,	16,  15,  15,  14,  14,
	13,  12,  12,  11,  11,	 10,  9,   9,	8,   8,	  7,   7,   6,
	5,   5,	  4,   4,   3,	 3,   2,   2,	1,   1,	  0
};

const std::vector<InstField> vop_inst_fields = {
	{ 31, 26, 0x13, true, RegClass::NotReg, "funct6" },
	{ 25, 25, 0x00, false, RegClass::NotReg, "vm" },
	{ 24, 20, 0x00, false, RegClass::Vector, "vs2" },
	{ 19, 15, 0x05, true, RegClass::NotReg, "vs1" },
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

// Compute vfrec7 for a single element using the SPEC-defined lookup table
// algorithm. Works generically for any c_check_f<T, U> type.
template <typename Tf> Tf vfrec7_calc(Tf val)
{
	using U = typename Tf::storage_type;
	uint8_t sign_bit_count = val.sign_bit;
	uint8_t exp_bit = val.exp_bit;
	uint8_t man_bit = val.man_bit;
	uint8_t bits = val.bits;
	U exp_bias = val.exp_bias;

	U sign = val.m_get_sign();
	U exp = val.m_get_exp();
	U man = val.m_get_man();

	U all_ones_exp = ((U)1 << exp_bit) - 1;
	U all_ones_man = ((U)1 << man_bit) - 1;

	Tf result;

	// Handle special cases per SPEC table

	// NaN
	if (exp == all_ones_exp && man != 0) {
		// sNaN or qNaN -> canonical NaN
		result.m_set_exp(all_ones_exp);
		result.m_set_man((U)1 << (man_bit - 1));
		result.m_set_sign(0);
		return result;
	}

	// Infinity
	if (exp == all_ones_exp && man == 0) {
		// +/-inf -> +/-0.0
		result.m_data.idata = 0;
		result.m_set_sign(sign);
		return result;
	}

	// Zero
	if (exp == 0 && man == 0) {
		// +/-0.0 -> +/-inf (DZ exception)
		result.m_set_exp(all_ones_exp);
		result.m_set_man(0);
		result.m_set_sign(sign);
		return result;
	}

	// Normal positive/negative value: compute estimate using lookup table
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
		// Per SPEC: normalized input exponent = 0 minus the number of
		// leading zeros in the significand
		normalized_exp = 0 - lz;
		// Per SPEC: normalized input significand is given by shifting
		// the input significand left by (1 - normalized_exp),
		// discarding the leading 1 bit
		normalized_man = (man << (1 - normalized_exp)) & all_ones_man;
	} else {
		// Normal
		normalized_exp = (int)exp;
		normalized_man = man;
	}

	// Step 2: Compute normalized output exponent
	// out_exp = 2*B - 1 - normalized_exp
	int norm_out_exp = 2 * (int)exp_bias - 1 - normalized_exp;

	// Step 3: Check for overflow (normalized output exponent outside [-1, 2*B])
	if (norm_out_exp > 2 * (int)exp_bias) {
		// Overflow: result depends on rounding mode
		// Using RNE (default) behavior: output is +/-inf
		// For positive sign -> +inf, for negative sign -> -inf
		result.m_set_exp(all_ones_exp);
		result.m_set_man(0);
		result.m_set_sign(sign);
		return result;
	}
	if (norm_out_exp < -1) {
		// This shouldn't happen for valid inputs per SPEC
		// but handle defensively: output +/-0
		result.m_data.idata = 0;
		result.m_set_sign(sign);
		return result;
	}

	// Step 4: Compute lookup table index
	// index = normalized_man[MSB -: 7]
	int index;
	if (man_bit >= 7) {
		index = (int)(normalized_man >> (man_bit - 7)) & 0x7F;
	} else {
		index = (int)(normalized_man << (7 - man_bit)) & 0x7F;
	}

	// Step 5: Look up normalized output significand (7 MSBs)
	uint8_t sig_out7 = vfrec7_table[index];

	// Step 6: Compute output significand
	U norm_out_man;
	if (man_bit >= 7) {
		norm_out_man = (U)sig_out7 << (man_bit - 7);
	} else {
		norm_out_man = (U)sig_out7 >> (7 - man_bit);
	}

	// Step 7: Handle subnormal output
	// Per SPEC: If the normalized output exponent is 0 or -1, the result
	// is subnormal: the output exponent is 0, and the output significand
	// is given by concatenating a 1 bit to the left of the normalized
	// output significand, then shifting that quantity right by
	// (1 - normalized_output_exponent).
	if (norm_out_exp <= 0) {
		// Prepend the implicit leading 1 bit
		U full_sig = ((U)1 << man_bit) | norm_out_man;
		// Shift right by (1 - norm_out_exp)
		int shift = 1 - norm_out_exp;
		if (shift < (int)(bits))
			full_sig >>= shift;
		else
			full_sig = 0;
		result.m_set_exp(0);
		result.m_set_man(full_sig & all_ones_man);
	} else {
		// Normal output
		result.m_set_exp((U)norm_out_exp);
		result.m_set_man(norm_out_man);
	}

	// Output sign = input sign
	result.m_set_sign(sign);

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

		// vfrec7.v: vd[i] = rec7_estimate(vs2[i])
		selfcheck_data[j] = vfrec7_calc<Ts2>(vs2_data[j]);
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
