/*
 * Copyright (c) 2026 Alibaba Group.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "../common/v_common.h"

const std::vector<InstField> vop_inst_fields = {
	{ 31, 26, 0x28, true, RegClass::NotReg, "funct6" },
	{ 25, 25, 0x01, true, RegClass::NotReg, "vm" },
	{ 24, 20, 0x00, false, RegClass::Vector, "vs2" },
	{ 19, 15, 0x11, true, RegClass::NotReg, "const_19_15" },
	{ 14, 12, OPMVV, true, RegClass::NotReg, "funct3" },
	{ 11, 7, 0x00, false, RegClass::Vector, "vd" },
	{ 6, 0, 0x77, true, RegClass::NotReg, "opcode" }
};

struct gf128_t {
	uint64_t hi;
	uint64_t lo;
};

gf128_t gf128_zero()
{
	return { 0, 0 };
}

gf128_t gf128_xor(const gf128_t &lhs, const gf128_t &rhs)
{
	return { lhs.hi ^ rhs.hi, lhs.lo ^ rhs.lo };
}

uint8_t brev8_byte(uint8_t x)
{
	uint8_t result = 0;
	for (int i = 0; i < 8; ++i) {
		result |= ((x >> i) & 1u) << (7 - i);
	}
	return result;
}

gf128_t load_eg128_from_e32(const uint32_t *data, int group_idx)
{
	int base = group_idx * 4;
	gf128_t result;
	result.hi = (static_cast<uint64_t>(data[base + 3]) << 32) |
		    static_cast<uint64_t>(data[base + 2]);
	result.lo = (static_cast<uint64_t>(data[base + 1]) << 32) |
		    static_cast<uint64_t>(data[base + 0]);
	return result;
}

void store_eg128_to_e32(uint32_t *data, int group_idx, const gf128_t &x)
{
	int base = group_idx * 4;
	data[base + 0] = static_cast<uint32_t>(x.lo & 0xffffffffu);
	data[base + 1] = static_cast<uint32_t>(x.lo >> 32);
	data[base + 2] = static_cast<uint32_t>(x.hi & 0xffffffffu);
	data[base + 3] = static_cast<uint32_t>(x.hi >> 32);
}

gf128_t gf128_brev8(const gf128_t &x)
{
	gf128_t result = gf128_zero();

	for (int byte_idx = 0; byte_idx < 8; ++byte_idx) {
		uint64_t shift = byte_idx * 8;
		uint8_t byte = static_cast<uint8_t>((x.lo >> shift) & 0xffu);
		result.lo |= static_cast<uint64_t>(brev8_byte(byte)) << shift;
	}

	for (int byte_idx = 0; byte_idx < 8; ++byte_idx) {
		uint64_t shift = byte_idx * 8;
		uint8_t byte = static_cast<uint8_t>((x.hi >> shift) & 0xffu);
		result.hi |= static_cast<uint64_t>(brev8_byte(byte)) << shift;
	}

	return result;
}

bool gf128_get_bit(const gf128_t &x, int bit_idx)
{
	if (bit_idx < 64) {
		return ((x.lo >> bit_idx) & 1ull) != 0;
	}
	return ((x.hi >> (bit_idx - 64)) & 1ull) != 0;
}

bool gf128_get_msb(const gf128_t &x)
{
	return ((x.hi >> 63) & 1ull) != 0;
}

void gf128_shl1_inplace(gf128_t &x)
{
	uint64_t lo_carry = x.lo >> 63;
	x.lo <<= 1;
	x.hi = (x.hi << 1) | lo_carry;
}

void gf128_xor_low8_inplace(gf128_t &x, uint8_t c)
{
	x.lo ^= static_cast<uint64_t>(c);
}

gf128_t gf128_mul_gcm(const gf128_t &y, const gf128_t &h_init)
{
	gf128_t z = gf128_zero();
	gf128_t h = h_init;

	for (int bit = 0; bit < 128; ++bit) {
		if (gf128_get_bit(y, bit)) {
			z = gf128_xor(z, h);
		}

		bool reduce = gf128_get_msb(h);
		gf128_shl1_inplace(h);
		if (reduce) {
			gf128_xor_low8_inplace(h, 0x87);
		}
	}

	return z;
}

gf128_t vgmul_group_model(const gf128_t &vd_old, const gf128_t &vs2_val)
{
	gf128_t y = gf128_brev8(vd_old);
	gf128_t h = gf128_brev8(vs2_val);
	gf128_t z = gf128_mul_gcm(y, h);
	return gf128_brev8(z);
}

int check_illegal(c_data &cur_data)
{
	int vd = cur_data.map_reg_index["vd"];
	int vs2 = cur_data.map_reg_index["vs2"];

	ValidationConfig config = { .check_align = true,
				    .check_overlap = false };

	int lmul_shift = (vector_cfg.lmul < 4) ? vector_cfg.lmul :
					       -(8 - vector_cfg.lmul);
	uint64_t effective_vlen_bits = vlen * 8;
	bool lmul_large_enough = false;
	if (lmul_shift >= 0) {
		lmul_large_enough =
			(effective_vlen_bits << lmul_shift) >= 128;
	} else {
		lmul_large_enough =
			(effective_vlen_bits >> (-lmul_shift)) >= 128;
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
	int illegal = !(
		VectorRegValidator::validate(VregOperand::one_pow(vd),
					     {
						     VregOperand::one_pow(vs2),
					     },
					     1, config)) ||
		      vector_cfg.sew != sew_e32 || (vector_cfg.len % 4) != 0 ||
		      (vector_cfg.vstart % 4) != 0 || !lmul_large_enough;
	print_illegal_status(illegal);
	return illegal;
}

template <typename Ts2, typename Td> int run_self_result(c_data &cur_data)
{
	static_assert(sizeof(Ts2) == 4, "vgmul.vv legal path requires e32 vs2");
	static_assert(sizeof(Td) == 4, "vgmul.vv legal path requires e32 vd");

	Td *vd_data = static_cast<Td *>(
		cur_data.map_preinst_typed_value["vd"].get());
	Ts2 *vs2_data = static_cast<Ts2 *>(
		cur_data.map_preinst_typed_value["vs2"].get());

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
		gf128_t vs2_val = load_eg128_from_e32(
			reinterpret_cast<uint32_t *>(vs2_data), group_idx);
		gf128_t result = vgmul_group_model(vd_old, vs2_val);
		store_eg128_to_e32(reinterpret_cast<uint32_t *>(selfcheck_data),
				   group_idx, result);
	}

	return 0;
}

template <typename Ts2, typename Td>
int per_run(int it, c_cfg &cur_cfg, c_data &cur_data)
{
	cur_data.set_value_from_inst(vop_inst_fields, vector_cfg.inst);

	if (random_mode) {
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

	load_multi_vector<Td, Ts2>(insts, cur_data, { "vd", "vs2" },
				   { vector_cfg.lmul, vector_cfg.lmul },
				   { vector_cfg.len, vector_cfg.len });

	store_multi_preinst_vector<Td, Ts2>(
		insts, cur_data, { "vd", "vs2" },
		{ vector_cfg.lmul, vector_cfg.lmul },
		{ vector_cfg.len, vector_cfg.len });

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

	save_multi_preinst_value_to_common<Td, Ts2>(
		cur_data, { "vd", "vs2" },
		{ vector_cfg.len, vector_cfg.len });

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
			has_error = per_run<uint32_t, uint32_t>(it, cur_cfg,
								cur_data);
			break;
		case sew_e16:
			has_error = per_run<uint32_t, uint32_t>(it, cur_cfg,
								cur_data);
			break;
		case sew_e32:
			has_error = per_run<uint32_t, uint32_t>(it, cur_cfg,
								cur_data);
			break;
		case sew_e64:
			has_error = per_run<uint32_t, uint32_t>(it, cur_cfg,
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
