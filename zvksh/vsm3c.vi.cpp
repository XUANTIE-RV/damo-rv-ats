/*
 * Copyright (c) 2026 Alibaba Group.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "../common/v_crypto_common.h"

const std::vector<InstField> vop_inst_fields = {
	{ 31, 26, 0x2b, true, RegClass::NotReg, "funct6" },
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
				    .check_overlap = true,
				    .force_no_overlap = true };
	int illegal = !(VectorRegValidator::validate(
			      VregOperand::one_pow(vd),
			      { VregOperand::one_pow(vs2) }, 1, config)) ||
		      vector_cfg.sew != sew_e32 || !crypto_check_egs(8) ||
		      !crypto_lmul_egw_ok(256);
	print_illegal_status(illegal);
	return illegal;
}

template <typename Td> int run_self_result(c_data &cur_data)
{
	static_assert(sizeof(Td) == 4, "vsm3c.vi legal path requires e32");
	uint32_t *vd_data = static_cast<uint32_t *>(
		cur_data.map_preinst_typed_value["vd"].get());
	uint32_t *vs2_data = static_cast<uint32_t *>(
		cur_data.map_preinst_typed_value["vs2"].get());
	cur_data.map_selfcheck_typed_value["vd"] =
		c_data::process_from_common<Td>(
			cur_data.map_preinst_reg_value["vd"]);
	uint32_t *selfcheck_data = static_cast<uint32_t *>(
		cur_data.map_selfcheck_typed_value["vd"].get());

	uint32_t rnds = cur_data.map_reg_index["uimm"] & 0x1f;
	for (int i = vector_cfg.vstart / 8; i < vector_cfg.len / 8; ++i) {
		auto s = crypto_load_group<uint32_t, 8>(vd_data, i);
		auto m = crypto_load_group<uint32_t, 8>(vs2_data, i);
		uint32_t a = crypto_rev8_32(s[0]);
		uint32_t b = crypto_rev8_32(s[1]);
		uint32_t c = crypto_rev8_32(s[2]);
		uint32_t d = crypto_rev8_32(s[3]);
		uint32_t e = crypto_rev8_32(s[4]);
		uint32_t f = crypto_rev8_32(s[5]);
		uint32_t g = crypto_rev8_32(s[6]);
		uint32_t h = crypto_rev8_32(s[7]);
		uint32_t w0 = crypto_rev8_32(m[0]);
		uint32_t w1 = crypto_rev8_32(m[1]);
		uint32_t w4 = crypto_rev8_32(m[4]);
		uint32_t w5 = crypto_rev8_32(m[5]);
		uint32_t x0 = w0 ^ w4;
		uint32_t x1 = w1 ^ w5;

		uint32_t j = 2 * rnds;
		uint32_t ss1 = crypto_rol32(
			crypto_rol32(a, 12) + e + crypto_rol32(sm3_t(j), j), 7);
		uint32_t ss2 = ss1 ^ crypto_rol32(a, 12);
		uint32_t tt1 = sm3_ff(a, b, c, j) + d + ss2 + x0;
		uint32_t tt2 = sm3_gg(e, f, g, j) + h + ss1 + w0;
		uint32_t c1 = crypto_rol32(b, 9);
		uint32_t a1 = tt1;
		uint32_t g1 = crypto_rol32(f, 19);
		uint32_t e1 = sm3_p0(tt2);

		j = 2 * rnds + 1;
		ss1 = crypto_rol32(crypto_rol32(a1, 12) + e1 +
					   crypto_rol32(sm3_t(j), j),
				   7);
		ss2 = ss1 ^ crypto_rol32(a1, 12);
		tt1 = sm3_ff(a1, a, c1, j) + c + ss2 + x1;
		tt2 = sm3_gg(e1, e, g1, j) + g + ss1 + w1;
		uint32_t c2 = crypto_rol32(a, 9);
		uint32_t a2 = tt1;
		uint32_t g2 = crypto_rol32(e, 19);
		uint32_t e2 = sm3_p0(tt2);

		std::array<uint32_t, 8> out = {
			crypto_rev8_32(a2), crypto_rev8_32(a1),
			crypto_rev8_32(c2), crypto_rev8_32(c1),
			crypto_rev8_32(e2), crypto_rev8_32(e1),
			crypto_rev8_32(g2), crypto_rev8_32(g1),
		};
		crypto_store_group<uint32_t, 8>(selfcheck_data, i, out);
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
