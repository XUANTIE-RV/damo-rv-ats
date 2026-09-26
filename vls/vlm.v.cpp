/*
 * Copyright (c) 2026 Alibaba Group.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "../common/v_common.h"

const std::vector<InstField> vop_inst_fields = {
	{ 31, 29, 0x00, true, RegClass::NotReg, "nf" },
	{ 28, 28, 0x00, true, RegClass::NotReg, "mew" },
	{ 27, 26, 0x00, true, RegClass::NotReg, "mop" },
	{ 25, 25, 0x01, true, RegClass::NotReg, "vm" },
	{ 24, 20, 0x0B, true, RegClass::NotReg, "lumop" },
	{ 19, 15, 0x00, false, RegClass::Int, "rs1" },
	{ 14, 12, 0x00, true, RegClass::NotReg, "width" },
	{ 11, 7, 0x00, false, RegClass::Vector, "vd" },
	{ 6, 0, 0x07, true, RegClass::NotReg, "opcode" }
};

int check_illegal(c_data &cur_data)
{
	/* vlm.v: EMUL is always 1 regardless of SEW/LMUL, no masking.
	 * Cannot use vext_check_load here because it computes EMUL from
	 * SEW/LMUL which does not apply to mask load/store instructions.
	 * Only need to verify nf==0 (mask load is not a segment instruction). */
	int nf = cur_data.map_reg_index["nf"];
	int illegal = (nf != 0);
	print_illegal_status(illegal);
	return illegal;
}

int run_self_result(c_data &cur_data)
{
	/* evl = ceil(vl / 8) */
	uint64_t evl = (vector_cfg.len + 7) / 8;

	uint8_t *mem_data = static_cast<uint8_t *>(
		cur_data.map_reg_typed_value["mem_data"].get());

	cur_data.map_selfcheck_typed_value["vd"] =
		c_data::process_from_common<uint8_t>(
			cur_data.map_preinst_reg_value["vd"]);
	uint8_t *selfcheck_data = static_cast<uint8_t *>(
		cur_data.map_selfcheck_typed_value["vd"].get());

	for (uint64_t j = 0; j < evl; j++) {
		if (j < vector_cfg.vstart) {
			continue;
		}

		selfcheck_data[j] = mem_data[j];
	}
	return 0;
}

int per_run(int it, c_cfg &cur_cfg, c_data &cur_data)
{
	cur_data.set_value_from_inst(vop_inst_fields, vector_cfg.inst);

	/* vlm.v: EMUL is always 1, EEW=8, evl = ceil(vl/8) */
	uint64_t emul = lmul_m1;
	uint64_t evl = (vector_cfg.len + 7) / 8;

	if (random_mode) {
		cur_data.register_type_with_random<uint8_t>("mem_data", evl);
		cur_data.register_type_with_random<uint8_t>("vd", vlenb);
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

	int rs1 = cur_data.map_reg_index["rs1"];
	int vd = cur_data.map_reg_index["vd"];

	std::vector<uint32_t> insts;

	save_context(insts);
	vzero_all(insts);

	/* Load vd initial data using EEW=8 and EMUL=1 */
	cur_data.map_reg_typed_value["vd"] =
		c_data::process_from_common<uint8_t>(
			cur_data.map_reg_value["vd"]);
	load_vector<uint8_t>(insts, vd,
			     cur_data.map_reg_typed_value["vd"].get(), emul,
			     vlenb);

	/* Store pre-instruction snapshot of vd */
	cur_data.map_preinst_typed_value["vd"] =
		make_zero_buffer<uint8_t>(vlenb);
	store_vector<uint8_t>(insts, vd,
			      cur_data.map_preinst_typed_value["vd"].get(),
			      emul, vlenb);

	/* Set actual vtype and vl for the target instruction */
	vsetvli_lmul_sew(insts, vector_cfg.lmul, vector_cfg.sew,
			 vector_cfg.len);
	csrrw(insts, CSR_VSTART, vector_cfg.vstart);

	/* Prepare memory source data and load address into rs1 */
	cur_data.map_reg_typed_value["mem_data"] =
		c_data::process_from_common<uint8_t>(
			cur_data.map_reg_value["mem_data"]);
	void *mem_ptr = cur_data.map_reg_typed_value["mem_data"].get();
	load_addr_to_gpr(insts, mem_ptr, rs1,
			 cur_data.map_reg_value["mem_data"].size());

	/* Execute the vlm.v instruction */
	insts.push_back(vector_cfg.inst);

	/* Store post-instruction vd */
	cur_data.map_afterinst_typed_value["vd"] =
		make_zero_buffer<uint8_t>(vlenb);
	store_vector<uint8_t>(insts, vd,
			      cur_data.map_afterinst_typed_value["vd"].get(),
			      emul, vlenb);

	restore_context(insts);
	run_instruction(insts);

	int has_illegal = check_afterinst_illegal();
	if (has_illegal >= 0)
		return has_illegal;

	/* Convert preinst typed values to common format for selfcheck */
	cur_data.map_preinst_reg_value["vd"] =
		c_data::process_to_common<uint8_t>(
			cur_data.map_preinst_typed_value["vd"], vlenb);

	run_self_result(cur_data);

	int is_error = check_multi_error<uint8_t>(cur_data, { "vd" }, { evl });

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
