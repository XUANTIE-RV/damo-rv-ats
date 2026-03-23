/*
 * Copyright (c) 2026 Alibaba Group.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __v_common__
#define __v_common__

#include "framework.h"

#define OPIVV 0x0
#define OPFVV 0x1
#define OPMVV 0x2
#define OPIVI 0x3
#define OPIVX 0x4
#define OPFVF 0x5
#define OPMVX 0x6
#define OPCFG 0x7

#define opcode_vector 0x57

#define CSR_VSTART 0x008
#define CSR_VXSAT 0X009 //Vector Fixed-Point Saturation Flag
#define CSR_VXRM 0x00A //Vector Fixed-Point Rounding Mode Register
#define CSR_VCSR 0x00F //Vector Control and Status Register
#define CSR_VL 0xC20
#define CSR_VTYPE 0xC21
#define CSR_VLENB 0xC22

uint64_t vlen = 0;

#define lmul_m1 0
#define lmul_m2 1
#define lmul_m4 2
#define lmul_m8 3
#define lmul_mf8 5
#define lmul_mf4 6
#define lmul_mf2 7

// lmul value to name mapping
const char *lmul_str(uint8_t lmul)
{
	switch (lmul) {
	case lmul_m1:
		return "m1";
	case lmul_m2:
		return "m2";
	case lmul_m4:
		return "m4";
	case lmul_m8:
		return "m8";
	case lmul_mf8:
		return "mf8";
	case lmul_mf4:
		return "mf4";
	case lmul_mf2:
		return "mf2";
	default:
		return "unknown";
	}
}

#define sew_e8 0
#define sew_e16 1
#define sew_e32 2
#define sew_e64 3

// sew value to name mapping
const char *sew_str(uint8_t sew)
{
	switch (sew) {
	case sew_e8:
		return "e8";
	case sew_e16:
		return "e16";
	case sew_e32:
		return "e32";
	case sew_e64:
		return "e64";
	default:
		return "unknown";
	}
}

// vxrm value to name mapping
const char *vxrm_str(uint8_t vxrm)
{
	switch (vxrm) {
	case 0:
		return "RNU round to nearest-up";
	case 1:
		return "RNE round to nearest-even";
	case 2:
		return "RDN round down";
	case 3:
		return "ROD round to odd";
	default:
		return "unknown";
	}
}

// vxsat value to name mapping
const char *vxsat_str(uint8_t vxsat)
{
	switch (vxsat) {
	case 0:
		return "Wrapping Mode";
	case 1:
		return "Saturation Mode";
	default:
		return "unknown";
	}
}

// vta value to name mapping
const char *vta_str(uint8_t vta)
{
	switch (vta) {
	case 0:
		return "Tail Element undisturbed";
	case 1:
		return "Tail Element Agnostic";
	default:
		return "unknown";
	}
}

// vma value to name mapping
const char *vma_str(uint8_t vma)
{
	switch (vma) {
	case 0:
		return "Masked Element undisturbed";
	case 1:
		return "Masked Element Agnostic";
	default:
		return "unknown";
	}
}

struct c_vector_cfg {
	uint64_t lmul;
	uint64_t sew;
	uint64_t vma;
	uint64_t vta;
	uint64_t len;
	uint64_t vstart;
	uint64_t vxrm;
	uint64_t vxsat;
	uint32_t inst;
	uint64_t rs1_data;

	std::vector<std::string> class_name;
	std::vector<uint64_t *> class_value_ptr;

	c_vector_cfg()
	{
		class_name.push_back("lmul");
		class_value_ptr.push_back(&lmul);
		class_name.push_back("sew");
		class_value_ptr.push_back(&sew);
		class_name.push_back("len");
		class_value_ptr.push_back(&len);
		class_name.push_back("vma");
		class_value_ptr.push_back(&vma);
		class_name.push_back("vta");
		class_value_ptr.push_back(&vta);
		class_name.push_back("vstart");
		class_value_ptr.push_back(&vstart);
		class_name.push_back("vxrm");
		class_value_ptr.push_back(&vxrm);
		class_name.push_back("vxsat");
		class_value_ptr.push_back(&vxsat);
		class_name.push_back("rs1_data");
		class_value_ptr.push_back(&rs1_data);
	}

	/* Extract vector configuration fields (lmul, sew, vta, vma, vxrm, vxsat,
	 * vl, vstart) from a c_cfg's CSR map (vtype, vcsr, vl, vstart). */
	void set_value(const c_cfg &cfg)
	{
		INFO << "Set c_vector_cfg value from c_cfg...\n";
		for (auto t : class_value_ptr) {
			*t = 0;
		}

		auto vtype_val = cfg.CSR.find("vtype");
		auto vcsr_val = cfg.CSR.find("vcsr");
		auto vl_val = cfg.CSR.find("vl");
		auto vstart_val = cfg.CSR.find("vstart");
		if (vtype_val != cfg.CSR.end()) {
			lmul = get_bits_range(vtype_val->second, 2, 0);
			sew = get_bits_range(vtype_val->second, 5, 3);
			vta = get_bit(vtype_val->second, 6);
			vma = get_bit(vtype_val->second, 7);
		} else {
			throw std::invalid_argument(
				"Error Missing 'vtype' in CSR for vector config");
		}

		if (vcsr_val != cfg.CSR.end()) {
			vxrm = get_bits_range(vcsr_val->second, 2, 1);
			vxsat = get_bit(vcsr_val->second, 0);
		}

		if (vl_val != cfg.CSR.end()) {
			len = vl_val->second;
		} else {
			throw std::invalid_argument(
				"Error Missing 'vl' in CSR for vector config");
		}

		if (vstart_val != cfg.CSR.end()) {
			vstart = vstart_val->second;
		} else {
			vstart = 0;
		}

		inst = cfg.INST;
	}

	/* Pack the current vector configuration fields back into a c_cfg object,
	 * encoding vtype from lmul/sew/vta/vma and vcsr from vxrm. */
	c_cfg get_value()
	{
		INFO << "Set c_cfg value from c_vector_cfg..." << std::endl;
		c_cfg cfg;

		auto vtype = (vta << 6) | (vma << 7) | (sew << 3) | lmul;
		cfg.CSR["vtype"] = vtype;
		cfg.CSR["vl"] = len;
		cfg.CSR["vstart"] = vstart;
		cfg.CSR["vcsr"] = (vxrm << 1);
		cfg.INST = inst;

		return cfg;
	}

	/* Randomly generate a legal vector configuration (SEW, LMUL, VL, vxrm).
	 * Ensures the LMUL/SEW combination satisfies LMUL*VLEN >= SEW constraint,
	 * avoids reserved LMUL=4, and picks VL within the valid element count. */
	void m_rand()
	{
		INFO << "Randomizing Vector Configuratoin..." << std::endl;

		sew = get_rand_bound<int>(0, 3);
		lmul = get_rand_bound<int>(0, 7);

		int total_bits = (lmul < 4) ? (vlen * (1 << lmul)) :
					      (vlen / (1 << (8 - lmul)));
		while (lmul == 4 || total_bits < (1 << sew) ||
		       (lmul > 4 && (8 >> (8 - lmul) < (1 << sew)))) {
			lmul = get_rand_bound<int>(0, 7);
			total_bits = (lmul < 4) ? (vlen * (1 << lmul)) :
						  (vlen / (1 << (8 - lmul)));
		}
		vma = 0;
		vta = 0;
		vxsat = 0;
		vxrm = get_rand_bound<int>(0, 3);

		len = get_rand_bound<int>(1, (total_bits / (1 << sew)));
		// vstart fixed to 0
		// vstart = get_rand_bound<int>(0,len-1);
		vstart = 0;
		for (int i = 0; i < class_value_ptr.size(); i++) {
			DEBUG << "- " << std::hex << class_name[i] << " =0x"
			      << *class_value_ptr[i] << std::endl;
		}
	}

	friend std::ostream &operator<<(std::ostream &os,
					const c_vector_cfg &cfg)
	{
		for (int i = 0; i < cfg.class_value_ptr.size(); i++) {
			os << std::hex << cfg.class_name[i] << ":"
			   << *cfg.class_value_ptr[i] << " ";
		}
		os << std::hex << "inst:" << cfg.inst;
		return os;
	}

	friend std::istream &operator>>(std::istream &is, c_vector_cfg &cfg)
	{
		for (int i = 0; i < cfg.class_value_ptr.size(); i++) {
			is >> *cfg.class_value_ptr[i];
		}
		return is;
	}

	void print()
	{
		INFO << "Print c_vector_cfg... " << std::endl;
		// INFO << "  vlenb = 0x" << std::hex << csr_vlenb << std::dec <<" ("<< csr_vlenb <<") "<< std::endl;
		INFO << "vstart= 0x" << std::hex << vstart << std::dec << " ("
		     << vstart << ") " << std::endl;
		INFO << "vl    = 0x" << std::hex << len << std::dec << " ("
		     << len << ") " << std::endl;
		INFO << "- lmul =" << std::hex << lmul << " " << lmul_str(lmul)
		     << std::endl;
		INFO << "- sew  =" << std::hex << sew << " " << sew_str(sew)
		     << std::endl;
		INFO << "- vta  =" << std::hex << vta << " " << vta_str(vta)
		     << std::endl;
		INFO << "- vma  =" << std::hex << vma << " " << vma_str(vma)
		     << std::endl;
		INFO << "- vxrm =" << std::hex << vxrm << " " << vxrm_str(vxrm)
		     << std::endl;
		INFO << "- vxsat=" << std::hex << vxsat << " "
		     << vxsat_str(vxsat) << std::endl;
	}
};

c_flag vector_flag;
c_vector_cfg vector_cfg;
uint64_t vl_placeholder[10000];
void *addr_placeholder[10000];
int vl_top = 0, addr_top = 0;
std::vector<void *> allocated_buffers;

/* Initialize the vector test environment: read VLENB CSR to get the vector
 * register length in bytes, and set up the global illegal-instruction flag. */
void init_vector_program()
{
	INFO << "init vector ... \n";
	vlen = CSRR(CSR_VLENB);
	if (global_flag_ptr == nullptr)
		global_flag_ptr = new c_flag();
	*global_flag_ptr = vector_flag;
}

/* Initialize the vector configuration for a single test iteration.
 * In random mode: randomize SEW/LMUL/VL, generate a random instruction encoding,
 * and optionally retry to produce an illegal instruction based on illegal_percent.
 * In fixed mode: load configuration from the pre-recorded global_cfg array. */
void init_vector_cfg(int it, c_cfg &cur_cfg, c_data &cur_data,
		     std::function<int(c_data &)> check_illegal,
		     const std::vector<InstField> vop_inst_fields)
{
	if (random_mode) {
		vector_cfg.m_rand();
		vector_cfg.inst = get_inst(vop_inst_fields);
		cur_data.set_value_from_inst(vop_inst_fields, vector_cfg.inst);
		int illegal = global_flag_ptr->illegal;
		int retry_count = 0;
		bool should_generate_illegal = get_rand_bound(0, run_times) <
					       illegal_percent * run_times;
		while (!illegal && illegal_percent >= 0 &&
		       should_generate_illegal && retry_count < max_retry) {
			vector_cfg.inst = get_inst(vop_inst_fields);
			cur_data.set_value_from_inst(vop_inst_fields,
						     vector_cfg.inst);
			illegal = check_illegal(cur_data);
			retry_count++;
		}
		cur_cfg = vector_cfg.get_value();
	} else {
		cur_cfg = global_cfg[it];
		vector_cfg.set_value(cur_cfg);
		cur_data.set_value_from_inst(vop_inst_fields, vector_cfg.inst);
	}

	INFO << "CSR: " << c_cfg::to_json(cur_cfg).at("CSR") << std::endl;
	vector_cfg.print();
}

/* Reset the placeholder buffer indices for VL, immediate values, and addresses. */
void reset_data()
{
	vl_top = 0;
	val_top = 0;
	addr_top = 0;
}

//vsetvli
//0 vtypeei[10:0] rs1 111 rd 1010111

/* Generate a VSETVLI instruction to configure the vector unit with the given
 * LMUL, SEW, VMA, VTA, and requested vector length (VL). Loads VL into x28
 * first, then emits the vsetvli encoding. */
void vsetvli_lmul_sew(std::vector<uint32_t> &insts, int lmul, int sew, int vl,
		      int vma = 0, int vta = 0)
{
	uint32_t vtypei = (vma << 7) + (vta << 6) + (sew << 3) + lmul;
	vl_placeholder[vl_top++] = vl;
	uint64_t ptr = (uint64_t)(&(vl_placeholder[vl_top - 1]));
	load_reg(insts, ptr, 28);
	uint32_t inst = (vtypei << 20) + (28 << 15) + ((0b111) << 12) +
			(28 << 7) + (0b1010111);
	insts.push_back(inst);
}

/* Generate a vector unit-stride load instruction (vle8/16/32/64) to load
 * 'vl' elements from memory address 'addr' into vector register 'vd'.
 * Automatically computes EEW and width encoding from sizeof(T).
 * Sets vsetvli with the given lmul before the load. nf=0, vm=1 (unmasked). */
template <typename T>
void load_vector(std::vector<uint32_t> &insts, uint32_t vd, void *addr,
		 int lmul, int vl)
{
	uint32_t eew = 0;
	for (int tmp = sizeof(T); tmp != 1; tmp = tmp / 2, eew++)
		;
	vsetvli_lmul_sew(insts, lmul, eew, vl);

	uint32_t width = eew;
	if (width != 0) {
		width = 4 + eew;
	}
	addr_placeholder[addr_top++] = addr;
	uint64_t ptr = (uint64_t)(&(addr_placeholder[addr_top - 1]));
	load_reg(insts, ptr, 28);
	uint32_t inst = (1 << 25) + (28 << 15) + (width << 12) + (vd << 7) +
			(0b0000111);
	insts.push_back(inst);
}

/* Load test data for a single vector register from c_data into the actual
 * vector register by generating vle instructions. Skips unmasked vm registers. */
template <typename T>
void load_single_vector(std::vector<uint32_t> &insts, c_data &cur_data,
			const std::string &reg_name, uint64_t lmul, uint64_t vl)
{
	if (cur_data.map_reg_index.find(reg_name) ==
	    cur_data.map_reg_index.end()) {
		DEBUG << "load multi vector:" << reg_name
		      << " not found in data\n";
		return;
	}

	if (reg_name == "vm" && cur_data.map_reg_index["vm"]) {
		DEBUG << "load multi vector: not " << reg_name << ",skip\n";
		return;
	}

	int vd = cur_data.map_reg_index[reg_name];
	cur_data.map_reg_typed_value[reg_name] = c_data::process_from_common<T>(
		cur_data.map_reg_value[reg_name]);
	void *data = cur_data.map_reg_typed_value[reg_name].get();
	load_vector<T>(insts, vd, data, lmul, vl);
}

/* Recursively load multiple vector registers from test data. Each type in the
 * template parameter pack corresponds to one register with its own LMUL and VL. */
template <typename T, typename... Rest>
void load_multi_vector(std::vector<uint32_t> &insts, c_data &cur_data,
		       std::vector<std::string> reg_names,
		       std::vector<uint64_t> lmuls, std::vector<uint64_t> vls)
{
	if (reg_names.size() != lmuls.size() ||
	    reg_names.size() != vls.size()) {
		throw std::runtime_error(
			"load multi vector: reg_names.size() != lmuls.size() || reg_names.size() != vls.size()");
	}

	load_single_vector<T>(insts, cur_data, *reg_names.begin(),
			      *lmuls.begin(), *vls.begin());

	auto reg_names_rest = std::vector<std::string>(reg_names.begin() + 1,
						       reg_names.end());
	auto lmuls_rest = std::vector<uint64_t>(lmuls.begin() + 1, lmuls.end());
	auto vls_rest = std::vector<uint64_t>(vls.begin() + 1, vls.end());

	if constexpr (sizeof...(Rest) > 0) {
		load_multi_vector<Rest...>(insts, cur_data, reg_names_rest,
					   lmuls_rest, vls_rest);
	}
}

/* Generate a vector unit-stride store instruction (vse8/16/32/64) to store
 * 'vl' elements from vector register 'vd' to memory address 'addr'.
 * Automatically computes EEW and width encoding from sizeof(T).
 * Sets vsetvli with the given lmul before the store. */
template <typename T>
void store_vector(std::vector<uint32_t> &insts, uint32_t vd, void *addr,
		  int lmul, int vl)
{
	uint32_t eew = 0;
	for (int tmp = sizeof(T); tmp != 1; tmp = tmp / 2, eew++)
		;
	vsetvli_lmul_sew(insts, lmul, eew, vl);

	uint32_t width = eew;
	if (width != 0) {
		width = 4 + eew;
	}
	addr_placeholder[addr_top++] = addr;
	uint64_t ptr = (uint64_t)(&(addr_placeholder[addr_top - 1]));
	load_reg(insts, ptr, 28);
	uint32_t inst = (1 << 25) + (28 << 15) + (width << 12) + (vd << 7) +
			(0b0100111);
	insts.push_back(inst);
}

/* Save a single vector register's content before instruction execution into
 * a zero-initialized buffer using vse instructions. Skips unmasked vm. */
template <typename T>
void store_single_preinst_vector(std::vector<uint32_t> &insts, c_data &cur_data,
				 const std::string &reg_name, uint64_t lmul,
				 uint64_t vl)
{
	if (cur_data.map_reg_index.find(reg_name) ==
	    cur_data.map_reg_index.end()) {
		DEBUG << "store multi preinst vector:" << reg_name
		      << " not found in data\n";
		return;
	}

	if (reg_name == "vm" && cur_data.map_reg_index["vm"]) {
		DEBUG << "store multi preinst vector: not " << reg_name
		      << ",skip \n";
		return;
	}

	int vd = cur_data.map_reg_index[reg_name];
	cur_data.map_preinst_typed_value[reg_name] = make_zero_buffer<T>(vl);
	void *data = cur_data.map_preinst_typed_value[reg_name].get();
	store_vector<T>(insts, vd, data, lmul, vl);
}

/* Recursively save multiple vector registers' pre-instruction values. */
template <typename T, typename... Rest>
void store_multi_preinst_vector(std::vector<uint32_t> &insts, c_data &cur_data,
				std::vector<std::string> reg_names,
				std::vector<uint64_t> lmuls,
				std::vector<uint64_t> vls)
{
	if (reg_names.size() != lmuls.size() ||
	    reg_names.size() != vls.size()) {
		throw std::runtime_error(
			"store multi preinst vector: reg_names.size() != lmuls.size() || reg_names.size() != vls.size()");
	}

	store_single_preinst_vector<T>(insts, cur_data, *reg_names.begin(),
				       *lmuls.begin(), *vls.begin());

	auto reg_names_rest = std::vector<std::string>(reg_names.begin() + 1,
						       reg_names.end());
	auto lmuls_rest = std::vector<uint64_t>(lmuls.begin() + 1, lmuls.end());
	auto vls_rest = std::vector<uint64_t>(vls.begin() + 1, vls.end());

	if constexpr (sizeof...(Rest) > 0) {
		store_multi_preinst_vector<Rest...>(
			insts, cur_data, reg_names_rest, lmuls_rest, vls_rest);
	}
}

/* Save a single vector register's content after instruction execution into
 * a zero-initialized buffer using vse instructions. Skips unmasked vm. */
template <typename T>
void store_single_afterinst_vector(std::vector<uint32_t> &insts,
				   c_data &cur_data,
				   const std::string &reg_name, uint64_t lmul,
				   uint64_t vl)
{
	if (cur_data.map_reg_index.find(reg_name) ==
	    cur_data.map_reg_index.end()) {
		DEBUG << "store multi afterinst vector:" << reg_name
		      << " not found in data\n";
		return;
	}

	if (reg_name == "vm" && cur_data.map_reg_index["vm"]) {
		DEBUG << "store multi afterinst vector: not " << reg_name
		      << ",skip\n";
		return;
	}

	int vd = cur_data.map_reg_index[reg_name];
	cur_data.map_afterinst_typed_value[reg_name] = make_zero_buffer<T>(vl);
	void *data = cur_data.map_afterinst_typed_value[reg_name].get();
	store_vector<T>(insts, vd, data, lmul, vl);
}

/* Recursively save multiple vector registers' post-instruction values. */
template <typename T, typename... Rest>
void store_multi_afterinst_vector(std::vector<uint32_t> &insts,
				  c_data &cur_data,
				  std::vector<std::string> reg_names,
				  std::vector<uint64_t> lmuls,
				  std::vector<uint64_t> vls)
{
	if (reg_names.size() != lmuls.size() ||
	    reg_names.size() != vls.size()) {
		throw std::runtime_error(
			"store multi afterinst vector: reg_names.size() != lmuls.size() || reg_names.size() != vls.size()");
	}

	store_single_afterinst_vector<T>(insts, cur_data, *reg_names.begin(),
					 *lmuls.begin(), *vls.begin());

	auto reg_names_rest = std::vector<std::string>(reg_names.begin() + 1,
						       reg_names.end());
	auto lmuls_rest = std::vector<uint64_t>(lmuls.begin() + 1, lmuls.end());
	auto vls_rest = std::vector<uint64_t>(vls.begin() + 1, vls.end());

	if constexpr (sizeof...(Rest) > 0) {
		store_multi_afterinst_vector<Rest...>(
			insts, cur_data, reg_names_rest, lmuls_rest, vls_rest);
	}
}

/* Check the post-instruction illegal status. If the instruction was expected
 * to be illegal, verify that SIGILL was raised and reset data. If an unexpected
 * illegal signal occurred, log the error. Returns: 0 = handled illegal,
 * 1 = fatal error (early_stop), -1 = instruction was legal, proceed to check. */
int check_afterinst_illegal()
{
	if (global_flag_ptr->illegal) {
		check_if_sigill();
		reset_data();
		if (global_flag_ptr->check_illegal_error) {
			ERROR << vector_cfg << std::endl;
			if (early_stop)
				return 1;
			global_flag_ptr->check_illegal_error = 0;
		}
		return 0;
	} else if (global_flag_ptr->check_illegal_error) {
		ERROR << vector_cfg << std::endl;
		global_flag_ptr->check_illegal_error = 0;
		if (early_stop)
			return 1;
		return 0;
	}
	return -1;
}

/* Compute the effective EMUL encoding by applying an emul_ratio offset to the
 * base LMUL value. The result wraps modulo 8 to stay within the 3-bit encoding.
 * emul_ratio: 0=1x, 1=2x, -1=0.5x, 2=4x, etc. */
uint64_t lmul_pow(uint64_t lmul_value, int emul_ratio)
{
	uint64_t emul = (lmul_value + emul_ratio) % 8;
	return emul;
}

/* Circular right rotation of x by 'shift' bits within SEW-bit width. */
template <typename T> T ror(T x, T shift)
{
	int SEW = sizeof(T) * 8;
	shift = shift & (SEW - 1);

	return (x >> shift) | (x << ((SEW - shift) % SEW));
}

/* Circular left rotation of x by 'shift' bits within SEW-bit width. */
template <typename T> T rol(T x, T shift)
{
	int SEW = sizeof(T) * 8;
	shift = shift & (SEW - 1);

	return ((x << (shift % SEW))) | (x >> ((SEW - shift)));
}

/* Bit-reverse: reverse the order of all bits within the SEW-bit width of x. */
template <typename T> T brev(T x)
{
	int SEW = sizeof(T) * 8;
	//printf("SEW=%d\n",SEW);
	T result = 0;
	for (int i = 0; i < SEW; ++i) {
		result |= ((x >> i) & 1) << (SEW - 1 - i);
	}
	//printf("x=%x,result=%x\n",x,result);
	return result;
}

/*
 * Vector register should aligned with the passed-in LMUL (EMUL).
 * If LMUL < 0, i.e. fractional LMUL, any vector register is allowed.
 */
int is_align(const int vd, const int lmul)
{
	return (lmul > 4 || (vd % (1 << lmul) == 0));
}

/* Destination vector register group cannot overlap source mask register. */
int is_legal_vm(int vm_bit, int vd)
{
	return (vm_bit != 0 || vd != 0);
}

int is_overlapped(const int8_t astart, int8_t asize, const int8_t bstart,
		  int8_t bsize)
{
	const int8_t aend = astart + asize;
	const int8_t bend = bstart + bsize;

	return std::max(aend, bend) - std::min(astart, bstart) < asize + bsize;
}

int is_legal_nf(int vd, int nf, int lmul)
{
	int size = (lmul < 4) ? nf << lmul : nf;
	return size <= 8 && vd + size <= 32;
}

/*
 * Vector unit-stride, strided, unit-stride segment, strided segment
 * store check function.
 *
 * Rules to be checked here:
 *   1. EMUL must within the range: 1/8 <= EMUL <= 8. (Section 7.3)
 *   2. Destination vector register number is multiples of EMUL.
 *      (Section 3.4.2, 7.3)
 *   3. The EMUL setting must be such that EMUL * NFIELDS ≤ 8. (Section 7.8)
 *   4. Vector register numbers accessed by the segment load or store
 *      cannot increment past 31. (Section 7.8)
 */
int vext_check_store(int vd, int nf, int eew)
{
	int tmp_lmul = (vector_cfg.lmul > 4) ? vector_cfg.lmul - 8 :
					       vector_cfg.lmul;
	int emul = eew - vector_cfg.sew + tmp_lmul;
	if (emul < -3 || emul > 3)
		return 0;
	if (emul < 0)
		emul = emul + 8;
	return is_align(vd, emul) && is_legal_nf(vd, nf, emul);
}

/*
 * Vector unit-stride, strided, unit-stride segment, strided segment
 * load check function.
 *
 * Rules to be checked here:
 *   1. All rules applies to store instructions are applies
 *      to load instructions.
 *   2. Destination vector register group for a masked vector
 *      instruction cannot overlap the source mask register (v0).
 *      (Section 5.3)
 */
int vext_check_load(int vd, int nf, int eew, int vm_bit)
{
	return vext_check_store(vd, nf, eew) && is_legal_vm(vm_bit, vd);
}

struct VregOperand {
	int idx; //(0,31),-1 is disable
	int emul_ratio; //operate to vector_cfg.lmul, 0=1x, 1=2x, -1=0.5x, 2=4x, etc.

	static VregOperand none()
	{
		return { -1, 0 };
	}
	static VregOperand one_pow(int idx)
	{
		return { idx, 0 };
	}
	static VregOperand two_pow(int idx)
	{
		return { idx, 1 };
	}
	static VregOperand half(int idx)
	{
		return { idx, -1 };
	}
	static VregOperand quad(int idx)
	{
		return { idx, 2 };
	}
	static VregOperand quarter(int idx)
	{
		return { idx, -2 };
	}
	static VregOperand eight_pow(int idx)
	{
		return { idx, 3 };
	}
	static VregOperand one_of_eight(int idx)
	{
		return { idx, -3 };
	}
	static VregOperand mask_dest(int idx)
	{
		if (vector_cfg.lmul < 4)
			return { idx, -(int)vector_cfg.lmul };
		return { idx, 8 - (int)vector_cfg.lmul };
	}
	static VregOperand custom(int idx, int emul_ratio)
	{
		return { idx, emul_ratio };
	}
};

enum class ArchConstraint { NONE, WIDEN, NARROW };

struct ValidationConfig {
	bool check_align = false;
	bool check_overlap = false;
	bool check_vm = false;
	bool check_vstart = false;
	bool force_no_overlap = false;

	ArchConstraint arch_constraint = ArchConstraint::NONE;

	std::function<bool(const std::optional<VregOperand> &,
			   const std::vector<VregOperand> &)>
		custom_checker = nullptr;
};

namespace Presets
{
static const ValidationConfig STANDARD = { .check_align = true,
					   .check_overlap = true,
					   .check_vm = true };

static const ValidationConfig WIDEN = { .check_align = true,
					.check_overlap = true,
					.check_vm = true,
					.arch_constraint =
						ArchConstraint::WIDEN };

static const ValidationConfig NARROW = { .check_align = true,
					 .check_overlap = true,
					 .check_vm = true,
					 .arch_constraint =
						 ArchConstraint::NARROW };

static const ValidationConfig GATHER = { .check_align = true,
					 .check_overlap = true,
					 .check_vm = true,
					 .force_no_overlap = true };
}

class VectorRegValidator {
    public:
	static bool validate(const std::optional<VregOperand> &dst,
			     const std::vector<VregOperand> &sources,
			     int vm_bit, ValidationConfig config)
	{
		if (config.check_vstart && vector_cfg.vstart != 0)
			return false;

		if (config.check_vm) {
			if (dst.has_value()) {
				if (!is_legal_vm(vm_bit, dst->idx))
					return false;
			}
		}

		if (!check_arch_constraints(config, dst, sources))
			return false;

		if (!check_vsetvli())
			return false;

		std::vector<VregOperand> all_regs;
		if (dst.has_value() && dst.value().idx != -1)
			all_regs.push_back(dst.value());

		for (const auto &src : sources) {
			if (src.idx != -1)
				all_regs.push_back(src);
		}

		if (config.check_align) {
			for (const auto &reg : all_regs) {
				if (!is_reg_aligned(reg.idx, vector_cfg.lmul,
						    reg.emul_ratio))
					return false;

				int size = get_reg_group_count(vector_cfg.lmul,
							       reg.emul_ratio);
				if (reg.idx + size > 32)
					return false;
				if (size > 8)
					return false;
			}
		}

		if (config.check_overlap && dst.has_value()) {
			const auto &dst_reg = dst.value();
			int dst_size = get_reg_group_count(vector_cfg.lmul,
							   dst_reg.emul_ratio);

			for (const auto &src_reg : sources) {
				if (src_reg.idx == -1)
					continue;

				int src_size = get_reg_group_count(
					vector_cfg.lmul, src_reg.emul_ratio);

				bool is_same_reg = (src_reg.idx == dst_reg.idx);
				if (is_same_reg && !config.force_no_overlap)
					continue;

				if (!check_no_overlap_rule(
					    dst_reg.idx, dst_size,
					    dst_reg.emul_ratio, src_reg.idx,
					    src_size, src_reg.emul_ratio)) {
					return false;
				}
			}
		}

		if (config.custom_checker)
			if (!config.custom_checker(dst, sources))
				return false;

		return true;
	}

	static inline int get_reg_group_count(int lmul_base, int emul_ratio)
	{
		int lmul = (lmul_base > 4) ? lmul_base - 8 : lmul_base;
		int emul = lmul + emul_ratio;
		if (emul < 0)
			return 1;
		return 1 << emul;
	}

	static inline bool is_reg_aligned(int val, int lmul_base,
					  int emul_ratio)
	{
		int lmul = (lmul_base > 4) ? lmul_base - 8 : lmul_base;
		int emul = lmul + emul_ratio;
		if (emul <= -4 || emul >= 4)
			return false;
		if (emul <= 0)
			return true;
		return (val & ((1 << emul) - 1)) == 0;
	}

	static bool is_overlapped(int start_a, int size_a, int start_b,
				  int size_b)
	{
		int end_a = start_a + size_a;
		int end_b = start_b + size_b;
		return std::max(end_a, end_b) - std::min(start_a, start_b) <
		       size_a + size_b;
	}

	static bool check_no_overlap_rule(int dst_idx, int dst_size,
					  int dst_ratio, int src_idx,
					  int src_size, int src_ratio)
	{
		if (dst_size > src_size) {
			int lmul = (vector_cfg.lmul > 4) ? vector_cfg.lmul - 8 :
							   vector_cfg.lmul;
			int src_emul = lmul + src_ratio;
			if (dst_idx < src_idx && src_emul >= 0 &&
			    is_overlapped(dst_idx, dst_size, src_idx,
					  src_size) &&
			    !is_overlapped(dst_idx, dst_size,
					   src_idx + src_size, src_size)) {
				return true;
			}
			return !is_overlapped(dst_idx, dst_size, src_idx,
					      src_size);
		}

		if (dst_size < src_size) {
			if (dst_idx == src_idx) {
				return true;
			}

			if (is_overlapped(dst_idx, dst_size, src_idx,
					  src_size)) {
				return false;
			}
			return true;
		}
		return !is_overlapped(dst_idx, dst_size, src_idx, src_size);
	}

	static bool
	check_arch_constraints(const ValidationConfig &config,
			       const std::optional<VregOperand> &dst,
			       const std::vector<VregOperand> &sources)
	{
		switch (config.arch_constraint) {
		case ArchConstraint::WIDEN: {
			if (vector_cfg.lmul == lmul_m8)
				return false;
			if (vector_cfg.sew >= sew_e64)
				return false;
			if (vector_cfg.sew + 1 > sew_e64)
				return false;
			break;
		}
		case ArchConstraint::NARROW: {
			if (vector_cfg.lmul == lmul_m8)
				return false;
			if (vector_cfg.sew >= sew_e64)
				return false;
			if (vector_cfg.sew + 1 > sew_e64)
				return false;
			break;
		}
		default:
			break;
		}
		return true;
	}

	/* Validate the current vtype configuration is legal:
	 * - LMUL must not be the reserved value 4, SEW must be < 4.
	 * - For fractional LMUL, the effective LMUL*VLEN must be >= SEW,
	 *   and LMUL*XLEN must be >= SEW (to ensure at least one element). */
	static bool check_vsetvli()
	{
		if (vector_cfg.lmul == 4 || vector_cfg.sew >= 4)
			return false;
		if (vector_cfg.lmul > 4 &&
		    (((vlen * 8) >> (8 - vector_cfg.lmul)) <
		     (8 << vector_cfg.sew)))
			return false;
		if (vector_cfg.lmul > 4 &&
		    ((64 >> (8 - vector_cfg.lmul)) < (8 << vector_cfg.sew)))
			return false;
		return true;
	}
};

constexpr int vm_enable = 0;
constexpr int vm_disable = 1;

#endif
