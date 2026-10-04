/*
 * Copyright (c) 2026 Alibaba Group.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "../common/v_common.h"

const std::vector<InstField> vop_inst_fields = {
	{ 31, 26, 0x05, true, RegClass::NotReg, "funct6" },
	{ 25, 25, 0x00, false, RegClass::NotReg, "vm" },
	{ 24, 20, 0x00, false, RegClass::Vector, "vs2" },
	{ 19, 15, 0x00, false, RegClass::Vector, "vs1" },
	{ 14, 12, OPFVV, true, RegClass::NotReg, "funct3" },
	{ 11, 7, 0x00, false, RegClass::Vector, "vd" },
	{ 6, 0, opcode_vector, true, RegClass::NotReg, "opcode" }
};

// =============================================================================
// vfredmin.vs: Floating-point minimum reduction
// vd[0] = minimumNumber(vs1[0], vs2[*])
// Uses IEEE 754 minimumNumber semantics.
// vstart != 0 raises illegal instruction exception.
// =============================================================================
int check_illegal(c_data &cur_data)
{
	int vm_bit = cur_data.map_reg_index["vm"];
	int vs2 = cur_data.map_reg_index["vs2"];
	int vs1 = cur_data.map_reg_index["vs1"];
	int vd = cur_data.map_reg_index["vd"];
	ValidationConfig config = { .check_align = true, .check_vstart = true };
	int illegal = !(
		VectorRegValidator::validate(VregOperand::none(),
					     {
						     VregOperand::none(),
						     VregOperand::one_pow(vs2),
					     },
					     vm_bit, config));
	print_illegal_status(illegal);
	return illegal;
}

template <typename Ts2, typename Td> int run_self_result(c_data &cur_data)
{
	int vm_bit = cur_data.map_reg_index["vm"];

	Td *vs1_data = static_cast<Td *>(
		cur_data.map_preinst_typed_value["vs1"].get());
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

	// Initialize accumulator with scalar operand vs1[0]
	Td accumulator = vs1_data[0];

	for (int j = 0; j < vector_cfg.len; j++) {
		int row = j / 8;
		int col = j % 8;
		// Skip inactive elements (masked off)
		if (!vm_bit && !(vm_data[row] & (1ull << col))) {
			continue;
		}

		// minimumNumber: if either is NaN, return the other
		// if both are NaN, return canonical NaN
		Td val = vs2_data[j];
		if (accumulator.m_is_nan() && val.m_is_nan()) {
			// Both NaN: result is canonical NaN
			Td cnan;
			cnan.m_set_exp((1ull << cnan.exp_bit) - 1);
			cnan.m_set_man(1ull << (cnan.man_bit - 1));
			accumulator = cnan;
		} else if (accumulator.m_is_nan()) {
			accumulator = val;
		} else if (val.m_is_nan()) {
			// accumulator stays
		} else {
			// Both are non-NaN: minimumNumber
			// -0 < +0 for minimumNumber
			if (accumulator.m_is_zero() && val.m_is_zero()) {
				// prefer -0 over +0
				if (!accumulator.m_get_sign() &&
				    val.m_get_sign())
					accumulator = val;
			} else if (val.m_data.fdata <
				   accumulator.m_data.fdata) {
				accumulator = val;
			}
		}
	}

	selfcheck_data[0] = accumulator;
	return 0;
}

template <typename Ts2, typename Td>
int per_run(int it, c_cfg &cur_cfg, c_data &cur_data)
{
	cur_data.set_value_from_inst(vop_inst_fields, vector_cfg.inst);

	if (random_mode) {
		cur_data.register_type_with_random<Td>("vs1", vlenb);
		cur_data.register_type_with_random<Ts2>("vs2", vector_cfg.len);
		cur_data.register_type_with_random<Td>("vd", vlenb);
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

	load_multi_vector<Td, Td, Ts2, uint8_t>(
		insts, cur_data, { "vd", "vs1", "vs2", "vm" },
		{ lmul_m1, lmul_m1, vector_cfg.lmul, lmul_m1 },
		{ vlenb, vlenb, vector_cfg.len, vlenb });

	store_multi_preinst_vector<Td, Td, Ts2, uint8_t>(
		insts, cur_data, { "vd", "vs1", "vs2", "vm" },
		{ lmul_m1, lmul_m1, vector_cfg.lmul, lmul_m1 },
		{ vlenb, vlenb, vector_cfg.len, vlenb });

	vsetvli_lmul_sew(insts, vector_cfg.lmul, vector_cfg.sew,
			 vector_cfg.len);
	csrrw(insts, CSR_VSTART, vector_cfg.vstart);
	csrrw(insts, CSR_VXRM, vector_cfg.vxrm);

	insts.push_back(vector_cfg.inst);

	store_multi_afterinst_vector<Td>(insts, cur_data, { "vd" }, { lmul_m1 },
					 { vlenb });

	restore_context(insts);

	run_instruction(insts);

	unpack_multi_after_run<Td, Td, Ts2, uint8_t>(
		cur_data, { "vd", "vs1", "vs2", "vm" },
		{ vlenb, vlenb, vector_cfg.len, vlenb });

	int has_illegal = check_afterinst_illegal();
	if (has_illegal >= 0)
		return has_illegal;

	save_multi_preinst_value_to_common<Td, Td, Ts2, uint8_t>(
		cur_data, { "vd", "vs1", "vs2", "vm" },
		{ vlenb, vlenb, vector_cfg.len, vlenb });

	run_self_result<Ts2, Td>(cur_data);

	int is_error = check_multi_error<Td>(cur_data, { "vd" }, { vlenb });

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
