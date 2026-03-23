/*
 * Copyright (c) 2026 Alibaba Group.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __DAMO_ARCH_TEST__
#define __DAMO_ARCH_TEST__

#include <vector>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <numeric>
#include <algorithm>
#include <sstream>
#include <random>
#include <string>
#include <fstream>
#include <filesystem>
#include <typeinfo>
#include <type_traits>
#include <typeindex>
#include <limits>
#include <sched.h>

#include <unistd.h>
#include <signal.h>
#include <sys/mman.h>
#include <ucontext.h>

#include "../thirdparty/json.hpp"
#include "../thirdparty/argparse.hpp"

using json = nlohmann::json;

enum LOGLEVEL {
	LOG_NONE = 0,
	LOG_ERROR,
	LOG_WARN,
	LOG_INFO,
	LOG_DETAIL,
	LOG_VERBOSE,
	LOG_DEBUG
};

LOGLEVEL global_log_level = LOG_ERROR;

const std::unordered_map<std::string, LOGLEVEL> log_level_map = {
	{ "NONE", LOG_NONE },	  { "ERROR", LOG_ERROR },
	{ "WARN", LOG_WARN },	  { "INFO", LOG_INFO },
	{ "DETAIL", LOG_DETAIL }, { "VERBOSE", LOG_VERBOSE },
	{ "DEBUG", LOG_DEBUG }
};

// vm value to name mapping
const char *vm_str(uint8_t vm)
{
	switch (vm) {
	case 0:
		return "Masked, Selected active";
	case 1:
		return "Unmasked, All active";
	default:
		return "unknown";
	}
}

LOGLEVEL get_log_level()
{
	return global_log_level;
}

bool set_log_level(const std::string &level)
{
	if (log_level_map.find(level) != log_level_map.end()) {
		global_log_level = log_level_map.at(level);
		return true;
	}
	return false;
}

#define _LOG_HEADER(level_stream, level_name) \
	level_stream << "[" #level_name "] " << std::flush

#define _ERROR_HEADER()                                               \
	std::cerr << "[ERROR] " << __FILE__ << ":" << __LINE__ << " " \
		  << std::flush

#define IF_LOG_LEVEL_ENABLED(level_value) if (get_log_level() >= (level_value))

#define ERROR IF_LOG_LEVEL_ENABLED(LOG_ERROR) _ERROR_HEADER()
#define WARN IF_LOG_LEVEL_ENABLED(LOG_WARN) _LOG_HEADER(std::cout, WARN)
#define INFO IF_LOG_LEVEL_ENABLED(LOG_INFO) _LOG_HEADER(std::cout, INFO)
#define DETAIL IF_LOG_LEVEL_ENABLED(LOG_DETAIL) _LOG_HEADER(std::cout, DETAIL)
#define VERBOSE \
	IF_LOG_LEVEL_ENABLED(LOG_VERBOSE) _LOG_HEADER(std::cout, VERBOSE)
#define DEBUG                           \
	IF_LOG_LEVEL_ENABLED(LOG_DEBUG) \
	(_LOG_HEADER(std::cout, DEBUG) << "(" << __func__ << ") ")

#define CSR_STR(s) _CSR_STR(s)
#define _CSR_STR(s) #s

#if __riscv_xlen == 64
#define CSRR(csr)                                             \
	({                                                    \
		uint64_t _temp;                               \
		asm volatile("csrr  %0, " CSR_STR(csr) "\n\r" \
			     : "=r"(_temp)::"memory");        \
		_temp;                                        \
	})
#else
#define CSRR(csr)                                             \
	({                                                    \
		uint32_t _temp;                               \
		asm volatile("csrr  %0, " CSR_STR(csr) "\n\r" \
			     : "=r"(_temp)::"memory");        \
		_temp;                                        \
	})
#endif

#define CSRW(csr, rs) \
	asm volatile("csrw  " CSR_STR(csr) ", %0\n\r" ::"rK"(rs) : "memory")
#define CSRS(csr, rs) \
	asm volatile("csrs  " CSR_STR(csr) ", %0\n\r" ::"rK"(rs) : "memory")
#define CSRC(csr, rs) \
	asm volatile("csrc  " CSR_STR(csr) ", %0\n\r" ::"rK"(rs) : "memory")

#define get_bit(x, num) ((x & (1ull << (num))) >> (num))

#define mask_bits(num) ((~(1ULL << (num + 1))) + (1ULL << (num + 2)))

#define get_bits_range(x, i, j) (((x) >> (j)) & (mask_bits((i) - (j))))

inline bool starts_with(const std::string &str, const std::string &prefix)
{
	if (prefix.size() > str.size())
		return false;
	return str.compare(0, prefix.size(), prefix) == 0;
}

struct c_cfg;
uint64_t illegal_counter;

namespace nlohmann
{
template <> struct adl_serializer<c_cfg> {
	static void from_json(const json &j, c_cfg &cfg);
	static void to_json(json &j, const c_cfg &cfg);
};
}

struct c_cfg {
	std::map<std::string, uint64_t> CSR;
	std::map<std::string, std::vector<uint64_t> > DATA;
	std::uint32_t INST;
	std::string DESC;

	/* Deserialize a c_cfg instance from a JSON object.
	 * Parses CSR map, DATA map, INST encoding, and DESC fields. */
	static c_cfg from_json(const json &j)
	{
		c_cfg cfg;

		if (j.contains("CSR") && j["CSR"].is_object()) {
			for (auto &[key, value] : j["CSR"].items()) {
				const std::string &hex_str =
					value.get<std::string>();
				cfg.CSR[key] = parse_value(hex_str);
			}
		}

		if (j.contains("DATA") && j["DATA"].is_object()) {
			for (auto &[key, value] : j["DATA"].items()) {
				std::vector<uint64_t> elements;
				for (auto &element : value) {
					std::string hex_str =
						element.get<std::string>();
					elements.push_back(
						parse_value(hex_str));
				}
				cfg.DATA[key] = std::move(elements);
			}
		}

		if (j.contains("INST")) {
			cfg.INST = parse_value(j["INST"].get<std::string>());
		}

		if (j.contains("DESC")) {
			cfg.DESC = j["DESC"].get<std::string>();
		}

		return cfg;
	}

	/* Serialize a c_cfg instance into a JSON object.
	 * Converts CSR/DATA values to hex strings. */
	static json to_json(const c_cfg &cfg)
	{
		json j;

		json csr_obj;
		for (const auto &[key, value] : cfg.CSR) {
			csr_obj[key] = fmt_hex(value);
		}
		j["CSR"] = csr_obj;

		json data_obj;
		for (const auto &[key, value] : cfg.DATA) {
			json arr = json::array();
			for (uint64_t val : value) {
				arr.push_back(fmt_hex(val));
			}
			data_obj[key] = arr;
		}
		j["DATA"] = data_obj;

		j["INST"] = fmt_hex(cfg.INST);

		j["DESC"] = cfg.DESC;

		return j;
	}

	/* Parse a numeric string supporting binary (0b), hex (0x), and decimal formats. */
	static uint64_t parse_value(const std::string &str)
	{
		if (starts_with(str, "0b") || starts_with(str, "0B")) {
			return std::stoull(str.substr(2), nullptr, 2);
		} else if (starts_with(str, "0x") || starts_with(str, "0X")) {
			return std::stoull(str, nullptr, 16);
		} else {
			return std::stoull(str, nullptr, 10);
		}
	}

	/* Format a uint64_t value as a "0x" prefixed hex string. */
	static std::string fmt_hex(uint64_t value)
	{
		std::stringstream ss;
		ss << "0x" << std::hex << value;
		return ss.str();
	}

	/* Load a vector of c_cfg instances from a JSON file. */
	static std::vector<c_cfg> from_json_file(const std::string &filename)
	{
		INFO << "get c_cfg from json file... \n";
		std::ifstream file(filename);
		if (!file.is_open()) {
			ERROR << "Failed to read JSON file: " << filename
			      << std::endl;
			exit(EXIT_FAILURE);
		}

		try {
			json j;
			file >> j;
			return j.get<std::vector<c_cfg> >();
		} catch (const std::exception &e) {
			ERROR << "Failed to parse JSON file: "
			      << std::string(e.what()) << std::endl;
			exit(EXIT_FAILURE);
		}
	}

	/* Save a vector of c_cfg instances to a JSON file with pretty-print. */
	static void to_json_file(const std::string &filename,
				 const std::vector<c_cfg> &configs)
	{
		INFO << "Saving c_cfg to JSON file...\n";
		json j = configs;

		std::ofstream file(filename);

		if (!file.is_open()) {
			ERROR << "Failed to open JSON file: " << filename
			      << std::endl;
			exit(EXIT_FAILURE);
		}

		file << j.dump(2) << std::endl;
		if (file.fail()) {
			ERROR << "Failed to write to JSON file: " << filename
			      << std::endl;
			exit(EXIT_FAILURE);
		}
	}
};

namespace nlohmann
{
inline void adl_serializer<c_cfg>::from_json(const json &j, c_cfg &cfg)
{
	cfg = c_cfg::from_json(j);
}

inline void adl_serializer<c_cfg>::to_json(json &j, const c_cfg &cfg)
{
	j = c_cfg::to_json(cfg);
}
}

enum class RegClass { NotReg, Vector, Float, Int, Csr };

/* Classify a register name into its register class (Vector/Float/Int/Csr)
 * based on the name prefix: 'v' for Vector, 'f' for Float, 'x'/'rs'/'rd' for Int. */
RegClass classify_register(const std::string &name)
{
	if (starts_with(name, "v")) {
		return RegClass::Vector;
	}

	if (starts_with(name, "f")) {
		return RegClass::Float;
	}

	if (starts_with(name, "x") || starts_with(name, "rs") ||
	    starts_with(name, "rd")) {
		return RegClass::Int;
	}

	return RegClass::NotReg;
}

struct InstField {
	int front_pos;
	int rear_pos;
	uint64_t default_val;
	bool is_fix;
	RegClass reg_class;
	std::string DESC;

	constexpr int width() const
	{
		return front_pos - rear_pos + 1;
	}

	InstField(int fp, int rp, uint64_t dv, bool fix, RegClass rc,
		  const std::string &desc)
		: front_pos(fp)
		, rear_pos(rp)
		, default_val(dv)
		, is_fix(fix)
		, reg_class(rc)
		, DESC(desc)
	{
	}
};

std::vector<c_cfg> global_cfg;

struct c_flag {
	uint32_t illegal;
	uint32_t check_illegal_error;
};
c_flag *global_flag_ptr = nullptr;

int global_signal_cnt = 0;

/* SIGILL signal handler: captures illegal instruction exceptions,
 * logs the faulting PC and instruction encoding, and advances PC by 4
 * to skip the illegal instruction and continue execution. */
void signal_handler(int signal_number, siginfo_t *si, void *uc)
{
	global_signal_cnt = 1;

	uint64_t pc = ((ucontext_t *)uc)->uc_mcontext.__gregs[0];
	uint32_t inst = *(uint32_t *)pc;
	INFO << "Illegal instruction detected at PC 0x" << std::hex << pc
	     << ", encoding 0x" << inst << std::endl;

	if (global_flag_ptr && !global_flag_ptr->illegal) {
		ERROR << "Unexpected illegal instruction signal\n";
		global_flag_ptr->check_illegal_error = 1;
	}
	((ucontext_t *)uc)->uc_mcontext.__gregs[0] += 4;
}

/* Register the SIGILL signal handler for illegal instruction detection. */
void set_signal_handler()
{
	struct sigaction sa;

	sa.sa_sigaction = signal_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;

	if (sigaction(SIGILL, &sa, NULL) == -1) {
		throw std::runtime_error("Failed to set signal handler");
	}
}

/* Verify that an expected SIGILL signal was raised for an illegal instruction.
 * If the instruction was marked illegal but no signal was received, flag an error. */
void check_if_sigill()
{
	INFO << "Checking for expected SIGILL signal...\n";
	if (global_flag_ptr && global_flag_ptr->illegal) {
		if (global_signal_cnt == 0) {
			ERROR << "Expected SIGILL signal was not raised\n";
			global_flag_ptr->check_illegal_error = 1;
		}
		global_flag_ptr->illegal = 0;
	}
	global_signal_cnt = 0;
}

bool random_mode = true;
int run_times = 1000;
unsigned long seed = 0;
bool record_mode = false;
bool early_stop = false;
float illegal_percent = -1;
int max_retry = 10;

argparse::ArgumentParser parser("damo-rv-ats");
std::mt19937_64 global_rng;
std::fstream dataFile;
std::ofstream outFile;
std::string filename;
std::string datafile;
std::string outfile;

/* Initialize the test program: parse command-line arguments (log level, runtime,
 * seed, record mode, illegal percent, etc.), set up the random engine, load
 * data file if in fixed mode, and register the SIGILL signal handler. */
void init_program(int argc, char *argv[])
{
	std::cout << "Starting testcase: " << filename << std::endl;
	INFO << "Initializing program...\n";

	parser.add_argument("--log-level", "-l")
		.default_value(std::string("ERROR"))
		.help("Set log level: NONE, ERROR, WARN, INFO, DETAIL, VERBOSE, DEBUG")
		.action([](const std::string &value) {
			std::string upper_value = value;
			std::transform(upper_value.begin(), upper_value.end(),
				       upper_value.begin(), ::toupper);
			if (!set_log_level(upper_value)) {
				std::cerr << "Invalid log level: " << value
					  << "\n";
				std::exit(1);
			}
			return value;
		});

	parser.add_argument("--runtime", "-r")
		.help("Number of iterations")
		.default_value(1000)
		.scan<'i', int>();
	parser.add_argument("--early-stop", "-e")
		.help("Stop Early on first error. Default is print ERROR without stopping")
		.default_value(false)
		.implicit_value(true);
	parser.add_argument("--seed", "-s")
		.help("Fix random seed for reproducibility")
		.default_value(0UL)
		.scan<'u', unsigned long>();
	parser.add_argument("--record")
		.help("Record execution trace into data file")
		.default_value(false)
		.implicit_value(true);
	parser.add_argument("--illegal-percent")
		.help("Random Percentage of illegal instructions,range[0,1]")
		.default_value(-1.0f)
		.scan<'f', float>();
	parser.add_argument("--max-retry")
		.help("Maximum number of retries for illegal instructions")
		.default_value(10)
		.scan<'i', int>();
	parser.add_argument("--data")
		.help("Switch to Fixed mode (default is Fuzzing mode). Read instruction form data file")
		.default_value(false)
		.implicit_value(true);

	try {
		parser.parse_args(argc, argv);
	} catch (const std::exception &err) {
		ERROR << "Failed to parse arguments: " << err.what()
		      << std::endl;
		exit(EXIT_FAILURE);
	}

	run_times = parser.get<int>("--runtime");
	seed = parser.get<unsigned long>("--seed");
	record_mode = parser.get<bool>("--record");
	early_stop = parser.get<bool>("--early-stop");
	illegal_percent = parser.get<float>("--illegal-percent");
	max_retry = parser.get<int>("--max-retry");
	random_mode = !parser.get<bool>("--data");

	INFO << "Random mode: " << random_mode << std::endl;
	INFO << "Number of iterations: " << run_times << std::endl;
	INFO << "Record mode: " << record_mode << std::endl;
	INFO << "Early stop: " << early_stop << std::endl;

	seed = seed ? seed : std::random_device{}();
	INFO << "Random seed: " << seed << std::endl;

	global_rng = std::mt19937_64(seed);

	if (!random_mode) {
		global_cfg = c_cfg::from_json_file(datafile);
	}

	set_signal_handler();
}

/* Generate a uniformly distributed random value of the specified integral type T. */
template <typename T> T get_rand()
{
	if constexpr (std::is_same_v<T, int8_t> || std::is_same_v<T, uint8_t> ||
		      std::is_same_v<T, int16_t> ||
		      std::is_same_v<T, uint16_t> ||
		      std::is_same_v<T, int32_t> ||
		      std::is_same_v<T, uint32_t> ||
		      std::is_same_v<T, int64_t> ||
		      std::is_same_v<T, uint64_t>) {
		std::uniform_int_distribution<T> dist;
		return dist(global_rng);
	} else {
		throw std::runtime_error("Unsupported type in get_rand<T>");
	}
}

/* Generate a random value within [minm, maxm] with boundary bias:
 * 10% chance to return minm, 10% chance to return maxm, 80% uniform random.
 * This helps stress-test edge cases at boundary values. */
template <typename T>
T get_rand_bound(T minm = std::numeric_limits<T>::min(),
		 T maxm = std::numeric_limits<T>::max())
{
	if (minm > maxm) {
		std::swap(minm, maxm);
	}

	std::uniform_int_distribution<int> choice(0, 9);

	int behavior = choice(global_rng);

	switch (behavior) {
	case 0:
		return minm;
	case 1:
		return maxm;
	default: {
		if constexpr (std::is_integral_v<T>) {
			std::uniform_int_distribution<T> dist(minm, maxm);
			return dist(global_rng);
		} else if constexpr (std::is_floating_point_v<T>) {
			std::uniform_real_distribution<T> dist(minm, maxm);
			return dist(global_rng);
		} else {
			return static_cast<T>(
				(static_cast<uint64_t>(global_rng()) %
				 (static_cast<uint64_t>(maxm - minm + 1))) +
				minm);
		}
	}
	}
}

/* Randomly generate a 32-bit RISC-V instruction encoding from the given field
 * definitions. Fixed fields use their default values; register and other fields
 * are randomized. Handles 16-bit compressed instruction padding (C.NOP). */
uint32_t get_inst(const std::vector<InstField> &fields)
{
	uint32_t inst = 0;
	bool is_16bit_inst = true;
	INFO << "Randomizing Vector Instruction Fileds..." << std::endl;

	for (const auto &f : fields) {
		uint64_t val = 0;

		if (f.is_fix) {
			val = f.default_val;
		} else if (f.reg_class == RegClass::Vector ||
			   f.reg_class == RegClass::Float ||
			   f.reg_class == RegClass::Csr) {
			uint64_t bits = f.width();
			val = get_rand_bound<uint64_t>() % ((1ull << bits));
		} else if (f.reg_class == RegClass::Int) {
			val = get_rand_bound<uint64_t>(
				8,
				27); //x0-x4 is reserved for special registers
		} else {
			uint64_t bits = f.width();
			val = get_rand_bound<uint64_t>() % ((1ull << bits));
		}
		inst |= static_cast<uint32_t>(val << f.rear_pos);

		if (f.front_pos > 15) {
			is_16bit_inst = false;
		}

		if (f.DESC == "funct6" || f.DESC == "funct3" ||
		    f.DESC == "opcode") {
			// funct6 , funct3 , opcode though generate randomly, but never used these value
		} else if (f.DESC == "vm") {
			INFO << "- " << f.DESC << " = 0x" << std::hex << val
			     << std::dec << " (" << val << ") " << vm_str(val)
			     << std::endl;
		} else {
			INFO << "- " << f.DESC << " = 0x" << std::hex << val
			     << std::dec << " (" << val << ") " << std::endl;
		}
	}

	if (is_16bit_inst && inst != 0) {
		INFO << "inst is 16bit inst,generate c.nop" << std::endl;
		uint32_t c_nop_high = 0x0001u << 16;
		inst = c_nop_high | inst;
	} else if (is_16bit_inst && inst == 0) {
		INFO << "Emitting pure C.NOP (0x0001)" << std::endl;
		inst = 0x0001U | (0x0001U << 16);
	}

	INFO << "- Instruction : 0x" << std::hex << inst << std::endl;
	return inst;
}

/* Finalize the test program: print summary statistics (illegal count, iterations),
 * save recorded data to JSON file if in record mode, and close data file. */
void end_program(int it)
{
	std::cout << "Finalizing program (expect illegal: " << illegal_counter
		  << ", iterations: " << it << ") ..." << std::endl;
	if (random_mode && record_mode) {
		c_cfg::to_json_file(datafile, global_cfg);
	}
	dataFile.close();
}

using SafeVoidPtr = std::shared_ptr<void>;
/* Create an aligned memory buffer from a vector, with at least 16-byte alignment.
 * Returns a shared_ptr with a custom deleter for safe automatic deallocation. */
template <typename T> SafeVoidPtr make_aligned_array(const std::vector<T> &data)
{
	if (data.empty())
		return nullptr;

	size_t bytes = data.size() * sizeof(T);
	size_t align = (alignof(T) < 16) ? 16 : alignof(T);

	void *raw = std::aligned_alloc(align, bytes);
	if (!raw)
		throw std::bad_alloc();
	std::memcpy(raw, data.data(), bytes);

	return SafeVoidPtr(raw, [](void *p) { std::free(p); });
}
/* Allocate a zero-initialized aligned buffer for 'count' elements of type T.
 * Returns a shared_ptr with a custom deleter for safe automatic deallocation. */
template <typename T> SafeVoidPtr make_zero_buffer(size_t count)
{
	if (count == 0)
		return nullptr;
	void *raw = std::aligned_alloc(alignof(T), sizeof(T) * count);
	if (!raw)
		throw std::bad_alloc();
	std::memset(raw, 0, sizeof(T) * count);
	return SafeVoidPtr(raw, [](void *p) { std::free(p); });
}

struct c_data {
	std::map<std::string, uint64_t> map_reg_index;

	std::map<std::string, std::vector<uint64_t> > map_reg_value;
	std::map<std::string, SafeVoidPtr> map_reg_typed_value;

	std::map<std::string, std::vector<uint64_t> > map_preinst_reg_value;
	std::map<std::string, SafeVoidPtr> map_preinst_typed_value;

	std::map<std::string, std::vector<uint64_t> > map_afterinst_reg_value;
	std::map<std::string, SafeVoidPtr> map_afterinst_typed_value;

	std::map<std::string, std::vector<uint64_t> > map_selfcheck_reg_value;
	std::map<std::string, SafeVoidPtr> map_selfcheck_typed_value;

	void set_value_from_cfg(const c_cfg &cfg)
	{
		INFO << "Loading c_data values from c_cfg...\n";
		for (auto &[key, value] : cfg.DATA) {
			map_reg_value[key] = value;
		}
	}

	void set_value_to_cfg(c_cfg &cfg)
	{
		INFO << "Saving c_data values to c_cfg...\n";
		for (auto &[key, value] : map_reg_value) {
			cfg.DATA[key] = value;
		}
	}

	/* Extract each instruction field value from the 32-bit encoding
	 * and store them in the register index map (map_reg_index). */
	void set_value_from_inst(std::vector<InstField> inst_field,
				 uint32_t inst)
	{
		for (auto f : inst_field) {
			map_reg_index[f.DESC] =
				get_bits_range(inst, f.front_pos, f.rear_pos);
		}
	}

	/* Build a descriptive string of all instruction fields and their values
	 * extracted from the given 32-bit instruction encoding. */
	std::string get_DESC_from_inst(std::vector<InstField> inst_field,
				       uint32_t inst)
	{
		std::stringstream ss;
		for (auto f : inst_field) {
			ss << f.DESC << "="
			   << get_bits_range(inst, f.front_pos, f.rear_pos)
			   << ",";
		}
		return ss.str();
	}

	/* Generate 'count' random values of type T and store them as the
	 * test data for the specified register in map_reg_value. */
	template <typename T>
	void register_type_with_random(const std::string &reg_name,
				       size_t count)
	{
		INFO << "Randomizing " << reg_name << " data with " << count
		     << " elements..." << std::endl;
		std::vector<uint64_t> raw_values;
		raw_values.reserve(count);

		for (size_t i = 0; i < count; ++i) {
			T val = get_rand<T>();
			raw_values.push_back(static_cast<uint64_t>(val));
		}

		map_reg_value[reg_name] = std::move(raw_values);
	}

	/* Generate 'count' random values of type T within [min_val, max_val]
	 * and store them as the test data for the specified register. */
	template <typename T>
	void register_type_with_bounded_random(const std::string &reg_name,
					       T min_val, T max_val,
					       size_t count)
	{
		INFO << "Randomizing " << reg_name
		     << " data with bounded range [" << min_val << ", "
		     << max_val << "], " << count << " elements" << std::endl;
		std::vector<uint64_t> raw_values;
		raw_values.reserve(count);

		for (size_t i = 0; i < count; ++i) {
			T val = get_rand_bound<T>(min_val, max_val);
			raw_values.push_back(static_cast<uint64_t>(val));
		}

		map_reg_value[reg_name] = std::move(raw_values);
	}

	/* Convert a typed memory buffer (SafeVoidPtr) into a common uint64_t vector
	 * representation for uniform storage and comparison. */
	template <typename T>
	static std::vector<uint64_t> process_to_common(const SafeVoidPtr &ptr,
						       size_t len)
	{
		if (!ptr || len == 0)
			return {};
		const T *p = static_cast<const T *>(ptr.get());
		std::vector<uint64_t> result;
		result.reserve(len);
		for (size_t i = 0; i < len; ++i) {
			result.push_back(static_cast<uint64_t>(p[i]));
		}
		return result;
	}

	/* Convert a common uint64_t vector back into a typed, aligned memory buffer
	 * (SafeVoidPtr) for use in instruction execution. */
	template <typename T>
	static SafeVoidPtr
	process_from_common(const std::vector<uint64_t> &data)
	{
		if (data.empty())
			return nullptr;

		size_t bytes = data.size() * sizeof(T);
		size_t align = (alignof(T) < 16) ? 16 : alignof(T);

		void *raw = std::aligned_alloc(align, bytes);
		if (!raw)
			throw std::bad_alloc();
		for (size_t i = 0; i < data.size(); ++i) {
			static_cast<T *>(raw)[i] = static_cast<T>(data[i]);
		}

		return SafeVoidPtr(raw, [](void *p) { std::free(p); });
	}
};

void to_json(json &j, const c_data &data)
{
	j = { { "map_reg_index", data.map_reg_index },
	      { "map_reg_value", data.map_reg_value },
	      { "map_preinst_reg_value", data.map_preinst_reg_value },
	      { "map_afterinst_reg_value", data.map_afterinst_reg_value },
	      { "map_selfcheck_reg_value", data.map_selfcheck_reg_value } };
}

/* Save a single register's pre-instruction typed value into the common
 * uint64_t vector format. Skips if the register is not found or if
 * the mask register (vm) is in unmasked mode. */
template <typename T>
void save_single_preinst_value_to_common(c_data &cur_data,
					 std::string &reg_name, uint64_t len)
{
	if (cur_data.map_reg_index.find(reg_name) ==
	    cur_data.map_reg_index.end()) {
		INFO << "Pre-instruction value not found for register: "
		     << reg_name << ", skipping\n";
		return;
	}

	if (reg_name == "vm" && cur_data.map_reg_index["vm"]) {
		INFO << "Mask register " << reg_name
		     << " is unmasked, skipping pre-instruction save\n";
		return;
	}

	cur_data.map_preinst_reg_value[reg_name] = c_data::process_to_common<T>(
		cur_data.map_preinst_typed_value[reg_name], len);
}

/* Recursively save multiple registers' pre-instruction values to common format.
 * Each type in the template parameter pack corresponds to one register. */
template <typename T, typename... Rest>
void save_multi_preinst_value_to_common(c_data &cur_data,
					std::vector<std::string> reg_names,
					std::vector<uint64_t> lens)
{
	if (reg_names.size() != lens.size()) {
		throw std::runtime_error(
			"Mismatched sizes: reg_names and lens must have the same length");
	}

	save_single_preinst_value_to_common<T>(cur_data, *reg_names.begin(),
					       *lens.begin());

	auto reg_names_rest = std::vector<std::string>(reg_names.begin() + 1,
						       reg_names.end());
	auto lens_rest = std::vector<uint64_t>(lens.begin() + 1, lens.end());

	if constexpr (sizeof...(Rest) > 0) {
		save_multi_preinst_value_to_common<Rest...>(
			cur_data, reg_names_rest, lens_rest);
	}
}

/* Sign-extend a value of the given bit width to a full 64-bit integer.
 * Uses arithmetic shift to propagate the sign bit. */
inline int64_t sign_extend(int64_t val, int width)
{
	int64_t shift = 64 - width;
	return (val << shift) >> shift;
}

/* Increment the global illegal instruction counter and log the legality status. */
void inline print_illegal_status(int illegal)
{
	illegal_counter += illegal;
	INFO << "Checking illegal instruction... "
	     << (illegal ? "illegal" : "legal") << std::endl;
}

void inline print_runtime_iteration(int it)
{
	INFO << "----------------"
	     << " Begin of per instruction test "
	     << "----------------" << std::endl;
	INFO << "Current run times: " << std::dec << it << std::endl;
}

void inline print_runtime_iteration_end(void)
{
	INFO << "----------------"
	     << "  End of per instruction test  "
	     << "----------------" << std::endl;
}

/* Compare a single register's post-instruction value against the golden model
 * (selfcheck) result element by element. On mismatch, logs pre-instruction
 * values and the differing elements for debugging. Returns 1 on error. */
template <typename T>
int check_single_error(c_data &cur_data, std::string reg_name, uint64_t len)
{
	if (cur_data.map_reg_index.find(reg_name) ==
	    cur_data.map_reg_index.end()) {
		INFO << "check multi error "
		     << " : " << reg_name << " not found in data\n";
		return 1;
	}

	cur_data.map_afterinst_reg_value[reg_name] =
		c_data::process_to_common<T>(
			cur_data.map_afterinst_typed_value[reg_name], len);

	cur_data.map_selfcheck_reg_value[reg_name] =
		c_data::process_to_common<T>(
			cur_data.map_selfcheck_typed_value[reg_name], len);

	int is_error = 0;
	auto afterinst_value = cur_data.map_afterinst_reg_value[reg_name];
	auto selfcheck_value = cur_data.map_selfcheck_reg_value[reg_name];

	for (int i = 0; i < afterinst_value.size(); i++) {
		is_error |= afterinst_value[i] != selfcheck_value[i];

		if (is_error) {
			for (auto &[preinst_key, preinst_value] :
			     cur_data.map_preinst_reg_value) {
				auto value = preinst_value[i];
				if (preinst_key == "vm") {
					int row = i / 8;
					int col = i % 8;
					value = (preinst_value[row] &
						 (1ULL << col));
				}
				ERROR << preinst_key << " preinst value "
				      << std::hex << value << std::dec
				      << std::endl;
			}
			ERROR << reg_name << " inst value " << std::hex
			      << afterinst_value[i]
			      << " is not equal to selfcheck value "
			      << selfcheck_value[i] << std::dec << std::endl;
		}
	}

	return is_error;
}

/* Recursively check multiple registers for errors against their golden model
 * results. Returns on the first register that has a mismatch. */
template <typename T, typename... Rest>
int check_multi_error(c_data &cur_data, std::vector<std::string> reg_names,
		      std::vector<uint64_t> lens)
{
	if (reg_names.size() != lens.size()) {
		throw std::runtime_error(
			"check multi error: reg_names.size() != lens.size()");
	}

	int is_error = check_single_error<T>(cur_data, reg_names[0], lens[0]);
	if (is_error)
		return is_error;
	auto reg_names_rest = std::vector<std::string>(reg_names.begin() + 1,
						       reg_names.end());
	auto lens_rest = std::vector<int>(lens.begin() + 1, lens.end());

	if constexpr (sizeof...(Rest) > 0) {
		return check_multi_error<Rest...>(cur_data, reg_names_rest,
						  lens_rest);
	}
	return is_error;
}

/* Encode a RISC-V ADDI instruction: rd = rs1 + sign_extend(imm[11:0]). */
uint32_t get_addi_inst(uint32_t rd, uint32_t rs1, uint32_t imm)
{
	uint32_t inst = 0;
	inst = (imm << 20) + (rs1 << 15) + (rd << 7) + (0x13);
	return inst;
}

/* Encode a RISC-V MUL instruction: rd = rs1 * rs2 (lower XLEN bits). */
uint32_t get_mul_inst(uint32_t rd, uint32_t rs1, uint32_t rs2)
{
	uint32_t inst = 0;
	inst = (1 << 25) + (rs2 << 20) + (rs1 << 15) + (0 << 12) + (rd << 7) +
	       (0x33);
	return inst;
}

/* Encode a RISC-V ADD instruction: rd = rs1 + rs2. */
uint32_t get_add_inst(uint32_t rd, uint32_t rs1, uint32_t rs2)
{
	uint32_t inst = 0;
	inst = (0 << 25) + (rs2 << 20) + (rs1 << 15) + (0 << 12) + (rd << 7) +
	       (0x33);
	return inst;
}

/* Encode a RISC-V SD (store doubleword) instruction: M[rs1 + imm] = rs2. */
uint32_t get_sd_inst(uint32_t rs2, uint32_t rs1, uint32_t imm)
{
	uint32_t inst = 0;
	uint32_t imm_1 = get_bits_range(imm, 11, 5);
	uint32_t imm_2 = get_bits_range(imm, 4, 0);
	inst = (imm_1 << 25) + (rs2 << 20) + (rs1 << 15) + (0x3 << 12) +
	       (imm_2 << 7) + (0x23);
	return inst;
}

/* Encode a RISC-V LD (load doubleword) instruction: rd = M[rs1 + imm]. */
uint32_t get_ld_inst(uint32_t rd, uint32_t rs1, uint32_t imm)
{
	uint32_t inst = 0;
	inst = (imm << 20) + (rs1 << 15) + (0x3 << 12) + (rd << 7) + (0x3);
	return inst;
}

/* Generate instructions to save all general-purpose registers (x1-x31, except
 * x2/sp) onto the stack. Decrements sp by 8*31 bytes to allocate stack space. */
void save_context(std::vector<uint32_t> &insts)
{
	uint32_t imm;
	uint32_t maxm_12 = 1u << 12;

	imm = maxm_12 - (8 * 31);
	insts.push_back(get_addi_inst(2, 2, imm));

	for (int i = 1; i < 32; i++) {
		if (i != 2)
			insts.push_back(get_sd_inst(i, 2, 8 * (i - 1)));
	}
}

/* Generate instructions to restore all general-purpose registers (x1-x31,
 * except x2/sp) from the stack and deallocate the stack frame. */
void restore_context(std::vector<uint32_t> &insts)
{
	uint32_t imm;
	uint32_t maxm_12 = 1u << 12;

	for (int i = 1; i < 32; i++) {
		if (i != 2)
			insts.push_back(get_ld_inst(i, 2, 8 * (i - 1)));
	}

	insts.push_back(get_addi_inst(2, 2, 8 * 31));
}

/* Generate an instruction sequence to load a 64-bit value from memory address
 * 'ptr' into register 'reg'. Constructs the full 64-bit address by splitting
 * it into 10-bit segments and combining them via ADDI+MUL chains. Uses x28-x31
 * as temporary registers. */
void load_reg(std::vector<uint32_t> &insts, uint64_t ptr, uint64_t reg)
{
	uint64_t inst = 0;

	uint64_t mask = (1ull << 10) - 1;
	inst = get_addi_inst(30, 0, 1ull << 10);
	insts.push_back(inst);

	uint64_t high4 = (ptr >> 60) & 0xf;
	if (high4 != 0) {
		inst = get_addi_inst(29, 0, high4);
		insts.push_back(inst);

		for (int j = 0; j < 6; j++) {
			inst = get_mul_inst(29, 29, 30);
			insts.push_back(inst);
		}
		inst = get_add_inst(31, 0, 29);
		insts.push_back(inst);
	} else {
		inst = get_add_inst(31, 0, 0);
		insts.push_back(inst);
	}

	for (int i = 50; i >= 0; i -= 10) {
		uint64_t t_ptr = get_bits_range(ptr, i + 9, i);
		inst = get_addi_inst(29, 0, t_ptr);
		insts.push_back(inst);

		for (int j = 0; j < i / 10; j++) {
			inst = get_mul_inst(29, 29, 30);
			insts.push_back(inst);
		}

		inst = get_add_inst(31, 31, 29);
		insts.push_back(inst);
	}

	inst = get_ld_inst(reg, 31, 0);
	insts.push_back(inst);
}

/* Generate an instruction sequence to store the value of register 'reg' to
 * memory address 'ptr'. Constructs the 64-bit address using ADDI+MUL chains
 * in 10-bit segments. Uses x28-x31 as temporary registers. */
void store_reg(std::vector<uint32_t> &insts, uint64_t ptr, uint64_t reg)
{
	uint64_t inst = 0;

	uint64_t mask = (1ull << 10) - 1;
	inst = get_addi_inst(30, 0, 1ull << 10);
	insts.push_back(inst);

	for (int i = 50; i >= 0; i -= 10) {
		uint64_t t_ptr = get_bits_range(ptr, i + 9, i);
		inst = get_addi_inst(29, 0, t_ptr);
		insts.push_back(inst);

		for (int j = 0; j < i / 10; j++) {
			inst = get_mul_inst(29, 29, 30);
			insts.push_back(inst);
		}

		if (i == 50) {
			inst = get_add_inst(31, 0, 29);
			insts.push_back(inst);
		} else {
			inst = get_add_inst(31, 31, 29);
			insts.push_back(inst);
		}
	}

	inst = get_sd_inst(reg, 31, 0);
	insts.push_back(inst);
}

/* Load a single scalar integer register value from test data into the target
 * register by generating load_reg instructions. Converts common format to
 * typed buffer and loads the memory address into the register. */
template <typename T>
void load_single_int(std::vector<uint32_t> &insts, c_data &cur_data,
		     const std::string &reg_name)
{
	if (cur_data.map_reg_index.find(reg_name) ==
	    cur_data.map_reg_index.end()) {
		INFO << "load multi int:" << reg_name << " not found in data\n";
		return;
	}

	int vd = cur_data.map_reg_index[reg_name];
	cur_data.map_reg_typed_value[reg_name] = c_data::process_from_common<T>(
		cur_data.map_reg_value[reg_name]);

	void *data = cur_data.map_reg_typed_value[reg_name].get();
	load_reg(insts, (uint64_t)data, vd);
}

/* Recursively load multiple scalar integer registers from test data.
 * Each type in the template parameter pack corresponds to one register. */
template <typename T, typename... Rest>
void load_multi_int(std::vector<uint32_t> &insts, c_data &cur_data,
		    std::vector<std::string> reg_names)
{
	load_single_int<T>(insts, cur_data, *reg_names.begin());

	auto reg_names_rest = std::vector<std::string>(reg_names.begin() + 1,
						       reg_names.end());

	if constexpr (sizeof...(Rest) > 0) {
		load_multi_int<Rest...>(insts, cur_data, reg_names_rest);
	}
}

/* Save a single scalar register's value before instruction execution by
 * generating store_reg instructions into a zero-initialized buffer. */
template <typename T>
void store_single_preinst_int(std::vector<uint32_t> &insts, c_data &cur_data,
			      const std::string &reg_name)
{
	if (cur_data.map_reg_index.find(reg_name) ==
	    cur_data.map_reg_index.end()) {
		INFO << __func__ << " : " << reg_name << " not found in data\n";
		return;
	}

	int vd = cur_data.map_reg_index[reg_name];
	cur_data.map_preinst_typed_value[reg_name] = make_zero_buffer<T>(1);

	void *data = cur_data.map_preinst_typed_value[reg_name].get();
	store_reg(insts, (uint64_t)data, vd);
}

/* Recursively save multiple scalar registers' pre-instruction values. */
template <typename T, typename... Rest>
void store_multi_preinst_int(std::vector<uint32_t> &insts, c_data &cur_data,
			     std::vector<std::string> reg_names)
{
	store_single_preinst_int<T>(insts, cur_data, *reg_names.begin());

	auto reg_names_rest = std::vector<std::string>(reg_names.begin() + 1,
						       reg_names.end());

	if constexpr (sizeof...(Rest) > 0) {
		store_multi_preinst_int<Rest...>(insts, cur_data,
						 reg_names_rest);
	}
}

/* Save a single scalar register's value after instruction execution by
 * generating store_reg instructions into a zero-initialized buffer. */
template <typename T>
void store_single_afterinst_int(std::vector<uint32_t> &insts, c_data &cur_data,
				const std::string &reg_name)
{
	if (cur_data.map_reg_index.find(reg_name) ==
	    cur_data.map_reg_index.end()) {
		INFO << __func__ << " : " << reg_name << " not found in data\n";
		return;
	}

	int vd = cur_data.map_reg_index[reg_name];
	cur_data.map_afterinst_typed_value[reg_name] = make_zero_buffer<T>(1);

	void *data = cur_data.map_afterinst_typed_value[reg_name].get();
	store_reg(insts, (uint64_t)data, vd);
}

/* Recursively save multiple scalar registers' post-instruction values. */
template <typename T, typename... Rest>
void store_multi_afterinst_int(std::vector<uint32_t> &insts, c_data &cur_data,
			       std::vector<std::string> reg_names)
{
	store_single_afterinst_int<T>(insts, cur_data, *reg_names.begin());

	auto reg_names_rest = std::vector<std::string>(reg_names.begin() + 1,
						       reg_names.end());

	if constexpr (sizeof...(Rest) > 0) {
		store_multi_afterinst_int<Rest...>(insts, cur_data,
						   reg_names_rest);
	}
}

uint64_t val_placeholder[10000];
int val_top = 0;
/* Generate a CSRRW instruction to write a value into the specified CSR.
 * First loads the value into temporary register x28 via load_reg, then
 * emits csrrw with rd=x0 (discard old CSR value).
 * Encoding: csrrw x0, csr, x28 */
void csrrw(std::vector<uint32_t> &insts, uint32_t reg, uint64_t val)
{
	val_placeholder[val_top++] = val;
	uint64_t ptr = (uint64_t)(&(val_placeholder[val_top - 1]));
	load_reg(insts, ptr, 28);
	uint32_t inst = (reg << 20) + (28 << 15) + (0b001 << 12) + (0 << 7) +
			(0b1110011);
	insts.push_back(inst);
}

/* JIT-execute a sequence of RISC-V instructions. Writes the instruction binary
 * to a file, maps it into an executable memory page via mmap, appends a
 * JALR return instruction, issues fence.i to synchronize the instruction cache,
 * then calls the code as a function pointer. Cleans up mmap after execution. */
void run_instruction(std::vector<uint32_t> &instructions_queue)
{
	DEBUG << "run_instruction using jit "
	      << "with inst count " << instructions_queue.size() << "...\n";
	//output instructions to file
	int kInstructionCount = instructions_queue.size();
	uint32_t *instructions = new uint32_t[kInstructionCount];
	for (int i = 0; i < kInstructionCount; ++i) {
		instructions[i] = instructions_queue[i];
	}

	std::string binfilename = filename + ".bin";
	std::ofstream outFile(binfilename, std::ios::binary);
	if (!outFile) {
		std::cerr << "Failed to open output file." << std::endl;
		exit(1);
	}
	outFile.write(reinterpret_cast<char *>(instructions),
		      kInstructionCount * sizeof(uint32_t));
	outFile.close();

	//mmap instructions to memory
	const size_t kInstructionSize = kInstructionCount * sizeof(uint32_t);
	void *codeMemory = mmap(nullptr, kInstructionSize + 4,
				PROT_READ | PROT_WRITE | PROT_EXEC,
				MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (codeMemory == MAP_FAILED) {
		std::cerr << "Failed to allocate executable memory."
			  << std::endl;
		exit(1);
	}

	std::ifstream inFile(binfilename, std::ios::binary);
	inFile.read(reinterpret_cast<char *>(codeMemory), kInstructionSize);
	inFile.close();

	uint32_t retInstruction = 0x8067; //jalr x0, 0(x1)
	memcpy(reinterpret_cast<uint8_t *>(codeMemory) + kInstructionSize,
	       &retInstruction, sizeof(retInstruction));
	//execute instructions
	void (*func)() = reinterpret_cast<void (*)()>(codeMemory);
	__asm__ volatile("fence.i");
	func();

	munmap(codeMemory, kInstructionSize + 4);
	delete[] instructions;
}

#endif
