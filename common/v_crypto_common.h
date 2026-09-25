/*
 * Copyright (c) 2026 Alibaba Group.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __v_crypto_common__
#define __v_crypto_common__

#include "v_common.h"

#include <array>

//bit operation
static inline uint32_t crypto_rol32(uint32_t x, unsigned n)
{
	n &= 31;
	return (x << n) | (x >> ((32 - n) & 31));
}

template <typename T> T crypto_rotr(T x, unsigned n)
{
	const unsigned bits = sizeof(T) * 8;
	n &= bits - 1;
	return (x >> n) | (x << ((bits - n) & (bits - 1)));
}

static inline uint32_t crypto_rev8_32(uint32_t x)
{
	return ((x & 0x000000ffu) << 24) | ((x & 0x0000ff00u) << 8) |
	       ((x & 0x00ff0000u) >> 8) | ((x & 0xff000000u) >> 24);
}

//Calculate how many bits a vector register group can actually hold under the current LMUL.
static inline uint64_t crypto_effective_lmul_bits()
{
	uint64_t bits = vlenb * 8;
	if (vector_cfg.lmul < 4)
		return bits << vector_cfg.lmul;
	return bits >> (8 - vector_cfg.lmul);
}

//Determine whether the current LMUL can hold at least one element group width.
static inline bool crypto_lmul_egw_ok(uint64_t egw)
{
	return crypto_effective_lmul_bits() >= egw;
}

//Calculate how many physical vector registers a vector register group will occupy, based on the current LMUL.
static inline int crypto_vector_group_regs()
{
	return VectorRegValidator::get_reg_group_count(vector_cfg.lmul, 0);
}

//Check whether the two register groups are non-overlapping.
static inline bool crypto_no_overlap(int a, int asize, int b, int bsize)
{
	return !VectorRegValidator::is_overlapped(a, asize, b, bsize);
}

//Check whether the vector register group of vd overlaps with that of the scalar-form vs2 element group.
static inline bool crypto_scalar_eg_no_overlap(int vd, int vs2, uint64_t egw)
{
	int vd_size = crypto_vector_group_regs();
	uint64_t vlen_bits = vlenb * 8;
	int scalar_size = (egw + vlen_bits - 1) / vlen_bits;

	if (vs2 + scalar_size > 32)
		return false;
	return crypto_no_overlap(vd, vd_size, vs2, scalar_size);
}

//Check whether vd satisfies the register number range and alignment requirements.
static inline bool crypto_check_vd_align_range(int vd)
{
	return VectorRegValidator::validate(VregOperand::one_pow(vd), {}, 1,
					    { .check_align = true,
					      .check_overlap = false });
}

//Check whether vl and vstart are integer multiples of the EGS.
static inline bool crypto_check_egs(uint64_t egs)
{
	return (vector_cfg.len % egs) == 0 && (vector_cfg.vstart % egs) == 0;
}

template <typename T, size_t N>
std::array<T, N> crypto_load_group(const T *data, int group_idx)
{
	std::array<T, N> out = {};
	for (size_t i = 0; i < N; ++i)
		out[i] = data[group_idx * N + i];
	return out;
}

template <typename T, size_t N>
void crypto_store_group(T *data, int group_idx, const std::array<T, N> &value)
{
	for (size_t i = 0; i < N; ++i)
		data[group_idx * N + i] = value[i];
}

template <typename Td, typename SelfFunc>
int crypto_per_run_vd_vs2(int it, c_cfg &cur_cfg, c_data &cur_data,
			  const std::vector<InstField> &inst_fields,
			  std::function<int(c_data &)> illegal_func,
			  SelfFunc self_func)
{
	cur_data.set_value_from_inst(inst_fields, vector_cfg.inst);

	if (random_mode) {
		cur_data.register_type_with_random<Td>("vd", vector_cfg.len);
		cur_data.register_type_with_random<Td>("vs2", vector_cfg.len);
		cur_data.set_value_to_cfg(cur_cfg);
		cur_cfg.DESC = cur_data.get_DESC_from_inst(inst_fields,
							   vector_cfg.inst);
		if (record_mode)
			global_cfg.push_back(cur_cfg);
	} else {
		cur_data.set_value_from_cfg(cur_cfg);
	}

	global_flag_ptr->illegal = illegal_func(cur_data);

	std::vector<uint32_t> insts;
	save_context(insts);

	load_multi_vector<Td, Td>(insts, cur_data, { "vd", "vs2" },
				  { vector_cfg.lmul, vector_cfg.lmul },
				  { vector_cfg.len, vector_cfg.len });
	store_multi_preinst_vector<Td, Td>(insts, cur_data, { "vd", "vs2" },
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

	save_multi_preinst_value_to_common<Td, Td>(
		cur_data, { "vd", "vs2" }, { vector_cfg.len, vector_cfg.len });

	self_func(cur_data);

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

template <typename Td, typename SelfFunc>
int crypto_per_run_vd_scalar_vs2(int it, c_cfg &cur_cfg, c_data &cur_data,
				 const std::vector<InstField> &inst_fields,
				 std::function<int(c_data &)> illegal_func,
				 SelfFunc self_func, uint64_t scalar_len = 4)
{
	cur_data.set_value_from_inst(inst_fields, vector_cfg.inst);

	if (random_mode) {
		cur_data.register_type_with_random<Td>("vd", vector_cfg.len);
		cur_data.register_type_with_random<Td>("vs2", scalar_len);
		cur_data.set_value_to_cfg(cur_cfg);
		cur_cfg.DESC = cur_data.get_DESC_from_inst(inst_fields,
							   vector_cfg.inst);
		if (record_mode)
			global_cfg.push_back(cur_cfg);
	} else {
		cur_data.set_value_from_cfg(cur_cfg);
	}

	global_flag_ptr->illegal = illegal_func(cur_data);

	std::vector<uint32_t> insts;
	save_context(insts);

	load_multi_vector<Td, Td>(insts, cur_data, { "vd", "vs2" },
				  { vector_cfg.lmul, lmul_m1 },
				  { vector_cfg.len, scalar_len });
	store_multi_preinst_vector<Td, Td>(insts, cur_data, { "vd", "vs2" },
					   { vector_cfg.lmul, lmul_m1 },
					   { vector_cfg.len, scalar_len });

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

	save_multi_preinst_value_to_common<Td, Td>(
		cur_data, { "vd", "vs2" }, { vector_cfg.len, scalar_len });

	self_func(cur_data);

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

template <typename T, typename SelfFunc>
int crypto_per_run_vd_vs2_vs1(int it, c_cfg &cur_cfg, c_data &cur_data,
			      const std::vector<InstField> &inst_fields,
			      std::function<int(c_data &)> illegal_func,
			      SelfFunc self_func)
{
	cur_data.set_value_from_inst(inst_fields, vector_cfg.inst);

	if (random_mode) {
		cur_data.register_type_with_random<T>("vd", vector_cfg.len);
		cur_data.register_type_with_random<T>("vs2", vector_cfg.len);
		cur_data.register_type_with_random<T>("vs1", vector_cfg.len);
		cur_data.set_value_to_cfg(cur_cfg);
		cur_cfg.DESC = cur_data.get_DESC_from_inst(inst_fields,
							   vector_cfg.inst);
		if (record_mode)
			global_cfg.push_back(cur_cfg);
	} else {
		cur_data.set_value_from_cfg(cur_cfg);
	}

	global_flag_ptr->illegal = illegal_func(cur_data);

	std::vector<uint32_t> insts;
	save_context(insts);

	load_multi_vector<T, T, T>(
		insts, cur_data, { "vd", "vs2", "vs1" },
		{ vector_cfg.lmul, vector_cfg.lmul, vector_cfg.lmul },
		{ vector_cfg.len, vector_cfg.len, vector_cfg.len });
	store_multi_preinst_vector<T, T, T>(
		insts, cur_data, { "vd", "vs2", "vs1" },
		{ vector_cfg.lmul, vector_cfg.lmul, vector_cfg.lmul },
		{ vector_cfg.len, vector_cfg.len, vector_cfg.len });

	vsetvli_lmul_sew(insts, vector_cfg.lmul, vector_cfg.sew,
			 vector_cfg.len);
	csrrw(insts, CSR_VSTART, vector_cfg.vstart);
	csrrw(insts, CSR_VXRM, vector_cfg.vxrm);

	insts.push_back(vector_cfg.inst);

	store_multi_afterinst_vector<T>(insts, cur_data, { "vd" },
					{ vector_cfg.lmul },
					{ vector_cfg.len });
	restore_context(insts);

	run_instruction(insts);

	int has_illegal = check_afterinst_illegal();
	if (has_illegal >= 0)
		return has_illegal;

	save_multi_preinst_value_to_common<T, T, T>(
		cur_data, { "vd", "vs2", "vs1" },
		{ vector_cfg.len, vector_cfg.len, vector_cfg.len });

	self_func(cur_data);

	int is_error =
		check_multi_error<T>(cur_data, { "vd" }, { vector_cfg.len });
	if (is_error) {
		DEBUG << std::hex << json(cur_data).dump(4) << std::endl;
		if (early_stop)
			return 1;
	}
	reset_data();
	return 0;
}

//AES forward S-box
static uint8_t aes_sbox_fwd_table[256] = {
	0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b,
	0xfe, 0xd7, 0xab, 0x76, 0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0,
	0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0, 0xb7, 0xfd, 0x93, 0x26,
	0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
	0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2,
	0xeb, 0x27, 0xb2, 0x75, 0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0,
	0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84, 0x53, 0xd1, 0x00, 0xed,
	0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
	0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f,
	0x50, 0x3c, 0x9f, 0xa8, 0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5,
	0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2, 0xcd, 0x0c, 0x13, 0xec,
	0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
	0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14,
	0xde, 0x5e, 0x0b, 0xdb, 0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c,
	0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79, 0xe7, 0xc8, 0x37, 0x6d,
	0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
	0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f,
	0x4b, 0xbd, 0x8b, 0x8a, 0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e,
	0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e, 0xe1, 0xf8, 0x98, 0x11,
	0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
	0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f,
	0xb0, 0x54, 0xbb, 0x16
};

//AES inverse S-box
static uint8_t aes_sbox_inv_table[256] = {
	0x52, 0x09, 0x6a, 0xd5, 0x30, 0x36, 0xa5, 0x38, 0xbf, 0x40, 0xa3, 0x9e,
	0x81, 0xf3, 0xd7, 0xfb, 0x7c, 0xe3, 0x39, 0x82, 0x9b, 0x2f, 0xff, 0x87,
	0x34, 0x8e, 0x43, 0x44, 0xc4, 0xde, 0xe9, 0xcb, 0x54, 0x7b, 0x94, 0x32,
	0xa6, 0xc2, 0x23, 0x3d, 0xee, 0x4c, 0x95, 0x0b, 0x42, 0xfa, 0xc3, 0x4e,
	0x08, 0x2e, 0xa1, 0x66, 0x28, 0xd9, 0x24, 0xb2, 0x76, 0x5b, 0xa2, 0x49,
	0x6d, 0x8b, 0xd1, 0x25, 0x72, 0xf8, 0xf6, 0x64, 0x86, 0x68, 0x98, 0x16,
	0xd4, 0xa4, 0x5c, 0xcc, 0x5d, 0x65, 0xb6, 0x92, 0x6c, 0x70, 0x48, 0x50,
	0xfd, 0xed, 0xb9, 0xda, 0x5e, 0x15, 0x46, 0x57, 0xa7, 0x8d, 0x9d, 0x84,
	0x90, 0xd8, 0xab, 0x00, 0x8c, 0xbc, 0xd3, 0x0a, 0xf7, 0xe4, 0x58, 0x05,
	0xb8, 0xb3, 0x45, 0x06, 0xd0, 0x2c, 0x1e, 0x8f, 0xca, 0x3f, 0x0f, 0x02,
	0xc1, 0xaf, 0xbd, 0x03, 0x01, 0x13, 0x8a, 0x6b, 0x3a, 0x91, 0x11, 0x41,
	0x4f, 0x67, 0xdc, 0xea, 0x97, 0xf2, 0xcf, 0xce, 0xf0, 0xb4, 0xe6, 0x73,
	0x96, 0xac, 0x74, 0x22, 0xe7, 0xad, 0x35, 0x85, 0xe2, 0xf9, 0x37, 0xe8,
	0x1c, 0x75, 0xdf, 0x6e, 0x47, 0xf1, 0x1a, 0x71, 0x1d, 0x29, 0xc5, 0x89,
	0x6f, 0xb7, 0x62, 0x0e, 0xaa, 0x18, 0xbe, 0x1b, 0xfc, 0x56, 0x3e, 0x4b,
	0xc6, 0xd2, 0x79, 0x20, 0x9a, 0xdb, 0xc0, 0xfe, 0x78, 0xcd, 0x5a, 0xf4,
	0x1f, 0xdd, 0xa8, 0x33, 0x88, 0x07, 0xc7, 0x31, 0xb1, 0x12, 0x10, 0x59,
	0x27, 0x80, 0xec, 0x5f, 0x60, 0x51, 0x7f, 0xa9, 0x19, 0xb5, 0x4a, 0x0d,
	0x2d, 0xe5, 0x7a, 0x9f, 0x93, 0xc9, 0x9c, 0xef, 0xa0, 0xe0, 0x3b, 0x4d,
	0xae, 0x2a, 0xf5, 0xb0, 0xc8, 0xeb, 0xbb, 0x3c, 0x83, 0x53, 0x99, 0x61,
	0x17, 0x2b, 0x04, 0x7e, 0xba, 0x77, 0xd6, 0x26, 0xe1, 0x69, 0x14, 0x63,
	0x55, 0x21, 0x0c, 0x7d
};

//SM4 S-box
static uint8_t sm4_sbox_table[256] = {
	0xd6, 0x90, 0xe9, 0xfe, 0xcc, 0xe1, 0x3d, 0xb7, 0x16, 0xb6, 0x14, 0xc2,
	0x28, 0xfb, 0x2c, 0x05, 0x2b, 0x67, 0x9a, 0x76, 0x2a, 0xbe, 0x04, 0xc3,
	0xaa, 0x44, 0x13, 0x26, 0x49, 0x86, 0x06, 0x99, 0x9c, 0x42, 0x50, 0xf4,
	0x91, 0xef, 0x98, 0x7a, 0x33, 0x54, 0x0b, 0x43, 0xed, 0xcf, 0xac, 0x62,
	0xe4, 0xb3, 0x1c, 0xa9, 0xc9, 0x08, 0xe8, 0x95, 0x80, 0xdf, 0x94, 0xfa,
	0x75, 0x8f, 0x3f, 0xa6, 0x47, 0x07, 0xa7, 0xfc, 0xf3, 0x73, 0x17, 0xba,
	0x83, 0x59, 0x3c, 0x19, 0xe6, 0x85, 0x4f, 0xa8, 0x68, 0x6b, 0x81, 0xb2,
	0x71, 0x64, 0xda, 0x8b, 0xf8, 0xeb, 0x0f, 0x4b, 0x70, 0x56, 0x9d, 0x35,
	0x1e, 0x24, 0x0e, 0x5e, 0x63, 0x58, 0xd1, 0xa2, 0x25, 0x22, 0x7c, 0x3b,
	0x01, 0x21, 0x78, 0x87, 0xd4, 0x00, 0x46, 0x57, 0x9f, 0xd3, 0x27, 0x52,
	0x4c, 0x36, 0x02, 0xe7, 0xa0, 0xc4, 0xc8, 0x9e, 0xea, 0xbf, 0x8a, 0xd2,
	0x40, 0xc7, 0x38, 0xb5, 0xa3, 0xf7, 0xf2, 0xce, 0xf9, 0x61, 0x15, 0xa1,
	0xe0, 0xae, 0x5d, 0xa4, 0x9b, 0x34, 0x1a, 0x55, 0xad, 0x93, 0x32, 0x30,
	0xf5, 0x8c, 0xb1, 0xe3, 0x1d, 0xf6, 0xe2, 0x2e, 0x82, 0x66, 0xca, 0x60,
	0xc0, 0x29, 0x23, 0xab, 0x0d, 0x53, 0x4e, 0x6f, 0xd5, 0xdb, 0x37, 0x45,
	0xde, 0xfd, 0x8e, 0x2f, 0x03, 0xff, 0x6a, 0x72, 0x6d, 0x6c, 0x5b, 0x51,
	0x8d, 0x1b, 0xaf, 0x92, 0xbb, 0xdd, 0xbc, 0x7f, 0x11, 0xd9, 0x5c, 0x41,
	0x1f, 0x10, 0x5a, 0xd8, 0x0a, 0xc1, 0x31, 0x88, 0xa5, 0xcd, 0x7b, 0xbd,
	0x2d, 0x74, 0xd0, 0x12, 0xb8, 0xe5, 0xb4, 0xb0, 0x89, 0x69, 0x97, 0x4a,
	0x0c, 0x96, 0x77, 0x7e, 0x65, 0xb9, 0xf1, 0x09, 0xc5, 0x6e, 0xc6, 0x84,
	0x18, 0xf0, 0x7d, 0xec, 0x3a, 0xdc, 0x4d, 0x20, 0x79, 0xee, 0x5f, 0x3e,
	0xd7, 0xcb, 0x39, 0x48
};

//Multiply by 2 in AES GF(2^8)
static inline uint8_t aes_xt2(uint8_t x)
{
	return static_cast<uint8_t>((x << 1) ^ ((x & 0x80) ? 0x1b : 0));
}

//Generic multiplication over the AES Galois field GF(2^8)
static inline uint8_t aes_gfmul(uint8_t x, uint8_t y)
{
	uint8_t out = 0;
	for (int i = 0; i < 8; ++i) {
		if ((y >> i) & 1)
			out ^= x;
		x = aes_xt2(x);
	}
	return out;
}

//Take a particular byte from a 32-bit word.
static inline uint8_t get_byte(uint32_t x, int idx)
{
	return static_cast<uint8_t>((x >> (idx * 8)) & 0xff);
}

//Combine 4 bytes into a 32-bit word.
static inline uint32_t pack_bytes(uint8_t b3, uint8_t b2, uint8_t b1,
				  uint8_t b0)
{
	return (static_cast<uint32_t>(b3) << 24) |
	       (static_cast<uint32_t>(b2) << 16) |
	       (static_cast<uint32_t>(b1) << 8) | b0;
}

//Apply the S-box to the 4 bytes of a 32-bit word.
static inline uint32_t aes_subword_fwd(uint32_t x)
{
	return pack_bytes(aes_sbox_fwd_table[get_byte(x, 3)],
			  aes_sbox_fwd_table[get_byte(x, 2)],
			  aes_sbox_fwd_table[get_byte(x, 1)],
			  aes_sbox_fwd_table[get_byte(x, 0)]);
}

static inline uint32_t aes_subword_inv(uint32_t x)
{
	return pack_bytes(aes_sbox_inv_table[get_byte(x, 3)],
			  aes_sbox_inv_table[get_byte(x, 2)],
			  aes_sbox_inv_table[get_byte(x, 1)],
			  aes_sbox_inv_table[get_byte(x, 0)]);
}

//AES MixColumn
static inline uint32_t aes_mixcolumn_fwd(uint32_t x)
{
	uint8_t s0 = get_byte(x, 0);
	uint8_t s1 = get_byte(x, 1);
	uint8_t s2 = get_byte(x, 2);
	uint8_t s3 = get_byte(x, 3);
	uint8_t b0 = aes_xt2(s0) ^ (aes_xt2(s1) ^ s1) ^ s2 ^ s3;
	uint8_t b1 = s0 ^ aes_xt2(s1) ^ (aes_xt2(s2) ^ s2) ^ s3;
	uint8_t b2 = s0 ^ s1 ^ aes_xt2(s2) ^ (aes_xt2(s3) ^ s3);
	uint8_t b3 = (aes_xt2(s0) ^ s0) ^ s1 ^ s2 ^ aes_xt2(s3);
	return pack_bytes(b3, b2, b1, b0);
}

//AES InvMixColumn
static inline uint32_t aes_mixcolumn_inv(uint32_t x)
{
	uint8_t s0 = get_byte(x, 0);
	uint8_t s1 = get_byte(x, 1);
	uint8_t s2 = get_byte(x, 2);
	uint8_t s3 = get_byte(x, 3);
	uint8_t b0 = aes_gfmul(s0, 0xe) ^ aes_gfmul(s1, 0xb) ^
		     aes_gfmul(s2, 0xd) ^ aes_gfmul(s3, 0x9);
	uint8_t b1 = aes_gfmul(s0, 0x9) ^ aes_gfmul(s1, 0xe) ^
		     aes_gfmul(s2, 0xb) ^ aes_gfmul(s3, 0xd);
	uint8_t b2 = aes_gfmul(s0, 0xd) ^ aes_gfmul(s1, 0x9) ^
		     aes_gfmul(s2, 0xe) ^ aes_gfmul(s3, 0xb);
	uint8_t b3 = aes_gfmul(s0, 0xb) ^ aes_gfmul(s1, 0xd) ^
		     aes_gfmul(s2, 0x9) ^ aes_gfmul(s3, 0xe);
	return pack_bytes(b3, b2, b1, b0);
}

using eg128 = std::array<uint32_t, 4>;

//AES ShiftRows
static inline eg128 aes_shift_rows_fwd(const eg128 &ic)
{
	return {
		pack_bytes(get_byte(ic[3], 3), get_byte(ic[2], 2),
			   get_byte(ic[1], 1), get_byte(ic[0], 0)),
		pack_bytes(get_byte(ic[0], 3), get_byte(ic[3], 2),
			   get_byte(ic[2], 1), get_byte(ic[1], 0)),
		pack_bytes(get_byte(ic[1], 3), get_byte(ic[0], 2),
			   get_byte(ic[3], 1), get_byte(ic[2], 0)),
		pack_bytes(get_byte(ic[2], 3), get_byte(ic[1], 2),
			   get_byte(ic[0], 1), get_byte(ic[3], 0)),
	};
}

static inline eg128 aes_shift_rows_inv(const eg128 &ic)
{
	return {
		pack_bytes(get_byte(ic[1], 3), get_byte(ic[2], 2),
			   get_byte(ic[3], 1), get_byte(ic[0], 0)),
		pack_bytes(get_byte(ic[2], 3), get_byte(ic[3], 2),
			   get_byte(ic[0], 1), get_byte(ic[1], 0)),
		pack_bytes(get_byte(ic[3], 3), get_byte(ic[0], 2),
			   get_byte(ic[1], 1), get_byte(ic[2], 0)),
		pack_bytes(get_byte(ic[0], 3), get_byte(ic[1], 2),
			   get_byte(ic[2], 1), get_byte(ic[3], 0)),
	};
}

//Apply SubBytes to a 128-bit state.
static inline eg128 aes_subbytes_fwd(const eg128 &x)
{
	return { aes_subword_fwd(x[0]), aes_subword_fwd(x[1]),
		 aes_subword_fwd(x[2]), aes_subword_fwd(x[3]) };
}

static inline eg128 aes_subbytes_inv(const eg128 &x)
{
	return { aes_subword_inv(x[0]), aes_subword_inv(x[1]),
		 aes_subword_inv(x[2]), aes_subword_inv(x[3]) };
}

//Apply MixColumns to a 128-bit state.
static inline eg128 aes_mixcolumns_fwd(const eg128 &x)
{
	return { aes_mixcolumn_fwd(x[0]), aes_mixcolumn_fwd(x[1]),
		 aes_mixcolumn_fwd(x[2]), aes_mixcolumn_fwd(x[3]) };
}

static inline eg128 aes_mixcolumns_inv(const eg128 &x)
{
	return { aes_mixcolumn_inv(x[0]), aes_mixcolumn_inv(x[1]),
		 aes_mixcolumn_inv(x[2]), aes_mixcolumn_inv(x[3]) };
}

static inline eg128 eg128_xor(const eg128 &a, const eg128 &b)
{
	return { a[0] ^ b[0], a[1] ^ b[1], a[2] ^ b[2], a[3] ^ b[3] };
}

//RotWord in the AES Key Schedule
static inline uint32_t aes_rotword(uint32_t x)
{
	return pack_bytes(get_byte(x, 0), get_byte(x, 3), get_byte(x, 2),
			  get_byte(x, 1));
}

//AES round constant Rcon
static inline uint32_t aes_decode_rcon(uint32_t r)
{
	static const uint32_t rcon[16] = { 0x00000001, 0x00000002, 0x00000004,
					   0x00000008, 0x00000010, 0x00000020,
					   0x00000040, 0x00000080, 0x0000001b,
					   0x00000036, 0,	   0,
					   0,	       0,	   0,
					   0 };
	return rcon[r & 0xf];
}

//Perform SM4 S-box substitution on a 32-bit word.
static inline uint32_t sm4_subword(uint32_t x)
{
	return pack_bytes(sm4_sbox_table[get_byte(x, 3)],
			  sm4_sbox_table[get_byte(x, 2)],
			  sm4_sbox_table[get_byte(x, 1)],
			  sm4_sbox_table[get_byte(x, 0)]);
}

//Linear transformation in the SM4 round function
static inline uint32_t sm4_linear_transform(uint32_t x, uint32_t s)
{
	return x ^ s ^ crypto_rol32(s, 2) ^ crypto_rol32(s, 10) ^
	       crypto_rol32(s, 18) ^ crypto_rol32(s, 24);
}

//The linear transformation used in the round function of the SM4 key expansion
static inline uint32_t sm4_round_key(uint32_t x, uint32_t s)
{
	return x ^ s ^ crypto_rol32(s, 13) ^ crypto_rol32(s, 23);
}

//The permutation function in SM3
static inline uint32_t sm3_p0(uint32_t x)
{
	return x ^ crypto_rol32(x, 9) ^ crypto_rol32(x, 17);
}

static inline uint32_t sm3_p1(uint32_t x)
{
	return x ^ crypto_rol32(x, 15) ^ crypto_rol32(x, 23);
}

//The Boolean functions in the SM3 compression function
static inline uint32_t sm3_ff(uint32_t x, uint32_t y, uint32_t z, uint32_t j)
{
	if (j <= 15)
		return x ^ y ^ z;
	return (x & y) | (x & z) | (y & z);
}

static inline uint32_t sm3_gg(uint32_t x, uint32_t y, uint32_t z, uint32_t j)
{
	if (j <= 15)
		return x ^ y ^ z;
	return (x & y) | ((~x) & z);
}

//Constants in SM3
static inline uint32_t sm3_t(uint32_t j)
{
	return (j <= 15) ? 0x79cc4519u : 0x7a879d8au;
}

//The message expansion formulas in SM3
static inline uint32_t sm3_w(uint32_t m16, uint32_t m9, uint32_t m3,
			     uint32_t m13, uint32_t m6)
{
	return sm3_p1(m16 ^ m9 ^ crypto_rol32(m3, 15)) ^ crypto_rol32(m13, 7) ^
	       m6;
}

//The big Σ0 (Sigma0) function in the SHA-2 compression function.
template <typename T> T sha_sum0(T x)
{
	if constexpr (sizeof(T) == 4)
		return crypto_rotr<T>(x, 2) ^ crypto_rotr<T>(x, 13) ^
		       crypto_rotr<T>(x, 22);
	else
		return crypto_rotr<T>(x, 28) ^ crypto_rotr<T>(x, 34) ^
		       crypto_rotr<T>(x, 39);
}

//The big Σ1 (Sigma1) function in the SHA-2 compression function.
template <typename T> T sha_sum1(T x)
{
	if constexpr (sizeof(T) == 4)
		return crypto_rotr<T>(x, 6) ^ crypto_rotr<T>(x, 11) ^
		       crypto_rotr<T>(x, 25);
	else
		return crypto_rotr<T>(x, 14) ^ crypto_rotr<T>(x, 18) ^
		       crypto_rotr<T>(x, 41);
}

//The small σ0 (sigma0) function in the SHA-2 message expansion.
template <typename T> T sha_sig0(T x)
{
	if constexpr (sizeof(T) == 4)
		return crypto_rotr<T>(x, 7) ^ crypto_rotr<T>(x, 18) ^ (x >> 3);
	else
		return crypto_rotr<T>(x, 1) ^ crypto_rotr<T>(x, 8) ^ (x >> 7);
}

//The small σ1 (sigma1) function in the SHA-2 message expansion.
template <typename T> T sha_sig1(T x)
{
	if constexpr (sizeof(T) == 4)
		return crypto_rotr<T>(x, 17) ^ crypto_rotr<T>(x, 19) ^
		       (x >> 10);
	else
		return crypto_rotr<T>(x, 19) ^ crypto_rotr<T>(x, 61) ^ (x >> 6);
}

//The Ch (choice) function in SHA-2, which selects between f and g based on each bit of e.
template <typename T> T sha_ch(T x, T y, T z)
{
	return (x & y) ^ ((~x) & z);
}

//The Maj (majority) function in SHA-2, which takes the majority value among three bits from a, b, and c.
template <typename T> T sha_maj(T x, T y, T z)
{
	return (x & y) ^ (x & z) ^ (y & z);
}

//Implements one round of state update in the SHA-2 compression function, updating the a~h state using Ch, Maj, Σ0, Σ1, and the message word.
template <typename T>
void sha2_round(T &a, T &b, T &c, T &d, T &e, T &f, T &g, T &h, T w)
{
	T t1 = h + sha_sum1(e) + sha_ch(e, f, g) + w;
	T t2 = sha_sum0(a) + sha_maj(a, b, c);
	h = g;
	g = f;
	f = e;
	e = d + t1;
	d = c;
	c = b;
	b = a;
	a = t1 + t2;
}

static inline void crypto_main_begin(int argc, char *argv[],
				     const char *source_file)
{
	filename = std::filesystem::path(source_file).filename().string();
	datafile = filename + ".data";
	outfile = filename + ".out";
	init_program(argc, argv);
	init_vector_program();
}

static inline void crypto_report_error(c_cfg &cur_cfg, int has_error)
{
	if (has_error) {
		ERROR << "CSR: " << c_cfg::to_json(cur_cfg).at("CSR")
		      << std::endl;
		ERROR << vector_cfg << std::endl;
	}
}

//SM4 vsm4r model: 4 rounds of SM4 encryption/decryption
static inline eg128 vsm4r_model(const eg128 &x, const eg128 &rk)
{
	uint32_t x4 = sm4_linear_transform(x[0], sm4_subword(x[1] ^ x[2] ^
							     x[3] ^ rk[0]));
	uint32_t x5 = sm4_linear_transform(x[1], sm4_subword(x[2] ^ x[3] ^ x4 ^
							     rk[1]));
	uint32_t x6 =
		sm4_linear_transform(x[2], sm4_subword(x[3] ^ x4 ^ x5 ^ rk[2]));
	uint32_t x7 =
		sm4_linear_transform(x[3], sm4_subword(x4 ^ x5 ^ x6 ^ rk[3]));
	return { x4, x5, x6, x7 };
}

//AES vaesef model: SubBytes → ShiftRows → AddRoundKey
static inline eg128 vaesef_model(const eg128 &state, const eg128 &rkey)
{
	return eg128_xor(aes_shift_rows_fwd(aes_subbytes_fwd(state)), rkey);
}

//AES vaesem model: SubBytes → ShiftRows → MixColumns → AddRoundKey
static inline eg128 vaesem_model(const eg128 &state, const eg128 &rkey)
{
	return eg128_xor(
		aes_mixcolumns_fwd(aes_shift_rows_fwd(aes_subbytes_fwd(state))),
		rkey);
}

//AES vaesdf model: InvShiftRows → InvSubBytes → AddRoundKey
static inline eg128 vaesdf_model(const eg128 &state, const eg128 &rkey)
{
	return eg128_xor(aes_subbytes_inv(aes_shift_rows_inv(state)), rkey);
}

//AES vaesdm model: InvShiftRows → InvSubBytes → AddRoundKey → InvMixColumns
static inline eg128 vaesdm_model(const eg128 &state, const eg128 &rkey)
{
	return aes_mixcolumns_inv(
		eg128_xor(aes_subbytes_inv(aes_shift_rows_inv(state)), rkey));
}

//GF(2^128) type for Zvkg (GCM/GMAC)
struct gf128_t {
	uint64_t hi;
	uint64_t lo;
};

static inline gf128_t gf128_zero()
{
	return { 0, 0 };
}

static inline gf128_t gf128_xor(const gf128_t &lhs, const gf128_t &rhs)
{
	return { lhs.hi ^ rhs.hi, lhs.lo ^ rhs.lo };
}

static inline uint8_t brev8_byte(uint8_t x)
{
	uint8_t result = 0;
	for (int i = 0; i < 8; ++i) {
		result |= ((x >> i) & 1u) << (7 - i);
	}
	return result;
}

static inline gf128_t load_eg128_from_e32(const uint32_t *data, int group_idx)
{
	int base = group_idx * 4;
	gf128_t result;
	result.hi = (static_cast<uint64_t>(data[base + 3]) << 32) |
		    static_cast<uint64_t>(data[base + 2]);
	result.lo = (static_cast<uint64_t>(data[base + 1]) << 32) |
		    static_cast<uint64_t>(data[base + 0]);
	return result;
}

static inline void store_eg128_to_e32(uint32_t *data, int group_idx,
				      const gf128_t &x)
{
	int base = group_idx * 4;
	data[base + 0] = static_cast<uint32_t>(x.lo & 0xffffffffu);
	data[base + 1] = static_cast<uint32_t>(x.lo >> 32);
	data[base + 2] = static_cast<uint32_t>(x.hi & 0xffffffffu);
	data[base + 3] = static_cast<uint32_t>(x.hi >> 32);
}

static inline gf128_t gf128_brev8(const gf128_t &x)
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

static inline bool gf128_get_bit(const gf128_t &x, int bit_idx)
{
	if (bit_idx < 64) {
		return ((x.lo >> bit_idx) & 1ull) != 0;
	}
	return ((x.hi >> (bit_idx - 64)) & 1ull) != 0;
}

static inline bool gf128_get_msb(const gf128_t &x)
{
	return ((x.hi >> 63) & 1ull) != 0;
}

static inline void gf128_shl1_inplace(gf128_t &x)
{
	uint64_t lo_carry = x.lo >> 63;
	x.lo <<= 1;
	x.hi = (x.hi << 1) | lo_carry;
}

static inline void gf128_xor_low8_inplace(gf128_t &x, uint8_t c)
{
	x.lo ^= static_cast<uint64_t>(c);
}

static inline gf128_t gf128_mul_gcm(const gf128_t &y, const gf128_t &h_init)
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

#endif
