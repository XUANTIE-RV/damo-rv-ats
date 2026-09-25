#ifndef __f_common__
#define __f_common__

#include "framework.h"

#define NV_FLAG (1U << 4)
#define DZ_FLAG (1U << 3)
#define OF_FLAG (1U << 2)
#define UF_FLAG (1U << 1)
#define NX_FLAG (1U << 0)

struct int4 {
	int8_t data : 4;
	uint8_t padding : 4;
	int4()
		: data(0)
		, padding(0)
	{
	}
	int4(int64_t other)
		: data(other)
		, padding(0)
	{
	}
	int4 operator+(const int4 &other) const
	{
		int4 res;
		res.data = this->data + other.data;
		return res;
	}
	int4 operator-(const int4 &other) const
	{
		int4 res;
		res.data = this->data - other.data;
		return res;
	}
	int4 operator*(const int4 &other) const
	{
		int4 res;
		res.data = this->data * other.data;
		return res;
	}
	bool operator>(const int4 &other) const
	{
		return this->data > other.data;
	}

	bool operator<(const int4 &other) const
	{
		return this->data < other.data;
	}

	operator int8_t() const
	{
		return data;
	}

	operator int16_t() const
	{
		return static_cast<int16_t>(data);
	}

	operator int32_t() const
	{
		return static_cast<int32_t>(data);
	}

	operator int64_t() const
	{
		return static_cast<int64_t>(data);
	}

	operator uint64_t() const
	{
		return static_cast<uint64_t>(data);
	}

#if __riscv_xlen != 32
	operator __int128_t() const
	{
		return static_cast<__int128_t>(data);
	}
#endif

	bool operator!=(const int4 &other) const
	{
		return this->data != other.data;
	}

	friend std::ostream &operator<<(std::ostream &output, const int4 cur)
	{
		output << static_cast<int64_t>(cur);
		return output;
	}

	friend std::istream &operator>>(std::istream &input, int4 &cur)
	{
		int64_t tmp;
		input >> tmp;
		cur.data = tmp;
		return input;
	}

	friend int8_t operator>>(const int4 &cur, int shift)
	{
		return static_cast<int8_t>(cur) >> shift;
	}

	friend int8_t operator>>(const int4 &cur, uint64_t shift)
	{
		return static_cast<int8_t>(cur) >> shift;
	}
};

struct uint4 {
	uint8_t data : 4;
	uint8_t padding : 4;
	uint4()
		: data(0)
		, padding(0)
	{
	}
	uint4(int other)
	{
		this->data = other;
	}
	uint4(int64_t other)
	{
		this->data = other;
	}
	uint4 operator+(const uint4 &other) const
	{
		uint4 res;
		res.data = this->data + other.data;
		return res;
	}
	uint4 operator-(const uint4 &other) const
	{
		uint4 res;
		res.data = this->data - other.data;
		return res;
	}
	uint4 operator*(const uint4 &other) const
	{
		uint4 res;
		res.data = this->data * other.data;
		return res;
	}
	bool operator>(const uint4 &other) const
	{
		return this->data > other.data;
	}
	bool operator<(const uint4 &other) const
	{
		return this->data < other.data;
	}

	operator uint8_t() const
	{
		return data;
	}

	operator uint16_t() const
	{
		return static_cast<uint16_t>(data);
	}

	operator uint64_t() const
	{
		return static_cast<uint64_t>(data);
	}

	operator uint32_t() const
	{
		return static_cast<uint32_t>(data);
	}

	operator int32_t() const
	{
		return static_cast<int32_t>(data);
	}

	operator int64_t() const
	{
		return static_cast<int64_t>(data);
	}

#if __riscv_xlen != 32
	operator __int128_t() const
	{
		return static_cast<__int128_t>(data);
	}
#endif

	bool operator!=(const uint4 &other) const
	{
		return this->data != other.data;
	}

	friend std::ostream &operator<<(std::ostream &output, const uint4 cur)
	{
		output << static_cast<uint64_t>(cur);
		return output;
	}

	friend std::istream &operator>>(std::istream &input, uint4 &cur)
	{
		uint64_t tmp;
		input >> tmp;
		cur.data = tmp;
		return input;
	}

	friend uint8_t operator>>(const uint4 &cur, int shift)
	{
		return static_cast<uint8_t>(cur) >> shift;
	}

	friend uint8_t operator>>(const uint4 &cur, uint64_t shift)
	{
		return static_cast<uint8_t>(cur) >> shift;
	}
};

template <typename T> struct is_my_integral : std::false_type {
};

template <> struct is_my_integral<int4> : std::true_type {
};

template <> struct is_my_integral<uint4> : std::true_type {
};

template <typename T>
inline constexpr bool is_my_integral_v = is_my_integral<T>::value;

struct e2m1 {
	uint8_t data;
	e2m1()
	{
	}
	e2m1(uint8_t val)
	{
		data = val;
	}
};

struct e2m3 {
	uint8_t data;
	e2m3()
	{
	}
	e2m3(uint8_t val)
	{
		data = val;
	}
};

struct e3m2 {
	uint8_t data;
	e3m2()
	{
	}
	e3m2(uint8_t val)
	{
		data = val;
	}
};

struct e5m2 {
	uint8_t data;
	e5m2()
	{
	}
	e5m2(uint8_t val)
	{
		data = val;
	}
};

struct e4m3 {
	uint8_t data;
	e4m3()
	{
	}
	e4m3(uint8_t val)
	{
		data = val;
	}
};

struct e8m0 {
	uint8_t data;
	e8m0()
	{
	}
	e8m0(uint8_t val)
	{
		data = val;
	}
};

struct bf20 {
	uint32_t data;
	bf20()
	{
	}
	bf20(uint32_t val)
	{
		data = val;
	}
};

/* RISC-V GCC does not support the ARM-specific __fp16 type.
 * Use the standard _Float16 type instead, which is supported by
 * RISC-V GCC (with or without the Zvfh extension). */
using fp16 = _Float16;
using bf16 = __bf16;

template <typename T> struct fp_traits {
	static constexpr bool is_valid = false;
};

#define FP_TRAIT_CONSTANTS(Sign, Exp, Man, Bias, HasInf, HasNan, HasZero,  \
			   HasDenorm, Storage, MaxNorm)                    \
	static constexpr bool is_valid = true;                             \
	static constexpr int sign_bits = Sign;                             \
	static constexpr int exp_bits = Exp;                               \
	static constexpr int man_bits = Man;                               \
	static constexpr int bias = Bias;                                  \
	static constexpr bool has_inf = HasInf;                            \
	static constexpr bool has_nan = HasNan;                            \
	static constexpr bool has_zero = HasZero;                          \
	static constexpr bool has_denorm = HasDenorm;                      \
	using storage_type = Storage;                                      \
	static constexpr storage_type max_unsigned_normal = MaxNorm;       \
	static constexpr int total_bits = sign_bits + exp_bits + man_bits; \
	static constexpr int storage_bits = sizeof(storage_type) * 8;

template <> struct fp_traits<e2m1> {
	FP_TRAIT_CONSTANTS(1, 2, 1, 0x1, false, false, true, true, uint8_t,
			   0x7);
};

template <> struct fp_traits<e2m3> {
	FP_TRAIT_CONSTANTS(1, 2, 3, 0x1, false, false, true, true, uint8_t,
			   0x1f);
};
template <> struct fp_traits<e3m2> {
	FP_TRAIT_CONSTANTS(1, 3, 2, 0x3, false, false, true, true, uint8_t,
			   0x1f);
};

template <> struct fp_traits<e5m2> {
	FP_TRAIT_CONSTANTS(1, 5, 2, 0xf, true, true, true, true, uint8_t, 0x7b);
};
template <> struct fp_traits<e4m3> {
	FP_TRAIT_CONSTANTS(1, 4, 3, 0x7, false, true, true, true, uint8_t,
			   0x7e);
};
template <> struct fp_traits<e8m0> {
	FP_TRAIT_CONSTANTS(0, 8, 0, 0x7f, false, true, false, false, uint8_t,
			   0xfe);
};

template <> struct fp_traits<bf20> {
	FP_TRAIT_CONSTANTS(1, 8, 11, 0x7f, true, true, true, true, uint32_t,
			   0x7f7ff);
};

template <> struct fp_traits<fp16> {
	FP_TRAIT_CONSTANTS(1, 5, 10, 0xf, true, true, true, true, uint16_t,
			   0x7bff);
};
template <> struct fp_traits<bf16> {
	FP_TRAIT_CONSTANTS(1, 8, 7, 0x7f, true, true, true, true, uint16_t,
			   0x7f7f);
};
template <> struct fp_traits<float> {
	FP_TRAIT_CONSTANTS(1, 8, 23, 0x7f, true, true, true, true, uint32_t,
			   0x7f7fffff);
};
template <> struct fp_traits<double> {
	FP_TRAIT_CONSTANTS(1, 11, 52, 0x3ff, true, true, true, true, uint64_t,
			   0x7fefffffffffffff);
};

template <typename T>
inline constexpr bool is_builtin_fp_v = std::is_floating_point_v<T>;

template <> inline constexpr bool is_builtin_fp_v<fp16> = true;
template <> inline constexpr bool is_builtin_fp_v<bf16> = true;

template <typename T, typename U> class c_check_f {
    public:
	using storage_type = U;
	using value_type = T;

	union {
		T fdata;
		U idata;
	} m_data;
	inline static uint8_t sign_bit;
	inline static uint8_t exp_bit;
	inline static uint8_t man_bit;
	inline static uint8_t bits;
	inline static U exp_bias;
	c_check_f()
		: m_data{}
	{
		static_assert(fp_traits<T>::is_valid,
			      "Unsupported floating-point format");
		sign_bit = fp_traits<T>::sign_bits;
		exp_bit = fp_traits<T>::exp_bits;
		man_bit = fp_traits<T>::man_bits;
		bits = fp_traits<T>::total_bits;
		exp_bias = fp_traits<T>::bias;
		m_data.idata = 0;
	}

	c_check_f(U idata)
		: c_check_f()
	{
		m_data.idata = idata;
	}

	c_check_f(const c_check_f &other)
		: m_data{}
	{
		this->m_data.idata = other.m_data.idata;
		this->sign_bit = other.sign_bit;
		this->exp_bias = other.exp_bias;
		this->exp_bit = other.exp_bit;
		this->man_bit = other.man_bit;
		this->bits = other.bits;
	}

	c_check_f &operator=(const c_check_f &other)
	{
		if (this == &other)
			return *this;

		this->m_data.idata = other.m_data.idata;
		this->sign_bit = other.sign_bit;
		this->exp_bias = other.exp_bias;
		this->exp_bit = other.exp_bit;
		this->man_bit = other.man_bit;
		this->bits = other.bits;
		return *this;
	}

	U m_get_sign() const
	{
		if constexpr (fp_traits<T>::sign_bits == 0)
			return 0;
		return get_bit(m_data.idata, bits - sign_bit);
	}

	U m_get_exp() const
	{
		return get_bits_range(m_data.idata, bits - sign_bit - 1,
				      bits - sign_bit - exp_bit);
	}

	U m_get_man() const
	{
		return get_bits_range(m_data.idata,
				      bits - sign_bit - exp_bit - 1,
				      bits - sign_bit - exp_bit - man_bit);
	}

	void m_set_sign(U val)
	{
		m_data.idata &= ~((1ull << (bits - sign_bit)));
		m_data.idata |= (val << (bits - sign_bit));
	}

	void m_set_exp(U val)
	{
		U tmp_exp = ((U)1 << exp_bit) - 1;
		tmp_exp = tmp_exp << (bits - sign_bit - exp_bit);
		m_data.idata &= ~tmp_exp;
		m_data.idata |= (val << (bits - sign_bit - exp_bit));
	}

	void m_set_man(U val)
	{
		U tmp_man = ((U)1 << man_bit) - 1;
		tmp_man = tmp_man << (bits - sign_bit - exp_bit - man_bit);
		m_data.idata &= ~tmp_man;
		m_data.idata |= (val << (bits - sign_bit - exp_bit - man_bit));
	}

	bool m_is_nan()
	{
		if constexpr (!fp_traits<T>::has_nan)
			return false;
		bool exp_check = this->m_get_exp() == (1ull << (exp_bit)) - 1;
		bool man_check = this->m_get_man() != 0;
		auto sign = this->m_get_sign();
		auto max_normal = fp_traits<T>::max_unsigned_normal;
		max_normal = ((sign << (bits - sign_bit)) | max_normal);
		bool bits_check = this->m_data.idata > max_normal;
		return exp_check && man_check && bits_check;
	}

	bool m_is_qnan()
	{
		if constexpr (!fp_traits<T>::has_nan)
			return false;
		bool man_check = get_bit(m_data.idata,
					 bits - sign_bit - exp_bit - 1) == 1;
		return this->m_is_nan() && man_check;
	}

	bool m_is_snan()
	{
		if constexpr (!fp_traits<T>::has_nan)
			return false;
		bool man_check = get_bit(m_data.idata,
					 bits - sign_bit - exp_bit - 1) == 0;
		return this->m_is_nan() && man_check;
	}

	bool m_is_zero()
	{
		if constexpr (!fp_traits<T>::has_zero)
			return false;
		bool exp_check = this->m_get_exp() == 0;
		bool man_check = this->m_get_man() == 0;
		return exp_check && man_check;
	}

	bool m_is_inf()
	{
		if constexpr (!fp_traits<T>::has_inf)
			return false;
		bool exp_check = this->m_get_exp() == (1ull << (exp_bit)) - 1;
		bool man_check = this->m_get_man() == 0;
		auto sign = this->m_get_sign();
		auto max_normal = fp_traits<T>::max_unsigned_normal;
		max_normal = ((sign << (bits - sign_bit)) | max_normal);
		bool bits_check = this->m_data.idata > max_normal;
		return exp_check && man_check && bits_check;
	}

	int m_is_positive_inf()
	{
		if constexpr (!fp_traits<T>::has_inf)
			return false;
		bool sign_check = this->m_get_sign() == 0;
		return this->m_is_inf() && sign_check;
	}

	int m_is_negtive_inf()
	{
		if constexpr (!fp_traits<T>::has_inf)
			return false;
		bool sign_check = this->m_get_sign() == 1;
		return this->m_is_inf() && sign_check;
	}

	int m_is_denormal()
	{
		if constexpr (!fp_traits<T>::has_denorm)
			return false;
		return this->m_get_exp() == 0 && this->m_get_man() != 0;
	}

	static c_check_f<T, U> get_snan(int sign)
	{
		if constexpr (!fp_traits<T>::has_nan)
			throw std::runtime_error(
				"current floating-point format has not NAN");
		c_check_f<T, U> result;
		result.m_set_sign(sign);
		result.m_set_exp(((1ull << (exp_bit)) - 1));
		U man = get_rand_bound<U>(1, (1u << (man_bit - 1) - 1));
		result.m_set_man(man);
		if (!result.m_is_snan())
			throw std::runtime_error(
				"current floating-point format has not SNaN");
		return result;
	}

	static c_check_f<T, U> get_qnan(int sign)
	{
		if constexpr (!fp_traits<T>::has_nan)
			throw std::runtime_error(
				"current floating-point format has not NAN");
		c_check_f<T, U> result;
		result.m_set_sign(sign);
		result.m_set_exp(((1ull << (exp_bit)) - 1));
		U man = get_rand_bound<U>(1u << (man_bit - 1),
					  1u << (man_bit)-1);
		result.m_set_man(man);
		if (!result.m_is_qnan())
			throw std::runtime_error(
				"current floating-point format has not QNaN");
		return result;
	}

	static c_check_f<T, U> get_cnan(int sign)
	{
		if constexpr (!fp_traits<T>::has_nan)
			throw std::runtime_error(
				"current floating-point format has not NAN");
		c_check_f<T, U> result;
		result.m_set_exp((1ull << (exp_bit)) - 1);
		result.m_set_man((1u << (man_bit - 1)));
		if (!result.m_is_nan())
			throw std::runtime_error(
				"current floating-point format has not CNaN");
		return result;
	}

	static c_check_f<T, U> get_nan(int sign)
	{
		if constexpr (!fp_traits<T>::has_nan)
			throw std::runtime_error(
				"current floating-point format has not NAN");
		c_check_f<T, U> result;
		int s = get_rand<int>() % 2;

		if (s) {
			try {
				result = result.get_qnan(sign);
			} catch (const std::exception &e) {
				result = result.get_cnan(sign);
			}
		} else {
			try {
				result = result.get_snan(sign);
			} catch (const std::exception &e) {
				result = result.get_cnan(sign);
			}
		}
		return result;
	}

	static c_check_f<T, U> get_inf(int sign)
	{
		if constexpr (!fp_traits<T>::has_inf)
			throw std::runtime_error(
				"current floating-point format has not INF");
		c_check_f<T, U> result;
		result.m_set_sign(sign);
		result.m_set_exp(((1ull << (exp_bit)) - 1));
		result.m_set_man(0);
		return result;
	}

	static c_check_f<T, U> get_zero(int sign)
	{
		if constexpr (!fp_traits<T>::has_zero)
			throw std::runtime_error(
				"current floating-point format has not ZERO");
		c_check_f<T, U> result;
		result.m_set_sign(sign);
		result.m_set_exp(0);
		result.m_set_man(0);
		return result;
	}

	static c_check_f<T, U> get_max_denormal(int sign)
	{
		if constexpr (!fp_traits<T>::has_denorm)
			throw std::runtime_error(
				"current floating-point format has not DENORM");
		c_check_f<T, U> result;
		result.m_set_sign(sign);
		result.m_set_exp(0);
		result.m_set_man((1 << (man_bit)) - 1);
		return result;
	}

	static c_check_f<T, U> get_min_denormal(int sign)
	{
		if constexpr (!fp_traits<T>::has_denorm)
			throw std::runtime_error(
				"current floating-point format has not DENORM");
		c_check_f<T, U> result;
		result.m_set_sign(sign);
		result.m_set_exp(0);
		result.m_set_man(1);
		return result;
	}

	static c_check_f<T, U> get_denormal(int sign)
	{
		if constexpr (!fp_traits<T>::has_denorm)
			throw std::runtime_error(
				"current floating-point format has not DENORM");
		c_check_f<T, U> result;
		result.m_set_sign(sign);
		result.m_set_exp(0);
		U man = get_rand<U>();
		man = get_bits_range(man, man_bit - 1, 0);
		if (man == 0)
			man = 1;
		result.m_set_man(man);
		return result;
	}

	static c_check_f<T, U> get_max_normal(int sign)
	{
		c_check_f<T, U> result;

		auto max_normal = fp_traits<T>::max_unsigned_normal;
		result.m_data.idata = max_normal;

		auto sign_check = get_bit(max_normal, bits - sign_bit);
		if (sign_check == 1 && sign == 0)
			sign = 1;
		result.m_set_sign(sign);
		return result;
	}

	static c_check_f<T, U> get_min_normal(int sign)
	{
		c_check_f<T, U> result;
		result.m_set_sign(sign);
		result.m_set_exp(1);
		result.m_set_man(0);
		return result;
	}

	static c_check_f<T, U> get_normal(int sign)
	{
		c_check_f<T, U> result;
		result.m_set_sign(sign);
		U exp_data = get_rand<U>();
		exp_data = get_bits_range(exp_data, exp_bit - 1, 0);
		U man = get_rand<U>();
		man = get_bits_range(man, man_bit - 1, 0);
		if (exp_data == 0)
			exp_data = 1;

		if (exp_data == ((1ull << (exp_bit)) - 1)) {
			exp_data--;
		}

		result.m_set_exp(exp_data);
		result.m_set_man(man);
		return result;
	}

	static c_check_f<T, U> get_class_rand()
	{
		std::uniform_int_distribution<uint64_t> dist;
		c_check_f<T, U> result;
		result.m_data.idata = dist(global_rng) & ((1ull << bits) - 1);
		return result;
	}

	static c_check_f<T, U> get_class_rand_bound(c_check_f<T, U> minm,
						    c_check_f<T, U> maxm)
	{
		std::uniform_int_distribution<int> choice(0, 9);

		int behavior = choice(global_rng);

		int sign = get_rand<int>() % 2;

		switch (behavior) {
		case 0:
			return minm;
		case 1:
			return maxm;
		case 2: {
			try {
				return c_check_f<T, U>::get_nan(sign);
			} catch (const std::exception &e) {
				return c_check_f<T, U>::get_normal(sign);
			}
		}
		case 3: {
			try {
				return c_check_f<T, U>::get_inf(sign);
			} catch (const std::exception &e) {
				return c_check_f<T, U>::get_normal(sign);
			}
		}
		case 4: {
			try {
				return c_check_f<T, U>::get_zero(sign);
			} catch (const std::exception &e) {
				return c_check_f<T, U>::get_normal(sign);
			}
		}
		case 5:
			return c_check_f<T, U>::get_normal(sign);
		case 6:
			return c_check_f<T, U>::get_denormal(sign);
		default: {
			U tmp_data = get_rand_bound<U>(minm.m_data.idata,
						       maxm.m_data.idata);
			return c_check_f<T, U>(tmp_data);
		}
		}
		return c_check_f<T, U>();
	}

	static uint64_t save_class_to_uint64(c_check_f<T, U> val)
	{
		return static_cast<uint64_t>(val.m_data.idata);
	}

	static c_check_f<T, U> load_class_from_uint64(uint64_t val)
	{
		c_check_f<T, U> result;
		result.m_data.idata = static_cast<U>(val);
		return result;
	}

	c_check_f<T, U> operator+(const c_check_f<T, U> &other) const
	{
		c_check_f<T, U> result;
		if constexpr (is_builtin_fp_v<T>) {
			result.m_data.fdata =
				this->m_data.fdata + other.m_data.fdata;
		}
		return result;
	}

	c_check_f<T, U> operator-(const c_check_f<T, U> &other) const
	{
		c_check_f<T, U> result;
		if constexpr (is_builtin_fp_v<T>) {
			result.m_data.fdata =
				this->m_data.fdata - other.m_data.fdata;
		}
		return result;
	}

	c_check_f<T, U> operator*(const c_check_f<T, U> &other) const
	{
		c_check_f<T, U> result;
		if constexpr (is_builtin_fp_v<T>) {
			result.m_data.fdata =
				this->m_data.fdata * other.m_data.fdata;
		}
		return result;
	}

	bool operator>(const c_check_f<T, U> &other) const
	{
		if constexpr (is_builtin_fp_v<T>)
			return this->m_data.fdata > other.m_data.fdata;
		else {
			if (m_is_nan() || other.m_is_nan())
				return false;
			if constexpr (fp_traits<T>::sign_bits == 0)
				return m_data.idata > other.m_data.idata;
			else {
				auto s1 = m_get_sign();
				auto s2 = other.m_get_sign();
				if (s1 != s2)
					return !s1;
				return s1 ? (m_data.idata <
					     other.m_data.idata) :
					    (m_data.idata > other.m_data.idata);
			}
		}
		return false;
	}

	bool operator<(const c_check_f<T, U> &other) const
	{
		if constexpr (is_builtin_fp_v<T>)
			return this->m_data.fdata < other.m_data.fdata;
		else {
			if (m_is_nan() || other.m_is_nan())
				return false;
			if constexpr (fp_traits<T>::sign_bits == 0)
				return m_data.idata < other.m_data.idata;
			else {
				auto s1 = m_get_sign();
				auto s2 = other.m_get_sign();
				if (s1 != s2)
					return s1;
				return s1 ? (m_data.idata >
					     other.m_data.idata) :
					    (m_data.idata < other.m_data.idata);
			}
		}
		return false;
	}

	bool operator!=(const c_check_f<T, U> &other) const
	{
		return this->m_data.idata != other.m_data.idata;
	}

	friend std::ostream &operator<<(std::ostream &os, c_check_f<T, U> &cfg)
	{
		os << static_cast<uint64_t>(cfg.m_data.idata);
		return os;
	}

	friend std::istream &operator>>(std::istream &is, c_check_f<T, U> &cfg)
	{
		uint64_t tmp;
		is >> tmp;
		cfg.m_data.idata = tmp;
		return is;
	}
};

template <typename T> T get_round_carry(uint64_t frm, T sign, T lsb, T g, T rs)
{
	switch (frm) {
	case 0:
		return (g & rs) | (lsb & g); //RNE
	case 1:
		return 0; //RTZ
	case 2:
		return sign & (g | rs); //RDN
	case 3:
		return (!sign) & (g | rs); //RUP
	case 4:
		return g; //RMM
	case 8:
		return (!lsb) & (g | rs); //ROD
	default:
		return 0;
	}
}

int64_t low_bit(int64_t x)
{
	return x & (-x);
}

template <typename T, typename U>
U change_f(uint64_t frm, uint64_t sat, T src, int &fflag)
{
	U dst{};

	if constexpr (std::is_integral_v<T> || is_my_integral_v<T>) {
		uint64_t sign = (src < static_cast<T>(0)) ? 1 : 0;
		uint64_t max_exp = dst.exp_bias;
		// 修复可疑点 1.4：e4m3 和 e2m1 格式的 max_unsigned_normal 的指数比通常的指数多 1
		// 这是因为这些非标准格式的最大正规数的指数偏移量特殊处理
		if constexpr (std::is_same_v<decltype(dst.m_data.fdata), e4m3> ||
			      std::is_same_v<decltype(dst.m_data.fdata), e2m1>)
			max_exp = max_exp + 1;

		// 修复可疑点 1.1：将 src 转换为无符号类型，避免负数右移的未定义行为
		uint64_t abs_src =
			(sign == 1) ?
				static_cast<uint64_t>(
					-static_cast<std::make_signed_t<T> >(
						src)) :
				static_cast<uint64_t>(src);

		if ((abs_src >> max_exp)) {
			if (sat &&
			    (std::is_same_v<decltype(dst.m_data.fdata), e4m3> ||
			     std::is_same_v<decltype(dst.m_data.fdata), e5m2>)) {
				fflag |= NX_FLAG;
				fflag |= OF_FLAG;
				return dst.get_max_normal(sign);
			} else {
				fflag |= NX_FLAG;
				fflag |= OF_FLAG;
				return dst.get_inf(sign);
			}
		} else {
			// 修复可疑点 1.2：处理零值输入
			if (abs_src == 0) {
				return dst.get_zero(sign);
			}

			// 修复可疑点 1.5：查找最高有效位的位置
			// 对于 abs_src == 1，循环结束后 src_exp == 0，表示 2^0 = 1
			// 对于 abs_src == 2，循环结束后 src_exp == 1，表示 2^1 = 2
			int src_exp = max_exp - 1;
			for (; !(abs_src >> src_exp) && src_exp >= 0; src_exp--)
				;
			uint64_t man = 0;
			bool truncated_bits_nonzero = false;
			for (int i = src_exp - 1; i >= 0; i--) {
				if ((abs_src >> i) & 1) {
					if (src_exp - i > dst.man_bit) {
						// 记录有被截断的非零位
						truncated_bits_nonzero = true;
					} else {
						man = man |
						      (1ull << (dst.man_bit -
								(src_exp - i)));
					}
				}
			}
			// 修复可疑点 1.3：如果有被截断的非零位，设置 NX_FLAG
			if (truncated_bits_nonzero) {
				fflag |= NX_FLAG;
			}
			dst.m_set_sign(sign);
			dst.m_set_exp(src_exp + dst.exp_bias);
			dst.m_set_man(man);
			return dst;
		}
	} else if constexpr (std::is_integral_v<U> || is_my_integral_v<U>) {
		// 修复可疑点 2.1 和 2.2：添加 NaN、Inf 检查和溢出保护
		c_check_f<double, uint64_t> tmp =
			change_f<T, c_check_f<double, uint64_t> >(frm, sat, src,
								  fflag);

		// 检查 NaN 和 Inf
		if (tmp.m_is_nan() || tmp.m_is_inf()) {
			fflag |= NV_FLAG;
			return static_cast<U>(0);
		}

		// 获取浮点数值并进行范围检查
		double tmp_val = tmp.m_data.fdata;

		// 修复可疑点 2.3：根据 frm 参数应用舍入模式
		// 在转换为整数之前，先根据舍入模式进行处理
		switch (frm) {
		case 0: // RNE: Round to Nearest, ties to Even
		{
			double floor_val = std::floor(tmp_val);
			double ceil_val = std::ceil(tmp_val);
			double diff_floor = tmp_val - floor_val;
			double diff_ceil = ceil_val - tmp_val;

			if (diff_floor < diff_ceil)
				tmp_val = floor_val;
			else if (diff_floor > diff_ceil)
				tmp_val = ceil_val;
			else {
				// ties to even
				long long floor_int =
					static_cast<long long>(floor_val);
				if (floor_int % 2 == 0)
					tmp_val = floor_val;
				else
					tmp_val = ceil_val;
			}
			break;
		}
		case 1: // RTZ: Round toward Zero (truncation)
			// 默认行为，无需额外处理
			break;
		case 2: // RDN: Round Down (toward -∞)
			tmp_val = std::floor(tmp_val);
			break;
		case 3: // RUP: Round Up (toward +∞)
			tmp_val = std::ceil(tmp_val);
			break;
		case 4: // RMM: Round to Nearest, ties to Max Magnitude
		{
			double floor_val = std::floor(tmp_val);
			double ceil_val = std::ceil(tmp_val);
			double diff_floor = tmp_val - floor_val;
			double diff_ceil = ceil_val - tmp_val;

			if (diff_floor < diff_ceil)
				tmp_val = floor_val;
			else if (diff_floor > diff_ceil)
				tmp_val = ceil_val;
			else {
				// ties to max magnitude
				if (std::abs(floor_val) > std::abs(ceil_val))
					tmp_val = floor_val;
				else
					tmp_val = ceil_val;
			}
			break;
		}
		case 8: // ROD: Round to Odd
		{
			double floor_val = std::floor(tmp_val);
			double ceil_val = std::ceil(tmp_val);
			long long floor_int = static_cast<long long>(floor_val);
			long long ceil_int = static_cast<long long>(ceil_val);

			if (floor_int % 2 != 0)
				tmp_val = floor_val;
			else if (ceil_int % 2 != 0)
				tmp_val = ceil_val;
			else {
				// 如果两者都是偶数，选择更接近的
				if (tmp_val - floor_val < ceil_val - tmp_val)
					tmp_val = floor_val;
				else
					tmp_val = ceil_val;
			}
			break;
		}
		default:
			// 默认使用 RTZ
			break;
		}

		// 定义 int64_t 的范围
		constexpr double INT64_MAX_D = static_cast<double>(INT64_MAX);
		constexpr double INT64_MIN_D = static_cast<double>(INT64_MIN);

		// 检查是否超出 int64_t 范围
		if (tmp_val > INT64_MAX_D) {
			fflag |= OF_FLAG;
			// 饱和处理：返回最大值
			if constexpr (std::is_signed_v<U>)
				return static_cast<U>(INT64_MAX);
			else
				return static_cast<U>(UINT64_MAX);
		} else if (tmp_val < INT64_MIN_D) {
			fflag |= OF_FLAG;
			// 饱和处理：返回最小值
			if constexpr (std::is_signed_v<U>)
				return static_cast<U>(INT64_MIN);
			else
				return static_cast<U>(0);
		}

		// 安全转换
		int64_t tmp_int = static_cast<int64_t>(tmp_val);
		return static_cast<U>(tmp_int);
	} else {
		auto sign = src.m_get_sign();
		auto exp_data = src.m_get_exp();
		auto man = src.m_get_man();

		dst.m_set_sign(sign);
		if (src.m_is_nan()) {
			if constexpr (std::is_same_v<decltype(dst.m_data.fdata),
						     e2m1>) {
				fflag |= NV_FLAG;
				return dst.get_max_normal(0);
			}
			if (src.m_is_snan())
				fflag |= NV_FLAG;
			return dst.get_cnan(sign);
		}

		if (src.m_is_inf()) {
			if (sat &&
			    (std::is_same_v<decltype(dst.m_data.fdata), e4m3> ||
			     std::is_same_v<decltype(dst.m_data.fdata), e5m2>)) {
				return dst.get_max_normal(sign);
			} else {
				if constexpr (std::is_same_v<
						      decltype(dst.m_data.fdata),
						      e4m3>) {
					return dst.get_cnan(sign);
				}
				return dst.get_inf(sign);
			}
		}
		if (src.m_is_zero())
			return dst.get_zero(sign);

		//wide
		if (src.exp_bit == dst.exp_bit) {
			decltype(dst.m_data.idata) temp_exp = exp_data;
			temp_exp = temp_exp + dst.exp_bias - src.exp_bias;
			decltype(dst.m_data.idata) temp_man = man;
			if (src.man_bit <= dst.man_bit) {
				// 修复可疑点 3.3：源尾数位宽小于等于目标时，左移位填充低位补零，无精度损失
				// 因此不需要设置 NX_FLAG，也不需要舍入处理
				temp_man = temp_man
					   << (dst.man_bit - src.man_bit);
				dst.m_set_exp(temp_exp);
				dst.m_set_man(temp_man);
				return dst;
			}
		}

		if (src.exp_bit < dst.exp_bit) {
			decltype(dst.m_data.idata) temp_exp = exp_data;
			temp_exp = temp_exp + dst.exp_bias - src.exp_bias;
			decltype(dst.m_data.idata) temp_man = man;
			if (src.man_bit <= dst.man_bit) {
				temp_man = temp_man
					   << (dst.man_bit - src.man_bit);
				if (exp_data != 0) {
					dst.m_set_exp(temp_exp);
					dst.m_set_man(temp_man);
				} else {
					// 修复可疑点 3.4：处理次正规数的规范化
					// 当源是次正规数（exp_data == 0）时，需要找到尾数的最高有效位
					temp_exp++;
					decltype(dst.m_data.idata)
						man_max_bit = 1;
					man_max_bit = man_max_bit
						      << (dst.man_bit);
					// 循环条件确保 temp_man 不为 0 且最高位不是 1 时继续左移
					// 如果 temp_man 为 0，循环立即退出，temp_exp 保持为 1（次正规数的最小指数）
					while (temp_man != 0 &&
					       (man_max_bit & temp_man) == 0) {
						temp_exp--;
						temp_man = temp_man << 1;
					}
					temp_man &= ~(man_max_bit);
					dst.m_set_exp(temp_exp);
					dst.m_set_man(temp_man);
				}
				return dst;
			}
		}

		//narrow
		uint64_t frac_cut =
			(src.man_bit >= dst.man_bit) ?
				get_bits_range(man, src.man_bit - 1,
					       src.man_bit - dst.man_bit) :
				man;
		uint64_t g =
			(src.man_bit - dst.man_bit - 1 >= 0) ?
				get_bit(man, src.man_bit - dst.man_bit - 1) :
				0;
		uint64_t r =
			(src.man_bit - dst.man_bit - 2 >= 0) ?
				get_bit(man, src.man_bit - dst.man_bit - 2) :
				0;
		// 修复可疑点 3.5：tail 计算时，当 src.man_bit - dst.man_bit - 3 < 0 时返回 0，避免无效位访问
		int64_t tail =
			(src.man_bit - dst.man_bit - 3 >= 0) ?
				get_bits_range(
					man, src.man_bit - dst.man_bit - 3, 0) :
				0;
		uint64_t lsb = (src.man_bit >= dst.man_bit) ?
				       get_bit(man, src.man_bit - dst.man_bit) :
				       0;
		tail = low_bit(tail) != 0;

		if ((g | r | tail) != 0) {
			fflag |= NX_FLAG;
		}

		int64_t temp_exp = exp_data;
		temp_exp = temp_exp + dst.exp_bias - src.exp_bias;
		decltype(dst.m_data.idata) exp_max = 1;
		exp_max = (exp_max << dst.exp_bit) - 1;
		uint64_t carry = get_round_carry<decltype(dst.m_data.idata)>(
			frm, sign, lsb, g, r | tail);
		if (temp_exp > exp_max) {
			fflag |= NX_FLAG;
			fflag |= OF_FLAG;
			if (sat == 1 &&
			    (std::is_same_v<decltype(dst.m_data.fdata), e4m3> ||
			     std::is_same_v<decltype(dst.m_data.fdata), e5m2>)) {
				return dst.get_max_normal(sign);
			}
			return dst.get_inf(sign);
		} else if (temp_exp == exp_max) {
			if constexpr (std::is_same_v<decltype(dst.m_data.fdata),
						     e4m3>) {
				if (frac_cut + carry >= 7) {
					fflag |= OF_FLAG;
					if (sat == 1) {
						return dst.get_max_normal(sign);
					}
					return dst.get_inf(sign);
				} else {
					dst.m_set_exp(temp_exp);
					dst.m_set_man(frac_cut + carry);
					return dst;
				}
			}
			if constexpr (std::is_same_v<decltype(dst.m_data.fdata),
						     e2m1>) {
				if (frac_cut + carry > 1) {
					fflag |= OF_FLAG;
					if (sat == 1) {
						return dst.get_max_normal(sign);
					}
					return dst.get_inf(sign);
				} else {
					dst.m_set_exp(temp_exp);
					dst.m_set_man(frac_cut + carry);
					return dst;
				}
			}
			if constexpr (std::is_same_v<decltype(dst.m_data.fdata),
						     e8m0>) {
				if (frac_cut + carry > 0) {
					fflag |= OF_FLAG;
					return dst.get_inf(sign);
				} else {
					dst.m_set_exp(temp_exp);
					dst.m_set_man(frac_cut + carry);
					return dst;
				}
			} else {
				fflag |= OF_FLAG;
				if (sat == 1 &&
				    (std::is_same_v<decltype(dst.m_data.fdata),
						    e4m3> ||
				     std::is_same_v<decltype(dst.m_data.fdata),
						    e5m2>)) {
					return dst.get_max_normal(sign);
				}
				return dst.get_inf(sign);
			}
		} else if (temp_exp == exp_max - 1) {
			if (frac_cut + carry > (1ull << dst.man_bit) - 1) {
				if constexpr (
					!(std::is_same_v<
						  decltype(dst.m_data.fdata),
						  e4m3> ||
					  std::is_same_v<
						  decltype(dst.m_data.fdata),
						  e2m1> ||
					  std::is_same_v<
						  decltype(dst.m_data.fdata),
						  e8m0>)) {
					fflag |= NX_FLAG;
					fflag |= OF_FLAG;
					if (sat == 1) {
						return dst.get_max_normal(sign);
					}
					return dst.get_inf(sign);
				}
			}
			dst.m_set_exp(temp_exp);
			dst.m_set_man(frac_cut + carry);
			return dst;
		} else if (temp_exp > 0) {
			uint64_t temp_man = frac_cut + carry;
			if (frac_cut + carry == (1ull << dst.man_bit)) {
				temp_exp++;
				temp_man = 0;
			}
			dst.m_set_exp(temp_exp);
			dst.m_set_man(temp_man);
			return dst;
		} else if (temp_exp == 0) {
			if (exp_data == 0) {
				// 源是次正规数，目标是次正规数
				uint64_t temp_man = frac_cut + carry;
				if (frac_cut + carry == (1ull << dst.man_bit)) {
					temp_exp++;
					temp_man = 0;
				}
				dst.m_set_exp(temp_exp);
				dst.m_set_man(temp_man);
				return dst;
			}
			// 如果 exp_data != 0，说明是从正规数转换为次正规数（下溢）
			// 继续执行后续的舍入和下溢处理逻辑
		}

		// 修复可疑点 3.6：UF_FLAG 设置条件
		// 当下溢发生时（结果是次正规数或零），且存在精度损失时设置 UF_FLAG
		// 当前条件检查尾数是否未溢出，结合后续的舍入逻辑判断是否需要设置 UF_FLAG
		if (frac_cut + carry <= ((1 << dst.man_bit) - 1)) {
			fflag |= UF_FLAG;
		}

		tail = tail | r;
		r = g;
		g = frac_cut & 1;
		frac_cut = (frac_cut >> 1) + (1 << (dst.man_bit - 1));

		while (temp_exp < 0 && (r | g | frac_cut) != 0) {
			tail = tail | r;
			r = g;
			g = frac_cut & 1;
			frac_cut = frac_cut >> 1;
			temp_exp++;
		}

		if (temp_exp < 0) {
			return dst.get_zero(sign);
		}

		carry = get_round_carry<decltype(dst.m_data.idata)>(
			frm, sign, lsb, g, r | tail);

		if (frac_cut + carry == (1 << dst.man_bit)) {
			// 修复可疑点 3.7：检查增加后的指数是否超出最大值
			temp_exp++;
			decltype(dst.m_data.idata) exp_max = 1;
			exp_max = (exp_max << dst.exp_bit) - 1;

			if (temp_exp > exp_max) {
				fflag |= NX_FLAG;
				fflag |= OF_FLAG;
				if (sat == 1 &&
				    (std::is_same_v<decltype(dst.m_data.fdata),
						    e4m3> ||
				     std::is_same_v<decltype(dst.m_data.fdata),
						    e5m2>)) {
					return dst.get_max_normal(sign);
				}
				return dst.get_inf(sign);
			}

			dst.m_set_exp(temp_exp);
			dst.m_set_man(0);
			return dst;
		}

		dst.m_set_exp(0);
		dst.m_set_man(frac_cut + carry);
		return dst;
	}
}

namespace std
{
template <typename T, typename U> struct numeric_limits<c_check_f<T, U> > {
	static constexpr bool is_specialized = true;
	static constexpr bool is_signed = fp_traits<T>::sign_bits != 0;
	static constexpr bool is_integer = false;
	static constexpr bool is_exact = false;
	static constexpr int radix = 2;
	static constexpr int digits = fp_traits<T>::man_bits + 1; // +1 隐式位
	static constexpr int digits10 = static_cast<int>(digits * 0.30103);
	static constexpr int max_digits10 =
		static_cast<int>(digits * 0.30103 + 2);
	static constexpr bool is_iec559 =
		false; // 若严格符合 IEEE 754 可设为 true
	static constexpr bool is_bounded = true;
	static constexpr bool is_modulo = false;
	static constexpr bool traps = false;
	static constexpr bool tinyness_before = false;
	static constexpr float_round_style round_style = round_to_nearest;

	// 浮点数语义修正
	static constexpr c_check_f<T, U> lowest() noexcept
	{
		if constexpr (is_signed)
			return c_check_f<T, U>::get_max_normal(1);
		else
			return c_check_f<T, U>::get_zero(0);
	}
	static constexpr c_check_f<T, U> min() noexcept
	{
		return c_check_f<T, U>::get_min_normal(0); // 最小正规格化数
	}
	static constexpr c_check_f<T, U> max() noexcept
	{
		return c_check_f<T, U>::get_max_normal(0); // 最大正有限值
	}

	// 精度与舍入
	static constexpr c_check_f<T, U> epsilon() noexcept
	{
		// 1.0 与下一个可表示值的差：2^(-man_bits)
		c_check_f<T, U> one;
		one.m_set_sign(0);
		one.m_set_exp(fp_traits<T>::bias);
		one.m_set_man(0);

		c_check_f<T, U> eps;
		eps.m_set_sign(0);
		eps.m_set_exp(fp_traits<T>::bias - fp_traits<T>::man_bits);
		eps.m_set_man(0);
		return eps;
	}
	static constexpr c_check_f<T, U> round_error() noexcept
	{
		// 通常返回 0.5 * epsilon
		c_check_f<T, U> half_eps;
		half_eps.m_set_sign(0);
		half_eps.m_set_exp(fp_traits<T>::bias - fp_traits<T>::man_bits -
				   1);
		half_eps.m_set_man(0);
		return half_eps;
	}

	// 特殊值
	static constexpr c_check_f<T, U> infinity() noexcept
	{
		if constexpr (fp_traits<T>::has_inf)
			return c_check_f<T, U>::get_inf(0);
		else
			return c_check_f<T, U>();
	}
	static constexpr c_check_f<T, U> quiet_NaN() noexcept
	{
		if constexpr (fp_traits<T>::has_nan)
			return c_check_f<T, U>::get_qnan(0);
		else
			return c_check_f<T, U>();
	}
	static constexpr c_check_f<T, U> signaling_NaN() noexcept
	{
		if constexpr (fp_traits<T>::has_nan)
			return c_check_f<T, U>::get_snan(0);
		else
			return c_check_f<T, U>();
	}
	static constexpr c_check_f<T, U> denorm_min() noexcept
	{
		if constexpr (fp_traits<T>::has_denorm)
			return c_check_f<T, U>::get_min_denormal(0);
		else
			return min();
	}

	static constexpr bool has_infinity = fp_traits<T>::has_inf;
	static constexpr bool has_quiet_NaN = fp_traits<T>::has_nan;
	static constexpr bool has_signaling_NaN = fp_traits<T>::has_nan;
	static constexpr float_denorm_style has_denorm =
		fp_traits<T>::has_denorm ? denorm_present : denorm_absent;
	static constexpr bool has_denorm_loss = false;
};
}

void *addr_placeholder[10000];
int addr_top = 0;
template <typename T>
void load_float(std::vector<uint32_t> &insts, void *addr, uint64_t reg)
{
	int width = 0;
	for (int tmp = sizeof(typename T::storage_type); tmp != 1;
	     tmp = tmp / 2, width++)
		;
	addr_placeholder[addr_top++] = addr;
	uint64_t ptr = (uint64_t)(&(addr_placeholder[addr_top - 1]));
	load_reg(insts, ptr, 28);
	uint32_t inst = (0 << 20) + (28 << 15) + (width << 12) + (reg << 7) +
			(0b0000111);
	insts.push_back(inst);
}

template <typename T>
void store_float(std::vector<uint32_t> &insts, void *addr, uint64_t reg)
{
	int width = 0;
	for (int tmp = sizeof(typename T::storage_type); tmp != 1;
	     tmp = tmp / 2, width++)
		;
	addr_placeholder[addr_top++] = addr;
	uint64_t ptr = (uint64_t)(&(addr_placeholder[addr_top - 1]));
	load_reg(insts, ptr, 28);
	uint32_t inst = (0 << 25) + (reg << 20) + (28 << 15) + (width << 12) +
			(0 << 7) + (0b0100111);
	insts.push_back(inst);
}

/* Load a single floating-point register value from test data into the target
 * register by generating load_float instructions. Converts common format to
 * typed buffer and loads the memory address into the register. */
template <typename T>
void load_single_float(std::vector<uint32_t> &insts, c_data &cur_data,
		       const std::string &reg_name)
{
	if (cur_data.map_reg_index.find(reg_name) ==
	    cur_data.map_reg_index.end()) {
		INFO << "load multi float:" << reg_name
		     << " not found in data\n";
		return;
	}

	int vd = cur_data.map_reg_index[reg_name];
	cur_data.map_reg_typed_value[reg_name] = c_data::process_from_common<T>(
		cur_data.map_reg_value[reg_name]);

	void *data = cur_data.map_reg_typed_value[reg_name].get();

	/* Record float load for baremetal generation */
	if (global_baremetal_gen.is_enabled()) {
		global_baremetal_gen.record_float_load(
			vd, data, sizeof(typename T::storage_type));
	}

	load_float<T>(insts, data, vd);
}

/* Recursively load multiple floating-point registers from test data.
 * Each type in the template parameter pack corresponds to one register. */
template <typename T, typename... Rest>
void load_multi_float(std::vector<uint32_t> &insts, c_data &cur_data,
		      std::vector<std::string> reg_names)
{
	load_single_float<T>(insts, cur_data, *reg_names.begin());

	auto reg_names_rest = std::vector<std::string>(reg_names.begin() + 1,
						       reg_names.end());

	if constexpr (sizeof...(Rest) > 0) {
		load_multi_float<Rest...>(insts, cur_data, reg_names_rest);
	}
}

/* Save a single floating-point register's value before instruction execution by
 * generating store_float instructions into a zero-initialized buffer. */
template <typename T>
void store_single_preinst_float(std::vector<uint32_t> &insts, c_data &cur_data,
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
	store_float<T>(insts, data, vd);
}

/* Recursively save multiple floating-point registers' pre-instruction values. */
template <typename T, typename... Rest>
void store_multi_preinst_float(std::vector<uint32_t> &insts, c_data &cur_data,
			       std::vector<std::string> reg_names)
{
	store_single_preinst_float<T>(insts, cur_data, *reg_names.begin());

	auto reg_names_rest = std::vector<std::string>(reg_names.begin() + 1,
						       reg_names.end());

	if constexpr (sizeof...(Rest) > 0) {
		store_multi_preinst_float<Rest...>(insts, cur_data,
						   reg_names_rest);
	}
}

/* Save a single floating-point register's value after instruction execution by
 * generating store_float instructions into a zero-initialized buffer. */
template <typename T>
void store_single_afterinst_float(std::vector<uint32_t> &insts,
				  c_data &cur_data, const std::string &reg_name)
{
	if (cur_data.map_reg_index.find(reg_name) ==
	    cur_data.map_reg_index.end()) {
		INFO << __func__ << " : " << reg_name << " not found in data\n";
		return;
	}

	int vd = cur_data.map_reg_index[reg_name];
	cur_data.map_afterinst_typed_value[reg_name] = make_zero_buffer<T>(1);

	void *data = cur_data.map_afterinst_typed_value[reg_name].get();
	store_float<T>(insts, data, vd);
}

/* Recursively save multiple floating-point registers' post-instruction values. */
template <typename T, typename... Rest>
void store_multi_afterinst_float(std::vector<uint32_t> &insts, c_data &cur_data,
				 std::vector<std::string> reg_names)
{
	store_single_afterinst_float<T>(insts, cur_data, *reg_names.begin());

	auto reg_names_rest = std::vector<std::string>(reg_names.begin() + 1,
						       reg_names.end());

	if constexpr (sizeof...(Rest) > 0) {
		store_multi_afterinst_float<Rest...>(insts, cur_data,
						     reg_names_rest);
	}
}

/* Pack typed data into a compact bit-packed buffer with O(vl) complexity.
 * Uses a 64-bit sliding buffer to avoid undefined behavior.
 * Returns a SafeVoidPtr that manages the allocated memory. */
template <typename T>
SafeVoidPtr compact_pack_to_storage(void *data, uint64_t len)
{
	if (!data || len == 0)
		return nullptr;

	size_t elem_bits = fp_traits<typename T::value_type>::total_bits;
	size_t total_bits = elem_bits * len;
	size_t total_bytes = (total_bits + 7) / 8;
	// Add padding to avoid any off-by-one writes at the tail.
	size_t alloc_bytes = total_bytes + 16;

	void *storage_data = std::malloc(alloc_bytes);
	if (!storage_data)
		throw std::bad_alloc();
	std::memset(storage_data, 0, alloc_bytes);

	const T *typed_data = static_cast<const T *>(data);
	uint8_t *byte_ptr = static_cast<uint8_t *>(storage_data);
	// Use alloc_bytes (not total_bytes) as the hard boundary so the flush
	// loop never breaks early while bits_in_buf is still >= 64, which would
	// cause a shift-by->=64 UB on the next iteration.
	uint8_t *end_ptr = static_cast<uint8_t *>(storage_data) + alloc_bytes;

	uint64_t bit_buf = 0;
	size_t bits_in_buf = 0;

	for (size_t i = 0; i < len; ++i) {
		uint64_t val =
			static_cast<uint64_t>(typed_data[i].m_data.idata);
		if (elem_bits < 64) {
			val &= (1ULL << elem_bits) - 1;
		}

		// Flush bytes until there is room for the next element.
		// This loop must reduce bits_in_buf below (64 - elem_bits + 1)
		// before we do the shift below, otherwise the shift is UB.
		while (bits_in_buf + elem_bits > 64) {
			*byte_ptr++ = static_cast<uint8_t>(bit_buf & 0xFF);
			bit_buf >>= 8;
			bits_in_buf -= 8;
		}

		// Safe: bits_in_buf + elem_bits <= 64 is guaranteed by the loop above.
		bit_buf |= (val << bits_in_buf);
		bits_in_buf += elem_bits;

		// Drain fully-packed bytes.
		while (bits_in_buf >= 8) {
			*byte_ptr++ = static_cast<uint8_t>(bit_buf & 0xFF);
			bit_buf >>= 8;
			bits_in_buf -= 8;
		}
	}

	// Write any remaining bits (< 8) into the last byte.
	if (bits_in_buf > 0 && byte_ptr < end_ptr) {
		*byte_ptr = static_cast<uint8_t>(bit_buf & 0xFF);
	}

	return SafeVoidPtr(storage_data, [](void *p) { std::free(p); });
}

/* Unpack compact bit-packed storage data back into a typed array of T.
 * Reverses the 64-bit sliding buffer packing process.
 * Caller must provide a pre-allocated result_data buffer via SafeVoidPtr. */
template <typename T>
void compact_unpack_from_storage(const void *storage_data, uint64_t len,
				 void *result_data)
{
	if (!storage_data || len == 0 || !result_data)
		return;

	size_t elem_bits = fp_traits<typename T::value_type>::total_bits;
	size_t total_bytes = (elem_bits * len + 7) / 8;

	T *typed_result = static_cast<T *>(result_data);
	const uint8_t *byte_ptr = static_cast<const uint8_t *>(storage_data);

	uint64_t bit_buf = 0;
	size_t bits_in_buf = 0;
	size_t byte_idx = 0;

	for (size_t i = 0; i < len; ++i) {
		// Refill buffer until we have enough bits for one element
		while (bits_in_buf < elem_bits && byte_idx < total_bytes) {
			bit_buf |= (static_cast<uint64_t>(*byte_ptr++)
				    << bits_in_buf);
			bits_in_buf += 8;
			byte_idx++;
		}

		uint64_t val = bit_buf & ((1ULL << elem_bits) - 1);
		bit_buf >>= elem_bits;
		bits_in_buf -= elem_bits;

		typed_result[i].m_data.idata =
			static_cast<typename T::storage_type>(val);
	}
}

#endif
