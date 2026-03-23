/*
 * Copyright (c) 2026 Alibaba Group.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "../common/v_common.h"

const std::vector<InstField> vop_inst_fields = {
	{ 31, 26, 0x12, true, RegClass::NotReg, "funct6" },
	{ 25, 25, 0x00, false, RegClass::NotReg, "vm" },
	{ 24, 20, 0x00, false, RegClass::Vector, "vs2" },
	{ 19, 15, 0x09, true, RegClass::NotReg, "rs1" },
	{ 14, 12, OPMVV, true, RegClass::NotReg, "funct3" },
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
	int illegal = !(
		VectorRegValidator::validate(VregOperand::one_pow(vd),
					     {
						     VregOperand::one_pow(vs2),
					     },
					     vm_bit, config));
	print_illegal_status(illegal);
	return illegal;
}

template <typename Ts2, typename Td> int run_self_result(c_data &cur_data)
{
	int vm_bit = cur_data.map_reg_index["vm"];
	int vs2 = cur_data.map_reg_index["vs2"];
	int vd = cur_data.map_reg_index["vd"];
	int SEW = sizeof(Ts2) * 8;
	int vlmax = vlen / sizeof(Td);

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

		Ts2 val = vs2_data[j];
		Td result = 0;

		if constexpr (sizeof(Ts2) == 1) {
			result = val;
		} else if constexpr (sizeof(Ts2) == 2) {
			result = ((val & 0xFF) << 8) | ((val >> 8) & 0xFF);
		} else if constexpr (sizeof(Ts2) == 4) {
			result = ((val & 0xFF) << 24) |
				 (((val >> 8) & 0xFF) << 16) |
				 (((val >> 16) & 0xFF) << 8) |
				 ((val >> 24) & 0xFF);
		} else if constexpr (sizeof(Ts2) == 8) {
			result = ((val & 0xFFULL) << 56) |
				 (((val >> 8) & 0xFFULL) << 48) |
				 (((val >> 16) & 0xFFULL) << 40) |
				 (((val >> 24) & 0xFFULL) << 32) |
				 (((val >> 32) & 0xFFULL) << 24) |
				 (((val >> 40) & 0xFFULL) << 16) |
				 (((val >> 48) & 0xFFULL) << 8) |
				 ((val >> 56) & 0xFFULL);
		}

		selfcheck_data[j] = result;
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

	load_multi_vector<Td, Ts2, uint8_t>(
		insts, cur_data, { "vd", "vs2", "vm" },
		{ vector_cfg.lmul, vector_cfg.lmul, lmul_m1 },
		{ vector_cfg.len, vector_cfg.len, vlen });

	store_multi_preinst_vector<Td, Ts2, uint8_t>(
		insts, cur_data, { "vd", "vs2", "vm" },
		{ vector_cfg.lmul, vector_cfg.lmul, lmul_m1 },
		{ vector_cfg.len, vector_cfg.len, vlen });

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

	int has_illegal = check_afterinst_illegal();
	if (has_illegal >= 0)
		return has_illegal;

	save_multi_preinst_value_to_common<Td, Ts2, uint8_t>(
		cur_data, { "vd", "vs2", "vm" },
		{ vector_cfg.len, vector_cfg.len, vlen });

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
			has_error = per_run<uint8_t, uint8_t>(it, cur_cfg,
							      cur_data);
			break;
		case sew_e16:
			has_error = per_run<uint16_t, uint16_t>(it, cur_cfg,
								cur_data);
			break;
		case sew_e32:
			has_error = per_run<uint32_t, uint32_t>(it, cur_cfg,
								cur_data);
			break;
		case sew_e64:
			has_error = per_run<uint64_t, uint64_t>(it, cur_cfg,
								cur_data);
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
