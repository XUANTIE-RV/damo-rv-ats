/*
 * Copyright (c) 2026 Alibaba Group.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "../common/v_common.h"

const std::vector<InstField> vop_inst_fields = {
	{ 31, 25, 0x40, true, RegClass::NotReg, "funct7" },
	{ 24, 20, 0x00, false, RegClass::Int, "rs2" },
	{ 19, 15, 0x00, false, RegClass::Int, "rs1" },
	{ 14, 12, 0x07, true, RegClass::NotReg, "funct3" },
	{ 11, 7, 0x00, false, RegClass::Int, "rd" },
	{ 6, 0, 0x57, true, RegClass::NotReg, "opcode" }
};

/* vtype value is loaded into rs2 at runtime; we generate a random
 * 11-bit vtype value and store it in rs1_data (reusing this field). */
uint64_t random_vtype_val;

/* Check if a vtype value represents an illegal configuration.
 * Returns 1 if illegal, 0 if legal. */
int check_illegal_vtype(uint64_t vtype_val)
{
	uint32_t vlmul = get_bits_range(vtype_val, 2, 0);
	uint32_t vsew = get_bits_range(vtype_val, 5, 3);
	uint32_t sew = (8 << vsew);
	uint32_t reserved = get_bits_range(vtype_val, 10, 8);

	/* Only universally agreed illegal conditions:
	 * reserved bits set or vsew encoding >= 4 (unsupported SEW).
	 * vlmul==4 and fractional LMUL constraints are implementation-defined. */
	if (reserved != 0 || vsew >= 4)
		return 1;
	return 0;
}

int check_illegal(c_data &cur_data)
{
	/* vsetvl gets vtype from rs2 register at runtime,
	 * so we cannot determine illegality from instruction encoding alone.
	 * Return 0 so the framework does not treat it as SIGILL. */
	return 0;
}

uint64_t compute_expected_vl(uint64_t avl, uint64_t vtype_val)
{
	uint32_t vlmul = get_bits_range(vtype_val, 2, 0);
	uint32_t vsew = get_bits_range(vtype_val, 5, 3);
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

	uint32_t rs2 = cur_data.map_reg_index["rs2"];
	uint32_t rs1 = cur_data.map_reg_index["rs1"];
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

	/* Reset vtype to a known state before executing vsetvl */
	vsetvli_lmul_sew(insts, 0, 0, 1);
	csrrw(insts, CSR_VSTART, 0);

	/* Load vtype value into rs2, then AVL into rs1 */
	val_placeholder[val_top] = random_vtype_val;
	load_reg(insts, (uint64_t)&val_placeholder[val_top], rs2);
	val_top++;
	load_reg(insts, (uint64_t)&vector_cfg.len, rs1);

	/* Capture the actual rs1 (AVL) and rs2 (vtype) values
	 * right before vsetvl executes, so we can verify correctly
	 * even when rs1==rs2 causes one to overwrite the other. */
	vl_placeholder[vl_top] = 0;
	store_reg(insts, (uint64_t)&vl_placeholder[vl_top], rs1);
	int rs1_slot = vl_top++;

	vl_placeholder[vl_top] = 0;
	store_reg(insts, (uint64_t)&vl_placeholder[vl_top], rs2);
	int rs2_slot = vl_top++;

	/* Execute vsetvl instruction */
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

	uint64_t actual_avl = vl_placeholder[rs1_slot];
	uint64_t actual_vtype_input = vl_placeholder[rs2_slot];
	uint64_t vtype_val = val_placeholder[vtype_slot];
	uint64_t vl_val = val_placeholder[vl_slot];
	uint64_t rd_val = vl_placeholder[rd_slot];

	/* Cross-validate: check_illegal prediction vs hardware vill bit */
	int expect_illegal = check_illegal_vtype(actual_vtype_input);
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
		/* Legal: check vtype matches the actual value in rs2 */
		if (vtype_val != actual_vtype_input) {
			has_error = 1;
			ERROR << "vtype mismatch: got 0x" << std::hex
			      << vtype_val << " expected 0x"
			      << actual_vtype_input << std::dec << std::endl;
		}

		/* Check VL is correct */
		uint64_t expected_vl =
			compute_expected_vl(actual_avl, actual_vtype_input);
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
		ERROR << "actual_rs2_vtype=0x" << std::hex << actual_vtype_input
		      << " actual_avl=" << std::dec << actual_avl
		      << " random_vtype_val=0x" << std::hex << random_vtype_val
		      << " cfg_len=" << std::dec << vector_cfg.len << std::endl;
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

		/* Generate random vtype value before init_vector_cfg,
		 * because check_illegal depends on it */
		random_vtype_val = rand() & 0xFF;

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
