/*
 * Copyright (c) 2026 Alibaba Group.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "../common/v_crypto_common.h"

const std::vector<InstField> vop_inst_fields = {
	{ 31, 26, 0x22, true, RegClass::NotReg, "funct6" },
	{ 25, 25, 0x01, true, RegClass::NotReg, "vm" },
	{ 24, 20, 0x00, false, RegClass::Vector, "vs2" },
	{ 19, 15, 0x00, false, RegClass::NotReg, "uimm" },
	{ 14, 12, OPMVV, true, RegClass::NotReg, "funct3" },
	{ 11, 7, 0x00, false, RegClass::Vector, "vd" },
	{ 6, 0, 0x77, true, RegClass::NotReg, "opcode" }
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
	static_assert(sizeof(Td) == 4, "vaeskf1.vi legal path requires e32");
	uint32_t *vs2_data = static_cast<uint32_t *>(
		cur_data.map_preinst_typed_value["vs2"].get());
	cur_data.map_selfcheck_typed_value["vd"] =
		c_data::process_from_common<Td>(
			cur_data.map_preinst_reg_value["vd"]);
	uint32_t *selfcheck_data = static_cast<uint32_t *>(
		cur_data.map_selfcheck_typed_value["vd"].get());

	uint32_t rnd = cur_data.map_reg_index["uimm"] & 0xf;
	if (rnd == 0 || rnd > 10)
		rnd ^= 0x8;
	uint32_t r = rnd - 1;

	for (int i = vector_cfg.vstart / 4; i < vector_cfg.len / 4; ++i) {
		eg128 cur = crypto_load_group<uint32_t, 4>(vs2_data, i);
		eg128 out;
		out[0] = aes_subword_fwd(aes_rotword(cur[3])) ^
			 aes_decode_rcon(r) ^ cur[0];
		out[1] = out[0] ^ cur[1];
		out[2] = out[1] ^ cur[2];
		out[3] = out[2] ^ cur[3];
		crypto_store_group<uint32_t, 4>(selfcheck_data, i, out);
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
		int has_error = crypto_per_run_vd_vs2<uint32_t>(
			it, cur_cfg, cur_data, vop_inst_fields, check_illegal,
			run_self_result<uint32_t>);
		crypto_report_error(cur_cfg, has_error);
		print_runtime_iteration_end();
		if (has_error && early_stop)
			break;
	}
	end_program(it);
	return 0;
}
