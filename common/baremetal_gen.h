/*
 * Copyright (c) 2026 Alibaba Group.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Baremetal code generator for DAMO-RV-ATS.
 *
 * Records high-level semantic operations during Linux fuzzing, then generates
 * a standalone bare-metal C program with inline assembly that can be compiled
 * and run on real RISC-V hardware without any OS dependency.
 *
 * Key design:
 *   - Data addresses are loaded via "la" pseudo-instruction (position-independent)
 *   - Illegal instructions are handled by a trap handler that skips (mepc += 4)
 *   - Compatible with damo-priv-test framework structure
 *
 * Usage: run the Linux test with --baremetal flag:
 *   qemu-riscv64 vadd.vv.elf --runtime 100 --baremetal
 *
 * Outputs:
 *   <testname>.baremetal.c       - C + inline asm main program
 *   <testname>.baremetal_entry.S - Entry point + trap handler
 *   <testname>.baremetal.ld      - Linker script
 *   <testname>.baremetal.mk      - Makefile for building
 */

#ifndef __BAREMETAL_GEN_H__
#define __BAREMETAL_GEN_H__

#include <cstdint>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <iostream>
#include <set>

/* A vector register load operation to replay in baremetal */
struct BmVectorOp {
	uint32_t vreg; /* vector register number */
	uint32_t eew; /* element width in bytes: 1/2/4/8 */
	uint32_t lmul; /* lmul encoding for vsetvli */
	uint32_t vl; /* number of elements */
	std::vector<uint8_t> data; /* raw data bytes */
	std::string symbol; /* generated data symbol name */
};

/* A scalar register load operation */
struct BmScalarOp {
	uint32_t reg; /* target GPR number */
	uint64_t value; /* 64-bit value to load */
};

/* A memory operand: scalar register points to a data buffer */
struct BmMemOp {
	uint32_t reg; /* GPR that holds the memory address */
	std::vector<uint8_t> data; /* data buffer contents */
	std::string symbol; /* generated data symbol name */
};

/* Expected result from selfcheck (software golden model).
 * Two kinds:
 *   - Vector register result: store vd via vse8.v, then byte-compare
 *   - Memory result: the target instruction wrote to a mem_op buffer,
 *     just byte-compare that buffer directly (no vse needed) */
struct BmExpectedResult {
	bool is_mem; /* true = memory result, false = vector reg */
	uint32_t vreg; /* destination vector register (if !is_mem) */
	std::string mem_symbol; /* mem_op data symbol to compare (if is_mem) */
	std::vector<uint8_t> data; /* expected raw data bytes (from selfcheck) */
	std::string symbol; /* generated expected-data symbol name */
};

/* A floating-point register load operation */
struct BmFloatOp {
	uint32_t reg; /* FP register number (f0-f31) */
	uint32_t width; /* data width in bytes: 4=float, 8=double */
	std::vector<uint8_t> data; /* raw data bytes */
	std::string symbol; /* generated data symbol name */
};

/* One complete test iteration */
struct BmIteration {
	int id;
	std::vector<BmVectorOp> vector_ops;
	std::vector<BmScalarOp> scalar_ops;
	std::vector<BmMemOp> mem_ops;
	std::vector<BmFloatOp> float_ops;

	/* Vector config for the target instruction */
	uint32_t lmul;
	uint32_t sew;
	uint32_t vma;
	uint32_t vta;
	uint64_t vl;
	uint64_t vstart;
	uint32_t vxrm;

	/* Target instruction */
	uint32_t target_inst;
	bool is_illegal;

	/* Expected results for verification (empty if illegal) */
	std::vector<BmExpectedResult> expected_results;
};

class BaremetalGenerator {
    private:
	std::vector<BmIteration> iterations_;
	std::string test_name_;
	bool enabled_ = false;

	/* Current iteration being built */
	BmIteration current_;
	bool building_ = false;
	int data_counter_ = 0;

    public:
	void set_enabled(bool en)
	{
		enabled_ = en;
	}
	bool is_enabled() const
	{
		return enabled_;
	}
	void set_test_name(const std::string &name)
	{
		test_name_ = name;
	}

	void begin_iteration(int iter_id)
	{
		if (!enabled_)
			return;
		current_ = BmIteration{};
		current_.id = iter_id;
		current_.is_illegal = false;
		building_ = true;
	}

	void record_vector_load(uint32_t vreg, uint32_t eew_bytes,
				uint32_t lmul, uint32_t vl, const void *data,
				size_t data_size)
	{
		if (!enabled_ || !building_)
			return;

		BmVectorOp op;
		op.vreg = vreg;
		op.eew = eew_bytes;
		op.lmul = lmul;
		op.vl = vl;
		op.data.resize(data_size);
		std::memcpy(op.data.data(), data, data_size);

		std::stringstream ss;
		ss << "bm_data_" << data_counter_++;
		op.symbol = ss.str();

		current_.vector_ops.push_back(std::move(op));
	}

	void record_scalar_load(uint32_t reg, uint64_t value)
	{
		if (!enabled_ || !building_)
			return;
		current_.scalar_ops.push_back({ reg, value });
	}

	void record_mem_operand(uint32_t reg, const void *data, size_t size)
	{
		if (!enabled_ || !building_)
			return;

		BmMemOp op;
		op.reg = reg;
		op.data.resize(size);
		std::memcpy(op.data.data(), data, size);

		std::stringstream ss;
		ss << "bm_data_" << data_counter_++;
		op.symbol = ss.str();

		current_.mem_ops.push_back(std::move(op));
	}

	void record_float_load(uint32_t reg, const void *data, uint32_t width)
	{
		if (!enabled_ || !building_)
			return;

		BmFloatOp op;
		op.reg = reg;
		op.width = width;
		op.data.resize(width);
		std::memcpy(op.data.data(), data, width);

		std::stringstream ss;
		ss << "bm_data_" << data_counter_++;
		op.symbol = ss.str();

		current_.float_ops.push_back(std::move(op));
	}

	void record_vector_config(uint32_t lmul, uint32_t sew, uint64_t vl,
				  uint64_t vstart, uint32_t vxrm, uint32_t vma,
				  uint32_t vta)
	{
		if (!enabled_ || !building_)
			return;
		current_.lmul = lmul;
		current_.sew = sew;
		current_.vl = vl;
		current_.vstart = vstart;
		current_.vxrm = vxrm;
		current_.vma = vma;
		current_.vta = vta;
	}

	void record_target_inst(uint32_t inst, bool is_illegal)
	{
		if (!enabled_ || !building_)
			return;
		current_.target_inst = inst;
		current_.is_illegal = is_illegal;
	}

	void end_iteration()
	{
		if (!enabled_ || !building_)
			return;
		building_ = false;
		iterations_.push_back(std::move(current_));
	}

	/* Append selfcheck expected result (vector register) to the last
	 * completed iteration. Baremetal stores vd via vse8.v then compares. */
	void append_expected_to_last(uint32_t vreg, const void *data,
				     size_t data_size)
	{
		if (!enabled_ || iterations_.empty())
			return;

		BmExpectedResult exp;
		exp.is_mem = false;
		exp.vreg = vreg;
		exp.data.resize(data_size);
		std::memcpy(exp.data.data(), data, data_size);

		std::stringstream ss;
		ss << "bm_expected_" << data_counter_++;
		exp.symbol = ss.str();

		iterations_.back().expected_results.push_back(std::move(exp));
	}

	/* Append selfcheck expected result (memory) to the last completed
	 * iteration. The target instruction wrote to a mem_op buffer —
	 * baremetal just compares that buffer directly.
	 * Uses the first mem_op's symbol as the actual data pointer. */
	void append_mem_expected_to_last(const void *data, size_t data_size)
	{
		if (!enabled_ || iterations_.empty())
			return;
		auto &iter = iterations_.back();
		if (iter.mem_ops.empty())
			return;

		BmExpectedResult exp;
		exp.is_mem = true;
		exp.vreg = 0;
		exp.mem_symbol = iter.mem_ops[0].symbol;
		exp.data.resize(data_size);
		std::memcpy(exp.data.data(), data, data_size);

		std::stringstream ss;
		ss << "bm_expected_" << data_counter_++;
		exp.symbol = ss.str();

		iter.expected_results.push_back(std::move(exp));
	}

	void generate(const std::string &base_path)
	{
		if (!enabled_ || iterations_.empty())
			return;

		generate_main_c(base_path + ".baremetal.c");
		generate_entry_s(base_path + ".baremetal_entry.S");
		generate_linker_script(base_path + ".baremetal.ld");
		generate_makefile(base_path + ".baremetal.mk");

		std::cout << "[Baremetal] Generated " << iterations_.size()
			  << " iterations:" << std::endl;
		std::cout << "  " << base_path << ".baremetal.c" << std::endl;
		std::cout << "  " << base_path << ".baremetal_entry.S"
			  << std::endl;
		std::cout << "  " << base_path << ".baremetal.ld" << std::endl;
		std::cout << "  " << base_path << ".baremetal.mk" << std::endl;
	}

	size_t iteration_count() const
	{
		return iterations_.size();
	}

    private:
	/* ============================================================
	 * Generate main.c: C + inline assembly
	 * ============================================================ */
	void generate_main_c(const std::string &filepath)
	{
		std::ofstream out(filepath);
		if (!out.is_open()) {
			std::cerr << "[Baremetal] Failed to open: " << filepath
				  << std::endl;
			return;
		}

		emit_c_header(out);
		emit_c_data_arrays(out);
		emit_c_iteration_functions(out);
		emit_c_main(out);
		out.close();
	}

	void emit_c_header(std::ofstream &out)
	{
		out << "/*\n";
		out << " * Auto-generated baremetal test: " << test_name_
		    << "\n";
		out << " * Iterations: " << iterations_.size() << "\n";
		out << " * Generated by DAMO-RV-ATS baremetal generator\n";
		out << " *\n";
		out << " * Build: make -f " << test_name_ << ".baremetal.mk\n";
		out << " */\n\n";
		out << "#include <stdint.h>\n\n";

		/* Verification infrastructure */
		out << "/* ============================================================\n";
		out << " * Verification Infrastructure\n";
		out << " * ============================================================ */\n\n";
		out << "extern volatile uint64_t tohost;\n\n";
		out << "static int test_fail_count = 0;\n";
		out << "static int test_fail_iter = -1;\n\n";
		out << "/* Illegal-instruction trap handshake variables.\n";
		out << " * expect_illegal: set by C before .word; checked by trap handler.\n";
		out << " * got_illegal:    set by trap handler; checked by C after .word. */\n";
		out << "volatile int expect_illegal = 0;\n";
		out << "volatile int got_illegal = 0;\n\n";
		out << "static void verify_result(int iter_id, const uint8_t *actual,\n";
		out << "                          const uint8_t *expected, int len)\n";
		out << "{\n";
		out << "    for (int i = 0; i < len; i++) {\n";
		out << "        if (actual[i] != expected[i]) {\n";
		out << "            test_fail_count++;\n";
		out << "            if (test_fail_iter < 0)\n";
		out << "                test_fail_iter = iter_id;\n";
		out << "            return;\n";
		out << "        }\n";
		out << "    }\n";
		out << "}\n\n";
	}

	void emit_c_data_arrays(std::ofstream &out)
	{
		out << "/* ============================================================\n";
		out << " * Test Data (vector register contents)\n";
		out << " * ============================================================ */\n\n";

		for (const auto &iter : iterations_) {
			for (const auto &vop : iter.vector_ops) {
				out << "static uint8_t __attribute__((aligned(16))) "
				    << vop.symbol << "[] = {\n";
				emit_c_byte_array(out, vop.data);
				out << "};\n\n";
			}
			for (const auto &mop : iter.mem_ops) {
				out << "static uint8_t __attribute__((aligned(16))) "
				    << mop.symbol << "[] = {\n";
				emit_c_byte_array(out, mop.data);
				out << "};\n\n";
			}
			for (const auto &fop : iter.float_ops) {
				out << "static uint8_t __attribute__((aligned(16))) "
				    << fop.symbol << "[] = {\n";
				emit_c_byte_array(out, fop.data);
				out << "};\n\n";
			}
		}

		/* Expected result data and actual result buffers */
		bool has_expected = false;
		for (const auto &iter : iterations_) {
			if (!iter.expected_results.empty()) {
				has_expected = true;
				break;
			}
		}
		if (has_expected) {
			out << "/* ============================================================\n";
			out << " * Expected Results & Actual Result Buffers\n";
			out << " * ============================================================ */\n\n";
			for (const auto &iter : iterations_) {
				for (const auto &exp : iter.expected_results) {
					out << "static const uint8_t __attribute__((aligned(16))) "
					    << exp.symbol << "[] = {\n";
					emit_c_byte_array(out, exp.data);
					out << "};\n\n";

					/* Memory results compare the mem_op
					 * buffer directly — no _actual needed */
					if (!exp.is_mem) {
						out << "static uint8_t __attribute__((aligned(16))) "
						    << exp.symbol << "_actual["
						    << exp.data.size()
						    << "];\n\n";
					}
				}
			}
		}
	}

	void emit_c_iteration_functions(std::ofstream &out)
	{
		out << "/* ============================================================\n";
		out << " * Test Iterations\n";
		out << " * ============================================================ */\n\n";

		for (const auto &iter : iterations_) {
			out << "static void test_iter_" << iter.id
			    << "(void)\n";
			out << "{\n";
			emit_c_iter_body(out, iter);
			out << "}\n\n";
		}
	}

	void emit_c_iter_body(std::ofstream &out, const BmIteration &iter)
	{
		/* Step 1: Zero all vector registers */
		out << "    /* Zero all vector registers */\n";
		out << "    asm volatile(\n";
		out << "        \"li t3, 128\\n\"\n";
		out << "        \"vsetvli t3, t3, e8, m1, tu, mu\\n\"\n";
		for (int i = 0; i < 32; i++) {
			out << "        \"vmv.v.i v" << i << ", 0\\n\"\n";
		}
		out << "        ::: \"t3\", \"memory\"\n";
		out << "    );\n\n";

		/* Step 2: Load vector register data */
		if (!iter.vector_ops.empty()) {
			out << "    /* Load vector register data */\n";
			for (const auto &vop : iter.vector_ops) {
				emit_c_vector_load(out, vop);
			}
			out << "\n";
		}

		/* Step 2b: Load floating-point register data */
		if (!iter.float_ops.empty()) {
			out << "    /* Load floating-point register data */\n";
			for (const auto &fop : iter.float_ops) {
				out << "    {\n";
				out << "        void *_fp = (void *)"
				    << fop.symbol << ";\n";
				if (fop.width <= 2) {
					/* FLH (half-precision load) is not in
					 * the base ISA string, so emit it as
					 * .word. Encoding: imm[11:0] | rs1 |
					 * width=001 | rd | 0000111 */
					uint32_t flh_enc =
						(0 << 20) |
						(0 /* placeholder rs1 */ << 15) |
						(1 << 12) | (fop.reg << 7) |
						0b0000111;
					/* rs1 = %0 chosen by GCC; we use
					 * constraint to pass the pointer via a
					 * temp register (t3). Load address
					 * first, then emit .word with t3 as
					 * rs1 (x28). */
					flh_enc = (0 << 20) | (28 << 15) |
						  (1 << 12) | (fop.reg << 7) |
						  0b0000111;
					out << "        asm volatile(\n";
					out << "            \"mv t3, %0\\n\"\n";
					out << "            \".word 0x"
					    << std::hex << std::setfill('0')
					    << std::setw(8) << flh_enc
					    << std::dec << "\\n\"\n";
					out << "            :: \"r\"(_fp) : \"t3\", \"memory\"\n";
					out << "        );\n";
				} else {
					const char *fld_inst =
						(fop.width == 8) ? "fld" :
								   "flw";
					out << "        asm volatile(\n";
					out << "            \"" << fld_inst
					    << " f" << fop.reg
					    << ", 0(%0)\\n\"\n";
					out << "            :: \"r\"(_fp) : \"memory\"\n";
					out << "        );\n";
				}
				out << "    }\n";
			}
			out << "\n";
		}

		/* Step 3: Configure vector unit for target instruction */
		out << "    /* Configure vector unit */\n";
		std::string vtype_asm = get_vtype_asm_string(
			iter.lmul, iter.sew, iter.vta, iter.vma);
		out << "    asm volatile(\n";
		out << "        \"li t3, " << iter.vl << "\\n\"\n";
		out << "        \"vsetvli t3, t3, " << vtype_asm << "\\n\"\n";
		if (iter.vstart > 0) {
			out << "        \"li t3, " << iter.vstart << "\\n\"\n";
			out << "        \"csrw vstart, t3\\n\"\n";
		}
		out << "        \"li t3, " << iter.vxrm << "\\n\"\n";
		out << "        \"csrw vxrm, t3\\n\"\n";
		out << "        ::: \"t3\", \"memory\"\n";
		out << "    );\n\n";

		/* Step 4: Set up illegal-instruction expectation */
		out << "    expect_illegal = " << (iter.is_illegal ? 1 : 0)
		    << ";\n";
		out << "    got_illegal = 0;\n\n";

		/* Step 5: Load scalar regs, mem operand addrs, and execute.
		 *
		 * Scalar operands and mem_op addresses may use any GPR
		 * including callee-saved ones (s0-s11 = x8-x9, x18-x27).
		 *
		 * Strategy:
		 *   1. Load each mem_op address from C into its target GPR
		 *      via separate asm blocks (GCC picks a safe reg for %0,
		 *      s0 is still valid for addressing).
		 *   2. In one combined asm block (no C operands):
		 *      a) Save all callee-saved regs onto stack
		 *      b) Re-load mem_op addresses from the data arrays
		 *         using la (since the arrays are global symbols)
		 *      c) Load scalar immediates (may overwrite s0 etc.)
		 *      d) Execute .word
		 *      e) Restore all callee-saved regs */
		{
			/* Collect all GPRs that will be loaded */
			std::set<uint32_t> loaded_gprs;
			for (const auto &sop : iter.scalar_ops)
				loaded_gprs.insert(sop.reg);
			for (const auto &mop : iter.mem_ops)
				loaded_gprs.insert(mop.reg);

			/* Check if any callee-saved reg is used */
			bool needs_save = false;
			for (uint32_t r : loaded_gprs) {
				if (r == 1 || r == 2 || r == 8 || r == 9 ||
				    (r >= 18 && r <= 27)) {
					needs_save = true;
					break;
				}
			}

			out << "    /* Execute target instruction (0x"
			    << std::hex << std::setfill('0') << std::setw(8)
			    << iter.target_inst << std::dec << ")";
			if (iter.is_illegal)
				out << " [ILLEGAL - expect trap]";
			out << " */\n";

			/* Everything in one asm block with no C operands:
			 * use `la` to load global symbol addresses directly
			 * in asm, so GCC doesn't need to prepare any regs. */
			out << "    asm volatile(\n";

			/* Save callee-saved regs */
			if (needs_save) {
				out << "        \"addi sp, sp, -112\\n\"\n";
				out << "        \"sd ra,  0(sp)\\n\"\n";
				out << "        \"sd s0,  8(sp)\\n\"\n";
				out << "        \"sd s1,  16(sp)\\n\"\n";
				out << "        \"sd s2,  24(sp)\\n\"\n";
				out << "        \"sd s3,  32(sp)\\n\"\n";
				out << "        \"sd s4,  40(sp)\\n\"\n";
				out << "        \"sd s5,  48(sp)\\n\"\n";
				out << "        \"sd s6,  56(sp)\\n\"\n";
				out << "        \"sd s7,  64(sp)\\n\"\n";
				out << "        \"sd s8,  72(sp)\\n\"\n";
				out << "        \"sd s9,  80(sp)\\n\"\n";
				out << "        \"sd s10, 88(sp)\\n\"\n";
				out << "        \"sd s11, 96(sp)\\n\"\n";
			}

			/* Load mem_op addresses using `la` (PIC-safe for
			 * static data in baremetal) */
			for (const auto &mop : iter.mem_ops) {
				out << "        \"la x" << mop.reg << ", "
				    << mop.symbol << "\\n\"\n";
			}

			/* Load scalar immediates */
			for (const auto &sop : iter.scalar_ops) {
				out << "        \"li x" << sop.reg << ", 0x"
				    << std::hex << sop.value << std::dec
				    << "\\n\"\n";
			}

			/* Target instruction */
			out << "        \".word 0x" << std::hex
			    << std::setfill('0') << std::setw(8)
			    << iter.target_inst << std::dec << "\\n\"\n";

			/* Restore callee-saved regs */
			if (needs_save) {
				out << "        \"ld ra,  0(sp)\\n\"\n";
				out << "        \"ld s0,  8(sp)\\n\"\n";
				out << "        \"ld s1,  16(sp)\\n\"\n";
				out << "        \"ld s2,  24(sp)\\n\"\n";
				out << "        \"ld s3,  32(sp)\\n\"\n";
				out << "        \"ld s4,  40(sp)\\n\"\n";
				out << "        \"ld s5,  48(sp)\\n\"\n";
				out << "        \"ld s6,  56(sp)\\n\"\n";
				out << "        \"ld s7,  64(sp)\\n\"\n";
				out << "        \"ld s8,  72(sp)\\n\"\n";
				out << "        \"ld s9,  80(sp)\\n\"\n";
				out << "        \"ld s10, 88(sp)\\n\"\n";
				out << "        \"ld s11, 96(sp)\\n\"\n";
				out << "        \"addi sp, sp, 112\\n\"\n";
			}

			out << "        ::: \"memory\"\n";
			out << "    );\n";
		}

		/* Step 7: Verify illegal/legal matches expectation */
		if (iter.is_illegal) {
			/* Expected illegal: verify trap actually fired */
			out << "    if (!got_illegal) {\n";
			out << "        /* Expected illegal but no trap — FAIL */\n";
			out << "        test_fail_count++;\n";
			out << "        if (test_fail_iter < 0)\n";
			out << "            test_fail_iter = " << iter.id
			    << ";\n";
			out << "    }\n";
		} else {
			/* Expected legal: verify no unexpected trap */
			out << "    if (got_illegal) {\n";
			out << "        /* Unexpected illegal trap — FAIL */\n";
			out << "        test_fail_count++;\n";
			out << "        if (test_fail_iter < 0)\n";
			out << "            test_fail_iter = " << iter.id
			    << ";\n";
			out << "    }\n";
		}

		/* Step 8: Verify data results (only for legal instructions) */
		if (!iter.is_illegal && !iter.expected_results.empty()) {
			out << "\n    /* Store result and verify */\n";
			for (const auto &exp : iter.expected_results) {
				emit_c_verify_result(out, iter, exp);
			}
		}
	}

	void emit_c_vector_load(std::ofstream &out, const BmVectorOp &vop)
	{
		const char *vle_inst = "vle8.v";
		const char *sew_str = "e8";
		switch (vop.eew) {
		case 1:
			vle_inst = "vle8.v";
			sew_str = "e8";
			break;
		case 2:
			vle_inst = "vle16.v";
			sew_str = "e16";
			break;
		case 4:
			vle_inst = "vle32.v";
			sew_str = "e32";
			break;
		case 8:
			vle_inst = "vle64.v";
			sew_str = "e64";
			break;
		}

		std::string lmul_str = get_lmul_string(vop.lmul);

		out << "    {\n";
		out << "        void *_ptr = (void *)" << vop.symbol << ";\n";
		out << "        asm volatile(\n";
		out << "            \"li t3, " << vop.vl << "\\n\"\n";
		out << "            \"vsetvli t3, t3, " << sew_str << ", "
		    << lmul_str << ", tu, mu\\n\"\n";
		out << "            \"" << vle_inst << " v" << vop.vreg
		    << ", (%0)\\n\"\n";
		out << "            :: \"r\"(_ptr) : \"t3\", \"memory\"\n";
		out << "        );\n";
		out << "    }\n";
	}

	void emit_c_scalar_load(std::ofstream &out, const BmScalarOp &sop)
	{
		out << "    asm volatile(\"li x" << sop.reg << ", 0x"
		    << std::hex << sop.value << std::dec
		    << "\\n\" ::: \"memory\");\n";
	}

	void emit_c_verify_result(std::ofstream &out, const BmIteration &iter,
				  const BmExpectedResult &exp)
	{
		size_t total_bytes = exp.data.size();

		if (exp.is_mem) {
			/* Memory result: the store instruction already wrote
			 * to the mem_op buffer. Just compare it directly. */
			out << "    verify_result(" << iter.id << ", "
			    << exp.mem_symbol << ", " << exp.symbol << ", "
			    << total_bytes << ");\n";
		} else {
			/* Vector register result: store vd via vse8.v to a
			 * temporary buffer, then compare. */
			std::string lmul_str = "m1";
			if (total_bytes > 16)
				lmul_str = "m2";
			if (total_bytes > 32)
				lmul_str = "m4";
			if (total_bytes > 64)
				lmul_str = "m8";

			out << "    {\n";
			out << "        void *_act = (void *)" << exp.symbol
			    << "_actual;\n";
			out << "        asm volatile(\n";
			out << "            \"li t3, " << total_bytes
			    << "\\n\"\n";
			out << "            \"vsetvli t3, t3, e8, " << lmul_str
			    << ", tu, mu\\n\"\n";
			out << "            \"vse8.v v" << exp.vreg
			    << ", (%0)\\n\"\n";
			out << "            :: \"r\"(_act) : \"t3\", \"memory\"\n";
			out << "        );\n";
			out << "        verify_result(" << iter.id << ", "
			    << exp.symbol << "_actual, " << exp.symbol << ", "
			    << total_bytes << ");\n";
			out << "    }\n";
		}
	}

	void emit_c_main(std::ofstream &out)
	{
		out << "/* ============================================================\n";
		out << " * Main Entry Point\n";
		out << " * ============================================================ */\n\n";
		out << "/* Return: 0 = all pass, 1 = has failures */\n";
		out << "int baremetal_main(void)\n";
		out << "{\n";
		for (const auto &iter : iterations_) {
			out << "    test_iter_" << iter.id << "();\n";
		}
		out << "\n    /* Signal result via tohost: 1 = SUCCESS, 3 = FAIL */\n";
		out << "    if (test_fail_count > 0) {\n";
		out << "        tohost = 3;  /* FAIL: encode (1 | (1 << 1)) */\n";
		out << "        return 1;\n";
		out << "    }\n";
		out << "    return 0;\n";
		out << "}\n";
	}

	void emit_c_byte_array(std::ofstream &out,
			       const std::vector<uint8_t> &data)
	{
		for (size_t i = 0; i < data.size(); i++) {
			if (i % 16 == 0)
				out << "    ";
			out << "0x" << std::hex << std::setfill('0')
			    << std::setw(2) << static_cast<unsigned>(data[i])
			    << std::dec;
			if (i < data.size() - 1)
				out << ",";
			if (i % 16 == 15 || i == data.size() - 1)
				out << "\n";
			else
				out << " ";
		}
	}

	/* ============================================================
	 * Generate entry.S: startup + trap handler
	 * ============================================================ */
	void generate_entry_s(const std::string &filepath)
	{
		std::ofstream out(filepath);
		if (!out.is_open()) {
			std::cerr << "[Baremetal] Failed to open: " << filepath
				  << std::endl;
			return;
		}

		out << "/*\n";
		out << " * Baremetal entry + trap handler for " << test_name_
		    << "\n";
		out << " * Auto-generated by DAMO-RV-ATS\n";
		out << " *\n";
		out << " * Trap handler: on illegal instruction, skip (mepc += 4)\n";
		out << " */\n\n";

		out << "/* HTIF interface for Sail/Spike */\n";
		out << ".section .htif, \"aw\"\n";
		out << ".global tohost\n";
		out << ".global fromhost\n";
		out << ".balign 64\n";
		out << "tohost:   .dword 0\n";
		out << "fromhost: .dword 0\n\n";

		out << ".section .text.entry\n";
		out << ".global _entry\n";
		out << "_entry:\n";
		out << "    /* Only hart 0 runs */\n";
		out << "    csrr a0, mhartid\n";
		out << "    bnez a0, _spin\n\n";
		out << "    /* Set up stack */\n";
		out << "    la sp, _stack_top\n\n";
		out << "    /* Enable vector & FP: set mstatus.VS=Initial(01), mstatus.FS=Initial(01) */\n";
		out << "    li t0, (1 << 9) | (1 << 13)\n";
		out << "    csrs mstatus, t0\n\n";
		out << "    /* Set up trap handler */\n";
		out << "    la t0, _trap_entry\n";
		out << "    csrw mtvec, t0\n\n";
		out << "    /* Clear BSS */\n";
		out << "    la a0, _bss_start\n";
		out << "    la a1, _bss_end\n";
		out << "_clear_bss:\n";
		out << "    bgeu a0, a1, _bss_done\n";
		out << "    sd zero, 0(a0)\n";
		out << "    addi a0, a0, 8\n";
		out << "    j _clear_bss\n";
		out << "_bss_done:\n\n";
		out << "    /* Jump to test */\n";
		out << "    call baremetal_main\n\n";
		out << "    /* Check return value: a0 = 0 means PASS, non-zero means FAIL */\n";
		out << "    bnez a0, _fail\n\n";
		out << "    /* Test PASS - signal success via tohost (HTIF) */\n";
		out << "_halt:\n";
		out << "    la t0, tohost\n";
		out << "    li t1, 1           /* tohost = 1 signals success */\n";
		out << "    sd t1, 0(t0)\n";
		out << "    li a0, 0\n";
		out << "    li a7, 93\n"; /* exit syscall for spike/pk */
		out << "    ecall\n";
		out << "_halt_loop:\n";
		out << "    wfi\n";
		out << "    j _halt_loop\n\n";

		out << "    /* Test FAIL - signal failure via tohost */\n";
		out << "_fail:\n";
		out << "    la t0, tohost\n";
		out << "    li t1, 3           /* tohost = 3 signals failure (exit code 1) */\n";
		out << "    sd t1, 0(t0)\n";
		out << "    li a0, 1\n";
		out << "    li a7, 93\n";
		out << "    ecall\n";
		out << "    j _halt_loop\n\n";

		out << "_spin:\n";
		out << "    wfi\n";
		out << "    j _spin\n\n";

		/* Trap handler: verify illegal expectation */
		out << "/* ====================================================\n";
		out << " * Trap handler\n";
		out << " *   - Illegal instruction: check expect_illegal, set got_illegal,\n";
		out << " *     skip (mepc += 4)\n";
		out << " *   - Unexpected illegal (expect_illegal==0): set got_illegal\n";
		out << " *     and still skip so C code can detect the mismatch\n";
		out << " *   - Other exceptions: halt\n";
		out << " * ==================================================== */\n";
		out << ".balign 4\n";
		out << "_trap_entry:\n";
		out << "    csrr t0, mcause\n";
		out << "    li t1, 2              /* CAUSE_ILLEGAL_INSTRUCTION */\n";
		out << "    beq t0, t1, _handle_illegal\n";
		out << "    li t1, 11             /* CAUSE_MACHINE_ECALL */\n";
		out << "    beq t0, t1, _handle_ecall\n\n";
		out << "    /* Unexpected trap - halt */\n";
		out << "    j _halt_loop\n\n";
		out << "_handle_ecall:\n";
		out << "    /* ecall from _halt/_fail: skip ecall (mepc += 4) to reach wfi loop */\n";
		out << "    csrr t0, mepc\n";
		out << "    addi t0, t0, 4\n";
		out << "    csrw mepc, t0\n";
		out << "    mret\n\n";
		out << "_handle_illegal:\n";
		out << "    /* Record that an illegal trap fired */\n";
		out << "    la t0, got_illegal\n";
		out << "    li t1, 1\n";
		out << "    sw t1, 0(t0)\n";
		out << "    /* Skip the illegal instruction (mepc += 4) */\n";
		out << "    csrr t0, mepc\n";
		out << "    addi t0, t0, 4\n";
		out << "    csrw mepc, t0\n";
		out << "    mret\n";

		out.close();
	}

	/* ============================================================
	 * Generate linker script
	 * ============================================================ */
	void generate_linker_script(const std::string &filepath)
	{
		std::ofstream out(filepath);
		if (!out.is_open()) {
			std::cerr << "[Baremetal] Failed to open: " << filepath
				  << std::endl;
			return;
		}

		out << "/*\n";
		out << " * Linker script for DAMO-RV-ATS baremetal test\n";
		out << " */\n\n";
		out << "OUTPUT_ARCH(riscv)\n";
		out << "ENTRY(_entry)\n\n";
		out << "SECTIONS\n";
		out << "{\n";
		out << "    . = 0x80000000;\n\n";
		out << "    .text : {\n";
		out << "        *(.text.entry)\n";
		out << "        *(.text .text.*)\n";
		out << "    }\n\n";
		out << "    . = ALIGN(16);\n";
		out << "    .rodata : {\n";
		out << "        *(.rodata .rodata.*)\n";
		out << "    }\n\n";
		out << "    . = ALIGN(16);\n";
		out << "    .data : {\n";
		out << "        *(.data .data.*)\n";
		out << "    }\n\n";
		out << "    . = ALIGN(16);\n";
		out << "    .bss : {\n";
		out << "        _bss_start = .;\n";
		out << "        *(.bss .bss.*)\n";
		out << "        *(COMMON)\n";
		out << "        _bss_end = .;\n";
		out << "    }\n\n";
		out << "    . = ALIGN(64);\n";
		out << "    /* HTIF tohost/fromhost for Sail/Spike */\n";
		out << "    .htif : {\n";
		out << "        *(.htif .htif.*)\n";
		out << "    }\n\n";
		out << "    . = ALIGN(16);\n";
		out << "    . = . + 0x20000;  /* 128KB stack */\n";
		out << "    _stack_top = .;\n";
		out << "}\n";

		out.close();
	}

	/* ============================================================
	 * Generate Makefile
	 * ============================================================ */
	void generate_makefile(const std::string &filepath)
	{
		std::ofstream out(filepath);
		if (!out.is_open()) {
			std::cerr << "[Baremetal] Failed to open: " << filepath
				  << std::endl;
			return;
		}

		std::string base = test_name_;
		/* Strip .cpp suffix if present */
		auto dot_pos = base.rfind(".cpp");
		if (dot_pos != std::string::npos)
			base = base.substr(0, dot_pos);

		out << "# Auto-generated Makefile for baremetal test: "
		    << test_name_ << "\n\n";
		out << "CROSS ?= riscv64-unknown-elf-\n";
		out << "CC    = $(CROSS)gcc\n";
		out << "AS    = $(CROSS)gcc\n";
		out << "LD    = $(CROSS)gcc\n";
		out << "OBJDUMP = $(CROSS)objdump\n\n";
		out << "MARCH  = rv64gcv\n";
		out << "MABI   = lp64d\n\n";
		out << "CFLAGS  = -march=$(MARCH) -mabi=$(MABI) -mcmodel=medany\n";
		out << "CFLAGS += -O0 -g -ffreestanding -nostdlib -fno-common\n";
		out << "ASFLAGS = -march=$(MARCH) -mabi=$(MABI)\n";
		out << "LDFLAGS = -nostartfiles -nostdlib -T " << test_name_
		    << ".baremetal.ld\n\n";
		out << "TARGET = " << base << ".baremetal.elf\n\n";
		out << "OBJS = " << test_name_ << ".baremetal_entry.o "
		    << test_name_ << ".baremetal.o\n\n";
		out << "all: $(TARGET)\n\n";
		out << "$(TARGET): $(OBJS)\n";
		out << "\t$(LD) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS)\n";
		out << "\t$(OBJDUMP) -S $@ > " << base << ".baremetal.asm\n\n";
		out << test_name_ << ".baremetal.o: " << test_name_
		    << ".baremetal.c\n";
		out << "\t$(CC) $(CFLAGS) -c $< -o $@\n\n";
		out << test_name_ << ".baremetal_entry.o: " << test_name_
		    << ".baremetal_entry.S\n";
		out << "\t$(AS) $(ASFLAGS) -c $< -o $@\n\n";
		out << "clean:\n";
		out << "\trm -f $(OBJS) $(TARGET) " << base
		    << ".baremetal.asm\n\n";
		out << ".PHONY: all clean\n";

		out.close();
	}

	/* ============================================================
	 * Helpers
	 * ============================================================ */
	std::string get_lmul_string(uint32_t lmul)
	{
		switch (lmul) {
		case 0:
			return "m1";
		case 1:
			return "m2";
		case 2:
			return "m4";
		case 3:
			return "m8";
		case 5:
			return "mf8";
		case 6:
			return "mf4";
		case 7:
			return "mf2";
		default:
			return "m1";
		}
	}

	std::string get_sew_string(uint32_t sew)
	{
		switch (sew) {
		case 0:
			return "e8";
		case 1:
			return "e16";
		case 2:
			return "e32";
		case 3:
			return "e64";
		default:
			return "e8";
		}
	}

	std::string get_vtype_asm_string(uint32_t lmul, uint32_t sew,
					 uint32_t vta, uint32_t vma)
	{
		std::string result =
			get_sew_string(sew) + ", " + get_lmul_string(lmul);
		result += vta ? ", ta" : ", tu";
		result += vma ? ", ma" : ", mu";
		return result;
	}
};

/* Global baremetal generator instance */
BaremetalGenerator global_baremetal_gen;

#endif /* __BAREMETAL_GEN_H__ */
