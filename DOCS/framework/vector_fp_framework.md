# Vector Floating-Point Framework 文档

> 对应源文件：`common/f_common.h`（构建于 `common/framework.h` 之上），与 `common/v_common.h` 配合使用
> 主要服务对象：`vf/` 目录的浮点向量测试用例（`vfadd.vv`、`vfdiv.vf`、`vfcvt.x.f.v`、`vfclass.v` 等）
>
> 阅读本文前建议先阅读 [Common Framework 文档](./common_framework.md) 与 [Vector Integer Framework 文档](./vector_int_framework.md)。

---

## 1. 定位与职责

浮点向量测试面临的特殊问题：

1. RISC-V 向量浮点涉及**多种浮点格式**（fp16/bf16/fp32/fp64，以及 zvfbfmin/zvfbfwma、OCP 类扩展中的 e4m3/e5m2/e2m1 等非标准格式），C++ 原生类型无法统一表达；
2. 黄金模型需要**位级操控**浮点数（判定/构造 NaN、Inf、denormal，执行舍入与格式转换）；
3. 随机测试数据必须**偏向特殊值**（NaN/Inf/±0/denormal/边界正规数），否则几乎命中不了浮点corner case；
4. 非 2 的幂位宽格式（如 6-bit 的 e2m3/e3m2）在向量寄存器中**紧凑位打包**存放，加载/回存需要专门的 pack/unpack。

`f_common.h` 用一套类型系统统一解决上述问题，核心是三件套：**`fp_traits<T>`（格式描述）+ `c_check_f<T,U>`（位级操控包装类）+ `change_f`（通用格式转换模型）**。

包含方式（浮点测试实际仍只包含 `v_common.h`，因为 `v_common.h` 已包含 `f_common.h`）：

```cpp
#include "../common/v_common.h"   // 内部 #include "f_common.h"
```

---

## 2. 浮点异常标志常量

对应 RISC-V `fflags`/`frm` 语义的标志位（供黄金模型累积异常标志）：

```cpp
NV_FLAG (1<<4)  // Invalid Operation
DZ_FLAG (1<<3)  // Divide by Zero
OF_FLAG (1<<2)  // Overflow
UF_FLAG (1<<1)  // Underflow
NX_FLAG (1<<0)  // Inexact
```

---

## 3. 扩展整型与浮点格式类型

### 3.1 4-bit 整型：`int4` / `uint4`

用位域（`int8_t data:4`）实现的 4-bit 有符号/无符号整型，重载了 `+ - * > < != >>`、流式 IO 以及到各宽度整型（含 `__int128_t`）的隐式转换，服务于 zvbb/移位类指令中 SEW<8 的场景。配套 trait `is_my_integral_v<T>` 将二者纳入"整型"判定。

### 3.2 非标准浮点格式

以下类型均为**只含存储字段的轻量 struct**（本身不携带运算逻辑，语义由 `fp_traits` + `c_check_f` 赋予）：

| 类型 | 存储 | 格式 | 典型用途 |
|---|---|---|---|
| `e2m1` | uint8_t | 1-2-1 (4-bit, FP4) | OCP MX / zvfp 类扩展 |
| `e2m3` | uint8_t | 1-2-3 (6-bit) | 窄格式扩展 |
| `e3m2` | uint8_t | 1-3-2 (6-bit) | 窄格式扩展 |
| `e4m3` | uint8_t | 1-4-3 (8-bit, 无 Inf) | OCP FP8 |
| `e5m2` | uint8_t | 1-5-2 (8-bit) | OCP FP8 |
| `e8m0` | uint8_t | 0-8-0 (scale factor) | MX 共享指数 |
| `bf20` | uint32_t | 1-8-11 | 扩展格式 |
| `fp16` | — | `_Float16` 别名 | Zvfh（RISC-V GCC 不支持 ARM 的 `__fp16`，故用标准 `_Float16`） |
| `bf16` | — | `__bf16` 别名 | Zvfbfmin/Zvfbfwma |

---

## 4. 格式描述：`fp_traits<T>`

每种浮点格式通过 `FP_TRAIT_CONSTANTS` 宏特化 `fp_traits<T>`，提供编译期常量：

```cpp
sign_bits / exp_bits / man_bits      // 三段位宽
bias                                 // 指数偏移
has_inf / has_nan / has_zero / has_denorm   // 特殊值支持性
storage_type                         // 底层存储类型 (uint8/16/32/64_t)
max_unsigned_normal                  // 最大正规数的无符号位模式
total_bits = sign+exp+man            // 逻辑位宽（紧凑打包依据）
storage_bits = sizeof(storage)*8
```

已特化格式：`e2m1, e2m3, e3m2, e5m2, e4m3, e8m0, bf20, fp16, bf16, float, double`。主模板 `is_valid=false`，配合 `c_check_f` 构造函数中的 `static_assert` 在编译期拦截不支持的格式。

`is_builtin_fp_v<T>`：判定 T 是否为"内建可直接运算"的浮点类型（`std::is_floating_point_v` + 特化的 `fp16`/`bf16`）。

---

## 5. 位级操控包装类：`c_check_f<T, U>`

```cpp
template <typename T, typename U> class c_check_f {
    union { T fdata; U idata; } m_data;   // 浮点视图 / 整数位模式视图
};
```

- `T` = 值类型（`float`、`fp16`、`e4m3` …），`U` = 整数存储类型（`uint32_t`、`uint16_t`、`uint8_t` …）；
- 典型实例化：`c_check_f<float, uint32_t>`、`c_check_f<fp16, uint16_t>`、`c_check_f<e4m3, uint8_t>`；
- 构造时从 `fp_traits<T>` 拉取 sign/exp/man/bits/bias 到静态成员。

### 5.1 位段访问

`m_get_sign() / m_get_exp() / m_get_man()` 与 `m_set_sign() / m_set_exp() / m_set_man()`：按 `bits`（逻辑位宽）从 `idata` 提取/写入三段，是所有特殊值判定与构造的基础。

### 5.2 特殊值判定

| 方法 | 语义 |
|---|---|
| `m_is_nan()` | exp 全 1 且 man≠0 且位模式 > max_normal（`has_nan=false` 的格式恒 false） |
| `m_is_qnan()` / `m_is_snan()` | NaN 且尾数最高位为 1 / 为 0 |
| `m_is_inf()` | exp 全 1 且 man==0 |
| `m_is_positive_inf()` / `m_is_negtive_inf()` | ±Inf |
| `m_is_zero()` | exp==0 且 man==0 |
| `m_is_denormal()` | exp==0 且 man≠0 |

### 5.3 特殊值构造（静态工厂）

`get_snan(sign)` / `get_qnan(sign)` / `get_cnan(sign)`（规范 NaN）/ `get_nan(sign)`（随机 S/Q NaN）/ `get_inf(sign)` / `get_zero(sign)` / `get_max_denormal(sign)` / `get_min_denormal(sign)` / `get_denormal(sign)` / `get_max_normal(sign)` / `get_min_normal(sign)` / `get_normal(sign)`。

格式不支持对应特殊值时抛 `std::runtime_error`（如 e4m3 无 Inf），调用方通常 try-catch 回退到 `get_normal`。

### 5.4 随机生成钩子（对接公共层泛型随机管线）

- `get_class_rand()`：在 `total_bits` 位宽内纯随机位模式（`get_rand<c_check_f<...>>()` 自动转发到此）；
- `get_class_rand_bound(min, max)`：**十分之一分派的特殊值偏置随机**——0/1 返回边界、2 NaN、3 Inf、4 Zero、5 Normal、6 Denormal、7~9 位模式区间随机；每种特殊值构造失败时回退 Normal。这是浮点 fuzzing 命中率的关键（`register_type_with_random` 由此自动获得特殊值偏置）。

### 5.5 序列化钩子

`save_class_to_uint64()` / `load_class_from_uint64()`：`idata` 位模式与 `uint64_t` 互转，使 `c_data` 的统一 `uint64_t` 存储、JSON 录制/回放对浮点类型透明。

### 5.6 运算符

- `+ - *`：仅对 `is_builtin_fp_v` 类型生效（直接走 `fdata` 原生浮点运算），供黄金模型书写自然的表达式；
- `> <`：内建类型走原生比较；非内建格式实现**符号感知的位模式比较**（NaN 恒 false，负数按位模式反序）；
- `!=`：位模式比较（因此 NaN 语义可与硬件逐位对齐）；
- 流式 IO：按 `idata` 整数读写。

### 5.7 `std::numeric_limits<c_check_f<T,U>>` 特化

提供 `lowest()/min()/max()/epsilon()/round_error()/infinity()/quiet_NaN()/signaling_NaN()/denorm_min()` 及 `has_infinity` 等 trait，使黄金模型能像使用原生类型一样使用 `std::numeric_limits`（例如 `vfcvt.x.f.v` 用 `numeric_limits<Td>::max()` 表达饱和行为）。

---

## 6. 舍入与格式转换模型

### 6.1 `get_round_carry<T>(frm, sign, lsb, g, rs)`

按 RISC-V 舍入模式计算进位位：

| frm | 模式 | carry |
|---|---|---|
| 0 | RNE | `(g & rs) \| (lsb & g)` |
| 1 | RTZ | `0` |
| 2 | RDN | `sign & (g \| rs)` |
| 3 | RUP | `!sign & (g \| rs)` |
| 4 | RMM | `g` |
| 8 | ROD | `!lsb & (g \| rs)` |

### 6.2 `change_f<T, U>(frm, sat, src, fflag)` — 通用三向转换黄金模型

单一函数模板覆盖三类转换（由 `src`/`dst` 类型经 `if constexpr` 分派）：

1. **整型 → 浮点**：符号提取、最高有效位定位求指数、尾数截取（截断位非零置 `NX`）、溢出时按 `sat` 决定饱和到 `max_normal`（仅 e4m3/e5m2）或 `Inf`（`NX|OF`）；
2. **浮点 → 整型**：先经 double 中转，按 frm 六种模式显式舍入（RNE ties-to-even / RTZ / RDN / RUP / RMM ties-to-magnitude / ROD），NaN/Inf 置 `NV`，超出 int64 范围置 `OF` 并饱和；
3. **浮点 → 浮点**（宽化/窄化）：
   - NaN → 传播 canonical NaN（SNaN 置 `NV`；e2m1 特殊：返回 max_normal）；Inf/Zero 直传（sat 模式下 e4m3/e5m2 饱和）；
   - 宽化：指数换 bias、尾数左移补零（无损，含源 denormal 的规范化处理）；
   - 窄化：截取 `frac_cut` 与 guard/round/tail 位（非零置 `NX`），`get_round_carry` 求进位，逐段处理 exp 上溢（`OF`，sat 饱和）、最大指数区（e4m3/e2m1/e8m0 各有专属阈值）、进位溢出递增指数、下溢到 denormal（循环右移直到 exp≥0，置 `UF`）、最终舍入进位可能再次上溢的边界。

`fflag` 以引用累积异常标志，供需要校验 `fflags` 的用例使用。辅助函数 `low_bit(x)` 提取最低置位。

---

## 7. 紧凑位打包（非标准位宽格式的向量加载路径）

向量寄存器中元素按 `total_bits` 紧凑排列，而 C++ 缓冲按 `storage_type` 对齐存放，两者需要转换：

- `compact_pack_to_storage<T>(data, len)`：把 `c_check_f[]` 数组按 `fp_traits<T::value_type>::total_bits` 位宽打包成连续字节流（64-bit 滑动窗口缓冲，O(vl)，尾部加 16 字节 padding 防越界）；
- `compact_unpack_from_storage<T>(storage, len, result)`：逆过程，把字节流解回类型化数组。

`v_common.h` 中的向量加载/回存路径对**非基础类型**（即 `c_check_f`）自动走该通道：

- `load_single_vector<T>`：`compact_pack_to_storage` 生成 `storage_<reg>` 字节流 → 以 `load_vector<uint8_t>`（`vle8.v`，长度 = `total_bits*vl/8`）注入向量寄存器；
- `store_single_preinst_vector<T>` / `store_single_afterinst_vector<T>`：以 `vse8.v` 存出到 `storage_<reg>` 零初始化缓冲；
- `unpack_single_after_run<T>` / `unpack_multi_after_run<T, Rest...>`：**`run_instruction()` 之后、比对之前**，把 preinst/afterinst 的 storage 字节流解包回类型化 `c_check_f[]` 数组，供黄金模型与 `check_multi_error` 使用。

对基础类型（`uint32_t`、`float` 等 `std::is_fundamental_v` 为真）以上打包路径全部为 no-op，直接按 `sizeof(T)` 对应的 vle/vse 宽度存取。

---

## 8. 标量浮点寄存器（f 寄存器）操作

供 `.vf` 形式指令（如 `vfadd.vf`，rs1 为标量浮点寄存器）使用：

| 函数 | 生成内容 |
|---|---|
| `load_float<T>(insts, addr, reg)` | 地址物化到 x28 后发射 FLW/FLD/FLH（width 由 `sizeof(T::storage_type)` 推导，主操作码 `0b0000111`） |
| `store_float<T>(insts, addr, reg)` | 对应 FSW/FSD/FSH（主操作码 `0b0100111`） |
| `load_single_float<T>` / `load_multi_float<T, Rest...>` | 从 `c_data` 取操作数装入 f 寄存器 |
| `store_single_preinst_float<T>` / `store_multi_preinst_float<...>` | 执行前 f 寄存器快照 |
| `store_single_afterinst_float<T>` / `store_multi_afterinst_float<...>` | 执行后 f 寄存器回存 |

地址经全局 `addr_placeholder[]` 池中转（与向量访存共用，`reset_data()` 统一复位）。

---

## 9. 浮点向量测试用例的标准写法

以 `vf/vfadd.vv.cpp` 为范例，与整型用例的结构差异集中在**类型实例化**与**数据通路**两点：

### 9.1 字段表

与整型一致，仅 funct3 换为 `OPFVV`（.vf 形式为 `OPFVF`，且 vs1 字段改为 `RegClass::Float` 的 rs1）。

### 9.2 SEW 分发 —— 类型三要素

```cpp
switch (vector_cfg.sew) {
case sew_e8:  break;   // 浮点无 e8, 直接跳过
case sew_e16: per_run<c_check_f<fp16, uint16_t>, ...>;   break;
case sew_e32: per_run<c_check_f<float, uint32_t>, ...>;  break;
case sew_e64: per_run<c_check_f<double, uint64_t>, ...>; break;
}
```

Ts1/Ts2/Td 全部使用 `c_check_f<格式, 存储>` 实例；bf16 场景用 `c_check_f<bf16, uint16_t>`，转换/窄格式指令用 `c_check_f<e4m3, uint8_t>` 等。

### 9.3 per_run 数据通路（相对整型新增一步）

```
load_multi_vector / store_multi_preinst_vector   // c_check_f 自动走 compact pack + vle8 路径
vsetvli + csrrw(VSTART/VXRM) + 被测指令
store_multi_afterinst_vector
run_instruction(insts)
unpack_multi_after_run<Td, Ts1, Ts2, uint8_t>(...)   // ★ 浮点特有: storage 字节流解包
check_afterinst_illegal → save_multi_preinst_value_to_common
run_self_result → check_multi_error<Td> → reset_data
```

### 9.4 黄金模型写法

- **元素级运算**：内建格式（fp16/float/double）直接利用 `c_check_f::operator+/-/*`（如 `vfadd` 模型即 `vs1[j] + vs2[j]`）；需要更高精度中间结果时提升到 `double`/`long double` 计算后再经 `change_f` 落回目标格式；
- **特殊值分支**：先用 `m_is_nan()/m_is_inf()/m_is_zero()/m_is_denormal()` 显式处理 NaN 传播、Inf 规则，再走数值路径（`vfcvt.x.f.v` 即：NaN→整型最大值、±Inf→饱和、其余 `std::nearbyint` 后范围截断）；
- **格式转换类指令**（vfcvt 家族、vfncvt/vfwcvt、bf16 转换）：直接调用 `change_f<T,U>(frm, sat, src, fflag)`；
- **掩码/vstart**：与整型完全一致（undisturbed 策略，跳过 `j < vstart` 与 mask=0 元素）。

### 9.5 与整型用例的其余共同点

`check_illegal()` 同样基于 `VectorRegValidator` + `Presets`（STANDARD/WIDEN/NARROW，加宽浮点指令 `vfwadd` 用 `two_pow(vd)`）；main 循环、录制回放、SIGILL 校验、`check_multi_error` 比对逻辑均复用公共层，无浮点特有分支。

---

## 10. 小结：浮点框架的分层

| 层 | 内容 | 提供者 |
|---|---|---|
| 格式描述 | `fp_traits<T>` 编译期常量 | f_common.h |
| 位级语义 | `c_check_f<T,U>`：特殊值判定/构造、比较、随机偏置、序列化钩子 | f_common.h |
| 数值模型 | `get_round_carry` / `change_f`：舍入与三向转换 | f_common.h |
| 数据搬运 | compact pack/unpack + vle8/vse8 通道、f 寄存器 load/store | f_common.h + v_common.h |
| 向量语境 | `vector_cfg` / JIT 序列 / 合法性检查 / 比对 | v_common.h + framework.h |
