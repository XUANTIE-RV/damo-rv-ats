# DAMO-RV-ATS

`DAMO-RV-ATS` is A RISC-V instruction compatibility test framework for automated regression verification and fuzzing testing of RISC-V  ISA extension instructions. Currently covers **V extension** (RVV 1.0), **Zvbb extension** (vector bit manipulation), and **Zvbc extension** (vector carryless multiplication), with a total of **200** instruction test cases.

---

## Project Structure

```
damo-rv-ats/
├── README.md                   # Project documentation (Chinese)
├── README_EN.md                # Project documentation (English)
├── LICENSE                     # Open source license
├── .clang-format               # Code formatting configuration
├── .pre-commit-config.yaml     # Pre-commit hook configuration
├── common/                     # Common framework and utility library
│   ├── framework.h             # Core test framework (config management, RNG engine, JIT execution, signal handling, etc.)
│   └── v_common.h          # Vector instruction utilities (register operations, legality checks, CSR configuration, etc.)
├── thirdparty/                 # Third-party dependencies
│   ├── argparse.hpp            # Command-line argument parsing library
│   └── json.hpp                # JSON serialization/deserialization library (nlohmann/json)
├── vx/                         # V extension instruction test cases (181 cases)
│   ├── Makefile
│   └── *.cpp                   # One test file per instruction
├── zvbb/                       # Zvbb extension instruction test cases (15 cases)
│   ├── Makefile
│   └── *.cpp
├── zvbc/                       # Zvbc extension instruction test cases (4 cases)
│   ├── Makefile
│   └── *.cpp
├── dev/                        # In-development test cases (experimental)
│   └── Makefile
└── SPEC/                       # Reference specification documents
    ├── v-st-ext.adoc           # RISC-V V extension custom specification
    └── vector-crypto.adoc      # RISC-V vector cryptography extension specification
```

---

## Test Case Coverage

### V Extension (`vx/`) — 181 Instructions

| Category | Instructions | Count |
|----------|-------------|-------|
| Integer Add/Sub | `vadd.vv/vx/vi`, `vsub.vv/vx`, `vrsub.vi/vx` | 8 |
| Logical Operations | `vand.vv/vx/vi`, `vor.vv/vx/vi`, `vxor.vv/vx/vi` | 9 |
| Shift Operations | `vsll.vv/vx/vi`, `vsrl.vv/vx/vi`, `vsra.vv/vx/vi` | 9 |
| Narrowing Shifts | `vnsrl.wv/wx/wi`, `vnsra.wv/wx/wi` | 6 |
| Min/Max | `vmin.vv/vx`, `vminu.vv/vx`, `vmax.vv/vx`, `vmaxu.vv/vx` | 8 |
| Integer Comparison | `vmseq.vv/vx/vi`, `vmsne.vv/vx/vi`, `vmslt.vv/vx`, `vmsltu.vv/vx`, `vmsle.vv/vx/vi`, `vmsleu.vv/vx/vi`, `vmsgt.vx/vi`, `vmsgtu.vx/vi` | 22 |
| Integer Multiply | `vmul.vv/vx`, `vmulh.vv/vx`, `vmulhu.vv/vx`, `vmulhsu.vv/vx` | 8 |
| Integer Divide/Remainder | `vdiv.vv/vx`, `vdivu.vv/vx`, `vrem.vv/vx`, `vremu.vv/vx` | 8 |
| Multiply-Add | `vmacc.vv/vx`, `vmadd.vv/vx`, `vnmsac.vv/vx`, `vnmsub.vv/vx` | 8 |
| Widening Arithmetic | `vwadd.vv/vx/wv/wx`, `vwaddu.vv/vx/wv/wx`, `vwsub.vv/vx/wv/wx`, `vwsubu.vv/vx/wv/wx` | 16 |
| Widening Multiply | `vwmul.vv/vx`, `vwmulu.vv/vx`, `vwmulsu.vv/vx` | 6 |
| Widening Multiply-Add | `vwmacc.vv/vx`, `vwmaccu.vv/vx`, `vwmaccsu.vv/vx`, `vwmaccus.vx` | 7 |
| Merge/Carry | `vmerge.vvm/vxm/vim`, `vadc.vvm/vxm/vim`, `vmadc.vv/vx/vi/vvm/vxm/vim` | 12 |
| Subtract with Borrow | `vsbc.vvm/vxm`, `vmsbc.vv/vx/vvm/vxm` | 6 |
| Data Move | `vmv.v.v`, `vmv.v.x`, `vmv.v.i`, `vmv.x.s`, `vmv.s.x` | 5 |
| Whole Register Move | `vmv1r.v`, `vmv2r.v`, `vmv4r.v`, `vmv8r.v` | 4 |
| Mask Logic | `vmand.mm`, `vmnand.mm`, `vmandn.mm`, `vmxor.mm`, `vmor.mm`, `vmnor.mm`, `vmorn.mm`, `vmxnor.mm` | 8 |
| Mask Bit Operations | `vmsbf.m`, `vmsif.m`, `vmsof.m` | 3 |
| Mask Count | `vcpop.m`, `vfirst.m`, `viota.m`, `vid.v` | 4 |
| Reduction | `vredsum.vs`, `vredand.vs`, `vredor.vs`, `vredxor.vs`, `vredmin.vs`, `vredminu.vs`, `vredmax.vs`, `vredmaxu.vs` | 8 |
| Widening Reduction | `vwredsum.vs`, `vwredsumu.vs` | 2 |
| Permutation | `vcompress.vm`, `vrgather.vv/vx/vi`, `vrgatherei16.vv`, `vslideup.vx/vi`, `vslidedown.vx/vi`, `vslide1up.vx`, `vslide1down.vx` | 10 |
| Sign/Zero Extension | `vsext.vf2/vf4/vf8`, `vzext.vf2/vf4/vf8` | 6 |

### Zvbb Extension (`zvbb/`) — 15 Instructions

| Category | Instructions | Count |
|----------|-------------|-------|
| Bitwise AND-NOT | `vandn.vv`, `vandn.vx` | 2 |
| Rotate | `vrol.vv/vx`, `vror.vv/vx/vi` | 5 |
| Bit/Byte Reverse | `vbrev.v`, `vrev8.v` | 2 |
| Leading/Trailing Zero Count | `vclz.v`, `vctz.v` | 2 |
| Population Count | `vcpop.v` | 1 |
| Widening Shift Left | `vwsll.vv/vx/vi` | 3 |

### Zvbc Extension (`zvbc/`) — 4 Instructions

| Category | Instructions | Count |
|----------|-------------|-------|
| Carryless Multiply | `vclmul.vv/vx`, `vclmulh.vv/vx` | 4 |

---

## Test Methodology

Each test case follows a unified **"Configure-Execute-Compare"** three-step verification pattern:

1. **Configuration Phase**
   - Randomly generate instruction encoding (operand register numbers, immediates, etc.)
   - Randomly generate operand data (with boundary value bias support)
   - Check instruction legality (`check_illegal`), with configurable illegal instruction ratio

2. **Execution Phase**
   - Inject instructions into executable memory via the JIT engine and run on the target CPU
   - Capture execution results (including `SIGILL` illegal instruction exceptions)

3. **Comparison Phase**
   - Compute expected results using the software golden model (`run_self_result`)
   - Compare hardware execution results with golden model results element by element
   - Output detailed error information (including CSR state, register data) when mismatches are found

Each instruction covers **8/16/32/64-bit** four element widths (SEW) by default. Some instructions (e.g., Zvbc's `vclmul`) are only legal under specific SEW values.

---

## Prerequisites

- **Cross-compilation Toolchain**: `riscv64-unknown-linux-gnu-gcc` / `riscv64-unknown-linux-gnu-g++`
- **C++ Standard**: C++17
- **QEMU**: `qemu-riscv64` (for emulator execution)
- **Target Architecture**:
  - V extension tests: `rv64gcv`
  - Zvbb extension tests: `rv64gcv_zvbb`
  - Zvbc extension tests: `rv64gcv_zvbc`

---

## Build & Run

### Compile

```bash
# Compile V extension test cases
cd vx && make

# Compile Zvbb extension test cases
cd zvbb && make

# Compile Zvbc extension test cases
cd zvbc && make
```

### Run on QEMU

```bash
# Run all test cases in the vx directory
cd vx && make qemu

# Run a single V extension test case
qemu-riscv64 vadd.vv.elf

# Run Zvbb/Zvbc test cases (cpu parameter is optional)
cd zvbb && make qemu
cd zvbc && make qemu <cpu=cpu_model>
```

### Command-Line Arguments

| Argument | Description | Default |
|----------|-------------|---------|
| `--data` | Switch to replay mode (read test cases from `.data` file) | `false` (default is Fuzzing mode) |
| `--runtime <N>` | Number of iterations | `1000` |
| `--seed <N>` | Random seed (for reproducibility) | `0` (auto-generated) |
| `--record` | Record test cases to `.data` file | `false` |
| `--early-stop` | Stop at first error | `false` |
| `--illegal-percent <F>` | Illegal instruction ratio, range [0,1] | `-1` (unrestricted) |
| `--max-retry <N>` | Maximum retry count for illegal instructions | `10` |
| `--log-level <L>` | Log level: `NONE`/`ERROR`/`WARN`/`INFO`/`DETAIL`/`VERBOSE`/`DEBUG` | `ERROR` |

### Usage Examples

```bash
# Fuzzing mode: random test 5000 iterations, specify seed, record test cases
qemu-riscv64 vadd.vv.elf --runtime 5000 --seed 42 --record

# Replay mode: read previously recorded test cases from .data file
qemu-riscv64 vadd.vv.elf --data --runtime 5000

# Fuzzing mode, stop at first error, verbose logging
qemu-riscv64 vadd.vv.elf --runtime 10000 --early-stop --log-level INFO
```

---

## Execution Modes

### Fuzzing Mode (Default)

Randomly generates instruction encodings and operand data, suitable for large-scale fuzz testing. Use `--seed` to specify a seed for reproducibility, and `--record` to save test cases to a `.data` file for later replay.

### Replay Mode (`--data`)

Reads pre-recorded test cases from a `.data` file for regression verification, suitable for bug reproduction and continuous integration.

---

## Data File Format

Test data is stored in JSON format in `.data` files. Each record contains the following fields:

| Field | Description |
|-------|-------------|
| `CSR` | CSR register values (`vtype`, `vcsr`, `vl`, `vstart`, etc.) |
| `DATA` | Operand data (`vs1`, `vs2`, `vd`, `vm`, etc., as hex arrays) |
| `INST` | Instruction encoding (hexadecimal) |
| `DESC` | Instruction description (field value mapping) |

### Example

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

## Core Architecture

### Test Framework (`common/framework.h`)

- **Configuration Management**: Manages test configurations (CSR register values, operand data, instruction encoding) via the `c_cfg` struct, with JSON serialization/deserialization support
- **Random Number Engine**: Based on `std::mt19937_64`, supports seed-based reproducibility, provides both `get_rand<T>()` full-range random and `get_rand_bound<T>()` boundary-value-biased random generation
- **JIT Execution Engine**: The `run_instruction()` function writes instruction sequences into `mmap`-allocated executable memory, synchronizes via `fence.i`, and executes directly on the target CPU
- **Signal Handling**: Registers a `SIGILL` signal handler to catch illegal instruction exceptions and verify the correctness of illegal instruction detection
- **Instruction Encoding Generation**: Describes instruction field layouts via `InstField`, and the `get_inst()` function randomly generates legal/illegal instruction encodings according to field definitions

### Vector Instruction Utility Library (`common/v_common.h`)

- **Vector Register Operations**: `load_vector` / `store_vector` — dynamically generates `vle`/`vse` instructions to load/store vector registers
- **CSR Operations**: `vsetvli_lmul_sew()` — configures vector length and element width; `csrrw()` — writes to CSR registers
- **Legality Checks**: Implements various constraint checks from the RISC-V V extension specification:
  - `is_legal_sss` — single-width source/destination register alignment check
  - `is_legal_widen_sss` — widening instruction legality check
  - `is_legal_narrow_sss` — narrowing instruction legality check
  - `is_legal_ext_check` — sign/zero extension instruction check
  - `is_legal_reduction` — reduction instruction check
  - `vext_check_load` / `vext_check_store` — memory access instruction legality check
  - `is_no_overlap` — register group overlap check (including Section 5.2 three rules)
- **Context Save/Restore**: `save_context` / `restore_context` — saves and restores general-purpose register state before and after JIT execution
- **Bit Operation Helpers**: `ror` / `rol` / `brev` — rotate right, rotate left, bit reverse operations (used for Zvbb golden model)
