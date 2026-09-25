/*
 * Copyright (c) 2026 Alibaba Group.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "../common/v_crypto_common.h"

const std::vector<InstField> vop_inst_fields = {
	{ 31, 26, 0x2c, true, RegClass::NotReg, "funct6" },
	{ 25, 25, 0x01, true, RegClass::NotReg, "vm" },
	{ 24, 20, 0x00, false, RegClass::Vector, "vs2" },
	{ 19, 15, 0x00, false, RegClass::Vector, "vs1" },
	{ 14, 12, OPMVV, true, RegClass::NotReg, "funct3" },
	{ 11, 7, 0x00, false, RegClass::Vector, "vd" },
	{ 6, 0, 0x77, true, RegClass::NotReg, "opcode" }
};

gf128_t vghsh_group_model(const gf128_t &vd_old, const gf128_t &vs1_val,
			  const gf128_t &vs2_val)
{
	gf128_t sum = gf128_xor(vd_old, vs1_val);
	gf128_t y = gf128_brev8(sum);
	gf128_t h = gf128_brev8(vs2_val);
	gf128_t z = gf128_mul_gcm(y, h);
	return gf128_brev8(z);
}

int check_illegal(c_data &cur_data)
{
	int vd = cur_data.map_reg_index["vd"];
	int vs2 = cur_data.map_reg_index["vs2"];
	int vs1 = cur_data.map_reg_index["vs1"];

	ValidationConfig config = { .check_align = true,
				    .check_overlap = false };

	int lmul_shift = (vector_cfg.lmul < 4) ? vector_cfg.lmul :
						 -(8 - vector_cfg.lmul);
	uint64_t effective_vlen_bits = vlenb * 8;
	bool lmul_large_enough = false;
	if (lmul_shift >= 0) {
		lmul_large_enough = (effective_vlen_bits << lmul_shift) >= 128;
	} else {
		lmul_large_enough = (effective_vlen_bits >> (-lmul_shift)) >=
				    128;
	}

	/* Spec classification vs framework handling:
	 * - LMUL*VLEN < 128: hard illegal per spec.
	 * - SEW != 32: reserved per spec.
	 * - vl % 4 != 0 / vstart % 4 != 0: reserved per spec for EGS=4.
	 *
	 * damo-rv-ats currently models only "normal" and "expected illegal"
	 * execution paths. In the current qemu-riscv64 regression environment,
	 * these reserved configurations are observed as illegal instructions, so
	 * they stay on the expected-illegal path to avoid unexpected SIGILL
	 * reports and to match the harness semantics.
	 */
	int illegal = !(VectorRegValidator::validate(
			      VregOperand::one_pow(vd),
			      {
				      VregOperand::one_pow(vs2),
				      VregOperand::one_pow(vs1),
			      },
			      1, config)) ||
		      vector_cfg.sew != sew_e32 || (vector_cfg.len % 4) != 0 ||
		      (vector_cfg.vstart % 4) != 0 || !lmul_large_enough;
	print_illegal_status(illegal);
	return illegal;
}

template <typename Ts1, typename Ts2, typename Td>
int run_self_result(c_data &cur_data)
{
	static_assert(sizeof(Ts1) == 4, "vghsh.vv legal path requires e32 vs1");
	static_assert(sizeof(Ts2) == 4, "vghsh.vv legal path requires e32 vs2");
	static_assert(sizeof(Td) == 4, "vghsh.vv legal path requires e32 vd");

	Td *vd_data =
		static_cast<Td *>(cur_data.map_preinst_typed_value["vd"].get());
	Ts2 *vs2_data = static_cast<Ts2 *>(
		cur_data.map_preinst_typed_value["vs2"].get());
	Ts1 *vs1_data = static_cast<Ts1 *>(
		cur_data.map_preinst_typed_value["vs1"].get());

	cur_data.map_selfcheck_typed_value["vd"] =
		c_data::process_from_common<Td>(
			cur_data.map_preinst_reg_value["vd"]);
	Td *selfcheck_data = static_cast<Td *>(
		cur_data.map_selfcheck_typed_value["vd"].get());

	int eg_len = vector_cfg.len / 4;
	int eg_start = vector_cfg.vstart / 4;

	for (int group_idx = eg_start; group_idx < eg_len; ++group_idx) {
		gf128_t vd_old = load_eg128_from_e32(
			reinterpret_cast<uint32_t *>(vd_data), group_idx);
		gf128_t vs1_val = load_eg128_from_e32(
			reinterpret_cast<uint32_t *>(vs1_data), group_idx);
		gf128_t vs2_val = load_eg128_from_e32(
			reinterpret_cast<uint32_t *>(vs2_data), group_idx);
		gf128_t result = vghsh_group_model(vd_old, vs1_val, vs2_val);
		store_eg128_to_e32(reinterpret_cast<uint32_t *>(selfcheck_data),
				   group_idx, result);
	}

	return 0;
}

template <typename Ts1, typename Ts2, typename Td>
int per_run(int it, c_cfg &cur_cfg, c_data &cur_data)
{
	cur_data.set_value_from_inst(vop_inst_fields, vector_cfg.inst);

	if (random_mode) {
		cur_data.register_type_with_random<Ts1>("vs1", vector_cfg.len);
		cur_data.register_type_with_random<Ts2>("vs2", vector_cfg.len);
		cur_data.register_type_with_random<Td>("vd", vector_cfg.len);
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

	load_multi_vector<Td, Ts2, Ts1>(
		insts, cur_data, { "vd", "vs2", "vs1" },
		{ vector_cfg.lmul, vector_cfg.lmul, vector_cfg.lmul },
		{ vector_cfg.len, vector_cfg.len, vector_cfg.len });

	store_multi_preinst_vector<Td, Ts2, Ts1>(
		insts, cur_data, { "vd", "vs2", "vs1" },
		{ vector_cfg.lmul, vector_cfg.lmul, vector_cfg.lmul },
		{ vector_cfg.len, vector_cfg.len, vector_cfg.len });

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

	save_multi_preinst_value_to_common<Td, Ts2, Ts1>(
		cur_data, { "vd", "vs2", "vs1" },
		{ vector_cfg.len, vector_cfg.len, vector_cfg.len });

	run_self_result<Ts1, Ts2, Td>(cur_data);

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
			has_error = per_run<uint32_t, uint32_t, uint32_t>(
				it, cur_cfg, cur_data);
			break;
		case sew_e16:
			has_error = per_run<uint32_t, uint32_t, uint32_t>(
				it, cur_cfg, cur_data);
			break;
		case sew_e32:
			has_error = per_run<uint32_t, uint32_t, uint32_t>(
				it, cur_cfg, cur_data);
			break;
		case sew_e64:
			has_error = per_run<uint32_t, uint32_t, uint32_t>(
				it, cur_cfg, cur_data);
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
