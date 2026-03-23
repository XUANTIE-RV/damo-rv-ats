/*
 * Copyright (c) 2026 Alibaba Group.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "../common/v_common.h"

const std::vector<InstField> vop_inst_fields = {
	{ 31, 26, 0x0E, true, RegClass::NotReg, "funct6" },
	{ 25, 25, 0x00, false, RegClass::NotReg, "vm" },
	{ 24, 20, 0x00, false, RegClass::Vector, "vs2" },
	{ 19, 15, 0x00, false, RegClass::Vector, "vs1" },
	{ 14, 12, OPIVV, true, RegClass::NotReg, "funct3" },
	{ 11, 7, 0x00, false, RegClass::Vector, "vd" },
	{ 6, 0, opcode_vector, true, RegClass::NotReg, "opcode" }
};

// =============================================================================
// vrgatherei16.vv: vd[i] = (vs1[i] >= VLMAX) ? 0 : vs2[vs1[i]]
// Index vector vs1 has EEW=16, so vs1's EMUL = (16/SEW)*LMUL.
// vd must not overlap vs1 or vs2.
// =============================================================================

// Compute the EMUL for the 16-bit index vector
// emul_index = (16/SEW) * LMUL
// In encoded form: lmul_index = lmul + (sew_e16 - sew)
// where sew_e16 = 4 (log2(16)=4), sew values: e8=3, e16=4, e32=5, e64=6
static int get_index_lmul()
{
	int tmp_lmul = (vector_cfg.lmul > 4) ? (int)vector_cfg.lmul - 8 :
					       (int)vector_cfg.lmul;
	int tmp_sew = (int)vector_cfg.sew;
	int index_lmul = tmp_lmul + ((int)sew_e16 - tmp_sew);
	// Clamp to valid range
	if (index_lmul < -3)
		index_lmul = -3;
	if (index_lmul > 3)
		index_lmul = 3;
	if (index_lmul < 0)
		index_lmul = index_lmul + 8;
	return index_lmul;
}

static int get_index_len()
{
	int idx_lmul = get_index_lmul();
	int idx_size = (idx_lmul > 4) ? 1 : 1 << idx_lmul;
	return idx_size * vlen /
	       2; // EEW=16, so elements per register = vlen/16bits = vlen/2
}

int check_illegal(c_data &cur_data)
{
	int vm_bit = cur_data.map_reg_index["vm"];
	int vs2 = cur_data.map_reg_index["vs2"];
	int vs1 = cur_data.map_reg_index["vs1"];
	int vd = cur_data.map_reg_index["vd"];

	int vd_size = (vector_cfg.lmul > 4) ? 1 : 1 << vector_cfg.lmul;
	int idx_lmul = get_index_lmul();
	int vs1_size = (idx_lmul > 4) ? 1 : 1 << idx_lmul;

	// Check EMUL validity: index_lmul must be in [-3, 3] range
	int tmp_lmul = (vector_cfg.lmul > 4) ? (int)vector_cfg.lmul - 8 :
					       (int)vector_cfg.lmul;
	int tmp_sew = (int)vector_cfg.sew;
	int raw_index_lmul = tmp_lmul + ((int)sew_e16 - tmp_sew);

	ValidationConfig config = { .check_align = true,
				    .check_overlap = true,
				    .check_vm = true,
				    .force_no_overlap = true };
	int illegal = !(VectorRegValidator::validate(
				VregOperand::one_pow(vd),
				{
					VregOperand::custom(
						vs1, sew_e16 - vector_cfg.sew),
					VregOperand::one_pow(vs2),
				},
				vm_bit, config) &&
			raw_index_lmul >= -3 && raw_index_lmul <= 3) ||
		      is_overlapped(vd, vd_size, vs1, vs1_size);
	print_illegal_status(illegal);
	return illegal;
}

template <typename Ts2, typename Td> int run_self_result(c_data &cur_data)
{
	int vm_bit = cur_data.map_reg_index["vm"];
	int vs2 = cur_data.map_reg_index["vs2"];
	int vs1 = cur_data.map_reg_index["vs1"];
	int vd = cur_data.map_reg_index["vd"];
	int SEW = sizeof(Ts2) * 8;
	int vlmax = vlen / sizeof(Td);

	// vs1 is always uint16_t (EEW=16)
	uint16_t *vs1_data = static_cast<uint16_t *>(
		cur_data.map_preinst_typed_value["vs1"].get());
	Ts2 *vs2_data = static_cast<Ts2 *>(
		cur_data.map_preinst_typed_value["vs2"].get());

	uint8_t *vm_data = nullptr;
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
		if (j < vector_cfg.vstart)
			continue;

		if (!vm_bit) {
			int row = j / 8;
			int col = j % 8;
			if (!(vm_data[row] & (1ull << col)))
				continue;
		}

		uint64_t idx = static_cast<uint64_t>(vs1_data[j]);
		if (idx >= (uint64_t)vlmax) {
			selfcheck_data[j] = static_cast<Td>(0);
		} else {
			selfcheck_data[j] = static_cast<Td>(vs2_data[idx]);
		}
	}

	return 0;
}

template <typename Ts2, typename Td>
int per_run(int it, c_cfg &cur_cfg, c_data &cur_data)
{
	cur_data.set_value_from_inst(vop_inst_fields, vector_cfg.inst);

	int idx_lmul = get_index_lmul();
	int idx_len = vector_cfg.len; // number of elements matches vd
	uint64_t vs2_len =
		std::max(vlen / sizeof(Ts2),
			 vector_cfg.len); // number of elements matches vs2

	if (random_mode) {
		cur_data.register_type_with_random<uint16_t>("vs1", idx_len);
		cur_data.register_type_with_random<Ts2>("vs2", vs2_len);
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

	load_multi_vector<Td, uint16_t, Ts2, uint8_t>(
		insts, cur_data, { "vd", "vs1", "vs2", "vm" },
		{ vector_cfg.lmul, (uint64_t)idx_lmul, vector_cfg.lmul,
		  lmul_m1 },
		{ vector_cfg.len, (uint64_t)idx_len, vs2_len, vlen });

	store_multi_preinst_vector<Td, uint16_t, Ts2, uint8_t>(
		insts, cur_data, { "vd", "vs1", "vs2", "vm" },
		{ vector_cfg.lmul, (uint64_t)idx_lmul, vector_cfg.lmul,
		  lmul_m1 },
		{ vector_cfg.len, (uint64_t)idx_len, vs2_len, vlen });

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

	save_multi_preinst_value_to_common<Td, uint16_t, Ts2, uint8_t>(
		cur_data, { "vd", "vs1", "vs2", "vm" },
		{ vector_cfg.len, (uint64_t)idx_len, vs2_len, vlen });

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
