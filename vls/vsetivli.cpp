/*
 * Copyright (c) 2026 Alibaba Group.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "../common/v_common.h"

const std::vector<InstField> vop_inst_fields = {
	{ 31, 30, 0x03, true, RegClass::NotReg, "bit3130" },
	{ 29, 20, 0x00, false, RegClass::NotReg, "zimm" },
	{ 19, 15, 0x00, false, RegClass::NotReg, "uimm" },
	{ 14, 12, 0x07, true, RegClass::NotReg, "funct3" },
	{ 11, 7, 0x00, false, RegClass::Int, "rd" },
	{ 6, 0, 0x57, true, RegClass::NotReg, "opcode" }
};

/* Check if a vtype zimm value represents an illegal configuration.
 * Returns 1 if illegal, 0 if legal. */
int check_illegal_vtype(uint32_t zimm)
{
	uint32_t vlmul = get_bits_range(zimm, 2, 0);
	uint32_t vsew = get_bits_range(zimm, 5, 3);
	uint32_t sew = (8 << vsew);
	uint32_t reserved = get_bits_range(zimm, 9, 8);

	/* Only universally agreed illegal conditions:
	 * reserved bits set or vsew encoding >= 4 (unsupported SEW).
	 * vlmul==4 and fractional LMUL constraints are implementation-defined. */
	if (reserved != 0 || vsew >= 4)
		return 1;
	return 0;
}

int check_illegal(c_data &cur_data)
{
	/* vsetivli does not trigger SIGILL; return 0 so the framework
	 * does not treat it as SIGILL. Actual vill check done after execution. */
	return 0;
}

/* Compute the expected VL given AVL, SEW and LMUL from zimm.
 * Follows RISC-V V spec: vl = min(AVL, VLMAX). */
uint64_t compute_expected_vl(uint64_t avl, uint32_t zimm)
{
	uint32_t vlmul = get_bits_range(zimm, 2, 0);
	uint32_t vsew = get_bits_range(zimm, 5, 3);
	uint32_t sew = (8 << vsew);

	uint64_t vlmax;
	if (vlmul <= 3) {
		vlmax = (vlenb * 8 / sew) << vlmul;
	} else {
		vlmax = (vlenb * 8 / sew) >> (8 - vlmul);
	}

	if (avl <= vlmax)
		return avl;
	return vlmax;
}

int per_run(int it, c_cfg &cur_cfg, c_data &cur_data)
{
	cur_data.set_value_from_inst(vop_inst_fields, vector_cfg.inst);

	uint32_t zimm = cur_data.map_reg_index["zimm"];
	uint32_t uimm = cur_data.map_reg_index["uimm"];
	uint32_t rd = cur_data.map_reg_index["rd"];

	if (random_mode) {
		cur_data.set_value_to_cfg(cur_cfg);
		cur_cfg.DESC = cur_data.get_DESC_from_inst(vop_inst_fields,
							   vector_cfg.inst);
		if (record_mode) {
			global_cfg.push_back(cur_cfg);
		}
	} else {
		cur_data.set_value_from_cfg(cur_cfg);
	}

	std::vector<uint32_t> insts;
	save_context(insts);

	/* Reset vtype to a known state before executing vsetivli */
	vsetvli_lmul_sew(insts, 0, 0, 1);
	csrrw(insts, CSR_VSTART, 0);

	/* Execute vsetivli instruction.
	 * AVL is the 5-bit immediate uimm encoded in the instruction,
	 * no need to load_reg. */
	insts.push_back(vector_cfg.inst);

	/* Store rd result (the returned VL) */
	vl_placeholder[vl_top] = 0;
	store_reg(insts, (uint64_t)&vl_placeholder[vl_top], rd);
	int rd_slot = vl_top++;

	/* Read CSR vtype and vl before restore_context overwrites them */
	int vtype_slot = csrr(insts, CSR_VTYPE);
	int vl_slot = csrr(insts, CSR_VL);

	restore_context(insts);
	run_instruction(insts);

	uint64_t vtype_val = val_placeholder[vtype_slot];
	uint64_t vl_val = val_placeholder[vl_slot];
	uint64_t rd_val = vl_placeholder[rd_slot];

	/* Cross-validate: check_illegal prediction vs hardware vill bit */
	int expect_illegal = check_illegal_vtype(zimm);
	uint32_t vill = get_bit(vtype_val, __riscv_xlen - 1);

	int has_error = 0;

	if (expect_illegal && !vill) {
		/* We predict illegal but hardware did not set vill — hardware bug */
		has_error = 1;
		ERROR << "check_illegal predicts illegal but vill not set"
		      << std::endl;
	} else if (vill) {
		/* Hardware set vill (we may or may not have predicted it):
		 * vl and rd must be 0 */
		if (vl_val != 0) {
			has_error = 1;
			ERROR << "vill set but vl != 0: vl=0x" << std::hex
			      << vl_val << std::dec << std::endl;
		}
		if (rd != 0 && rd_val != 0) {
			has_error = 1;
			ERROR << "vill set but rd != 0: rd=0x" << std::hex
			      << rd_val << std::dec << std::endl;
		}
	} else {
		/* Legal: check vtype matches zimm */
		if (vtype_val != zimm) {
			has_error = 1;
			ERROR << "vtype mismatch: got 0x" << std::hex
			      << vtype_val << " expected 0x" << zimm << std::dec
			      << std::endl;
		}

		/* Check VL is correct: AVL = uimm (5-bit immediate) */
		uint64_t expected_vl = compute_expected_vl(uimm, zimm);
		if (vl_val != expected_vl) {
			has_error = 1;
			ERROR << "vl mismatch: got 0x" << std::hex << vl_val
			      << " expected 0x" << expected_vl << std::dec
			      << std::endl;
		}

		/* Check rd == VL (when rd != x0) */
		if (rd != 0 && rd_val != expected_vl) {
			has_error = 1;
			ERROR << "rd mismatch: got 0x" << std::hex << rd_val
			      << " expected 0x" << expected_vl << std::dec
			      << std::endl;
		}
	}

	if (has_error) {
		ERROR << "CSR: vtype=0x" << std::hex << vtype_val << " vl=0x"
		      << vl_val << " rd(x" << std::dec << rd << ")=0x"
		      << std::hex << rd_val << std::dec << std::endl;
		ERROR << "zimm=0x" << std::hex << zimm << " uimm=" << std::dec
		      << uimm << std::endl;
		DEBUG << std::hex << json(cur_data).dump(4) << std::endl;
		if (early_stop)
			return 1;
	}
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

		reset_data();
		print_runtime_iteration_end();
		if (has_error && early_stop)
			break;
	}

	end_program(it);
	return 0;
}
