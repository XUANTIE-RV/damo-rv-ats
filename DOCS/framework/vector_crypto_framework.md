# Vector Crypto Framework 文档

> 对应源文件：`common/v_crypto_common.h`（构建于 `common/v_common.h` 之上）
> 主要服务对象：`zvkned/`、`zvknh/`、`zvksed/`、`zvksh/`、`zvkg/`、`zvkgs/` 等基于**元素组（Element Group）**的向量密码扩展测试
>
> 阅读本文前建议先阅读 [Common Framework 文档](./common_framework.md) 与 [Vector Integer Framework 文档](./vector_int_framework.md)。

---

## 1. 定位与职责

向量密码扩展（Zvk*）与整型向量运算的本质差异：

1. **操作粒度不是单个元素，而是 128-bit 元素组（EGW=128）**——AES/SM4 状态、GHASH 中间值都以 4×e32 为一个逻辑单元；
2. **约束体系不同**——除寄存器对齐/重叠外，还要求 `SEW==e32`、`vl` 与 `vstart` 是 EGS（元素组大小，通常为 4）的整数倍、LMUL 能容纳至少一个 128-bit 元素组；
3. **黄金模型是密码算法本身**——S-box、ShiftRows、MixColumns、GF(2^128) 乘法等，逻辑复杂但与被测指令一一对应，适合沉淀到公共层复用；
4. **指令编码高度模板化**——密码指令全部 unmasked（vm=1 固定）、funct6 固定、部分字段是常数，操作数组合只有 `vd+vs2`、`vd+vs2(标量组)+vs1` 等少数几种，可以把整个"单轮执行驱动"抽成公共函数。

`v_crypto_common.h` 针对以上四点提供了**算法模型库 + 密码专用合法性检查 + 模板化 per-run 驱动器**三层设施。

包含方式：

```cpp
#include "../common/v_crypto_common.h"   // 内部 #include "v_common.h"
```

> 边界说明：`zvbb/`（向量位操作，`vrol/vror/vbrev/vclz/vctz/vandn/vwsll` 等）与 `zvbc/`（向量无进位乘法）虽然同属"Vector Crypto"大类，但它们是**逐元素**运算，测试用例直接包含 `v_common.h`，复用整型框架的 per_run 模式与 `ror/rol/brev` 原语，不使用本层的元素组设施。本文档描述的是 Zvk* 元素组类扩展的专用框架。

---

## 2. 元素组模型与密码专用合法性检查

### 2.1 LMUL / 元素组容量

| 函数 | 语义 |
|---|---|
| `crypto_effective_lmul_bits()` | 当前 LMUL 下一个向量寄存器组的实际位容量：`lmul<4` 时 `VLEN<<lmul`，分数 LMUL 时 `VLEN>>(8-lmul)` |
| `crypto_lmul_egw_ok(egw)` | 寄存器组能否容纳至少一个 `egw` 位（典型 `egw=128`）的元素组——LMUL 过小（如 mf8 + VLEN=128）时指令非法 |
| `crypto_vector_group_regs()` | 当前 LMUL 下寄存器组占用的物理寄存器数（转发 `VectorRegValidator::get_reg_group_count`） |

### 2.2 寄存器约束

| 函数 | 语义 |
|---|---|
| `crypto_no_overlap(a, asize, b, bsize)` | 两个寄存器组不重叠（转发 `VectorRegValidator::is_overlapped` 取反） |
| `crypto_scalar_eg_no_overlap(vd, vs2, egw)` | `.vs` 形式指令：vs2 只承载**一个标量元素组**（占 `ceil(egw/VLEN)` 个寄存器），校验其与 vd 寄存器组不重叠且 `vs2+size ≤ 32` |
| `crypto_check_vd_align_range(vd)` | vd 的对齐与编号范围（转发 `VectorRegValidator::validate`，仅开 `check_align`，不查重叠） |
| `crypto_check_egs(egs)` | `vl % egs == 0 && vstart % egs == 0`——vl/vstart 必须是元素组大小的整数倍 |

### 2.3 典型 `check_illegal()` 组合

以 `zvkned/vaesef.vv.cpp` 为例，密码指令的非法判定是"通用对齐 + 三条密码约束"的叠加：

```cpp
int illegal = !(VectorRegValidator::validate(
                    VregOperand::one_pow(vd),
                    { VregOperand::one_pow(vs2) }, 1,
                    { .check_align = true, .check_overlap = false })) ||
              vector_cfg.sew != sew_e32 ||     // 密码指令固定 e32
              !crypto_check_egs(4) ||          // vl/vstart 是 EGS=4 的倍数
              !crypto_lmul_egw_ok(128);        // LMUL 装得下 128-bit 元素组
```

要点：vm 字段在编码表中是 `is_fix=true, default=1`（恒 unmasked），因此 validate 时 vm_bit 传 1 且通常关闭 `check_overlap`/`check_vm`；`.vs` 形式则改用 `crypto_scalar_eg_no_overlap` 校验标量源。

---

## 3. 元素组数据搬运

黄金模型在 `uint32_t` 数组（向量寄存器按 e32 视角的快照）上以组为单位读写：

```cpp
using eg128 = std::array<uint32_t, 4>;   // 一个 128-bit 元素组

template <typename T, size_t N>
std::array<T, N> crypto_load_group(const T *data, int group_idx);   // 取第 group_idx 组
template <typename T, size_t N>
void crypto_store_group(T *data, int group_idx, const std::array<T, N> &value);  // 写回
```

另有 GF(2^128) 专用的组装载/回存（zvkg 使用，`gf128_t{hi,lo}` 与 4×uint32 的小端字序互转）：

```cpp
gf128_t load_eg128_from_e32(const uint32_t *data, int group_idx);
void    store_eg128_to_e32(uint32_t *data, int group_idx, const gf128_t &x);
```

---

## 4. 模板化单轮驱动器（crypto_per_run 家族）

密码测试的 per_run 高度一致，公共层直接提供成品驱动器。三者签名形态相同：

```cpp
template <typename Td, typename SelfFunc>
int crypto_per_run_vd_vs2(int it, c_cfg &cur_cfg, c_data &cur_data,
                          const std::vector<InstField> &inst_fields,
                          std::function<int(c_data&)> illegal_func,
                          SelfFunc self_func);
```

| 驱动器 | 操作数组合 | 典型指令 |
|---|---|---|
| `crypto_per_run_vd_vs2<Td>` | vd、vs2 均为完整向量组（各 `vector_cfg.len` 个元素） | `vaesef.vv`、`vsm4r.vv`、`vsha2ms.vv` |
| `crypto_per_run_vd_scalar_vs2<Td>` | vd 为完整向量组，vs2 为**单个标量元素组**（`scalar_len`，默认 4×e32，以 `lmul_m1` 加载） | `.vs` 形式：`vaesef.vs`、`vaesz.vs` |
| `crypto_per_run_vd_vs2_vs1<T>` | vd、vs2、vs1 三个完整向量组 | `vaeskf2.vi`、`vsm4k`、`vgmul`（三操作数形态） |

驱动器内部完成的整轮流程（调用方只需提供编码表、illegal_func、黄金模型三件东西）：

```
set_value_from_inst                     // 解码指令字段
→ 随机模式: register_type_with_random<Td> 生成操作数 + set_value_to_cfg + (record 时入 global_cfg)
  回放模式: set_value_from_cfg
→ global_flag_ptr->illegal = illegal_func(cur_data)
→ JIT 序列: save_context
            load_multi_vector / store_multi_preinst_vector
            vsetvli_lmul_sew + csrrw(VSTART) + csrrw(VXRM)
            insts.push_back(vector_cfg.inst)      // 被测指令
            store_multi_afterinst_vector({"vd"})
            restore_context
→ run_instruction(insts)
→ check_afterinst_illegal()               // ≥0 直接返回
→ save_multi_preinst_value_to_common
→ self_func(cur_data)                     // 黄金模型
→ check_multi_error<Td>({"vd"}, {len})    // 逐元素比对
→ reset_data()
```

与整型手写 per_run 的差异：不加载/不快照 `vm`（密码指令恒 unmasked）、不执行 `vzero_all`、比对目标只有 `vd`。

### 4.1 main 辅助

| 函数 | 用途 |
|---|---|
| `crypto_main_begin(argc, argv, __FILE__)` | 一行完成 `filename/datafile/outfile` 设置 + `init_program` + `init_vector_program` |
| `crypto_report_error(cur_cfg, has_error)` | 出错时统一打印 CSR JSON 与 `vector_cfg` 上下文 |

由此密码测试的 `main()` 收敛为极简形态：

```cpp
int main(int argc, char *argv[])
{
    crypto_main_begin(argc, argv, __FILE__);
    for (it = 0; it < run_times; it++) {
        ...
        init_vector_cfg(it, cur_cfg, cur_data, check_illegal, vop_inst_fields);
        int has_error = crypto_per_run_vd_vs2<uint32_t>(
            it, cur_cfg, cur_data, vop_inst_fields, check_illegal,
            run_self_result<uint32_t>);
        crypto_report_error(cur_cfg, has_error);
        if (has_error && early_stop) break;
    }
    end_program(it);
}
```

注意：SEW 已在 `check_illegal` 中锁定为 e32，因此**无需按 SEW 分发模板**，元素类型恒为 `uint32_t`。

---

## 5. 密码算法黄金模型库

### 5.1 位操作原语

| 函数 | 语义 |
|---|---|
| `crypto_rol32(x, n)` | 32-bit 循环左移 |
| `crypto_rotr<T>(x, n)` | 任意宽度（32/64-bit）循环右移，SHA-2 用 |
| `crypto_rev8_32(x)` | 32-bit 字节序反转 |

### 5.2 AES（Zvkned）

数据表与 32-bit 字级原语：

- `aes_sbox_fwd_table[256]` / `aes_sbox_inv_table[256]`：正/逆 S-box；
- `aes_xt2(x)`：GF(2^8) 乘 2（含 0x1b 约减）；`aes_gfmul(x, y)`：GF(2^8) 通用乘法；
- `get_byte(x, idx)` / `pack_bytes(b3,b2,b1,b0)`：字/字节拆装；
- `aes_subword_fwd/inv`、`aes_mixcolumn_fwd/inv`：字级 SubWord、MixColumn；
- `aes_rotword`、`aes_decode_rcon`：密钥扩展用 RotWord 与轮常量 Rcon。

128-bit 状态级（`eg128`）组合：

- `aes_subbytes_fwd/inv`、`aes_shift_rows_fwd/inv`、`aes_mixcolumns_fwd/inv`、`eg128_xor`。

**指令级模型**（黄金模型直接调用）：

| 模型函数 | 对应指令 | 语义 |
|---|---|---|
| `vaesef_model(state, rkey)` | vaesef | SubBytes → ShiftRows → AddRoundKey |
| `vaesem_model(state, rkey)` | vaesem | SubBytes → ShiftRows → MixColumns → AddRoundKey |
| `vaesdf_model(state, rkey)` | vaesdf | InvShiftRows → InvSubBytes → AddRoundKey |
| `vaesdm_model(state, rkey)` | vaesdm | InvShiftRows → InvSubBytes → AddRoundKey → InvMixColumns |

### 5.3 SM4（Zvksed）

- `sm4_sbox_table[256]`：SM4 S-box；
- `sm4_subword(x)`：字级 S-box 代换（τ 变换）；
- `sm4_linear_transform(x, s)`：轮函数线性变换 L（`x ^ s ^ rol(s,2) ^ rol(s,10) ^ rol(s,18) ^ rol(s,24)`）；
- `sm4_round_key(x, s)`：密钥扩展线性变换 L'（rol 13/23）；
- `vsm4r_model(x, rk)`：**指令级模型**——一次执行 4 轮 SM4 加/解密，输出 `{x4,x5,x6,x7}`。

### 5.4 SM3（Zvksh）

- `sm3_p0(x)` / `sm3_p1(x)`：置换函数 P0（rol 9/17）、P1（rol 15/23）；
- `sm3_ff(x,y,z,j)` / `sm3_gg(x,y,z,j)`：布尔函数 FF/GG（j≤15 与 j>15 分段）；
- `sm3_t(j)`：常量 T（0x79cc4519 / 0x7a879d8a）；
- `sm3_w(m16,m9,m3,m13,m6)`：消息扩展公式。

### 5.5 SHA-2（Zvknh，32/64-bit 双宽度模板）

- `sha_sum0<T>` / `sha_sum1<T>`：大 Σ0/Σ1（`sizeof(T)==4` 走 SHA-224/256 旋转常量，否则 SHA-384/512）；
- `sha_sig0<T>` / `sha_sig1<T>`：小 σ0/σ1（消息扩展）；
- `sha_ch<T>` / `sha_maj<T>`：Ch / Maj 布尔函数；
- `sha2_round<T>(a..h, w)`：**一轮压缩函数状态更新**，`vsha2ms`/`vsha2c[hl]` 的模型由其组合而成。

### 5.6 GF(2^128) / GCM（Zvkg）

```cpp
struct gf128_t { uint64_t hi; uint64_t lo; };
```

- `gf128_zero()` / `gf128_xor(a, b)`：零元与加法（XOR）；
- `brev8_byte(x)` / `gf128_brev8(x)`：按字节位反转（GCM 位序约定）；
- `gf128_get_bit(x, i)` / `gf128_get_msb(x)`：取位/取最高位；
- `gf128_shl1_inplace(x)` / `gf128_xor_low8_inplace(x, c)`：左移 1 位与低 8 位 XOR（约减用）；
- `gf128_mul_gcm(y, h)`：**GCM 域乘黄金模型**——128 轮"按 y 的位选择性 XOR h，h 左移、MSB 溢出时 XOR 0x87 约减"，对应 `vgmul.vv`；`vghsh.vv` 在其上叠加 `Y ^ (X*H)` 的 hash 步骤。

---

## 6. 密码测试用例的标准写法

以 `zvkned/vaesef.vv.cpp` 为范例，四步：

### 6.1 编码字段表——固定字段占多数

```cpp
const std::vector<InstField> vop_inst_fields = {
    { 31, 26, 0x28, true,  RegClass::NotReg, "funct6" },       // 指令身份, 固定
    { 25, 25, 0x01, true,  RegClass::NotReg, "vm" },           // 恒 unmasked
    { 24, 20, 0x00, false, RegClass::Vector, "vs2" },          // 随机
    { 19, 15, 0x03, true,  RegClass::NotReg, "const_19_15" },  // 编码常数
    { 14, 12, OPMVV, true, RegClass::NotReg, "funct3" },
    { 11,  7, 0x00, false, RegClass::Vector, "vd" },           // 随机
    {  6,  0, 0x77, true,  RegClass::NotReg, "opcode" }        // OP-P (0b1110111)
};
```

只有 vd/vs2 随机，因此 `--illegal-percent` 的非法路径主要由寄存器对齐/重叠与 SEW/EGS/LMUL 约束触发。

### 6.2 `check_illegal()`

通用对齐校验 + 密码三约束（见 §2.3）。

### 6.3 黄金模型——按元素组遍历

```cpp
template <typename Td> int run_self_result(c_data &cur_data)
{
    // preinst 快照取 uint32_t 视图, selfcheck 以 preinst vd 初始化(undisturbed)
    for (int i = vector_cfg.vstart / 4; i < vector_cfg.len / 4; ++i) {
        eg128 state = crypto_load_group<uint32_t, 4>(vd_data, i);
        eg128 rkey  = crypto_load_group<uint32_t, 4>(vs2_data, i);
        crypto_store_group<uint32_t, 4>(selfcheck_data, i,
                                        vaesef_model(state, rkey));
    }
}
```

循环边界体现 EGS 语义：从 `vstart/4` 组起、到 `len/4` 组止（§2.2 已保证两者整除）。

### 6.4 `main()`

`crypto_main_begin` + `init_vector_cfg` + 一个 `crypto_per_run_*` 调用 + `crypto_report_error`（见 §4.1），无 SEW switch。

---

## 7. 小结：密码框架的分层

| 层 | 内容 | 提供者 |
|---|---|---|
| 算法模型 | AES/SM4/SM3/SHA-2/GF(2^128) 原语与指令级 model 函数 | v_crypto_common.h |
| 元素组语义 | `eg128`、`crypto_load_group/store_group`、EGW/EGS 容量检查 | v_crypto_common.h |
| 合法性约束 | `crypto_check_egs` / `crypto_lmul_egw_ok` / `crypto_scalar_eg_no_overlap` / `crypto_check_vd_align_range` | v_crypto_common.h |
| 执行驱动 | `crypto_per_run_vd_vs2` / `_vd_scalar_vs2` / `_vd_vs2_vs1`、`crypto_main_begin`、`crypto_report_error` | v_crypto_common.h |
| 向量语境 | `vector_cfg` / JIT 序列 / SIGILL / 比对 | v_common.h + framework.h |
