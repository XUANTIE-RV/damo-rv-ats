/*
 * Copyright (c) 2026 Alibaba Group.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "../common/v_crypto_common.h"

const std::vector<InstField> vop_inst_fields = {
	{ 31, 26, 0x21, true, RegClass::NotReg, "funct6" },
	{ 25, 25, 0x01, true, RegClass::NotReg, "vm" },
	{ 24, 20, 0x00, false, RegClass::Vector, "vs2" },
	{ 19, 15, 0x00, false, RegClass::NotReg, "uimm" },
	{ 14, 12, OPMVV, true, RegClass::NotReg, "funct3" },
	{ 11, 7, 0x00, false, RegClass::Vector, "vd" },
	{ 6, 0, 0x77, true, RegClass::NotReg, "opcode" }
};

static const uint32_t sm4_ck[32] = {
	0x00070e15, 0x1c232a31, 0x383f464d, 0x545b6269,
	0x70777e85, 0x8c939aa1, 0xa8afb6bd, 0xc4cbd2d9,
	0xe0e7eef5, 0xfc030a11, 0x181f262d, 0x343b4249,
	0x50575e65, 0x6c737a81, 0x888f969d, 0xa4abb2b9,
	0xc0c7ced5, 0xdce3eaf1, 0xf8ff060d, 0x141b2229,
	0x30373e45, 0x4c535a61, 0x686f767d, 0x848b9299,
	0xa0a7aeb5, 0xbcc3cad1, 0xd8dfe6ed, 0xf4fb0209,
	0x10171e25, 0x2c333a41, 0x484f565d, 0x646b7279
};

int check_illegal(c_data &cur_data)
{
	int vd = cur_data.map_reg_index["vd"];
	int vs2 = cur_data.map_reg_index["vs2"];
	ValidationConfig config = { .check_align = true,
				    .check_overlap = false };
	int illegal = !(VectorRegValidator::validate(
			      VregOperand::one_pow(vd),
			      { VregOperand::one_pow(vs2) }, 1, config)) ||
		      vector_cfg.sew != sew_e32 || !crypto_check_egs(4) ||
		      !crypto_lmul_egw_ok(128);
	print_illegal_status(illegal);
	return illegal;
}

template <typename Td> int run_self_result(c_data &cur_data)
{
	static_assert(sizeof(Td) == 4, "vsm4k.vi legal path requires e32");
	uint32_t *vs2_data = static_cast<uint32_t *>(
		cur_data.map_preinst_typed_value["vs2"].get());
	cur_data.map_selfcheck_typed_value["vd"] =
		c_data::process_from_common<Td>(
			cur_data.map_preinst_reg_value["vd"]);
	uint32_t *selfcheck_data = static_cast<uint32_t *>(
		cur_data.map_selfcheck_typed_value["vd"].get());

	uint32_t rnd = cur_data.map_reg_index["uimm"] & 0x7;
	for (int i = vector_cfg.vstart / 4; i < vector_cfg.len / 4; ++i) {
		eg128 rk = crypto_load_group<uint32_t, 4>(vs2_data, i);
		uint32_t rk4 = sm4_round_key(
			rk[0], sm4_subword(rk[1] ^ rk[2] ^ rk[3] ^
					   sm4_ck[4 * rnd]));
		uint32_t rk5 = sm4_round_key(
			rk[1], sm4_subword(rk[2] ^ rk[3] ^ rk4 ^
					   sm4_ck[4 * rnd + 1]));
		uint32_t rk6 = sm4_round_key(
			rk[2], sm4_subword(rk[3] ^ rk4 ^ rk5 ^
					   sm4_ck[4 * rnd + 2]));
		uint32_t rk7 = sm4_round_key(
			rk[3], sm4_subword(rk4 ^ rk5 ^ rk6 ^
					   sm4_ck[4 * rnd + 3]));
		crypto_store_group<uint32_t, 4>(selfcheck_data, i,
						 { rk4, rk5, rk6, rk7 });
	}
	return 0;
}

int main(int argc, char *argv[])
{
	crypto_main_begin(argc, argv, __FILE__);
	int it;
	for (it = 0; it < run_times; it++) {
		if (!random_mode && it >= global_cfg.size())
			break;
		print_runtime_iteration(it);
		c_cfg cur_cfg;
		c_data cur_data;
		init_vector_cfg(it, cur_cfg, cur_data, check_illegal,
				vop_inst_fields);
		int has_error =
			crypto_per_run_vd_vs2<uint32_t>(
				it, cur_cfg, cur_data, vop_inst_fields,
				check_illegal, run_self_result<uint32_t>);
		crypto_report_error(cur_cfg, has_error);
		print_runtime_iteration_end();
		if (has_error && early_stop)
			break;
	}
	end_program(it);
	return 0;
}
