# Common Framework 文档

> 对应源文件：`common/framework.h`
>
> 本文档描述 DAMO-RV-ATS 测试框架的公共基础设施层。该层与具体指令类别（整型向量 / 浮点向量 / 向量密码）无关，为所有测试用例提供统一的配置管理、随机化、JIT 执行与结果比对能力。

---

## 1. 总体定位

`framework.h` 是整个测试框架的根基，所有其它公共头文件（`v_common.h`、`f_common.h`、`v_crypto_common.h`）都直接或间接包含它。它实现了测试方法论中"**配置 → 执行 → 比对**"三步法所需的全部通用机制：

```
┌────────────────────────────────────────────────────────┐
│                    framework.h (公共层)                  │
│                                                        │
│  日志系统   命令行解析   随机引擎   c_cfg/c_data 数据模型   │
│  指令编码   JIT 执行    SIGILL 处理   黄金模型比对         │
└────────────────────────────────────────────────────────┘
              ▲               ▲                ▲
              │               │                │
      v_common.h        f_common.h     v_crypto_common.h
      (向量整型)         (向量浮点)        (向量密码)
```

核心设计特点：

- **纯头文件（header-only）**：所有实现都在 `.h` 中，测试用例 `.cpp` 直接 `#include` 后编译为独立可执行文件；
- **软件黄金模型（self-check）**：不依赖外部参考模型进程，期望结果由测试用例内的 C++ 函数计算，与真实指令执行结果逐元素比对；
- **JIT 裸指令注入**：被测指令以 32 位编码字形式动态生成，写入可执行内存后直接调用，因此可以测试编译器尚未支持的扩展指令；
- **双模式运行**：Fuzzing 模式（随机生成）与 Data 回放模式（从 JSON 数据文件复现），支持录制回归。

---

## 2. 日志系统

框架提供 7 级日志，通过全局变量 `global_log_level` 控制：

| 级别 | 宏 | 说明 |
|---|---|---|
| `LOG_NONE` | — | 完全静默 |
| `LOG_ERROR` | `ERROR` | 错误（输出到 `stderr`，附带 `__FILE__:__LINE__`），默认级别 |
| `LOG_WARN` | `WARN` | 警告 |
| `LOG_INFO` | `INFO` | 关键流程信息 |
| `LOG_DETAIL` | `DETAIL` | 详细信息 |
| `LOG_VERBOSE` | `VERBOSE` | 逐元素数据转储 |
| `LOG_DEBUG` | `DEBUG` | 调试信息（附带函数名） |

所有宏都通过 `IF_LOG_LEVEL_ENABLED(level)` 做级别过滤，未启用的级别零开销。日志级别可用命令行 `--log-level`/`-l` 设置（大小写不敏感，由 `set_log_level()` 解析）。

---

## 3. 命令行参数与全局运行状态

`init_program(argc, argv)` 基于 `thirdparty/argparse.hpp` 解析命令行，是每个测试用例 `main()` 调用的第一个框架函数。支持的参数：

| 参数 | 简写 | 默认值 | 对应全局变量 | 说明 |
|---|---|---|---|---|
| `--log-level` | `-l` | `ERROR` | `global_log_level` | 日志级别 |
| `--runtime` | `-r` | 1000 | `run_times` | 迭代次数；回放模式下未显式指定时自动调整为数据文件条目数 |
| `--early-stop` | `-e` | false | `early_stop` | 首个错误即停止 |
| `--seed` | `-s` | 0（取 `random_device`） | `seed` | 固定随机种子以便复现 |
| `--record` | — | false | `record_mode` | 录制模式：将每轮配置写入 JSON 数据文件 |
| `--illegal-percent` | — | -1（关闭） | `illegal_percent` | 目标非法指令比例 [0,1]，触发重试生成 |
| `--max-retry` | — | 10 | `max_retry` | 生成非法指令的最大重试次数 |
| `--data` | — | false | `random_mode = !data` | 切换到固定数据回放模式 |

其它由 `init_program` 完成的初始化：

- 用种子构造全局随机引擎 `global_rng`（`std::mt19937_64`）；
- 回放模式下调用 `c_cfg::from_json_file(datafile)` 加载全部测试配置到 `global_cfg`；
- 调用 `set_signal_handler()` 注册 SIGILL 处理器。

配套的文件名约定：测试用例在 `main()` 开头设置 `filename`（取 `__FILE__` 的文件名部分）、`datafile = filename + ".data"`、`outfile = filename + ".out"`。

`end_program(it)` 在测试结束时打印统计信息（非法指令计数 `illegal_counter`、实际迭代数），录制模式下将 `global_cfg` 序列化为 JSON 数据文件。

---

## 4. 配置模型：`c_cfg`

`c_cfg` 是**一次测试迭代的完整可序列化配置**，即录制/回放的数据单元：

```cpp
struct c_cfg {
    std::map<std::string, uint64_t> CSR;                 // vtype/vl/vstart/vcsr 等
    std::map<std::string, std::vector<uint64_t>> DATA;   // 各寄存器的操作数数据
    std::uint32_t INST;                                  // 32 位指令编码
    std::string DESC;                                    // 指令字段描述串
};
```

关键能力：

- **JSON 双向序列化**：`from_json()` / `to_json()`，数值一律以 `0x` 十六进制字符串形式存储；通过特化 `nlohmann::adl_serializer<c_cfg>` 支持 `json j = cfg` 的隐式转换；
- **多进制解析**：`parse_value()` 支持 `0b`/`0B`（二进制）、`0x`/`0X`（十六进制）、十进制字符串；
- **文件级批量读写**：`from_json_file()` / `to_json_file()`（pretty-print，缩进 2）操作 `std::vector<c_cfg>`。

---

## 5. 指令编码模型：`InstField` 与 `get_inst()`

### 5.1 `InstField` 字段描述

每条被测指令的编码格式用一张 `std::vector<InstField>` 字段表描述：

```cpp
struct InstField {
    int front_pos;        // 字段高位 bit 位置
    int rear_pos;         // 字段低位 bit 位置
    uint64_t default_val; // 固定字段的值
    bool is_fix;          // true=固定字段, false=随机字段
    RegClass reg_class;   // NotReg / Vector / Float / Int / Csr
    std::string DESC;     // 字段名, 如 "funct6"、"vm"、"vs2"、"vd"
};
```

`RegClass` 由 `classify_register()` 按名称前缀归类：`v*` → Vector，`f*` → Float，`x*`/`rs*`/`rd*` → Int。

### 5.2 随机指令生成 `get_inst()`

`get_inst(fields)` 按字段表拼装 32 位指令编码：

- **固定字段**（`is_fix=true`）：直接取 `default_val`（funct6/funct3/opcode 等）；
- **寄存器字段**（Vector/Float/Csr）：在字段位宽范围内随机；
- **整型寄存器字段**（Int）：随机范围限制在 `x8~x27`，因为 `x0-x4` 等低号寄存器被框架保留（x2=sp、x28-x31=临时寄存器）；
- **其它字段**：位宽内随机。

若所有字段都落在低 16 位（16-bit 压缩指令空间），会自动在高 16 位补一条 `C.NOP`（`0x0001`），保证 JIT 流中指令按 4 字节对齐解析。

---

## 6. 数据模型：`c_data`

`c_data` 是**一次测试迭代的运行时数据容器**，围绕"寄存器名"（字段 `DESC`，如 `"vd"`、`"vs1"`、`"vm"`）组织五组映射：

| 成员 | 内容 |
|---|---|
| `map_reg_index` | 字段名 → 指令编码中提取的寄存器号/字段值（由 `set_value_from_inst()` 解码填充） |
| `map_reg_value` / `map_reg_typed_value` | 字段名 → 输入操作数（统一的 `uint64_t` 向量 / 类型化对齐内存缓冲） |
| `map_preinst_reg_value` / `map_preinst_typed_value` | 指令执行**前**各寄存器快照 |
| `map_afterinst_reg_value` / `map_afterinst_typed_value` | 指令执行**后**目标寄存器实际结果（硬件真值） |
| `map_selfcheck_reg_value` / `map_selfcheck_typed_value` | 黄金模型计算的期望结果 |

关键方法：

- `set_value_from_inst(fields, inst)`：从 32 位编码按字段表反解出各字段值，写入 `map_reg_index`；
- `get_DESC_from_inst()`：生成 `"funct6=0,vm=1,..."` 形式的描述串，存入 `c_cfg.DESC` 便于人工排查；
- `register_type_with_random<T>(name, count)` / `register_type_with_bounded_random<T>(...)`：生成 count 个类型 T 的随机值作为操作数；
- `process_to_common<T>()` / `process_from_common<T>()`：**统一数据表示**的核心——任意元素类型 T（`uint32_t`、`float`、自定义浮点包装类等）与 `std::vector<uint64_t>` 之间的双向转换。自定义类型通过静态钩子 `save_class_to_uint64()` / `load_class_from_uint64()` 接入；`process_from_common` 使用 `aligned_alloc` 保证至少 16 字节对齐；
- `set_value_from_cfg()` / `set_value_to_cfg()`：与 `c_cfg.DATA` 的互相拷贝，支撑回放/录制。

内存缓冲统一用 `SafeVoidPtr`（带 `free` 删除器的 `shared_ptr<void>`）管理，配套工厂函数 `make_aligned_array<T>()`、`make_zero_buffer<T>()`。

---

## 7. 随机数工具

| 函数 | 行为 |
|---|---|
| `get_rand<T>()` | 全值域均匀随机。整型走 `uniform_int_distribution`；非基础类型（如浮点包装类）转发到 `T::get_class_rand()` 静态钩子 |
| `get_rand_bound<T>(min, max)` | **边界偏置随机**：10% 概率返回 min、10% 概率返回 max、80% 均匀随机，用于强化边界值压力测试。非基础类型转发到 `T::get_class_rand_bound()` |

这套 `if constexpr` + 静态钩子的设计使得框架对"任意元素类型"泛化：测试用例只需为其自定义类型实现 `get_class_rand` / `get_class_rand_bound` / `save_class_to_uint64` / `load_class_from_uint64` 四个静态函数，即可无缝接入随机化与序列化管线（`f_common.h` 的 `c_check_f` 即如此实现）。

位操作辅助：`get_bit(x, n)`、`mask_bits(n)`、`get_bits_range(x, i, j)`、`sign_extend(val, width)`。

---

## 8. 机器码生成原语（JIT 序列构建）

框架不依赖汇编器，而是在 C++ 中直接编码 RISC-V 机器指令，压入 `std::vector<uint32_t>` 指令队列：

### 8.1 标量指令编码器

| 函数 | 指令 |
|---|---|
| `get_addi_inst(rd, rs1, imm)` | `addi` |
| `get_add_inst` / `get_mul_inst` | `add` / `mul` |
| `get_ld_inst` / `get_sd_inst` / `get_lw_inst` / `get_sw_inst` | 访存指令 |
| `get_load_inst` / `get_store_inst` | XLEN 自适应（RV64→ld/sd，RV32→lw/sw），`XLEN_BYTES` 常量 |

### 8.2 上下文与地址构造

- `save_context(insts)` / `restore_context(insts)`：在 JIT 序列首尾保存/恢复 x1-x31（除 sp）到栈，保证被测代码段不破坏宿主程序状态；
- `load_reg(insts, ptr, reg)` / `store_reg(insts, ptr, reg)`：**64 位地址物化**——将任意内存地址按 10-bit 一段拆分，用 `ADDI`+`MUL`（乘 1024）链式累加构造到目标寄存器（临时寄存器固定用 x29-x31），随后发射 `ld`/`sd`。之所以不用 `lui/auipc`，是因为 ADDI 的 12-bit 立即数配合乘法可以精确表达任意 64 位地址且与 XLEN 无关；
- `csrr(insts, csr)`：发射 `csrrs x28, csr, x0` 读 CSR 并 store 到 `val_placeholder[]`，返回槽位下标，`run_instruction()` 之后从 `val_placeholder[下标]` 取回读值；
- `csrrw(insts, csr, val)`：先 `load_reg` 将值装入 x28，再发射 `csrrw x0, csr, x28` 写 CSR（用于设置 vstart、vxrm 等）。

CSR 直接访问宏（宿主侧，非 JIT）：`CSRR(csr)` / `CSRW` / `CSRS` / `CSRC`，通过内联汇编实现，`CSRR` 依 `__riscv_xlen` 返回 32/64 位。

---

## 9. JIT 执行引擎：`run_instruction()`

`run_instruction(insts)` 是框架的执行核心，流程：

1. 将指令队列写入二进制文件 `<filename>.bin`（留存现场便于反汇编排查）；
2. `mmap` 一块 `PROT_READ|PROT_WRITE|PROT_EXEC` 的匿名内存（大小 +4 字节），用 RAII 守卫确保 `munmap`；
3. 从 `.bin` 文件读回指令流到该内存；
4. 末尾追加返回指令 `jalr x0, 0(x1)`（`0x8067`）；
5. 执行 `fence.i` 同步指令缓存；
6. 将内存首地址强转为函数指针并调用——被测指令在宿主进程内真实执行；
7. 返回后自动清理。

---

## 10. 非法指令（SIGILL）处理

框架把"非法指令是否被正确拒绝"也作为验证目标：

- `set_signal_handler()`：注册 `sigaction(SIGILL)`，处理器 `signal_handler()` 记录出错 PC 与指令编码，将 PC **+4 跳过非法指令**继续执行，并置 `global_signal_cnt = 1`；
- `c_flag`（全局 `global_flag_ptr`）：
  - `illegal`：本轮指令被 `check_illegal()` 判定为**期望非法**；
  - `check_illegal_error`：出现"期望非法但未触发 SIGILL"或"期望合法却触发 SIGILL"的不一致；
- `check_if_sigill()`：期望非法时校验信号确实到达，否则置错误标志；
- `print_illegal_status(illegal)`：累加全局 `illegal_counter` 并记录合法性判定结果。

配合 `--illegal-percent`：`init_vector_cfg()` 会按目标比例反复重新随机指令编码（最多 `max_retry` 次）直到 `check_illegal()` 判定为非法，从而主动提高非法路径覆盖率。

---

## 11. 结果比对（黄金模型校验）

- `check_single_error<T>(cur_data, reg_name, len)`：将 `afterinst`（硬件实际结果）与 `selfcheck`（黄金模型期望）两个类型化缓冲统一转成 `uint64_t` 向量后**逐元素比对**；不一致时打印所有相关寄存器的 pre-instruction 值和差异元素详情（`vm` 掩码会按 row/col 展开到元素粒度），返回 1；
- `check_multi_error<T, Rest...>(cur_data, names, lens)`：变参模板递归展开，依次校验多个寄存器（每个寄存器可以有各自的元素类型 T），首个出错即返回；
- `save_single_preinst_value_to_common<T>()` / `save_multi_preinst_value_to_common<T, Rest...>()`：执行结束后把 pre-instruction 类型化快照转回统一 `uint64_t` 格式（供黄金模型使用与错误转储）；`vm` 处于 unmasked 模式时自动跳过。

---

## 12. 测试用例的标准骨架

每个测试用例 `.cpp` 都遵循同一模板（各类别框架在此基础上扩展）：

```cpp
#include "../common/v_common.h"   // 或 v_crypto_common.h

// 1. 指令编码字段表
const std::vector<InstField> vop_inst_fields = { ... };

// 2. 合法性检查: 返回 1 表示当前配置+编码应为非法指令
int check_illegal(c_data &cur_data) { ... }

// 3. 黄金模型: 用 preinst 数据计算期望结果写入 map_selfcheck_typed_value
int run_self_result(c_data &cur_data) { ... }

// 4. 单轮执行: 随机操作数 → 构建 JIT 序列 → 执行 → 比对
int per_run(int it, c_cfg &cur_cfg, c_data &cur_data) { ... }

int main(int argc, char *argv[])
{
    filename = ...; datafile = filename + ".data";
    init_program(argc, argv);       // 解析参数/随机引擎/SIGILL
    init_vector_program();          // 类别层初始化(读 VLENB 等)

    for (it = 0; it < run_times; it++) {
        init_vector_cfg(it, cur_cfg, cur_data, check_illegal, vop_inst_fields);
        has_error = per_run(it, cur_cfg, cur_data);   // 按 SEW 分发模板实例
        if (has_error && early_stop) break;
    }
    end_program(it);                // 统计输出 + 录制落盘
}
```

其中 `per_run()` 内部的标准 JIT 序列结构为：

```
save_context            ; 保护宿主寄存器
  <加载操作数到向量/标量寄存器>
  <store preinst 快照>
  vsetvli / csrrw       ; 配置向量上下文
  <被测指令 32-bit 编码>
  <store afterinst 结果>
restore_context
```

---

## 13. 两种运行模式小结

| | Fuzzing 模式（默认） | Data 回放模式（`--data`） |
|---|---|---|
| 配置来源 | `m_rand()` + `get_inst()` 现场随机 | `global_cfg[it]`（JSON 数据文件） |
| 操作数来源 | `register_type_with_random<T>()` | `c_data::set_value_from_cfg()` |
| 迭代数 | `--runtime` | 未指定则跑完全部数据条目 |
| 配套 | `--record` 录制、`--seed` 复现、`--illegal-percent` 定向非法 | 用于 bug 复现与回归 |

录制文件为 `<用例名>.cpp.data`，内容是 `c_cfg` 数组的 JSON，包含 CSR、全部操作数与指令编码，可完整复现任何一轮迭代。
