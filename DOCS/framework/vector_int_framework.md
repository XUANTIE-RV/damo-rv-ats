# Vector Integer Framework 文档

> 对应源文件：`common/v_common.h`（构建于 `common/framework.h` 之上）
> 主要服务对象：`vx/`（整型向量运算）、`vls/`（向量加载/存储）等目录的测试用例
>
> 阅读本文前建议先阅读 [Common Framework 文档](./common_framework.md)。

---

## 1. 定位与职责

`v_common.h` 是**向量类别公共层**，为所有基于 RVV 的测试（整型、浮点、密码）提供向量语境下的配置、指令发射与合法性检查设施。整型向量测试（`vx/` 目录，如 `vadd.vv`、`vdivu.vx`、`vwadd.vv`、`vcompress.vm` 等）直接以它作为唯一公共头文件：

```cpp
#include "../common/v_common.h"
```

层次关系：

```
framework.h  ──►  v_common.h  ──►  整型向量测试 (vx/, vls/, zvabd/ ...)
                       ▲
                       ├── f_common.h ──► 浮点向量测试 (vf/)
                       └── v_crypto_common.h ──► 密码向量测试 (zvk*/, zvbb/ ...)
```

---

## 2. 向量基础常量

### 2.1 指令编码常量

| 常量 | 值 | 含义 |
|---|---|---|
| `opcode_vector` | `0x57` | OP-V 主操作码 |
| `OPIVV` | 0x0 | 整型 vector-vector |
| `OPFVV` | 0x1 | 浮点 vector-vector |
| `OPMVV` | 0x2 | 掩码/密码类 vector-vector |
| `OPIVI` | 0x3 | 整型 vector-immediate |
| `OPIVX` | 0x4 | 整型 vector-scalar |
| `OPFVF` | 0x5 | 浮点 vector-scalar |
| `OPMVX` | 0x6 | 掩码/密码类 vector-scalar |
| `OPCFG` | 0x7 | vset* 配置类 |

加载/存储指令另用 `0b0000111`（LOAD-FP）与 `0b0100111`（STORE-FP）主操作码，由框架函数内部处理。

### 2.2 向量 CSR

| 宏 | 地址 | 说明 |
|---|---|---|
| `CSR_VSTART` | 0x008 | 起始元素索引 |
| `CSR_VXSAT` | 0x009 | 定点饱和标志 |
| `CSR_VXRM` | 0x00A | 定点舍入模式 |
| `CSR_VCSR` | 0x00F | 向量控制状态（vxrm[2:1] + vxsat[0]） |
| `CSR_VL` | 0xC20 | 向量长度 |
| `CSR_VTYPE` | 0xC21 | 向量类型 |
| `CSR_VLENB` | 0xC22 | 向量寄存器字节长度 |

全局变量 `vlenb` 在 `init_vector_program()` 中通过 `CSRR(CSR_VLENB)` 读取一次，之后所有元素数/缓冲区大小计算都基于它。

### 2.3 LMUL / SEW 编码

```cpp
lmul_m1=0  lmul_m2=1  lmul_m4=2  lmul_m8=3
lmul_mf8=5 lmul_mf4=6 lmul_mf2=7        // 4 为保留值
sew_e8=0   sew_e16=1  sew_e32=2  sew_e64=3
```

配套 `lmul_str()` / `sew_str()` / `vxrm_str()` / `vxsat_str()` / `vta_str()` / `vma_str()` 将编码翻译为可读字符串用于日志输出。

---

## 3. 向量配置：`c_vector_cfg`

全局单例 `vector_cfg` 描述**当前迭代的向量执行语境**，字段：`lmul`、`sew`、`vma`、`vta`、`len`(vl)、`vstart`、`vxrm`、`vxsat`、`inst`（被测指令编码）、`rs1_data`。

三个核心方法：

### 3.1 `m_rand()` — 随机合法配置

- `sew`、`lmul` 用边界偏置随机（`get_rand_bound`）生成；
- **循环重试保证约束**：跳过保留值 `lmul==4`；保证 `LMUL*VLEN >= SEW`（分数 LMUL 时向量寄存器至少装下一个元素）；
- `vma = vta = vxsat = 0`（undisturbed / wrapping 固定策略）；`vxrm` 随机 0~3；
- `len`(vl) 在 `[0, VLMAX]` 内边界偏置随机；`vstart` 固定为 0。

### 3.2 `set_value(cfg)` / `get_value()` — 与 `c_cfg` 互转

- `set_value`：从 `c_cfg.CSR` 的 `vtype`（lmul=[2:0], sew=[5:3], vta=bit6, vma=bit7）、`vcsr`（vxrm=[2:1], vxsat=bit0）、`vl`、`vstart` 解码——回放模式使用；
- `get_value`：反向打包为 `c_cfg`（`vtype = (vta<<6)|(vma<<7)|(sew<<3)|lmul`）——录制模式使用。

### 3.3 `print()` / `operator<<` — 调试输出

打印 `vlenb/vstart/vl/lmul/sew/vta/vma/vxrm/vxsat` 的编码值与可读名称。

---

## 4. 每轮迭代的配置入口：`init_vector_cfg()`

```cpp
void init_vector_cfg(int it, c_cfg &cur_cfg, c_data &cur_data,
                     std::function<int(c_data&)> check_illegal,
                     const std::vector<InstField> vop_inst_fields);
```

- **Fuzzing 模式**：`vector_cfg.m_rand()` 随机向量语境 → `get_inst(vop_inst_fields)` 随机指令编码 → `set_value_from_inst()` 解码字段；若指定了 `--illegal-percent`，按目标比例反复重新生成编码（最多 `max_retry` 次）直至 `check_illegal()` 判定非法；
- **回放模式**：直接取 `global_cfg[it]` 并解码。

`init_vector_program()` 则是向量测试的一次性初始化：读 `vlenb`、建立全局 `c_flag`（`vector_flag`）。

---

## 5. 向量 JIT 指令发射原语

以下函数都向 `std::vector<uint32_t>` 指令队列追加机器码（配合公共层的 `run_instruction()` 执行）：

| 函数 | 生成内容 |
|---|---|
| `vsetvli_lmul_sew(insts, lmul, sew, vl, vma, vta)` | 先 `load_reg` 把 vl 装入 x28，再发射 `vsetvli x28, x28, vtypei`；vl 值经 `vl_placeholder[]` 池中转 |
| `vmv_v_i(insts, imm, vd)` | `vmv.v.i vd, imm` |
| `vzero_all(insts)` | `vsetvli`(m1/e8/vlenb) 后对 v0~v31 逐一 `vmv.v.i 0`，清空整个向量寄存器堆（隔离上一轮残留） |
| `load_vector<T>(insts, vd, addr, lmul, vl)` | `vsetvli` + 单元步进加载 `vle{8,16,32,64}.v`；EEW/width 编码由 `sizeof(T)` 自动推导；**vd≥32 时拒绝发射**（防止 vd 位溢出污染 width 字段产生畸形编码） |
| `store_vector<T>(insts, vd, addr, lmul, vl)` | `vsetvli` + 单元步进存储 `vse{8,16,32,64}.v`，同样的 vd≥32 防护 |
| `load_addr_to_gpr(insts, mem_ptr, gpr, mem_size)` | 将内存地址物化到 GPR（供 load/store 类指令的 rs1 使用），地址经 `addr_placeholder[]` 池中转 |
| `reset_data()` | 复位 `vl_top` / `val_top` / `addr_top` 三个占位符池下标（每轮迭代结束后调用） |

寄存器级批量操作（变参模板递归展开，每个寄存器可用各自的元素类型 T、LMUL、VL）：

| 函数 | 用途 |
|---|---|
| `load_single_vector<T>` / `load_multi_vector<T, Rest...>` | 把 `c_data` 中的操作数加载进真实向量寄存器；`vm` 处于 unmasked 时自动跳过；**vl=0 时按 1 个元素加载**（`vmv.x.s`/`vcpop.m` 等在 vl=0 时仍读 vs2[0]） |
| `store_single_preinst_vector<T>` / `store_multi_preinst_vector<...>` | 执行前将寄存器内容 `vse` 到零初始化缓冲（preinst 快照） |
| `store_single_afterinst_vector<T>` / `store_multi_afterinst_vector<...>` | 执行后将目标寄存器 `vse` 到缓冲（afterinst 硬件真值） |

---

## 6. 合法性检查体系（本层核心）

整型向量测试的 `check_illegal()` 依赖一套声明式的寄存器组约束校验体系。

### 6.1 `VregOperand` — 带 EMUL 比例的操作数描述

```cpp
struct VregOperand { int idx; int emul_ratio; };  // idx=-1 表示不参与
```

`emul_ratio` 是相对当前 `vector_cfg.lmul` 的 EMUL 偏移：`0`=1×、`1`=2×、`-1`=0.5×、`2`=4×、`-2`=1/4×……提供具名工厂：

| 工厂 | 语义 | 典型场景 |
|---|---|---|
| `none()` | 禁用（idx=-1） | 无该操作数 |
| `one_pow(idx)` | EMUL = LMUL | 普通 .vv/.vx 指令 |
| `two_pow(idx)` / `quad(idx)` / `eight_pow(idx)` | EMUL = 2/4/8×LMUL | 加宽指令的 vd（如 `vwadd.vv` 用 `two_pow(vd)`） |
| `half(idx)` / `quarter(idx)` / `one_of_eight(idx)` | EMUL = 1/2、1/4、1/8×LMUL | 缩窄指令的源、掩码结果 |
| `mask_dest(idx)` | EMUL = 1/LMUL | 掩码输出（`vms*`、`vm*` 类） |
| `custom(idx, ratio)` | 任意比例 | 特殊指令 |

### 6.2 `ValidationConfig` 与 `Presets`

```cpp
struct ValidationConfig {
    bool check_align;        // 寄存器组对齐 + 越界检查
    bool check_overlap;      // 目的/源重叠规则检查
    bool check_vm;           // vd 与 v0 掩码冲突检查
    bool check_vstart;       // 要求 vstart==0
    bool force_no_overlap;   // 相同编号也视为非法重叠(gather 类)
    ArchConstraint arch_constraint;  // NONE / WIDEN / NARROW
    std::function<...> custom_checker;  // 自定义附加校验
};
```

预置组合（`namespace Presets`）：

| 预置 | 内容 |
|---|---|
| `Presets::STANDARD` | align + overlap + vm |
| `Presets::WIDEN` | STANDARD + 加宽约束（LMUL≠m8、SEW<e64） |
| `Presets::NARROW` | STANDARD + 缩窄约束 |
| `Presets::GATHER` | STANDARD + `force_no_overlap`（索引加载类禁止 vd 与源同组重叠） |

### 6.3 `VectorRegValidator::validate()`

```cpp
static bool validate(const std::optional<VregOperand> &dst,
                     const std::vector<VregOperand> &sources,
                     int vm_bit, ValidationConfig config);
```

按序执行以下检查，任一失败即返回 false（⇒ 被测指令应为非法）：

1. **vstart 检查**（可选）；
2. **vm 检查**：masked（vm_bit=0）时 vd 不得为 v0（`is_legal_vm`）；
3. **架构约束**：WIDEN/NARROW 时 LMUL≠m8、SEW 不能是 e64（保证 2×SEW/2×EMUL 可表示）；
4. **vsetvli 语境检查**（`check_vsetvli`）：LMUL≠保留值 4、SEW<4；分数 LMUL 时 `LMUL*VLEN ≥ SEW` 且 `LMUL*XLEN ≥ SEW`；
5. **对齐与范围检查**：每个操作数寄存器组必须按 EMUL 对齐（`is_reg_aligned`），组内寄存器数 ≤8 且 `idx+size ≤ 32`；
6. **重叠检查**（`check_no_overlap_rule`，实现 RVV spec 的目的/源重叠规则）：
   - dst 组 > src 组（加宽）：仅允许 dst 与 src 的**最低编号部分**重叠（且 src EMUL≥1 时按编号方向判定）；
   - dst 组 < src 组（缩窄）：允许 `dst_idx == src_idx`（与最低部分重叠），其余重叠非法；
   - 等大：任何重叠非法（`force_no_overlap` 时同编号也非法）；
7. **自定义检查**（可选）。

### 6.4 独立辅助函数

| 函数 | 用途 |
|---|---|
| `is_align(vd, lmul)` | 寄存器号是否按 LMUL 对齐（分数 LMUL 恒真） |
| `is_legal_vm(vm_bit, vd)` | masked 时 vd≠v0 |
| `is_overlapped(astart, asize, bstart, bsize)` | 两个寄存器组区间是否重叠 |
| `is_legal_nf(vd, nf, lmul)` | 段访存：`EMUL*NFIELDS ≤ 8` 且寄存器号不越过 v31 |
| `vext_check_store(vd, nf, eew)` | load/store 类指令 EMUL 推导（`emul = eew - sew + lmul`）+ 范围 [-3,3] + 对齐 + nf 检查 |
| `vext_check_load(vd, nf, eew, vm_bit)` | store 的全部规则 + 掩码目的组不得与 v0 重叠 |
| `lmul_pow(lmul, ratio)` | EMUL 编码计算（模 8 回绕） |

---

## 7. 黄金模型辅助（整型）

`v_common.h` 为整型黄金模型提供的位运算原语：

- `ror<T>(x, shift)` / `rol<T>(x, shift)`：SEW 位宽内循环右移/左移；
- `brev<T>(x)`：SEW 位宽内按位反转；
- `sign_extend(val, width)`（公共层）：符号扩展。

整型测试的黄金模型直接用 C++ 原生整型语义（隐式回绕、提升）逐元素计算，例如 `vadd.vv` 的模型即 `static_cast<Td>(vs1[j]) + vs2[j]`。

---

## 8. 非法指令执行路径：`check_afterinst_illegal()`

`run_instruction()` 返回后调用，三态返回值：

| 返回 | 含义 | 调用方动作 |
|---|---|---|
| `0` | 本轮为非法指令且 SIGILL 行为符合预期（或已记录不一致错误） | 直接进入下一轮 |
| `1` | 非法校验不一致且 `early_stop` | 终止测试 |
| `-1` | 指令合法 | 继续做黄金模型比对 |

内部逻辑：期望非法 → `check_if_sigill()` 验证信号确实触发；未期望却收到 SIGILL → 置 `check_illegal_error` 报 ERROR。

---

## 9. 整型向量测试用例的标准写法

以 `vx/vadd.vv.cpp` 为范例，五个组成部分：

### 9.1 字段表（编码即文档）

```cpp
const std::vector<InstField> vop_inst_fields = {
    { 31, 26, 0x00, true,  RegClass::NotReg, "funct6" },   // vadd.vv = 0b000000
    { 25, 25, 0x00, false, RegClass::NotReg, "vm" },       // 随机 masked/unmasked
    { 24, 20, 0x00, false, RegClass::Vector, "vs2" },
    { 19, 15, 0x00, false, RegClass::Vector, "vs1" },
    { 14, 12, OPIVV, true, RegClass::NotReg, "funct3" },
    { 11,  7, 0x00, false, RegClass::Vector, "vd" },
    {  6,  0, opcode_vector, true, RegClass::NotReg, "opcode" }
};
```

### 9.2 `check_illegal()`

从 `cur_data.map_reg_index` 取出 vm/vs2/vs1/vd，调用 `VectorRegValidator::validate(...)`（普通指令用 STANDARD 组合，加宽指令把 dst 换成 `VregOperand::two_pow(vd)` 并用 WIDEN 约束），取反后 `print_illegal_status()` 并返回。

### 9.3 `run_self_result<Ts1, Ts2, Td>()` — 黄金模型

统一模式：

1. 取 preinst 类型化缓冲（`map_preinst_typed_value`）与 `vm` 掩码数据；
2. 用 preinst 的 vd 值初始化 selfcheck 缓冲（**undisturbed 策略**：非活动元素保留旧值）；
3. 逐元素循环 `[0, vl)`，跳过 `j < vstart` 与掩码为 0 的元素（掩码按 `row=j/8, col=j%8` 位寻址）；
4. 活动元素按指令语义计算写入 selfcheck 缓冲。

### 9.4 `per_run<Ts1, Ts2, Td>()` — 单轮驱动

```
set_value_from_inst → (随机模式) register_type_with_random 各操作数 + vm(vlenb 字节)
                    → (回放模式) set_value_from_cfg
→ check_illegal 置 global_flag_ptr->illegal
→ JIT 序列: save_context / vzero_all
            load_multi_vector<Td,Ts1,Ts2,uint8_t>({"vd","vs1","vs2","vm"}, ...)
            store_multi_preinst_vector<...>
            vsetvli_lmul_sew + csrrw(VSTART) + csrrw(VXRM)
            insts.push_back(vector_cfg.inst)          // 被测指令
            store_multi_afterinst_vector<Td>({"vd"})
            restore_context
→ run_instruction(insts)
→ check_afterinst_illegal()  (≥0 直接返回)
→ save_multi_preinst_value_to_common → run_self_result → check_multi_error<Td>
→ reset_data()
```

注意 vm 恒以 `uint8_t` 类型、`lmul_m1`、`vlenb` 长度加载/快照。

### 9.5 `main()` — SEW 分发

```cpp
switch (vector_cfg.sew) {
case sew_e8:  has_error = per_run<uint8_t,  uint8_t,  uint8_t>(...);  break;
case sew_e16: has_error = per_run<uint16_t, uint16_t, uint16_t>(...); break;
case sew_e32: has_error = per_run<uint32_t, uint32_t, uint32_t>(...); break;
case sew_e64: has_error = per_run<uint64_t, uint64_t, uint64_t>(...); break;
}
```

三个模板参数分别对应 vs1、vs2、vd 的元素类型，加宽/缩窄指令各自不同（如 `vwadd.vv`：`per_run<Ts, Ts, Td>` 中 `sizeof(Td) == 2*sizeof(Ts)`）；有符号/无符号指令选择对应的 `intN_t`/`uintN_t`。

---

## 10. 与其它类别框架的边界

- `v_common.h` **不包含**任何浮点特有设施（特殊值生成、格式转换）——那些在 `f_common.h`；
- `v_common.h` **不包含**密码算法模型——那些在 `v_crypto_common.h`；
- 但两者都构建在本层的 `vector_cfg` / `init_vector_cfg` / JIT 发射原语 / `VectorRegValidator` 之上。整型框架即是这套设施"原样使用"的形态，浮点与密码框架则分别叠加了类型系统与算法模型层。
