/*
 * Copyright (c) 2026 Alibaba Group.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "../common/v_common.h"

const std::vector<InstField> vop_inst_fields = {
	{ 31, 26, 0x10, true, RegClass::NotReg, "funct6" },
	{ 25, 25, 0x01, true, RegClass::NotReg, "vm" },
	{ 24, 20, 0x00, true, RegClass::NotReg, "vs2" },
	{ 19, 15, 0x00, false, RegClass::Float, "rs1" },
	{ 14, 12, OPFVF, true, RegClass::NotReg, "funct3" },
	{ 11, 7, 0x00, false, RegClass::Vector, "vd" },
	{ 6, 0, opcode_vector, true, RegClass::NotReg, "opcode" }
};

// =============================================================================
// vfmv.s.f: vd[0] = f[rs1]
// Copies the scalar floating-point register to element 0 of the
// destination vector register.
// The other elements in vd (0 < index < VLEN/SEW) are treated as tail
// elements using the current tail agnostic/undisturbed policy.
// If vstart >= vl, no operation is performed and vd is not updated.
// When vl=0, no elements are updated regardless of vstart.
// Ignores LMUL and vector register groups.
// vm=0 is reserved (illegal encoding).
// vs2 is fixed to 0x00.
// =============================================================================
int check_illegal(c_data &cur_data)
{
	// vm is fixed to 1 (vm=0 is reserved/illegal encoding)
	// vs2 is fixed to 0x00
	// Ignores LMUL, so no alignment checks needed
	// rs1 is a scalar float register, so no vector overlap checks needed
	int vd = cur_data.map_reg_index["vd"];
	int vm_bit = cur_data.map_reg_index["vm"];
	int illegal = !vext_check_load(vd, 1, vector_cfg.sew, vm_bit);
	print_illegal_status(illegal);
	return illegal;
}

template <typename Td> int run_self_result(c_data &cur_data)
{
	Td *rs1_data = static_cast<Td *>(
		cur_data.map_preinst_typed_value["rs1"].get());

	cur_data.map_selfcheck_typed_value["vd"] =
		c_data::process_from_common<Td>(
			cur_data.map_preinst_reg_value["vd"]);
	Td *selfcheck_data = static_cast<Td *>(
		cur_data.map_selfcheck_typed_value["vd"].get());

	// vfmv.s.f: vd[0] = rs1
	// If vstart >= vl, no operation is performed (vd unchanged)
	// Otherwise, copy rs1 to vd[0], other elements are tail
	if (vector_cfg.vstart < vector_cfg.len) {
		selfcheck_data[0] = rs1_data[0];
	}

	return 0;
}

template <typename Td> int per_run(int it, c_cfg &cur_cfg, c_data &cur_data)
{
	cur_data.set_value_from_inst(vop_inst_fields, vector_cfg.inst);

	if (random_mode) {
		cur_data.register_type_with_random<Td>("rs1", 1);
		cur_data.register_type_with_random<Td>("vd", vector_cfg.len);
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

	load_multi_vector<Td>(insts, cur_data, { "vd" }, { vector_cfg.lmul },
			      { vector_cfg.len });

	load_multi_float<Td>(insts, cur_data, { "rs1" });

	store_multi_preinst_vector<Td>(insts, cur_data, { "vd" },
				       { vector_cfg.lmul }, { vector_cfg.len });

	store_multi_preinst_float<Td>(insts, cur_data, { "rs1" });

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

	unpack_multi_after_run<Td>(cur_data, { "vd" }, { vector_cfg.len });

	int has_illegal = check_afterinst_illegal();
	if (has_illegal >= 0)
		return has_illegal;

	save_multi_preinst_value_to_common<Td, Td>(cur_data, { "rs1", "vd" },
						   { 1, vector_cfg.len });

	run_self_result<Td>(cur_data);

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
