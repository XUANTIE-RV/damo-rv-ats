/*
 * Copyright (c) 2026 Alibaba Group.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "../common/v_common.h"

const std::vector<InstField> vop_inst_fields = {
	{ 31, 29, 0x06, true, RegClass::NotReg, "nf" },
	{ 28, 28, 0x00, true, RegClass::NotReg, "mew" },
	{ 27, 26, 0x02, true, RegClass::NotReg, "mop" },
	{ 25, 25, 0x00, false, RegClass::NotReg, "vm" },
	{ 24, 20, 0x00, false, RegClass::Int, "rs2" },
	{ 19, 15, 0x00, false, RegClass::Int, "rs1" },
	{ 14, 12, 0x07, true, RegClass::NotReg, "width" },
	{ 11, 7, 0x00, false, RegClass::Vector, "vs3" },
	{ 6, 0, 0x27, true, RegClass::NotReg, "opcode" }
};

static constexpr int NFIELDS = 7;

/* Large memory buffer to absorb arbitrary signed strides. The base address
 * passed to rs1 sits in the middle so positive/negative strides both stay
 * within bounds. */
static constexpr uint64_t MEM_BUF_BYTES = 65536;
static constexpr uint64_t MEM_BUF_HALF = MEM_BUF_BYTES / 2;
/* Stride magnitude is bounded so j*|stride| + f*sizeof(T) stays inside
 * MEM_BUF_HALF for any plausible (vlen, lmul, nf, eew) combination. */
static constexpr int64_t STRIDE_ABS_MAX = 128;

int check_illegal(c_data &cur_data)
{
	int nf = cur_data.map_reg_index["nf"];
	int vs3 = cur_data.map_reg_index["vs3"];
	int illegal = !(vext_check_store(vs3, nf + 1, sew_e64));
	print_illegal_status(illegal);
	return illegal;
}

int run_self_result(c_data &cur_data)
{
	int vm_bit = cur_data.map_reg_index["vm"];
	int rs2 = cur_data.map_reg_index["rs2"];

	uint8_t *vm_data = nullptr;
	if (!vm_bit) {
		vm_data = static_cast<uint8_t *>(
			cur_data.map_preinst_typed_value["vm"].get());
	}

	int64_t stride = 0;
	if (rs2 != 0) {
		int64_t *sv = static_cast<int64_t *>(
			cur_data.map_reg_typed_value["stride_value"].get());
		stride = sv[0];
	}

	/* Initialize selfcheck with the original memory content (full buffer) */
	cur_data.map_selfcheck_typed_value["mem_result"] =
		c_data::process_from_common<uint8_t>(
			cur_data.map_preinst_reg_value["mem_result"]);
	uint8_t *selfcheck_buf = static_cast<uint8_t *>(
		cur_data.map_selfcheck_typed_value["mem_result"].get());
	uint8_t *base8 = selfcheck_buf + MEM_BUF_HALF;

	uint64_t *vs3_field_data[NFIELDS];
	for (int f = 0; f < NFIELDS; f++) {
		vs3_field_data[f] = static_cast<uint64_t *>(
			cur_data.map_preinst_typed_value["vs3_f" +
							 std::to_string(f)]
				.get());
	}

	for (uint64_t j = 0; j < vector_cfg.len; j++) {
		int row = j / 8;
		int col = j % 8;

		if (j < vector_cfg.vstart ||
		    (!vm_bit && !(vm_data[row] & (1ull << col)))) {
			continue;
		}

		for (int f = 0; f < NFIELDS; f++) {
			/* addr = base + j * stride + f * sizeof(T) */
			int64_t off = (int64_t)j * stride +
				      (int64_t)f * (int64_t)sizeof(uint64_t);
			*reinterpret_cast<uint64_t *>(base8 + off) =
				vs3_field_data[f][j];
		}
	}
	return 0;
}

int per_run(int it, c_cfg &cur_cfg, c_data &cur_data)
{
	cur_data.set_value_from_inst(vop_inst_fields, vector_cfg.inst);

	/* EMUL for vssseg7e64: EEW=8(sew_e64), so emul_ratio = sew_e64 - sew */
	uint64_t emul =
		lmul_pow(vector_cfg.lmul, -(int)vector_cfg.sew + (int)sew_e64);

	/* Per-field vector register stride (capped at 1 for fractional EMUL). */
	int vs3_stride = (emul > 4) ? 1 : (1 << emul);

	/* Compute EMUL-based register group size in bytes */
	int emul_signed = (int)sew_e64 - (int)vector_cfg.sew +
			  ((vector_cfg.lmul > 4) ? (int)vector_cfg.lmul - 8 :
						   (int)vector_cfg.lmul);
	uint64_t field_bytes;
	if (emul_signed >= 0)
		field_bytes = vlenb << emul_signed;
	else
		field_bytes = vlenb >> (-emul_signed);

	if (random_mode) {
		for (int f = 0; f < NFIELDS; f++) {
			std::string field_name = "vs3_f" + std::to_string(f);
			cur_data.register_type_with_random<uint64_t>(
				field_name, field_bytes / sizeof(uint64_t));
		}
		/* Allocate the large destination buffer; rs1 base = mid. */
		cur_data.register_type_with_random<uint8_t>("mem_result",
							    MEM_BUF_BYTES);
		/* Generate one signed 64-bit stride, clamped to [-MAX, MAX]. */
		cur_data.register_type_with_random<int64_t>("stride_value", 1);
		{
			int64_t *sv = reinterpret_cast<int64_t *>(
				cur_data.map_reg_value["stride_value"].data());
			int64_t s = sv[0] % (2 * STRIDE_ABS_MAX + 1);
			if (s < -STRIDE_ABS_MAX)
				s += (2 * STRIDE_ABS_MAX + 1);
			if (s > STRIDE_ABS_MAX)
				s -= (2 * STRIDE_ABS_MAX + 1);
			sv[0] = s;
		}
		if (!cur_data.map_reg_index["vm"])
			cur_data.register_type_with_random<uint8_t>("vm",
								    vlenb);
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

	int vm_bit = cur_data.map_reg_index["vm"];
	int rs1 = cur_data.map_reg_index["rs1"];
	int rs2 = cur_data.map_reg_index["rs2"];
	int vs3 = cur_data.map_reg_index["vs3"];

	/* Skip iteration when rs1==rs2: load_reg would overwrite the base
	 * address with the stride value, causing illegal memory access. */
	if (rs1 == rs2) {
		reset_data();
		return 0;
	}

	std::vector<uint32_t> insts;

	save_context(insts);
	vzero_all(insts);

	/* Load source data for each field register group */
	for (int f = 0; f < NFIELDS; f++) {
		std::string field_name = "vs3_f" + std::to_string(f);
		cur_data.map_reg_typed_value[field_name] =
			c_data::process_from_common<uint64_t>(
				cur_data.map_reg_value[field_name]);
		if (vs3 + f * vs3_stride >= 32)
			continue;
		load_vector<uint64_t>(
			insts, vs3 + f * vs3_stride,
			cur_data.map_reg_typed_value[field_name].get(), emul,
			field_bytes / sizeof(uint64_t));
	}

	if (!vm_bit) {
		load_single_vector<uint8_t>(insts, cur_data, "vm", lmul_m1,
					    vlenb);
	}

	/* Store pre-instruction snapshots for each field */
	for (int f = 0; f < NFIELDS; f++) {
		std::string field_name = "vs3_f" + std::to_string(f);
		cur_data.map_preinst_typed_value[field_name] =
			make_zero_buffer<uint64_t>(field_bytes /
						   sizeof(uint64_t));
		if (vs3 + f * vs3_stride >= 32)
			continue;
		store_vector<uint64_t>(
			insts, vs3 + f * vs3_stride,
			cur_data.map_preinst_typed_value[field_name].get(),
			emul, field_bytes / sizeof(uint64_t));
	}
	if (!vm_bit) {
		cur_data.map_preinst_typed_value["vm"] =
			make_zero_buffer<uint8_t>(vlenb);
		store_vector<uint8_t>(
			insts, 0, cur_data.map_preinst_typed_value["vm"].get(),
			lmul_m1, vlenb);
	}

	/* Prepare destination buffer with initial content; preinst snapshot
	 * captures the full buffer as a baseline for selfcheck. */
	cur_data.map_reg_typed_value["mem_result"] =
		c_data::process_from_common<uint8_t>(
			cur_data.map_reg_value["mem_result"]);
	cur_data.map_preinst_typed_value["mem_result"] =
		c_data::process_from_common<uint8_t>(
			cur_data.map_reg_value["mem_result"]);

	/* Set actual vtype and vl for the target instruction */
	vsetvli_lmul_sew(insts, vector_cfg.lmul, vector_cfg.sew,
			 vector_cfg.len);
	csrrw(insts, CSR_VSTART, vector_cfg.vstart);

	/* Load destination base into rs1 (mid-buffer for signed strides). */
	uint8_t *mem_buf = static_cast<uint8_t *>(
		cur_data.map_reg_typed_value["mem_result"].get());
	void *base_ptr = mem_buf + MEM_BUF_HALF;
	load_addr_to_gpr(insts, base_ptr, rs1, MEM_BUF_HALF);

	/* Materialize stride into rs2 (skip when rs2 == x0). load_reg() loads
	 * M[ptr] into reg, so we stage the stride scalar into val_placeholder
	 * (same trick csrrw uses) so that load_reg reads the correct value. */
	if (rs2 != 0) {
		cur_data.map_reg_typed_value["stride_value"] =
			c_data::process_from_common<int64_t>(
				cur_data.map_reg_value["stride_value"]);
		int64_t *sv = static_cast<int64_t *>(
			cur_data.map_reg_typed_value["stride_value"].get());
		val_placeholder[val_top++] = (uint64_t)sv[0];
		uint64_t stride_ptr = (uint64_t)(&val_placeholder[val_top - 1]);
		load_reg(insts, stride_ptr, rs2);
	}

	/* Execute the vssseg7e64.v instruction */
	insts.push_back(vector_cfg.inst);

	restore_context(insts);
	run_instruction(insts);

	int has_illegal = check_afterinst_illegal();
	if (has_illegal >= 0)
		return has_illegal;

	/* Convert preinst typed values to common format for selfcheck */
	for (int f = 0; f < NFIELDS; f++) {
		std::string field_name = "vs3_f" + std::to_string(f);
		cur_data.map_preinst_reg_value[field_name] =
			c_data::process_to_common<uint64_t>(
				cur_data.map_preinst_typed_value[field_name],
				field_bytes / sizeof(uint64_t));
	}
	cur_data.map_preinst_reg_value["mem_result"] =
		c_data::process_to_common<uint8_t>(
			cur_data.map_preinst_typed_value["mem_result"],
			MEM_BUF_BYTES);
	if (!vm_bit) {
		cur_data.map_preinst_reg_value["vm"] =
			c_data::process_to_common<uint8_t>(
				cur_data.map_preinst_typed_value["vm"], vlenb);
	}

	run_self_result(cur_data);

	/* Remove vs3 field entries from preinst map before mem_result check.
	 * check_single_error iterates ALL preinst entries for verbose logging,
	 * and vs3_f* vectors are much shorter than MEM_BUF_BYTES, causing OOB. */
	for (int f = 0; f < NFIELDS; f++) {
		std::string field_name = "vs3_f" + std::to_string(f);
		cur_data.map_preinst_reg_value.erase(field_name);
	}

	/* Set afterinst for memory: the actual store destination */
	cur_data.map_afterinst_typed_value["mem_result"] =
		cur_data.map_reg_typed_value["mem_result"];

	int is_error = check_multi_error<uint8_t>(cur_data, { "mem_result" },
						  { MEM_BUF_BYTES });

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

		int has_error = per_run(it, cur_cfg, cur_data);

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
