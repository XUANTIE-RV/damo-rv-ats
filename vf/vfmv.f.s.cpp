/*
 * Copyright (c) 2026 Alibaba Group.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "../common/v_common.h"

const std::vector<InstField> vop_inst_fields = {
	{ 31, 26, 0x10, true, RegClass::NotReg, "funct6" },
	{ 25, 25, 0x01, true, RegClass::NotReg, "vm" },
	{ 24, 20, 0x00, false, RegClass::Vector, "vs2" },
	{ 19, 15, 0x00, true, RegClass::NotReg, "rs1" },
	{ 14, 12, OPFVV, true, RegClass::NotReg, "funct3" },
	{ 11, 7, 0x00, false, RegClass::Float, "rd" },
	{ 6, 0, opcode_vector, true, RegClass::NotReg, "opcode" }
};

// =============================================================================
// vfmv.f.s: f[rd] = vs2[0]
// Copies a single SEW-wide element from index 0 of the source vector
// register to a destination scalar floating-point register.
// Ignores LMUL and vector register groups.
// Executes even if vstart >= vl or vl=0.
// vm=0 is reserved (illegal encoding).
// rs1 is fixed to 0x00.
// =============================================================================
int check_illegal(c_data &cur_data)
{
	// vm is fixed to 1 (vm=0 is reserved/illegal encoding)
	// rs1 is fixed to 0x00
	// Ignores LMUL, so no alignment checks needed for vs2
	// rd is a scalar float register, so no vector overlap checks needed
	int vs2 = cur_data.map_reg_index["vs2"];
	int vm_bit = cur_data.map_reg_index["vm"];
	int illegal = !vext_check_load(vs2, 1, vector_cfg.sew, vm_bit);
	print_illegal_status(illegal);
	return illegal;
}

template <typename Ts2> int run_self_result(c_data &cur_data)
{
	Ts2 *vs2_data = static_cast<Ts2 *>(
		cur_data.map_preinst_typed_value["vs2"].get());

	cur_data.map_selfcheck_typed_value["rd"] = make_zero_buffer<Ts2>(1);
	Ts2 *selfcheck_data = static_cast<Ts2 *>(
		cur_data.map_selfcheck_typed_value["rd"].get());

	// vfmv.f.s: rd = vs2[0]
	// The instruction executes regardless of vstart and vl values
	selfcheck_data[0] = vs2_data[0];

	return 0;
}

template <typename Ts2> int per_run(int it, c_cfg &cur_cfg, c_data &cur_data)
{
	cur_data.set_value_from_inst(vop_inst_fields, vector_cfg.inst);

	if (random_mode) {
		cur_data.register_type_with_random<Ts2>("vs2", vector_cfg.len);
		cur_data.register_type_with_random<Ts2>("rd", 1);
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

	load_multi_vector<Ts2>(insts, cur_data, { "vs2" }, { vector_cfg.lmul },
			       { vector_cfg.len });

	load_multi_float<Ts2>(insts, cur_data, { "rd" });

	store_multi_preinst_vector<Ts2>(insts, cur_data, { "vs2" },
					{ vector_cfg.lmul },
					{ vector_cfg.len });

	store_multi_preinst_float<Ts2>(insts, cur_data, { "rd" });

	vsetvli_lmul_sew(insts, vector_cfg.lmul, vector_cfg.sew,
			 vector_cfg.len);
	csrrw(insts, CSR_VSTART, vector_cfg.vstart);
	csrrw(insts, CSR_VXRM, vector_cfg.vxrm);

	insts.push_back(vector_cfg.inst);

	store_multi_afterinst_float<Ts2>(insts, cur_data, { "rd" });

	restore_context(insts);

	run_instruction(insts);

	unpack_multi_after_run<Ts2>(cur_data, { "vs2" }, { vector_cfg.len });

	int has_illegal = check_afterinst_illegal();
	if (has_illegal >= 0)
		return has_illegal;

	save_multi_preinst_value_to_common<Ts2, Ts2>(cur_data, { "vs2", "rd" },
						     { vector_cfg.len, 1 });

	run_self_result<Ts2>(cur_data);

	int is_error = check_multi_error<Ts2>(cur_data, { "rd" }, { 1 });

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
			has_error = per_run<c_check_f<fp16, uint16_t> >(
				it, cur_cfg, cur_data);
			break;
		case sew_e32:
			has_error = per_run<c_check_f<float, uint32_t> >(
				it, cur_cfg, cur_data);
			break;
		case sew_e64:
			has_error = per_run<c_check_f<double, uint64_t> >(
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
