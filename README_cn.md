# DAMO-RV-ATS

[English](README_en.md) | **中文**

`DAMO-RV-ATS` 是面向 RISC-V ISA 扩展的自检式（self-checking）兼容性验证测试框架。它向被测实现注入随机指令序列，并与每个用例内置的软件黄金模型逐元素比对结果，因此一次运行验证的是真实的体系结构行为，而不仅仅是"指令没有触发异常"。

当前覆盖 **667 条**指令测试用例，涵盖 RVV 1.0 基础扩展（整数、浮点、访存）以及向量位操作与向量密码扩展。

---

## 特性

- **自检比对** — 每个用例实现黄金模型（`run_self_result`），与目标机执行结果逐元素比对；不一致时输出 CSR 状态、向量配置与寄存器内容。
- **随机激励** — 指令编码、操作数数据（偏向边界值）与向量配置在每轮迭代中重新随机；任意一次运行都可以通过种子复现。
- **非法指令覆盖** — 通过 `SIGILL` 信号处理器校验保留编码与非法寄存器组组合确实被拒绝，并在预期异常未发生时报错。
- **三种执行通路** — QEMU user 模式、RISC-V Linux 原生运行，以及可脱离操作系统运行的裸机镜像（也可在 Sail 模型上执行）。
- **录制与回放** — 模糊测试结果以 JSON 格式的 `.data` 文件保存，可确定性地回放，用于回归验证与 bug 复现。
- **一条指令一个文件** — 新增测试只需一个 `.cpp` 加上纯头文件框架，无需学习额外的运行器机制，扩展目录的 `Makefile` 会自动识别新文件。

---

## 支持的指令扩展集

每个扩展集位于独立的顶层目录，可以单独构建与运行。

### RVV 1.0 基础扩展

| 目录 | 覆盖范围 | 用例数 | `-march` |
|------|----------|--------|----------|
| `vx/` | **整数** — 带进位/借位加减、逻辑与移位、最值、比较、乘除与取余、乘加、加宽与缩窄算术、定点（饱和与舍入）运算、掩码逻辑与掩码位操作、归约、排列、数据搬移、符号/零扩展 | 213 | `rv64gcv` |
| `vf/` | **浮点** — 加减乘除与开方、最值、符号注入、完整的融合乘加族、加宽浮点算术、浮点比较、浮点↔整数与浮点↔浮点转换（含向零舍入与奇数舍入）、浮点归约、`vfrec7`/`vfrsqrt7`、浮点搬移与 slide | 101 | `rv64gcv` |
| `vls/` | **访存与配置** — 单位步长、跨步、索引（有序/无序）访存，以及各自的 segment（2–8 字段）变体，掩码访存、fault-only-first 加载、整寄存器搬移，以及 `vsetvli`/`vsetivli`/`vsetvl` | 313 | `rv64gcv` |

### 向量位操作与向量密码扩展

| 目录 | 扩展 | 覆盖范围 | 用例数 | `-march` |
|------|------|----------|--------|----------|
| `zvbb/` | Zvbb | 按位与非、循环左/右移、位反转与字节反转、前导/尾随零计数、位计数、加宽移位 | 15 | `rv64gcv_zvbb` |
| `zvbc/` | Zvbc | 无进位乘法（低位与高位） | 4 | `rv64gcv_zvbc` |
| `zvkg/` | Zvkg | GCM/GMAC 伽罗华域乘累加 | 2 | `rv64gcv_zvkg` |
| `zvkned/` | Zvkned | AES-128 加/解密轮函数、密钥扩展、白化 | 11 | `rv64gcv_zvkned` |
| `zvknh/` | Zvknhb | SHA-2 消息扩展与压缩函数 | 3 | `rv64gcv_zvknhb` |
| `zvksed/` | Zvksed | SM4 轮函数与密钥扩展 | 3 | `rv64gcv_zvksed` |
| `zvksh/` | Zvksh | SM3 消息扩展与压缩函数 | 2 | `rv64gcv_zvksh` |

同样支持 `rv32` 目标 —— 使用 `xlen=32` 构建即可，此时 ABI 切换为 `ilp32d`，并在 `qemu-riscv32` 下运行。

---

## 随机化维度

每轮迭代中，框架随机化以下内容：

- **`vtype`** — SEW（8/16/32/64）、LMUL（1/8 至 8）、`vta`、`vma`
- **`vl` 与 `vstart`** — 包括部分有效与空向量体的场景
- **`vcsr`** — `vxrm` 舍入模式与 `vxsat` 饱和标志
- **指令编码** — 操作数寄存器号、立即数，以及 `vm` 掩码位
- **操作数数据** — 全范围随机并偏向边界值（零、±1、有符号/无符号极值、2 的幂，浮点用例还包含 NaN/Inf 模式）
- **可配置比例的非法编码**（`--illegal-percent`）

执行前，每个生成的用例都会按照该扩展的合法性约束进行检查 —— 寄存器组对齐、加宽/缩窄的重叠规则、EEW/EMUL 组合、访存对齐等。判定为非法的编码必须触发 `SIGILL`，判定为合法的编码则不得触发。

需要注意的是，并非所有指令在任意 SEW 下都合法（例如 Zvbc 仅在 `SEW=64` 下定义，AES/SM3/SM4/GCM 指令仅在 `SEW=32` 下定义），因此每个用例会约束自身的配置空间。

---

## 项目结构

```
damo-rv-ats/
├── Makefile                  # 顶层构建入口，遍历 EXTENSIONS 列表
├── common/                   # 纯头文件框架与工具库
│   ├── framework.h           # 配置管理、随机数引擎、JIT 执行、SIGILL 处理、命令行解析、录制/回放
│   ├── v_common.h            # 向量工具：寄存器加载/存储、vsetvl、合法性检查
│   ├── f_common.h            # 软件浮点/整数模型类型、NaN/Inf 判定、带舍入模式的转换
│   ├── v_crypto_common.h     # 向量密码扩展共用的黄金模型原语
│   └── baremetal_gen.h       # 在模糊测试过程中记录语义并生成独立裸机程序
├── thirdparty/               # argparse.hpp、json.hpp（nlohmann/json）
├── NORM/                     # V 扩展 normative rules 参考与规则-覆盖点映射
├── vx/ vf/ vls/              # RVV 1.0 基础扩展测试用例
├── zvbb/ zvbc/               # 向量位操作扩展测试用例
├── zvkg/ zvkned/ zvknh/      # 向量密码扩展测试用例
├── zvksed/ zvksh/
├── LICENSE                   # Apache-2.0
├── .clang-format             # 代码风格
└── .pre-commit-config.yaml   # Pre-commit 钩子
```

每个扩展目录下每条指令对应一个 `.cpp`，并带有提供相同 target 集合的 `Makefile`。

---

## 环境依赖

- **交叉编译工具链** — `riscv64-unknown-linux-gnu-g++`（默认），或 `COMPILER=clang` 时的 `riscv64-unknown-linux-gnu-clang++`；C++17，静态链接产物
- **QEMU user 模式** — `qemu-riscv64`（`xlen=32` 时为 `qemu-riscv32`）
- **可选，仅裸机流程需要** — `riscv64-unknown-elf-gcc` 与 `objdump`、`qemu-system-riscv64`、`sail_riscv_sim`

---

## 构建与运行

### 在项目顶层

```bash
make                          # 构建默认 EXTENSIONS 列表中的所有扩展
make EXTENSIONS="vx zvbb"     # 构建指定子集
make COMPILER=clang           # 使用 clang 构建
make xlen=32                  # 构建 rv32 目标
make DEBUG=1                  # 以 -O0 -g 构建

make qemu                     # 构建并在 QEMU user 模式下运行全部用例
make test                     # 原生运行（需要 RISC-V 主机）
make clean
make help                     # 列出全部 target 与选项
```

### 单个扩展或单个用例

```bash
cd vx && make -j8             # 构建单个扩展
cd vx && make qemu            # 构建并在 QEMU 下运行

qemu-riscv64 vx/vadd.vv.elf --runtime 5000 --seed 42
```

### 裸机流程

裸机流程先在 QEMU user 模式下进行模糊测试并记录每轮迭代的语义，然后生成一个可以脱离操作系统编译执行的独立程序。

```bash
make baremetal-generate EXTENSIONS=vx CASE=vadd.vv RUNTIME=100   # 生成 .baremetal.c / _entry.S / .ld
make baremetal-compile  EXTENSIONS=vx CASE=vadd.vv               # 构建 <case>.baremetal.elf（及 .asm）
make baremetal-run      EXTENSIONS=vx CASE=vadd.vv               # 在 qemu-system-riscv64 -M virt -cpu max 下运行
make baremetal-sail     EXTENSIONS=vx CASE=vadd.vv               # 在 Sail RISC-V 模型中运行
```

省略 `CASE=` 则处理该扩展下的全部用例。生成的代码与数据地址无关（使用 `la`），通过 trap handler 跳过非法指令（`mepc += 4`），并以 `ecall` 标记执行结束。`baremetal-run` 会从 QEMU 中断日志中统计并报告非法指令数与 ecall 数。

---

## 命令行参数

| 参数 | 说明 | 默认值 |
|------|------|--------|
| `--data` | 回放模式 —— 从 `.data` 文件读取用例，而非随机生成 | `false` |
| `--runtime`, `-r` | 迭代次数。回放模式下若未显式指定，则执行文件中的全部用例 | `1000` |
| `--seed`, `-s` | 固定随机种子以便复现 | `0`（自动生成） |
| `--record` | 将执行过的用例写入 `.data` 文件 | `false` |
| `--early-stop`, `-e` | 首个不一致即停止，而非继续执行 | `false` |
| `--illegal-percent` | 生成非法编码的比例，范围 `[0,1]` | `-1`（不限制） |
| `--max-retry` | 生成非法指令时的最大重试次数 | `10` |
| `--log-level`, `-l` | `NONE` / `ERROR` / `WARN` / `INFO` / `DETAIL` / `VERBOSE` / `DEBUG` | `ERROR` |
| `--baremetal` | 在生成数据文件的同时输出裸机源码（隐含 `--record`） | `false` |

```bash
# 固定种子模糊测试 5000 轮并录制用例
qemu-riscv64 vx/vadd.vv.elf --runtime 5000 --seed 42 --record

# 回放已录制的文件
qemu-riscv64 vx/vadd.vv.elf --data

# 首错即停并输出详细日志
qemu-riscv64 vx/vadd.vv.elf --runtime 10000 --early-stop --log-level INFO
```

不一致通过 `ERROR` 通道报告，并附带失败迭代的 CSR 值与向量配置；运行结束时输出汇总行，报告预期非法的用例数与实际执行的迭代数。

---

## 运行模式

### 模糊测试模式（默认）

每轮迭代生成全新的编码、配置与操作数。使用 `--seed` 让运行可复现，使用 `--record` 将其持久化以供后续回放。

### 回放模式（`--data`）

从 `<case>.cpp.data` 读取先前录制的用例并确定性重放 —— 回归测试与 bug 复现所使用的模式。

### 裸机模式（`--baremetal`）

在执行常规 Linux/QEMU 流程的同时记录足以重建每轮迭代的状态，并在退出时写出裸机源码。

---

## 数据文件格式

录制的用例以 JSON 数组保存，每个元素包含复现一轮迭代所需的全部信息：

| 字段 | 说明 |
|------|------|
| `CSR` | CSR 寄存器值 —— `vtype`、`vcsr`、`vl`、`vstart` 等 |
| `DATA` | 操作数数据 —— `vs1`、`vs2`、`vd`、`vm` 等，以十六进制数组表示 |
| `INST` | 指令编码（十六进制） |
| `DESC` | 编码各字段的解析值 |

```json
[
  {
    "CSR": {"vcsr": "0x6", "vl": "0x1", "vstart": "0x0", "vtype": "0x6"},
    "DATA": {"vd": ["0x9d"], "vs1": ["0xcc"], "vs2": ["0xb8"]},
    "INST": "0x3160c57",
    "DESC": "funct6=0, vm=1, vs2=17, vs1=12, funct3=0, vd=24, opcode=87"
  },
  {
    "CSR": {"vcsr": "0x4", "vl": "0x4", "vstart": "0x0", "vtype": "0x6"},
    "DATA": {
      "vd": ["0xd7", "0xe5", "0x3b", "0x50"],
      "vm": ["0x2f", "0xd8", "0x6c", "0x82"],
      "vs1": ["0x1a", "0x56", "0xce", "0xd9"],
      "vs2": ["0xa4", "0xdc", "0x10", "0x23"]
    },
    "INST": "0x68fd7",
    "DESC": "funct6=0, vm=0, vs2=0, vs1=13, funct3=0, vd=31, opcode=87"
  }
]
```

---

## 核心架构

### `common/framework.h`

- **配置管理** — `c_cfg` 保存单轮迭代的 CSR 值、操作数数据与指令编码，支持与 JSON 相互序列化。
- **随机数引擎** — 基于 `std::mt19937_64`，种子来自 `--seed` 或 `std::random_device`；`get_rand<T>()` 提供全范围随机，`get_rand_bound<T>()` 提供边界值偏向随机。
- **JIT 执行** — `run_instruction()` 将生成的指令序列拷贝到 `mmap` 分配的可执行内存，执行 `fence.i` 后在目标机上原地执行。
- **信号处理** — `SIGILL` 处理器记录预期异常是否被触发。
- **编码生成** — `InstField` 表描述指令的位域布局，`get_inst()` 按字段随机填充，并遵循所要求的合法性与 `--illegal-percent` 比例。

### `common/v_common.h`

- **寄存器访问** — `load_vector` / `store_vector` 及其多寄存器变体，在被测指令前后生成 `vle`/`vse` 序列。
- **CSR 配置** — `vsetvli_lmul_sew()` 配置向量长度与元素宽度；`csrrw()` 写入 `vstart`、`vxrm` 等。
- **合法性检查** — 实现 V 规范中的约束集合：单宽度、加宽与缩窄的寄存器组对齐，扩展与归约检查，访存检查，以及寄存器组重叠规则。
- **上下文保存/恢复** — 在 JIT 序列前后保存与恢复通用寄存器状态。

### `common/f_common.h`

浮点测试使用的软件模型类型：`fp16`/`bf16` 与窄整数类型及其确定的运算语义、NaN/Inf 判定，以及以舍入模式（`frm`）为参数并上报标志位的转换辅助函数。

### `common/v_crypto_common.h`

密码扩展共用的黄金模型原语 —— 位循环移位、字节反转、有效 LMUL 位宽计算，以及 AES/SHA/SM3/SM4/GCM 的轮函数。

### `common/baremetal_gen.h`

挂接 Linux 模糊测试路径，记录每轮迭代消费的向量/标量/内存状态，并输出 `<case>.cpp.baremetal.c`、`<case>.cpp.baremetal_entry.S`、`<case>.cpp.baremetal.ld` 与 `<case>.cpp.baremetal.mk`；`baremetal-compile` 会将这些文件编译为 `<case>.baremetal.elf`。

---

## Normative Rules 参考（`NORM/`）

- `NORM/v_norm.md` — 从 RISC-V "V" 扩展 v1.0 规范中提取的 435 条 normative rules，按章节分组，附中文解释以及每条规则关联的指令/CSR。
- `NORM/vx.yaml` — normative rule 名称到覆盖点的机器可读映射。

---

## 新增测试用例

1. 从最接近的扩展目录复制一个现有 `.cpp`。
2. 用 `InstField` 表描述指令编码。
3. 实现 `check_illegal()` —— 该指令的操作数与配置约束。
4. 实现 `run_self_result<Ts1, Ts2, Td>()` —— 黄金模型，需正确处理 `vstart` 与掩码。
5. 实现 `per_run()` —— 构建指令序列（`save_context` → 加载操作数 → `vsetvli_lmul_sew` → 被测指令 → 存储结果 → `restore_context`），调用 `run_instruction()`，然后进行比对。
6. 将文件放入扩展目录即可；`Makefile` 的通配符会自动识别，顶层 `EXTENSIONS` 列表也已包含该目录。

---

## 许可证

Apache License 2.0 —— 详见 [LICENSE](LICENSE)。
