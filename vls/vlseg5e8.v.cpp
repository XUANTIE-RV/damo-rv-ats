/*
 * Copyright (c) 2026 Alibaba Group.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "../common/v_common.h"

const std::vector<InstField> vop_inst_fields = {
	{ 31, 29, 0x04, true, RegClass::NotReg, "nf" },
	{ 28, 28, 0x00, true, RegClass::NotReg, "mew" },
	{ 27, 26, 0x00, true, RegClass::NotReg, "mop" },
	{ 25, 25, 0x00, false, RegClass::NotReg, "vm" },
	{ 24, 20, 0x00, true, RegClass::NotReg, "lumop" },
	{ 19, 15, 0x00, false, RegClass::Int, "rs1" },
	{ 14, 12, 0x00, true, RegClass::NotReg, "width" },
	{ 11, 7, 0x00, false, RegClass::Vector, "vd" },
	{ 6, 0, 0x07, true, RegClass::NotReg, "opcode" }
};

static constexpr int NFIELDS = 5;

int check_illegal(c_data &cur_data)
{
	int nf = cur_data.map_reg_index["nf"];
	int vm_bit = cur_data.map_reg_index["vm"];
	int vd = cur_data.map_reg_index["vd"];
	int illegal = !(vext_check_load(vd, nf + 1, sew_e8, vm_bit));
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

	for (int f = 0; f < NFIELDS; f++) {
		std::string field_name = "vd_f" + std::to_string(f);

		cur_data.map_selfcheck_typed_value[field_name] =
			c_data::process_from_common<uint8_t>(
				cur_data.map_preinst_reg_value[field_name]);
		uint8_t *selfcheck_data = static_cast<uint8_t *>(
			cur_data.map_selfcheck_typed_value[field_name].get());

		for (uint64_t j = 0; j < vector_cfg.len; j++) {
			int row = j / 8;
			int col = j % 8;

			if (j < vector_cfg.vstart ||
			    (!vm_bit && !(vm_data[row] & (1ull << col)))) {
				continue;
			}

			selfcheck_data[j] = mem_data[j * NFIELDS + f];
		}
	}
	return 0;
}

int per_run(int it, c_cfg &cur_cfg, c_data &cur_data)
{
	cur_data.set_value_from_inst(vop_inst_fields, vector_cfg.inst);

	/* EMUL for vlseg5e8: EEW=8(sew_e8), so emul_ratio = sew_e8 - sew */
	uint64_t emul =
		lmul_pow(vector_cfg.lmul, -(int)vector_cfg.sew + (int)sew_e8);

	/* Per-field vector register stride (capped at 1 for fractional EMUL). */
	int vd_stride = (emul > 4) ? 1 : (1 << emul);

	/* Compute EMUL-based register group size in bytes */
	int emul_signed = (int)sew_e8 - (int)vector_cfg.sew +
			  ((vector_cfg.lmul > 4) ? (int)vector_cfg.lmul - 8 :
						   (int)vector_cfg.lmul);
	uint64_t field_bytes;
	if (emul_signed >= 0)
		field_bytes = vlenb << emul_signed;
	else
		field_bytes = vlenb >> (-emul_signed);

	uint64_t mem_elements = vector_cfg.len * NFIELDS;

	if (random_mode) {
		cur_data.register_type_with_random<uint8_t>("mem_data",
							    mem_elements);
		for (int f = 0; f < NFIELDS; f++) {
			std::string field_name = "vd_f" + std::to_string(f);
			cur_data.register_type_with_random<uint8_t>(
				field_name, field_bytes / sizeof(uint8_t));
		}
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

	int vm_bit = cur_data.map_reg_index["vm"];
	int rs1 = cur_data.map_reg_index["rs1"];
	int vd = cur_data.map_reg_index["vd"];

	std::vector<uint32_t> insts;

	save_context(insts);
	vzero_all(insts);

	/* Load initial data for each field register group */
	for (int f = 0; f < NFIELDS; f++) {
		std::string field_name = "vd_f" + std::to_string(f);
		cur_data.map_reg_typed_value[field_name] =
			c_data::process_from_common<uint8_t>(
				cur_data.map_reg_value[field_name]);
		if (vd + f * vd_stride >= 32)
			continue;
		load_vector<uint8_t>(
			insts, vd + f * vd_stride,
			cur_data.map_reg_typed_value[field_name].get(), emul,
			field_bytes / sizeof(uint8_t));
	}

	if (!vm_bit) {
		load_single_vector<uint8_t>(insts, cur_data, "vm", lmul_m1,
					    vlenb);
	}

	/* Store pre-instruction snapshots for each field */
	for (int f = 0; f < NFIELDS; f++) {
		std::string field_name = "vd_f" + std::to_string(f);
		cur_data.map_preinst_typed_value[field_name] =
			make_zero_buffer<uint8_t>(field_bytes /
						  sizeof(uint8_t));
		if (vd + f * vd_stride >= 32)
			continue;
		store_vector<uint8_t>(
			insts, vd + f * vd_stride,
			cur_data.map_preinst_typed_value[field_name].get(),
			emul, field_bytes / sizeof(uint8_t));
	}
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

	/* Prepare memory source data and load address into rs1 */
	cur_data.map_reg_typed_value["mem_data"] =
		c_data::process_from_common<uint8_t>(
			cur_data.map_reg_value["mem_data"]);
	void *mem_ptr = cur_data.map_reg_typed_value["mem_data"].get();
	load_addr_to_gpr(insts, mem_ptr, rs1,
			 cur_data.map_reg_value["mem_data"].size());

	/* Execute the vlseg5e8.v instruction */
	insts.push_back(vector_cfg.inst);

	/* Store post-instruction results for each field */
	for (int f = 0; f < NFIELDS; f++) {
		std::string field_name = "vd_f" + std::to_string(f);
		cur_data.map_afterinst_typed_value[field_name] =
			make_zero_buffer<uint8_t>(field_bytes /
						  sizeof(uint8_t));
		if (vd + f * vd_stride >= 32)
			continue;
		store_vector<uint8_t>(
			insts, vd + f * vd_stride,
			cur_data.map_afterinst_typed_value[field_name].get(),
			emul, field_bytes / sizeof(uint8_t));
	}

	restore_context(insts);
	run_instruction(insts);

	int has_illegal = check_afterinst_illegal();
	if (has_illegal >= 0)
		return has_illegal;

	/* Convert preinst typed values to common format for selfcheck */
	for (int f = 0; f < NFIELDS; f++) {
		std::string field_name = "vd_f" + std::to_string(f);
		cur_data.map_preinst_reg_value[field_name] =
			c_data::process_to_common<uint8_t>(
				cur_data.map_preinst_typed_value[field_name],
				field_bytes / sizeof(uint8_t));
	}
	if (!vm_bit) {
		cur_data.map_preinst_reg_value["vm"] =
			c_data::process_to_common<uint8_t>(
				cur_data.map_preinst_typed_value["vm"], vlenb);
	}

	run_self_result(cur_data);

	std::vector<std::string> check_names;
	std::vector<uint64_t> check_sizes;
	for (int f = 0; f < NFIELDS; f++) {
		check_names.push_back("vd_f" + std::to_string(f));
		check_sizes.push_back(vector_cfg.len);
	}

	int is_error =
		check_multi_error<uint8_t>(cur_data, check_names, check_sizes);

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
