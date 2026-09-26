/*
 * Copyright (c) 2026 Alibaba Group.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "../common/v_common.h"

/* vs1r.v: Vector Store 1 Register, EEW=8
 * Encoding: nf=0, mew=0, mop=00, vm=1, sumop=01000, width=000, opcode=0100111
 * NFIELDS=1, evl = 1 * VLEN/8 = vlenb
 * vs3 has no alignment constraint (NFIELDS=1).
 */

const std::vector<InstField> vop_inst_fields = {
	{ 31, 29, 0x00, true, RegClass::NotReg, "nf" },
	{ 28, 28, 0x00, true, RegClass::NotReg, "mew" },
	{ 27, 26, 0x00, true, RegClass::NotReg, "mop" },
	{ 25, 25, 0x01, true, RegClass::NotReg, "vm" },
	{ 24, 20, 0x08, true, RegClass::NotReg, "sumop" },
	{ 19, 15, 0x00, false, RegClass::Int, "rs1" },
	{ 14, 12, 0x00, true, RegClass::NotReg, "width" },
	{ 11, 7, 0x00, false, RegClass::Vector, "vs3" },
	{ 6, 0, 0x27, true, RegClass::NotReg, "opcode" }
};

int check_illegal(c_data &cur_data)
{
	/* NFIELDS=1: no alignment constraint on vs3 */
	int illegal = 0;
	print_illegal_status(illegal);
	return illegal;
}

int run_self_result(c_data &cur_data)
{
	uint64_t evl = vlenb;

	uint8_t *vs3_data = static_cast<uint8_t *>(
		cur_data.map_preinst_typed_value["vs3"].get());

	/* Initialize selfcheck with the original memory content */
	cur_data.map_selfcheck_typed_value["mem_result"] =
		c_data::process_from_common<uint8_t>(
			cur_data.map_preinst_reg_value["mem_result"]);
	uint8_t *selfcheck_data = static_cast<uint8_t *>(
		cur_data.map_selfcheck_typed_value["mem_result"].get());

	for (uint64_t j = 0; j < evl; j++) {
		if (j < vector_cfg.vstart) {
			continue;
		}
		selfcheck_data[j] = vs3_data[j];
	}
	return 0;
}

int per_run(int it, c_cfg &cur_cfg, c_data &cur_data)
{
	cur_data.set_value_from_inst(vop_inst_fields, vector_cfg.inst);

	uint64_t evl = vlenb;
	uint64_t nregs = 1;
	uint64_t total_bytes = nregs * vlenb;

	if (random_mode) {
		cur_data.register_type_with_random<uint8_t>("vs3", total_bytes);
		cur_data.register_type_with_random<uint8_t>("mem_result", evl);
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
	int vs3 = cur_data.map_reg_index["vs3"];

	std::vector<uint32_t> insts;

	save_context(insts);
	vzero_all(insts);

	/* Load vs3 source data into the vector register */
	cur_data.map_reg_typed_value["vs3"] =
		c_data::process_from_common<uint8_t>(
			cur_data.map_reg_value["vs3"]);
	load_vector<uint8_t>(insts, vs3,
			     cur_data.map_reg_typed_value["vs3"].get(), lmul_m1,
			     total_bytes);

	/* Store pre-instruction snapshot of vs3 */
	cur_data.map_preinst_typed_value["vs3"] =
		make_zero_buffer<uint8_t>(total_bytes);
	store_vector<uint8_t>(insts, vs3,
			      cur_data.map_preinst_typed_value["vs3"].get(),
			      lmul_m1, total_bytes);

	/* Prepare memory destination buffer with initial content and save
	 * pre-instruction snapshot */
	cur_data.map_reg_typed_value["mem_result"] =
		c_data::process_from_common<uint8_t>(
			cur_data.map_reg_value["mem_result"]);
	cur_data.map_preinst_typed_value["mem_result"] =
		c_data::process_from_common<uint8_t>(
			cur_data.map_reg_value["mem_result"]);

	/* Set vtype and vl (whole register instructions ignore these) */
	vsetvli_lmul_sew(insts, vector_cfg.lmul, vector_cfg.sew,
			 vector_cfg.len);
	csrrw(insts, CSR_VSTART, vector_cfg.vstart);

	/* Load memory destination address into rs1 */
	void *mem_ptr = cur_data.map_reg_typed_value["mem_result"].get();
	load_addr_to_gpr(insts, mem_ptr, rs1,
			 cur_data.map_reg_value["mem_data"].size());

	/* Execute the vs1r.v instruction */
	insts.push_back(vector_cfg.inst);

	restore_context(insts);
	run_instruction(insts);

	int has_illegal = check_afterinst_illegal();
	if (has_illegal >= 0)
		return has_illegal;

	/* Convert preinst typed values to common format for selfcheck */
	cur_data.map_preinst_reg_value["vs3"] =
		c_data::process_to_common<uint8_t>(
			cur_data.map_preinst_typed_value["vs3"], total_bytes);
	cur_data.map_preinst_reg_value["mem_result"] =
		c_data::process_to_common<uint8_t>(
			cur_data.map_preinst_typed_value["mem_result"], evl);

	run_self_result(cur_data);

	/* Set afterinst for memory: the actual store destination */
	cur_data.map_afterinst_typed_value["mem_result"] =
		cur_data.map_reg_typed_value["mem_result"];

	int is_error =
		check_multi_error<uint8_t>(cur_data, { "mem_result" }, { evl });

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
