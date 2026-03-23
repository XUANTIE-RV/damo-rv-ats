/*
 * Copyright (c) 2026 Alibaba Group.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "../common/v_common.h"

const std::vector<InstField> vop_inst_fields = {
	{ 31, 26, 0x31, true, RegClass::NotReg, "funct6" },
	{ 25, 25, 0x00, false, RegClass::NotReg, "vm" },
	{ 24, 20, 0x00, false, RegClass::Vector, "vs2" },
	{ 19, 15, 0x00, false, RegClass::Vector, "vs1" },
	{ 14, 12, OPIVV, true, RegClass::NotReg, "funct3" },
	{ 11, 7, 0x00, false, RegClass::Vector, "vd" },
	{ 6, 0, opcode_vector, true, RegClass::NotReg, "opcode" }
};

// =============================================================================
// vwredsum.vs: 2*SEW = 2*SEW + sum(sign-extend(SEW))
// Signed widening sum reduction.
// Sign-extends SEW-wide vector elements to 2*SEW before summing,
// then adds the 2*SEW-width scalar element vs1[0].
// Result stored in vd[0] (2*SEW-width).
// Overflows wrap around.
// vstart != 0 raises illegal instruction exception.
// =============================================================================
int check_illegal(c_data &cur_data)
{
	int vm_bit = cur_data.map_reg_index["vm"];
	int vs2 = cur_data.map_reg_index["vs2"];
	int vs1 = cur_data.map_reg_index["vs1"];
	int vd = cur_data.map_reg_index["vd"];
	// Widening reduction check combines:
	//   1. Standard reduction checks (is_legal_reduction):
	//      - vs2 alignment to LMUL (Section 3.4.2)
	//      - vstart == 0 (reduction trap rule)
	//   2. Widening-specific checks:
	//      - SEW < e64 (2*SEW must exist, max SEW for widening is e32)
	//      - LMUL != m8 (2*LMUL must be valid)
	// vd and vs1 are scalar operands (lmul_m1), no alignment/overlap checks needed.
	// vd can overlap all source operands including mask register.
	ValidationConfig config = { .check_align = true, .check_vstart = true };
	int illegal = !(VectorRegValidator::validate(
			      VregOperand::none(),
			      {
				      VregOperand::none(),
				      VregOperand::one_pow(vs2),
			      },
			      vm_bit, config)) ||
		      vector_cfg.sew >= sew_e64;
	print_illegal_status(illegal);
	return illegal;
}

template <typename Ts2, typename Td> int run_self_result(c_data &cur_data)
{
	int vm_bit = cur_data.map_reg_index["vm"];
	int vd = cur_data.map_reg_index["vd"];

	Td *vs1_data = static_cast<Td *>(
		cur_data.map_preinst_typed_value["vs1"].get());
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

	// Initialize accumulator with scalar operand vs1[0] (2*SEW-width)
	Td accumulator = static_cast<Td>(vs1_data[0]);

	for (int j = 0; j < vector_cfg.len; j++) {
		int row = j / 8;
		int col = j % 8;
		// Skip inactive elements (masked off)
		if (!vm_bit && !(vm_data[row] & (1ull << col))) {
			continue;
		}

		// Sign-extend SEW-wide element to 2*SEW before adding
		typedef typename std::make_signed<Ts2>::type STs2;
		accumulator = accumulator +
			      static_cast<Td>(static_cast<STs2>(vs2_data[j]));
	}

	selfcheck_data[0] = accumulator;
	return 0;
}

template <typename Ts2, typename Td>
int per_run(int it, c_cfg &cur_cfg, c_data &cur_data)
{
	cur_data.set_value_from_inst(vop_inst_fields, vector_cfg.inst);

	if (random_mode) {
		// vs1 and vd are scalar operands (2*SEW-width), stored in single vector register
		cur_data.register_type_with_random<Td>("vs1", vlen);
		cur_data.register_type_with_random<Ts2>("vs2", vector_cfg.len);
		cur_data.register_type_with_random<Td>("vd", vlen);
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

	// vs1 and vd use lmul_m1 (scalar operands, 2*SEW-width)
	// vs2 uses vector_cfg.lmul (SEW-width vector)
	load_multi_vector<Td, Td, Ts2, uint8_t>(
		insts, cur_data, { "vd", "vs1", "vs2", "vm" },
		{ lmul_m1, lmul_m1, vector_cfg.lmul, lmul_m1 },
		{ vlen, vlen, vector_cfg.len, vlen });

	store_multi_preinst_vector<Td, Td, Ts2, uint8_t>(
		insts, cur_data, { "vd", "vs1", "vs2", "vm" },
		{ lmul_m1, lmul_m1, vector_cfg.lmul, lmul_m1 },
		{ vlen, vlen, vector_cfg.len, vlen });

	vsetvli_lmul_sew(insts, vector_cfg.lmul, vector_cfg.sew,
			 vector_cfg.len);
	csrrw(insts, CSR_VSTART, vector_cfg.vstart);
	csrrw(insts, CSR_VXRM, vector_cfg.vxrm);

	insts.push_back(vector_cfg.inst);

	store_multi_afterinst_vector<Td>(insts, cur_data, { "vd" }, { lmul_m1 },
					 { vlen });

	restore_context(insts);

	run_instruction(insts);

	int has_illegal = check_afterinst_illegal();
	if (has_illegal >= 0)
		return has_illegal;

	save_multi_preinst_value_to_common<Td, Td, Ts2, uint8_t>(
		cur_data, { "vd", "vs1", "vs2", "vm" },
		{ vlen, vlen, vector_cfg.len, vlen });

	run_self_result<Ts2, Td>(cur_data);

	int is_error = check_multi_error<Td>(cur_data, { "vd" }, { vlen });

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
			has_error = per_run<uint8_t, int16_t>(it, cur_cfg,
							      cur_data);
			break;
		case sew_e16:
			has_error = per_run<uint16_t, int32_t>(it, cur_cfg,
							       cur_data);
			break;
		case sew_e32:
			has_error = per_run<uint32_t, int64_t>(it, cur_cfg,
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
