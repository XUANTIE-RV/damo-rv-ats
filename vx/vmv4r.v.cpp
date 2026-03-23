/*
 * Copyright (c) 2026 Alibaba Group.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "../common/v_common.h"

const std::vector<InstField> vop_inst_fields = {
	{ 31, 26, 0x27, true, RegClass::NotReg, "funct6" },
	{ 25, 25, 0x01, true, RegClass::NotReg, "vm" },
	{ 24, 20, 0x00, false, RegClass::Vector, "vs2" },
	{ 19, 15, 0x03, true, RegClass::NotReg, "simm" },
	{ 14, 12, OPIVI, true, RegClass::NotReg, "funct3" },
	{ 11, 7, 0x00, false, RegClass::Vector, "vd" },
	{ 6, 0, opcode_vector, true, RegClass::NotReg, "opcode" }
};

// =============================================================================
// vmv4r.v: Copy 4 vector registers. NREG=4, simm=0x03.
// vd and vs2 must be 4-aligned.
// EEW=SEW, EMUL=4, evl=4*VLEN/SEW.
// =============================================================================
int check_illegal(c_data &cur_data)
{
	int vs2 = cur_data.map_reg_index["vs2"];
	int vd = cur_data.map_reg_index["vd"];
	// NREG=4: vd and vs2 must be 4-aligned
	int illegal = !((vd % 4 == 0) && (vs2 % 4 == 0));
	print_illegal_status(illegal);
	return illegal;
}

template <typename Td> int run_self_result(c_data &cur_data)
{
	int vs2 = cur_data.map_reg_index["vs2"];
	int vd = cur_data.map_reg_index["vd"];

	Td *vs2_data = static_cast<Td *>(
		cur_data.map_preinst_typed_value["vs2"].get());

	cur_data.map_selfcheck_typed_value["vd"] =
		c_data::process_from_common<Td>(
			cur_data.map_preinst_reg_value["vd"]);
	Td *selfcheck_data = static_cast<Td *>(
		cur_data.map_selfcheck_typed_value["vd"].get());

	// Copy 4 registers: evl = 4*VLEN/SEW
	int evl = 4 * vlen / sizeof(Td);
	for (int j = 0; j < evl; j++) {
		selfcheck_data[j] = vs2_data[j];
	}

	return 0;
}

template <typename Td> int per_run(int it, c_cfg &cur_cfg, c_data &cur_data)
{
	cur_data.set_value_from_inst(vop_inst_fields, vector_cfg.inst);

	int evl = 4 * vlen / sizeof(Td);

	if (random_mode) {
		cur_data.register_type_with_random<Td>("vs2", evl);
		cur_data.register_type_with_random<Td>("vd", evl);
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

	// EMUL=4 => lmul_m4
	load_multi_vector<Td, Td>(insts, cur_data, { "vd", "vs2" },
				  { lmul_m4, lmul_m4 },
				  { (uint64_t)evl, (uint64_t)evl });

	store_multi_preinst_vector<Td, Td>(insts, cur_data, { "vd", "vs2" },
					   { lmul_m4, lmul_m4 },
					   { (uint64_t)evl, (uint64_t)evl });

	vsetvli_lmul_sew(insts, vector_cfg.lmul, vector_cfg.sew,
			 vector_cfg.len);
	csrrw(insts, CSR_VSTART, vector_cfg.vstart);
	csrrw(insts, CSR_VXRM, vector_cfg.vxrm);

	insts.push_back(vector_cfg.inst);

	store_multi_afterinst_vector<Td>(insts, cur_data, { "vd" }, { lmul_m4 },
					 { (uint64_t)evl });

	restore_context(insts);

	run_instruction(insts);

	int has_illegal = check_afterinst_illegal();
	if (has_illegal >= 0)
		return has_illegal;

	save_multi_preinst_value_to_common<Td, Td>(
		cur_data, { "vd", "vs2" }, { (uint64_t)evl, (uint64_t)evl });

	run_self_result<Td>(cur_data);

	int is_error =
		check_multi_error<Td>(cur_data, { "vd" }, { (uint64_t)evl });

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
			has_error = per_run<uint8_t>(it, cur_cfg, cur_data);
			break;
		case sew_e16:
			has_error = per_run<uint16_t>(it, cur_cfg, cur_data);
			break;
		case sew_e32:
			has_error = per_run<uint32_t>(it, cur_cfg, cur_data);
			break;
		case sew_e64:
			has_error = per_run<uint64_t>(it, cur_cfg, cur_data);
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
