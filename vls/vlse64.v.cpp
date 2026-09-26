/*
 * Copyright (c) 2026 Alibaba Group.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "../common/v_common.h"

const std::vector<InstField> vop_inst_fields = {
	{ 31, 29, 0x00, true, RegClass::NotReg, "nf" },
	{ 28, 28, 0x00, true, RegClass::NotReg, "mew" },
	{ 27, 26, 0x02, true, RegClass::NotReg, "mop" },
	{ 25, 25, 0x00, false, RegClass::NotReg, "vm" },
	{ 24, 20, 0x00, false, RegClass::Int, "rs2" },
	{ 19, 15, 0x00, false, RegClass::Int, "rs1" },
	{ 14, 12, 0x07, true, RegClass::NotReg, "width" },
	{ 11, 7, 0x00, false, RegClass::Vector, "vd" },
	{ 6, 0, 0x07, true, RegClass::NotReg, "opcode" }
};

int check_illegal(c_data &cur_data)
{
	int nf = cur_data.map_reg_index["nf"];
	int vm_bit = cur_data.map_reg_index["vm"];
	int vd = cur_data.map_reg_index["vd"];
	int illegal = !(vext_check_load(vd, nf, sew_e64, vm_bit));
	print_illegal_status(illegal);
	return illegal;
}

int run_self_result(c_data &cur_data)
{
	int vm_bit = cur_data.map_reg_index["vm"];

	uint8_t *mem_data = static_cast<uint8_t *>(
		cur_data.map_reg_typed_value["mem_data"].get());

	uint8_t *vm_data = nullptr;
	if (!vm_bit) {
		vm_data = static_cast<uint8_t *>(
			cur_data.map_preinst_typed_value["vm"].get());
	}

	cur_data.map_selfcheck_typed_value["vd"] =
		c_data::process_from_common<uint64_t>(
			cur_data.map_preinst_reg_value["vd"]);
	uint64_t *selfcheck_data = static_cast<uint64_t *>(
		cur_data.map_selfcheck_typed_value["vd"].get());

	int64_t stride = (int64_t)vector_cfg.rs1_data;

	for (uint64_t j = 0; j < vector_cfg.len; j++) {
		int row = j / 8;
		int col = j % 8;

		if (j < vector_cfg.vstart ||
		    (!vm_bit && !(vm_data[row] & (1ull << col)))) {
			continue;
		}

		int64_t byte_offset = (int64_t)j * stride;
		selfcheck_data[j] =
			*(uint64_t *)((uint8_t *)mem_data + byte_offset);
	}
	return 0;
}

int per_run(int it, c_cfg &cur_cfg, c_data &cur_data)
{
	cur_data.set_value_from_inst(vop_inst_fields, vector_cfg.inst);

	/* EMUL for vlse64: EEW=64(sew_e64=3), so emul_ratio = sew_e64 - sew */
	uint64_t emul =
		lmul_pow(vector_cfg.lmul, (int)sew_e64 - (int)vector_cfg.sew);

	/* Compute EMUL-based register group size in bytes */
	int emul_signed = (int)sew_e64 - (int)vector_cfg.sew +
			  ((vector_cfg.lmul > 4) ? (int)vector_cfg.lmul - 8 :
						   (int)vector_cfg.lmul);
	uint64_t vd_bytes;
	if (emul_signed >= 0)
		vd_bytes = vlenb << emul_signed;
	else
		vd_bytes = vlenb >> (-emul_signed);

	/* Generate stride: random multiple of EEW bytes, range [1, 4] */
	int64_t stride = (int64_t)(get_rand_bound<int>(1, 4)) *
			 (int64_t)sizeof(uint64_t);
	/* Compute memory buffer size based on stride */
	uint64_t mem_size;
	if (vector_cfg.len == 0)
		mem_size = sizeof(uint64_t);
	else
		mem_size = stride * (vector_cfg.len - 1) + sizeof(uint64_t);

	if (random_mode) {
		cur_data.register_type_with_random<uint8_t>("mem_data",
							    mem_size);
		cur_data.register_type_with_random<uint64_t>(
			"vd", vd_bytes / sizeof(uint64_t));
		if (!cur_data.map_reg_index["vm"])
			cur_data.register_type_with_random<uint8_t>("vm",
								    vlenb);
		/* Store stride in rs1_data field for selfcheck */
		vector_cfg.rs1_data = (uint64_t)stride;
		cur_data.set_value_to_cfg(cur_cfg);
		cur_cfg.DESC = cur_data.get_DESC_from_inst(vop_inst_fields,
							   vector_cfg.inst);
		if (record_mode) {
			global_cfg.push_back(cur_cfg);
		}
	} else {
		cur_data.set_value_from_cfg(cur_cfg);
		stride = (int64_t)vector_cfg.rs1_data;
		if (vector_cfg.len == 0)
			mem_size = sizeof(uint64_t);
		else
			mem_size = stride * (vector_cfg.len - 1) +
				   sizeof(uint64_t);
	}

	global_flag_ptr->illegal = check_illegal(cur_data);

	int vm_bit = cur_data.map_reg_index["vm"];
	int rs1 = cur_data.map_reg_index["rs1"];
	int rs2 = cur_data.map_reg_index["rs2"];
	int vd = cur_data.map_reg_index["vd"];

	/* Skip iteration when rs1==rs2: load_reg would overwrite the base
	 * address with the stride value, causing illegal memory access.
	 * Cannot rely on check_illegal alone because the framework still
	 * executes the full instruction sequence even for illegal cases. */
	if (rs1 == rs2) {
		reset_data();
		return 0;
	}

	std::vector<uint32_t> insts;

	save_context(insts);
	vzero_all(insts);

	/* Load vd initial data using EEW=64 and EMUL */
	cur_data.map_reg_typed_value["vd"] =
		c_data::process_from_common<uint64_t>(
			cur_data.map_reg_value["vd"]);
	load_vector<uint64_t>(insts, vd,
			      cur_data.map_reg_typed_value["vd"].get(), emul,
			      vd_bytes / sizeof(uint64_t));

	if (!vm_bit) {
		load_single_vector<uint8_t>(insts, cur_data, "vm", lmul_m1,
					    vlenb);
	}

	/* Store pre-instruction snapshots */
	cur_data.map_preinst_typed_value["vd"] =
		make_zero_buffer<uint64_t>(vd_bytes / sizeof(uint64_t));
	store_vector<uint64_t>(insts, vd,
			       cur_data.map_preinst_typed_value["vd"].get(),
			       emul, vd_bytes / sizeof(uint64_t));
	if (!vm_bit) {
		cur_data.map_preinst_typed_value["vm"] =
			make_zero_buffer<uint8_t>(vlenb);
		store_vector<uint8_t>(
			insts, 0, cur_data.map_preinst_typed_value["vm"].get(),
			lmul_m1, vlenb);
	}

	/* Set actual vtype and vl for the target instruction */
	vsetvli_lmul_sew(insts, vector_cfg.lmul, vector_cfg.sew,
			 vector_cfg.len);
	csrrw(insts, CSR_VSTART, vector_cfg.vstart);

	/* Prepare memory source data and load base address into rs1 */
	cur_data.map_reg_typed_value["mem_data"] =
		c_data::process_from_common<uint8_t>(
			cur_data.map_reg_value["mem_data"]);
	void *mem_ptr = cur_data.map_reg_typed_value["mem_data"].get();
	load_addr_to_gpr(insts, mem_ptr, rs1,
			 cur_data.map_reg_value["mem_data"].size());

	/* Load stride value into rs2 */
	val_placeholder[val_top++] = (uint64_t)stride;
	uint64_t stride_ptr = (uint64_t)(&(val_placeholder[val_top - 1]));
	load_reg(insts, stride_ptr, rs2);

	/* Execute the vlse64.v instruction */
	insts.push_back(vector_cfg.inst);

	/* Store post-instruction vd */
	cur_data.map_afterinst_typed_value["vd"] =
		make_zero_buffer<uint64_t>(vd_bytes / sizeof(uint64_t));
	store_vector<uint64_t>(insts, vd,
			       cur_data.map_afterinst_typed_value["vd"].get(),
			       emul, vd_bytes / sizeof(uint64_t));

	restore_context(insts);
	run_instruction(insts);

	int has_illegal = check_afterinst_illegal();
	if (has_illegal >= 0)
		return has_illegal;

	/* Convert preinst typed values to common format for selfcheck */
	cur_data.map_preinst_reg_value["vd"] =
		c_data::process_to_common<uint64_t>(
			cur_data.map_preinst_typed_value["vd"],
			vd_bytes / sizeof(uint64_t));
	if (!vm_bit) {
		cur_data.map_preinst_reg_value["vm"] =
			c_data::process_to_common<uint8_t>(
				cur_data.map_preinst_typed_value["vm"], vlenb);
	}

	run_self_result(cur_data);

	int is_error = check_multi_error<uint64_t>(cur_data, { "vd" },
						   { vector_cfg.len });

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
