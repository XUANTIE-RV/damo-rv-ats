/*
 * Copyright (c) 2026 Alibaba Group.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "../common/v_crypto_common.h"

const std::vector<InstField> vop_inst_fields = {
	{ 31, 26, 0x2f, true, RegClass::NotReg, "funct6" },
	{ 25, 25, 0x01, true, RegClass::NotReg, "vm" },
	{ 24, 20, 0x00, false, RegClass::Vector, "vs2" },
	{ 19, 15, 0x00, false, RegClass::Vector, "vs1" },
	{ 14, 12, OPMVV, true, RegClass::NotReg, "funct3" },
	{ 11, 7, 0x00, false, RegClass::Vector, "vd" },
	{ 6, 0, 0x77, true, RegClass::NotReg, "opcode" }
};

int check_illegal(c_data &cur_data)
{
	int vd = cur_data.map_reg_index["vd"];
	int vs2 = cur_data.map_reg_index["vs2"];
	int vs1 = cur_data.map_reg_index["vs1"];
	ValidationConfig config = { .check_align = true,
				    .check_overlap = true,
				    .force_no_overlap = true };
	uint64_t egw = (vector_cfg.sew == sew_e64) ? 256 : 128;
	int illegal = !(VectorRegValidator::validate(
			      VregOperand::one_pow(vd),
			      { VregOperand::one_pow(vs2),
				VregOperand::one_pow(vs1) },
			      1, config)) ||
		      (vector_cfg.sew != sew_e32 && vector_cfg.sew != sew_e64) ||
		      !crypto_check_egs(4) || !crypto_lmul_egw_ok(egw);
	print_illegal_status(illegal);
	return illegal;
}

template <typename T> int run_self_result(c_data &cur_data)
{
	static_assert(sizeof(T) == 4 || sizeof(T) == 8,
		      "vsha2cl.vv legal path requires e32/e64");
	T *vd_data = static_cast<T *>(
		cur_data.map_preinst_typed_value["vd"].get());
	T *vs2_data = static_cast<T *>(
		cur_data.map_preinst_typed_value["vs2"].get());
	T *vs1_data = static_cast<T *>(
		cur_data.map_preinst_typed_value["vs1"].get());
	cur_data.map_selfcheck_typed_value["vd"] =
		c_data::process_from_common<T>(
			cur_data.map_preinst_reg_value["vd"]);
	T *selfcheck_data = static_cast<T *>(
		cur_data.map_selfcheck_typed_value["vd"].get());

	for (int i = vector_cfg.vstart / 4; i < vector_cfg.len / 4; ++i) {
		auto vd = crypto_load_group<T, 4>(vd_data, i);
		auto vs2 = crypto_load_group<T, 4>(vs2_data, i);
		auto vs1 = crypto_load_group<T, 4>(vs1_data, i);
		T h = vd[0], g = vd[1], d = vd[2], c = vd[3];
		T f = vs2[0], e = vs2[1], b = vs2[2], a = vs2[3];
		T w0 = vs1[0];
		T w1 = vs1[1];
		sha2_round(a, b, c, d, e, f, g, h, w0);
		sha2_round(a, b, c, d, e, f, g, h, w1);
		crypto_store_group<T, 4>(selfcheck_data, i,
					 { f, e, b, a });
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
		int has_error = 0;
		if (vector_cfg.sew == sew_e64)
			has_error = crypto_per_run_vd_vs2_vs1<uint64_t>(
				it, cur_cfg, cur_data, vop_inst_fields,
				check_illegal, run_self_result<uint64_t>);
		else
			has_error = crypto_per_run_vd_vs2_vs1<uint32_t>(
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
