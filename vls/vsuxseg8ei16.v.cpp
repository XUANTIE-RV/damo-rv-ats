/*
 * Copyright (c) 2026 Alibaba Group.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "../common/v_common.h"

const std::vector<InstField> vop_inst_fields = {
	{ 31, 29, 0x07, true, RegClass::NotReg, "nf" },
	{ 28, 28, 0x00, true, RegClass::NotReg, "mew" },
	{ 27, 26, 0x01, true, RegClass::NotReg, "mop" },
	{ 25, 25, 0x00, false, RegClass::NotReg, "vm" },
	{ 24, 20, 0x00, false, RegClass::Vector, "vs2" },
	{ 19, 15, 0x00, false, RegClass::Int, "rs1" },
	{ 14, 12, 0x05, true, RegClass::NotReg, "width" },
	{ 11, 7, 0x00, false, RegClass::Vector, "vs3" },
	{ 6, 0, 0x27, true, RegClass::NotReg, "opcode" }
};

static constexpr int NFIELDS = 8;
static constexpr uint64_t MEM_BUF_BYTES = 8192;

/* Index EEW=16, so index EMUL = (16/SEW)*LMUL */
static int get_index_emul_signed()
{
	int tmp_lmul = (vector_cfg.lmul > 4) ? (int)vector_cfg.lmul - 8 :
					       (int)vector_cfg.lmul;
	return tmp_lmul + ((int)sew_e16 - (int)vector_cfg.sew);
}

static uint64_t get_index_emul()
{
	return lmul_pow(vector_cfg.lmul, (int)sew_e16 - (int)vector_cfg.sew);
}

int check_illegal(c_data &cur_data)
{
	int nf = cur_data.map_reg_index["nf"];
	int vs3 = cur_data.map_reg_index["vs3"];
	int vs2 = cur_data.map_reg_index["vs2"];

	int idx_emul_signed = get_index_emul_signed();
	if (idx_emul_signed < -3 || idx_emul_signed > 3) {
		print_illegal_status(1);
		return 1;
	}

	if (!is_legal_nf(vs3, nf + 1, vector_cfg.lmul)) {
		print_illegal_status(1);
		return 1;
	}

	int index_eew_ratio = (int)sew_e16 - (int)vector_cfg.sew;

	ValidationConfig config = {
		.check_align = true,
		.check_overlap = false,
		.check_vm = false,
	};

	int illegal = !(VectorRegValidator::validate(
		VregOperand::one_pow(vs3),
		{ VregOperand::custom(vs2, index_eew_ratio) }, 0, config));

	print_illegal_status(illegal);
	return illegal;
}

template <typename Td, typename Tidx> int run_self_result(c_data &cur_data)
{
	int vm_bit = cur_data.map_reg_index["vm"];

	Tidx *vs2_data = static_cast<Tidx *>(
		cur_data.map_preinst_typed_value["vs2"].get());

	uint8_t *vm_data = nullptr;
	if (!vm_bit) {
		vm_data = static_cast<uint8_t *>(
			cur_data.map_preinst_typed_value["vm"].get());
	}

	/* Initialize selfcheck with the original memory content */
	cur_data.map_selfcheck_typed_value["mem_result"] =
		c_data::process_from_common<uint8_t>(
			cur_data.map_preinst_reg_value["mem_result"]);
	uint8_t *selfcheck_data = static_cast<uint8_t *>(
		cur_data.map_selfcheck_typed_value["mem_result"].get());

	for (uint64_t j = 0; j < vector_cfg.len; j++) {
		int row = j / 8;
		int col = j % 8;

		if (j < vector_cfg.vstart ||
		    (!vm_bit && !(vm_data[row] & (1ull << col)))) {
			continue;
		}

		for (int f = 0; f < NFIELDS; f++) {
			Td *vs3_field_data = static_cast<Td *>(
				cur_data.map_preinst_typed_value
					["vs3_f" + std::to_string(f)]
						.get());
			uint64_t byte_offset = (uint64_t)vs2_data[j] +
					       (uint64_t)f * sizeof(Td);
			*(Td *)(selfcheck_data + byte_offset) =
				vs3_field_data[j];
		}
	}
	return 0;
}

template <typename Td, typename Tidx>
int per_run(int it, c_cfg &cur_cfg, c_data &cur_data)
{
	cur_data.set_value_from_inst(vop_inst_fields, vector_cfg.inst);

	/* Data EMUL = LMUL (EEW=SEW) */
	int data_emul_signed = (vector_cfg.lmul > 4) ?
				       (int)vector_cfg.lmul - 8 :
				       (int)vector_cfg.lmul;
	uint64_t vs3_bytes;
	if (data_emul_signed >= 0)
		vs3_bytes = vlenb << data_emul_signed;
	else
		vs3_bytes = vlenb >> (-data_emul_signed);

	/* Per-field vector register stride */
	int vs3_stride = 1;
	if (data_emul_signed > 0)
		vs3_stride = 1 << data_emul_signed;

	/* Index EMUL and bytes */
	int idx_emul_signed = get_index_emul_signed();
	uint64_t idx_bytes;
	if (idx_emul_signed >= 0)
		idx_bytes = vlenb << idx_emul_signed;
	else
		idx_bytes = vlenb >> (-idx_emul_signed);
	uint64_t idx_emul = get_index_emul();

	uint64_t sew_bytes = sizeof(Td);
	uint64_t idx_sew_bytes = sizeof(Tidx);
	uint64_t mem_size = MEM_BUF_BYTES;

	if (random_mode) {
		for (int f = 0; f < NFIELDS; f++) {
			std::string field_name = "vs3_f" + std::to_string(f);
			cur_data.register_type_with_random<Td>(
				field_name, vs3_bytes / sew_bytes);
		}
		cur_data.register_type_with_random<uint8_t>("mem_result",
							    mem_size);
		/* Generate random index values: SEW-aligned, within mem bounds */
		cur_data.map_reg_value["vs2"] =
			std::vector<uint64_t>(idx_bytes / idx_sew_bytes);
		{
			auto &v = cur_data.map_reg_value["vs2"];
			uint64_t max_offset = mem_size - NFIELDS * sew_bytes;
			uint64_t aligned_max =
				(max_offset / sew_bytes) * sew_bytes;
			uint64_t clamp = MEM_BUF_BYTES - NFIELDS * sew_bytes;
			if (aligned_max > clamp)
				aligned_max = (clamp / sew_bytes) * sew_bytes;
			for (uint64_t i = 0; i < idx_bytes / idx_sew_bytes;
			     i++) {
				uint64_t val =
					get_rand_bound<uint64_t>(
						0, aligned_max / sew_bytes) *
					sew_bytes;
				v[i] = val;
			}
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
	int vs2 = cur_data.map_reg_index["vs2"];
	int vs3 = cur_data.map_reg_index["vs3"];

	std::vector<uint32_t> insts;

	save_context(insts);
	vzero_all(insts);

	/* Load source data for each field register group */
	for (int f = 0; f < NFIELDS; f++) {
		std::string field_name = "vs3_f" + std::to_string(f);
		cur_data.map_reg_typed_value[field_name] =
			c_data::process_from_common<Td>(
				cur_data.map_reg_value[field_name]);
		if (vs3 + f * vs3_stride >= 32)
			continue;
		load_vector<Td>(insts, vs3 + f * vs3_stride,
				cur_data.map_reg_typed_value[field_name].get(),
				vector_cfg.lmul, vs3_bytes / sew_bytes);
	}

	if (!vm_bit) {
		load_single_vector<uint8_t>(insts, cur_data, "vm", lmul_m1,
					    vlenb);
	}

	/* Load vs2 index data */
	cur_data.map_reg_typed_value["vs2"] = c_data::process_from_common<Tidx>(
		cur_data.map_reg_value["vs2"]);
	load_vector<Tidx>(insts, vs2, cur_data.map_reg_typed_value["vs2"].get(),
			  idx_emul, idx_bytes / idx_sew_bytes);

	/* Store pre-instruction snapshots for each field */
	for (int f = 0; f < NFIELDS; f++) {
		std::string field_name = "vs3_f" + std::to_string(f);
		cur_data.map_preinst_typed_value[field_name] =
			make_zero_buffer<Td>(vs3_bytes / sew_bytes);
		if (vs3 + f * vs3_stride >= 32)
			continue;
		store_vector<Td>(
			insts, vs3 + f * vs3_stride,
			cur_data.map_preinst_typed_value[field_name].get(),
			vector_cfg.lmul, vs3_bytes / sew_bytes);
	}
	if (!vm_bit) {
		cur_data.map_preinst_typed_value["vm"] =
			make_zero_buffer<uint8_t>(vlenb);
		store_vector<uint8_t>(
			insts, 0, cur_data.map_preinst_typed_value["vm"].get(),
			lmul_m1, vlenb);
	}
	cur_data.map_preinst_typed_value["vs2"] =
		make_zero_buffer<Tidx>(idx_bytes / idx_sew_bytes);
	store_vector<Tidx>(insts, vs2,
			   cur_data.map_preinst_typed_value["vs2"].get(),
			   idx_emul, idx_bytes / idx_sew_bytes);

	/* Prepare memory destination buffer */
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

	/* Load memory destination address into rs1 */
	void *mem_ptr = cur_data.map_reg_typed_value["mem_result"].get();
	load_addr_to_gpr(insts, mem_ptr, rs1,
			 cur_data.map_reg_value["mem_data"].size());

	/* Execute the vsuxseg8ei16.v instruction */
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
			c_data::process_to_common<Td>(
				cur_data.map_preinst_typed_value[field_name],
				vs3_bytes / sew_bytes);
	}
	cur_data.map_preinst_reg_value["vs2"] = c_data::process_to_common<Tidx>(
		cur_data.map_preinst_typed_value["vs2"],
		idx_bytes / idx_sew_bytes);
	cur_data.map_preinst_reg_value["mem_result"] =
		c_data::process_to_common<uint8_t>(
			cur_data.map_preinst_typed_value["mem_result"],
			mem_size);
	if (!vm_bit) {
		cur_data.map_preinst_reg_value["vm"] =
			c_data::process_to_common<uint8_t>(
				cur_data.map_preinst_typed_value["vm"], vlenb);
	}

	run_self_result<Td, Tidx>(cur_data);

	/* Set afterinst for memory: the actual store destination */
	cur_data.map_afterinst_typed_value["mem_result"] =
		cur_data.map_reg_typed_value["mem_result"];

	/* Remove vs3 field entries from preinst map before mem_result check */
	for (int f = 0; f < NFIELDS; f++) {
		std::string field_name = "vs3_f" + std::to_string(f);
		cur_data.map_preinst_reg_value.erase(field_name);
	}

	int is_error = check_multi_error<uint8_t>(cur_data, { "mem_result" },
						  { mem_size });

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
			has_error = per_run<uint8_t, uint16_t>(it, cur_cfg,
							       cur_data);
			break;
		case sew_e16:
			has_error = per_run<uint16_t, uint16_t>(it, cur_cfg,
								cur_data);
			break;
		case sew_e32:
			has_error = per_run<uint32_t, uint16_t>(it, cur_cfg,
								cur_data);
			break;
		case sew_e64:
			has_error = per_run<uint64_t, uint16_t>(it, cur_cfg,
								cur_data);
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
