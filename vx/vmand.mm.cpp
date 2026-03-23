/*
 * Copyright (c) 2026 Alibaba Group.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "../common/v_common.h"

const std::vector<InstField> vop_inst_fields = {
	{ 31, 26, 0x19, true, RegClass::NotReg, "funct6" },
	{ 25, 25, 0x01, true, RegClass::NotReg, "vm" },
	{ 24, 20, 0x00, false, RegClass::Vector, "vs2" },
	{ 19, 15, 0x00, false, RegClass::Vector, "vs1" },
	{ 14, 12, OPMVV, true, RegClass::NotReg, "funct3" },
	{ 11, 7, 0x00, false, RegClass::Vector, "vd" },
	{ 6, 0, opcode_vector, true, RegClass::NotReg, "opcode" }
};

int check_illegal(c_data &cur_data)
{
	int vs2 = cur_data.map_reg_index["vs2"];
	int vs1 = cur_data.map_reg_index["vs1"];
	int vd = cur_data.map_reg_index["vd"];
	int vm_bit = cur_data.map_reg_index["vm"];
	ValidationConfig config = { .check_align = false,
				    .check_overlap = false };
	int illegal = !(
		VectorRegValidator::validate(VregOperand::mask_dest(vd),
					     {
						     VregOperand::one_pow(vs1),
						     VregOperand::one_pow(vs2),
					     },
					     vm_bit, config));
	print_illegal_status(illegal);
	return illegal;
}

int run_self_result(c_data &cur_data)
{
	int vm_bit = cur_data.map_reg_index["vm"];
	int vs2 = cur_data.map_reg_index["vs2"];
	int vs1 = cur_data.map_reg_index["vs1"];
	int vd = cur_data.map_reg_index["vd"];

	uint8_t *vs1_data = static_cast<uint8_t *>(
		cur_data.map_preinst_typed_value["vs1"].get());
	uint8_t *vs2_data = static_cast<uint8_t *>(
		cur_data.map_preinst_typed_value["vs2"].get());

	cur_data.map_selfcheck_typed_value["vd"] =
		c_data::process_from_common<uint8_t>(
			cur_data.map_preinst_reg_value["vd"]);
	uint8_t *selfcheck_data = static_cast<uint8_t *>(
		cur_data.map_selfcheck_typed_value["vd"].get());

	for (int j = 0; j < vector_cfg.len; j++) {
		if (j < vector_cfg.vstart) {
			continue;
		}

		int byte_idx = j / 8;
		int bit_idx = j % 8;
		int vs2_bit = (vs2_data[byte_idx] >> bit_idx) & 1;
		int vs1_bit = (vs1_data[byte_idx] >> bit_idx) & 1;

		int result = vs2_bit & vs1_bit;

		if (result)
			selfcheck_data[byte_idx] |= (1u << bit_idx);
		else
			selfcheck_data[byte_idx] &= ~(1u << bit_idx);
	}

	uint8_t *uncheck_data = static_cast<uint8_t *>(
		cur_data.map_afterinst_typed_value["vd"].get());
	for (int j = vector_cfg.len; j < vlen * 8; j++) {
		int byte_idx = j / 8;
		int bit_idx = j % 8;
		selfcheck_data[byte_idx] |= uncheck_data[byte_idx] &
					    (1u << bit_idx);
	}

	return 0;
}

int per_run(int it, c_cfg &cur_cfg, c_data &cur_data)
{
	cur_data.set_value_from_inst(vop_inst_fields, vector_cfg.inst);

	if (random_mode) {
		cur_data.register_type_with_random<uint8_t>("vs1", vlen);
		cur_data.register_type_with_random<uint8_t>("vs2", vlen);
		cur_data.register_type_with_random<uint8_t>("vd", vlen);
		if (!cur_data.map_reg_index["vm"])
			cur_data.register_type_with_random<uint8_t>("vm", vlen);
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

	load_multi_vector<uint8_t, uint8_t, uint8_t, uint8_t>(
		insts, cur_data, { "vd", "vs1", "vs2", "vm" },
		{ lmul_m1, lmul_m1, lmul_m1, lmul_m1 },
		{ vlen, vlen, vlen, vlen });

	store_multi_preinst_vector<uint8_t, uint8_t, uint8_t, uint8_t>(
		insts, cur_data, { "vd", "vs1", "vs2", "vm" },
		{ lmul_m1, lmul_m1, lmul_m1, lmul_m1 },
		{ vlen, vlen, vlen, vlen });

	vsetvli_lmul_sew(insts, vector_cfg.lmul, vector_cfg.sew,
			 vector_cfg.len);
	csrrw(insts, CSR_VSTART, vector_cfg.vstart);
	csrrw(insts, CSR_VXRM, vector_cfg.vxrm);

	insts.push_back(vector_cfg.inst);

	store_multi_afterinst_vector<uint8_t>(insts, cur_data, { "vd" },
					      { lmul_m1 }, { vlen });

	restore_context(insts);

	run_instruction(insts);

	int has_illegal = check_afterinst_illegal();
	if (has_illegal >= 0)
		return has_illegal;

	save_multi_preinst_value_to_common<uint8_t, uint8_t, uint8_t, uint8_t>(
		cur_data, { "vd", "vs1", "vs2", "vm" },
		{ vlen, vlen, vlen, vlen });

	run_self_result(cur_data);

	int is_error = check_multi_error<uint8_t>(cur_data, { "vd" }, { vlen });

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

		int has_error = per_run(it, cur_cfg, cur_data);

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
