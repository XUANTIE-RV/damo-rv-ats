**中文 | [English](README_EN.md)**

# DAMO-RV-ATS

`DAMO-RV-ATS` 是 RISC-V 指令兼容性验证测试框架，用于对RISC-V处理器的标准扩展指令进行兼容性回归验证和自动化随机测试。当前覆盖 **V 扩展**（RVV 1.0）、**Zvbb 扩展**（向量位操作）和 **Zvbc 扩展**（向量无进位乘法），共计 **200 条**指令测试用例。

---

## 项目结构

```
damo-rv-ats/
├── Makefile                    # 顶层构建入口（自动遍历所有扩展子目录）
├── README.md                   # 项目说明文档
├── LICENSE                     # 开源许可证
├── .clang-format               # 代码格式化配置
├── .pre-commit-config.yaml     # Pre-commit 钩子配置
├── common/                     # 公共框架与工具库
│   ├── framework.h             # 核心测试框架（配置管理、随机数引擎、JIT 执行、信号处理等）
│   └── v_common.h          # 向量指令公共工具（寄存器操作、合法性检查、CSR 配置等）
├── thirdparty/                 # 第三方依赖
│   ├── argparse.hpp            # 命令行参数解析库
│   └── json.hpp                # JSON 序列化/反序列化库（nlohmann/json）
├── vx/                         # V 扩展指令测试用例（181 条）
│   ├── Makefile
│   └── *.cpp                   # 每条指令一个测试文件
├── zvbb/                       # Zvbb 扩展指令测试用例（15 条）
│   ├── Makefile
│   └── *.cpp
├── zvbc/                       # Zvbc 扩展指令测试用例（4 条）
│   ├── Makefile
│   └── *.cpp
├── dev/                        # 开发中的测试用例（实验性）
│   └── Makefile
└── SPEC/                       # 参考规范文档
    ├── v-st-ext.adoc           # RISC-V V 扩展自定义规范
    └── vector-crypto.adoc      # RISC-V 向量加密扩展规范
```

---

## 测试用例覆盖范围

### V 扩展 (`vx/`) — 181 条指令

| 类别 | 指令 | 数量 |
|------|------|------|
| 整数加减 | `vadd.vv/vx/vi`, `vsub.vv/vx`, `vrsub.vi/vx` | 8 |
| 逻辑运算 | `vand.vv/vx/vi`, `vor.vv/vx/vi`, `vxor.vv/vx/vi` | 9 |
| 移位运算 | `vsll.vv/vx/vi`, `vsrl.vv/vx/vi`, `vsra.vv/vx/vi` | 9 |
| 缩窄移位 | `vnsrl.wv/wx/wi`, `vnsra.wv/wx/wi` | 6 |
| 比较/最值 | `vmin.vv/vx`, `vminu.vv/vx`, `vmax.vv/vx`, `vmaxu.vv/vx` | 8 |
| 整数比较 | `vmseq.vv/vx/vi`, `vmsne.vv/vx/vi`, `vmslt.vv/vx`, `vmsltu.vv/vx`, `vmsle.vv/vx/vi`, `vmsleu.vv/vx/vi`, `vmsgt.vx/vi`, `vmsgtu.vx/vi` | 22 |
| 整数乘法 | `vmul.vv/vx`, `vmulh.vv/vx`, `vmulhu.vv/vx`, `vmulhsu.vv/vx` | 8 |
| 整数除法/取余 | `vdiv.vv/vx`, `vdivu.vv/vx`, `vrem.vv/vx`, `vremu.vv/vx` | 8 |
| 乘加运算 | `vmacc.vv/vx`, `vmadd.vv/vx`, `vnmsac.vv/vx`, `vnmsub.vv/vx` | 8 |
| 加宽算术 | `vwadd.vv/vx/wv/wx`, `vwaddu.vv/vx/wv/wx`, `vwsub.vv/vx/wv/wx`, `vwsubu.vv/vx/wv/wx` | 16 |
| 加宽乘法 | `vwmul.vv/vx`, `vwmulu.vv/vx`, `vwmulsu.vv/vx` | 6 |
| 加宽乘加 | `vwmacc.vv/vx`, `vwmaccu.vv/vx`, `vwmaccsu.vv/vx`, `vwmaccus.vx` | 7 |
| 合并/进位 | `vmerge.vvm/vxm/vim`, `vadc.vvm/vxm/vim`, `vmadc.vv/vx/vi/vvm/vxm/vim` | 12 |
| 借位减 | `vsbc.vvm/vxm`, `vmsbc.vv/vx/vvm/vxm` | 6 |
| 数据搬移 | `vmv.v.v`, `vmv.v.x`, `vmv.v.i`, `vmv.x.s`, `vmv.s.x` | 5 |
| 整寄存器搬移 | `vmv1r.v`, `vmv2r.v`, `vmv4r.v`, `vmv8r.v` | 4 |
| 掩码逻辑 | `vmand.mm`, `vmnand.mm`, `vmandn.mm`, `vmxor.mm`, `vmor.mm`, `vmnor.mm`, `vmorn.mm`, `vmxnor.mm` | 8 |
| 掩码位操作 | `vmsbf.m`, `vmsif.m`, `vmsof.m` | 3 |
| 掩码计数 | `vcpop.m`, `vfirst.m`, `viota.m`, `vid.v` | 4 |
| 归约指令 | `vredsum.vs`, `vredand.vs`, `vredor.vs`, `vredxor.vs`, `vredmin.vs`, `vredminu.vs`, `vredmax.vs`, `vredmaxu.vs` | 8 |
| 加宽归约 | `vwredsum.vs`, `vwredsumu.vs` | 2 |
| 排列指令 | `vcompress.vm`, `vrgather.vv/vx/vi`, `vrgatherei16.vv`, `vslideup.vx/vi`, `vslidedown.vx/vi`, `vslide1up.vx`, `vslide1down.vx` | 10 |
| 符号/零扩展 | `vsext.vf2/vf4/vf8`, `vzext.vf2/vf4/vf8` | 6 |

### Zvbb 扩展 (`zvbb/`) — 15 条指令

| 类别 | 指令 | 数量 |
|------|------|------|
| 按位与非 | `vandn.vv`, `vandn.vx` | 2 |
| 循环移位 | `vrol.vv/vx`, `vror.vv/vx/vi` | 5 |
| 位反转/字节反转 | `vbrev.v`, `vrev8.v` | 2 |
| 前导/尾随零计数 | `vclz.v`, `vctz.v` | 2 |
| 位计数 | `vcpop.v` | 1 |
| 加宽左移 | `vwsll.vv/vx/vi` | 3 |

### Zvbc 扩展 (`zvbc/`) — 4 条指令

| 类别 | 指令 | 数量 |
|------|------|------|
| 无进位乘法 | `vclmul.vv/vx`, `vclmulh.vv/vx` | 4 |

---

## 测试方法论

每个测试用例遵循统一的 **"配置-执行-比对"** 三步验证模式：

1. **配置阶段**
   - 随机生成指令编码（操作数寄存器号、立即数等）
   - 随机生成操作数数据（支持边界值偏向）
   - 检查指令合法性（`check_illegal`），支持配置非法指令比例

2. **执行阶段**
   - 通过 JIT 引擎将指令注入可执行内存并在目标 CPU 上运行
   - 捕获执行结果（包括 `SIGILL` 非法指令异常）

3. **比对阶段**
   - 使用软件黄金模型（`run_self_result`）计算期望结果
   - 逐元素比对硬件执行结果与黄金模型结果
   - 发现不一致时输出详细错误信息（含 CSR 状态、寄存器数据）

每条指令默认覆盖 **8/16/32/64-bit** 四种元素宽度（SEW），部分指令（如 Zvbc 的 `vclmul`）仅在特定 SEW 下合法。

---

## 环境依赖

- **交叉编译工具链**：
  - GCC（默认）：`riscv64-unknown-linux-gnu-gcc` / `riscv64-unknown-linux-gnu-g++`
  - Clang（可选）：`riscv64-unknown-linux-gnu-clang` / `riscv64-unknown-linux-gnu-clang++`
- **C++ 标准**：C++17
- **QEMU**：`qemu-riscv64`（用于模拟器运行）
- **目标架构**：
  - V 扩展测试：`rv64gcv`
  - Zvbb 扩展测试：`rv64gcv_zvbb`
  - Zvbc 扩展测试：`rv64gcv_zvbc`

---

## 构建与运行

### 编译

```bash
# 在项目顶层编译所有扩展测试用例（vx, zvbb, zvbc）
make

# 使用 Clang 编译所有扩展测试用例
make COMPILER=clang

# 仅编译指定扩展
make EXTENSIONS="vx zvbb"

# 编译单个扩展测试用例
cd vx && make

# 使用 Clang 编译单个扩展
cd vx && make COMPILER=clang
```

### 在 QEMU 上运行

```bash
# 在项目顶层运行所有扩展的测试用例
make qemu

# 使用 Clang 编译并在 QEMU 上运行
make COMPILER=clang qemu

# 运行 vx 目录下所有测试用例
cd vx && make qemu

# 运行单个 V 扩展测试用例
qemu-riscv64 vadd.vv.elf

# 运行 Zvbb/Zvbc 测试用例（可以定 cpu 参数）
cd zvbb && make qemu
cd zvbc && make qemu <cpu=cpu_model>
```

### 命令行参数

| 参数 | 说明 | 默认值 |
|------|------|--------|
| `--data` | 切换为回放模式（从 `.data` 文件读取用例） | `false`（默认为 Fuzzing 模式） |
| `--runtime <N>` | 迭代次数 | `1000` |
| `--seed <N>` | 随机种子（用于复现） | `0`（自动生成） |
| `--record` | 记录测试用例到 `.data` 文件 | `false` |
| `--early-stop` | 首个错误即停止 | `false` |
| `--illegal-percent <F>` | 非法指令比例，范围 [0,1] | `-1`（不限制） |
| `--max-retry <N>` | 非法指令最大重试次数 | `10` |
| `--log-level <L>` | 日志级别：`NONE`/`ERROR`/`WARN`/`INFO`/`DETAIL`/`VERBOSE`/`DEBUG` | `ERROR` |

### 运行示例

```bash
# Fuzzing 模式：随机测试 5000 次，指定种子，记录测试用例
qemu-riscv64 vadd.vv.elf --runtime 5000 --seed 42 --record

# 回放模式：从 .data 文件读取已记录的测试用例
qemu-riscv64 vadd.vv.elf --data --runtime 5000

# Fuzzing 模式，首错即停，详细日志
qemu-riscv64 vadd.vv.elf --runtime 10000 --early-stop --log-level INFO
```

---

## 运行模式

### Fuzzing 模式（默认）

随机生成指令编码和操作数数据，适用于大规模模糊测试。可通过 `--seed` 指定种子以复现测试，通过 `--record` 将测试用例记录到 `.data` 文件中供后续回放。

### 回放模式（`--data`）

从 `.data` 文件中读取预先记录的测试用例进行回归验证，适用于 bug 复现和持续集成。

---

## 数据文件格式

测试数据以 JSON 格式存储在 `.data` 文件中，每条记录包含以下字段：

| 字段 | 说明 |
|------|------|
| `CSR` | CSR 寄存器值（`vtype`、`vcsr`、`vl`、`vstart` 等） |
| `DATA` | 操作数数据（`vs1`、`vs2`、`vd`、`vm` 等，以十六进制数组表示） |
| `INST` | 指令编码（十六进制） |
| `DESC` | 指令描述（各字段值映射） |

### 示例

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

### 测试框架 (`common/framework.h`)

- **配置管理**：通过 `c_cfg` 结构体管理测试配置（CSR 寄存器值、操作数数据、指令编码），支持 JSON 格式的序列化与反序列化
- **随机数引擎**：基于 `std::mt19937_64`，支持种子复现，提供 `get_rand<T>()` 全范围随机和 `get_rand_bound<T>()` 边界值偏向随机两种生成方式
- **JIT 执行引擎**：通过 `run_instruction()` 函数，将指令序列写入 `mmap` 分配的可执行内存并通过 `fence.i` 同步后直接在目标 CPU 上执行
- **信号处理**：注册 `SIGILL` 信号处理器，用于捕获非法指令异常并验证非法指令检测的正确性
- **指令编码生成**：通过 `InstField` 描述指令字段布局，`get_inst()` 函数按字段定义随机生成合法/非法指令编码

### 向量指令工具库 (`common/v_common.h`)

- **向量寄存器操作**：`load_vector` / `store_vector` — 通过动态生成 `vle`/`vse` 指令来加载/存储向量寄存器
- **CSR 操作**：`vsetvli_lmul_sew()` — 配置向量长度和元素宽度；`csrrw()` — 写入 CSR 寄存器
- **合法性检查**：实现了 RISC-V V 扩展规范中的各种约束检查：
  - `is_legal_sss` — 单宽度源/目标寄存器对齐检查
  - `is_legal_widen_sss` — 加宽指令合法性检查
  - `is_legal_narrow_sss` — 缩窄指令合法性检查
  - `is_legal_ext_check` — 符号/零扩展指令检查
  - `is_legal_reduction` — 归约指令检查
  - `vext_check_load` / `vext_check_store` — 访存指令合法性检查
  - `is_no_overlap` — 寄存器组重叠检查（含 Section 5.2 三条规则）
- **上下文保存/恢复**：`save_context` / `restore_context` — 在 JIT 执行前后保存和恢复通用寄存器状态
- **位操作辅助**：`ror` / `rol` / `brev` — 循环右移、循环左移、位反转等运算（用于 Zvbb 黄金模型）
