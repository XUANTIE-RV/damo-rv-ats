/*
 * Copyright (c) 2026 Alibaba Group.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "../common/v_common.h"

const std::vector<InstField> vop_inst_fields = {
	{ 31, 26, 0x10, true, RegClass::NotReg, "funct6" },
	{ 25, 25, 0x01, true, RegClass::NotReg, "vm" },
	{ 24, 20, 0x00, false, RegClass::Vector, "vs2" },
	{ 19, 15, 0x00, true, RegClass::NotReg, "vs1" },
	{ 14, 12, OPMVV, true, RegClass::NotReg, "funct3" },
	{ 11, 7, 0x00, false, RegClass::Int, "rd" },
	{ 6, 0, opcode_vector, true, RegClass::NotReg, "opcode" }
};

// =============================================================================
// vmv.x.s: x[rd] = vs2[0]
// Copies a single SEW-wide element from index 0 of vs2 to integer register rd.
// If SEW < XLEN, the value is sign-extended to XLEN bits.
// If SEW > XLEN, the least-significant XLEN bits are transferred.
// Ignores LMUL and vector register groups.
// Executes even if vstart >= vl or vl=0.
// vm=0 is reserved (illegal).
// =============================================================================
int check_illegal(c_data &cur_data)
{
	// vm is fixed to 1 (vm=0 is reserved/illegal encoding)
	// vs1 is fixed to 0x00
	// Ignores LMUL, so no alignment checks needed
	// rd is a scalar register, so no vector overlap checks needed
	// Reference: vmadc.vvm.cpp uses VectorRegValidator::validate, but
	// vmv.x.s does not require any such checks since:
	//   - vm is always 1 (no mask check)
	//   - LMUL is ignored (no alignment check)
	//   - destination is scalar rd (no vector overlap check)
	int vs2 = cur_data.map_reg_index["vs2"];
	int vm_bit = cur_data.map_reg_index["vm"];
	int illegal = !vext_check_load(vs2, 1, vector_cfg.sew, vm_bit);
	print_illegal_status(illegal);
	return illegal;
}

template <typename Ts2> int run_self_result(c_data &cur_data)
{
	int vs2 = cur_data.map_reg_index["vs2"];
	int rd = cur_data.map_reg_index["rd"];

	Ts2 *vs2_data = static_cast<Ts2 *>(
		cur_data.map_preinst_typed_value["vs2"].get());

	cur_data.map_selfcheck_typed_value["rd"] =
		make_zero_buffer<uint64_t>(1);
	uint64_t *selfcheck_data = static_cast<uint64_t *>(
		cur_data.map_selfcheck_typed_value["rd"].get());

	// vmv.x.s: rd = sign_extend(vs2[0]) to XLEN
	// The instruction executes regardless of vstart and vl values
	using SignedTs2 = typename std::make_signed<Ts2>::type;
	int64_t val = static_cast<int64_t>(static_cast<SignedTs2>(vs2_data[0]));
	selfcheck_data[0] = static_cast<uint64_t>(val);

	return 0;
}

template <typename Ts2> int per_run(int it, c_cfg &cur_cfg, c_data &cur_data)
{
	cur_data.set_value_from_inst(vop_inst_fields, vector_cfg.inst);

	if (random_mode) {
		cur_data.register_type_with_random<Ts2>("vs2", vector_cfg.len);
		cur_data.register_type_with_random<uint64_t>("rd", 1);
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

	load_multi_vector<Ts2>(insts, cur_data, { "vs2" }, { vector_cfg.lmul },
			       { vector_cfg.len });

	load_multi_int<uint64_t>(insts, cur_data, { "rd" });

	store_multi_preinst_vector<Ts2>(insts, cur_data, { "vs2" },
					{ vector_cfg.lmul },
					{ vector_cfg.len });

	store_multi_preinst_int<uint64_t>(insts, cur_data, { "rd" });

	vsetvli_lmul_sew(insts, vector_cfg.lmul, vector_cfg.sew,
			 vector_cfg.len);
	csrrw(insts, CSR_VSTART, vector_cfg.vstart);
	csrrw(insts, CSR_VXRM, vector_cfg.vxrm);

	insts.push_back(vector_cfg.inst);

	store_multi_afterinst_int<uint64_t>(insts, cur_data, { "rd" });

	restore_context(insts);

	run_instruction(insts);

	int has_illegal = check_afterinst_illegal();
	if (has_illegal >= 0)
		return has_illegal;

	save_multi_preinst_value_to_common<Ts2, uint64_t>(
		cur_data, { "vs2", "rd" }, { vector_cfg.len, 1 });

	run_self_result<Ts2>(cur_data);

	int is_error = check_multi_error<uint64_t>(cur_data, { "rd" }, { 1 });

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
			has_error = per_run<uint8_t>(it, cur_cfg, cur_data);
			break;
		case sew_e16:
			has_error = per_run<uint16_t>(it, cur_cfg, cur_data);
			break;
		case sew_e32:
			has_error = per_run<uint32_t>(it, cur_cfg, cur_data);
			break;
		case sew_e64:
			has_error = per_run<uint64_t>(it, cur_cfg, cur_data);
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
