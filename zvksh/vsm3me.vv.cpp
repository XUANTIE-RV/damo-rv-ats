/*
 * Copyright (c) 2026 Alibaba Group.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "../common/v_crypto_common.h"

const std::vector<InstField> vop_inst_fields = {
	{ 31, 26, 0x20, true, RegClass::NotReg, "funct6" },
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
	int base_ok = VectorRegValidator::validate(
		VregOperand::one_pow(vd), { VregOperand::one_pow(vs2) }, 1,
		config);
	ValidationConfig vs1_config = { .check_align = true,
					.check_overlap = false };
	int vs1_ok = VectorRegValidator::validate(
		std::optional<VregOperand>(), { VregOperand::one_pow(vs1) }, 1,
		vs1_config);
	int illegal = !base_ok || !vs1_ok || vector_cfg.sew != sew_e32 ||
		      !crypto_check_egs(8) || !crypto_lmul_egw_ok(256);
	print_illegal_status(illegal);
	return illegal;
}

template <typename Td> int run_self_result(c_data &cur_data)
{
	static_assert(sizeof(Td) == 4, "vsm3me.vv legal path requires e32");
	uint32_t *vs1_data = static_cast<uint32_t *>(
		cur_data.map_preinst_typed_value["vs1"].get());
	uint32_t *vs2_data = static_cast<uint32_t *>(
		cur_data.map_preinst_typed_value["vs2"].get());
	cur_data.map_selfcheck_typed_value["vd"] =
		c_data::process_from_common<Td>(
			cur_data.map_preinst_reg_value["vd"]);
	uint32_t *selfcheck_data = static_cast<uint32_t *>(
		cur_data.map_selfcheck_typed_value["vd"].get());

	for (int i = vector_cfg.vstart / 8; i < vector_cfg.len / 8; ++i) {
		auto a = crypto_load_group<uint32_t, 8>(vs1_data, i);
		auto b = crypto_load_group<uint32_t, 8>(vs2_data, i);
		uint32_t w[24] = {};
		for (int j = 0; j < 8; ++j)
			w[j] = crypto_rev8_32(a[j]);
		for (int j = 0; j < 8; ++j)
			w[j + 8] = crypto_rev8_32(b[j]);

		w[16] = sm3_w(w[0], w[7], w[13], w[3], w[10]);
		w[17] = sm3_w(w[1], w[8], w[14], w[4], w[11]);
		w[18] = sm3_w(w[2], w[9], w[15], w[5], w[12]);
		w[19] = sm3_w(w[3], w[10], w[16], w[6], w[13]);
		w[20] = sm3_w(w[4], w[11], w[17], w[7], w[14]);
		w[21] = sm3_w(w[5], w[12], w[18], w[8], w[15]);
		w[22] = sm3_w(w[6], w[13], w[19], w[9], w[16]);
		w[23] = sm3_w(w[7], w[14], w[20], w[10], w[17]);

		std::array<uint32_t, 8> out = {};
		for (int j = 0; j < 8; ++j)
			out[j] = crypto_rev8_32(w[j + 16]);
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
		int has_error =
			crypto_per_run_vd_vs2_vs1<uint32_t>(it, cur_cfg,
							    cur_data,
							    vop_inst_fields,
							    check_illegal,
							    run_self_result<uint32_t>);
		crypto_report_error(cur_cfg, has_error);
		print_runtime_iteration_end();
		if (has_error && early_stop)
			break;
	}
	end_program(it);
	return 0;
}
