# DAMO-RV-ATS

**English** | [中文](README_cn.md)

`DAMO-RV-ATS` is a self-checking compatibility test framework for RISC-V ISA extensions. It drives randomized instruction sequences at a target implementation and compares the results against a software golden model built into every test case, so a run validates actual architectural behaviour instead of merely checking that an instruction does not trap.

Current coverage: **667 instruction test cases** spanning the RVV 1.0 base extension (integer, floating-point, load/store) plus the vector bit-manipulation and vector cryptography extensions.

---

## Highlights

- **Self-checking** — every case implements a golden model (`run_self_result`) and compares it element by element with the value produced by the target; a mismatch dumps the CSR state, vector configuration and register contents.
- **Randomized stimulus** — instruction encodings, operand data (biased toward boundary values) and the vector configuration are re-randomized each iteration, and any run can be reproduced from its seed.
- **Illegal-instruction coverage** — a `SIGILL` handler verifies that reserved encodings and illegal register-group combinations are actually rejected, and reports it when an expected trap does not occur.
- **Three execution flows** — QEMU user mode, native RISC-V Linux, and a generated bare-metal image with no OS dependency that can also run in the Sail model.
- **Record & replay** — fuzzing results are stored as JSON `.data` files and can be replayed deterministically for regression testing and bug reproduction.
- **One file per instruction** — a new test is a single `.cpp` plus the header-only framework; there is no runner infrastructure to learn, and the extension `Makefile` picks new files up automatically.

---

## Supported Extension Sets

Each extension set lives in its own top-level directory and can be built and run independently.

### RVV 1.0 base extension

| Directory | Scope | Cases | `-march` |
|-----------|-------|-------|----------|
| `vx/` | **Integer** — add/subtract with carry and borrow, logical and shift operations, min/max, compare, multiply/divide/remainder, multiply-add, widening and narrowing arithmetic, fixed-point (saturating and rounding) operations, mask logic and mask bit operations, reductions, permutations, data moves, sign/zero extension | 213 | `rv64gcv` |
| `vf/` | **Floating-point** — add/subtract/multiply/divide/square-root, min/max, sign-inject, the full fused multiply-add family, widening FP arithmetic, FP compare, FP↔integer and FP↔FP conversions (including round-toward-zero and round-to-odd), FP reductions, `vfrec7`/`vfrsqrt7`, FP moves and slides | 101 | `rv64gcv` |
| `vls/` | **Loads, stores and configuration** — unit-stride, strided and indexed (ordered/unordered) accesses, segment variants (2–8 fields) of each, mask load/store, fault-only-first loads, whole-register moves, and `vsetvli`/`vsetivli`/`vsetvl` | 313 | `rv64gcv` |

### Vector bit-manipulation and cryptography

| Directory | Extension | Scope | Cases | `-march` |
|-----------|-----------|-------|-------|----------|
| `zvbb/` | Zvbb | AND-NOT, rotate left/right, bit and byte reverse, count leading/trailing zeros, population count, widening shift | 15 | `rv64gcv_zvbb` |
| `zvbc/` | Zvbc | Carryless multiply, low and high half | 4 | `rv64gcv_zvbc` |
| `zvkg/` | Zvkg | GCM/GMAC Galois field multiply-accumulate | 2 | `rv64gcv_zvkg` |
| `zvkned/` | Zvkned | AES-128 encryption/decryption rounds, key expansion, whitening | 11 | `rv64gcv_zvkned` |
| `zvknh/` | Zvknhb | SHA-2 message schedule and compression | 3 | `rv64gcv_zvknhb` |
| `zvksed/` | Zvksed | SM4 rounds and key expansion | 3 | `rv64gcv_zvksed` |
| `zvksh/` | Zvksh | SM3 message expansion and compression | 2 | `rv64gcv_zvksh` |

`rv32` targets are supported as well — build with `xlen=32`, which switches the ABI to `ilp32d` and runs under `qemu-riscv32`.

---

## Randomized Dimensions

Per iteration the framework randomizes:

- **`vtype`** — SEW (8/16/32/64), LMUL (1/8 through 8), `vta`, `vma`
- **`vl` and `vstart`** — including partial and empty vector bodies
- **`vcsr`** — `vxrm` rounding mode and `vxsat` saturation flag
- **Encodings** — operand register numbers, immediates, and the `vm` mask bit
- **Operand data** — full-range random with a bias toward boundary values (zero, ±1, signed/unsigned extremes, powers of two, and NaN/Inf patterns for FP tests)
- **A configurable fraction of illegal encodings** (`--illegal-percent`)

Before execution each generated case is checked against the extension's legality constraints — register-group alignment, widening/narrowing overlap rules, EEW/EMUL combinations, and memory alignment. Encodings judged illegal must raise `SIGILL`; encodings judged legal must not.

Note that not every instruction is legal under every SEW (Zvbc, for example, is only defined for `SEW=64`, and the AES/SM3/SM4/GCM instructions only for `SEW=32`), so each test constrains its own configuration space.

---

## Project Layout

```
damo-rv-ats/
├── Makefile                  # Top-level dispatcher; iterates over EXTENSIONS
├── common/                   # Header-only framework and helpers
│   ├── framework.h           # Config management, RNG, JIT execution, SIGILL handling, CLI, record/replay
│   ├── v_common.h            # Vector utilities: register load/store, vsetvl, legality checks
│   ├── f_common.h            # Software FP/integer model types, NaN/Inf classification, rounding-aware conversion
│   ├── v_crypto_common.h     # Golden-model primitives shared by the vector crypto extensions
│   └── baremetal_gen.h       # Records semantics during fuzzing and emits a standalone bare-metal program
├── thirdparty/               # argparse.hpp, json.hpp (nlohmann/json)
├── NORM/                     # Normative-rule reference for the V extension and rule-to-coverpoint mapping
├── vx/ vf/ vls/              # RVV 1.0 base extension test cases
├── zvbb/ zvbc/               # Vector bit-manipulation test cases
├── zvkg/ zvkned/ zvknh/      # Vector cryptography test cases
├── zvksed/ zvksh/
├── LICENSE                   # Apache-2.0
├── .clang-format             # Code style
└── .pre-commit-config.yaml   # Pre-commit hooks
```

Every extension directory holds one `.cpp` per instruction and a `Makefile` exposing the same set of targets.

---

## Prerequisites

- **Cross toolchain** — `riscv64-unknown-linux-gnu-g++` (default) or `riscv64-unknown-linux-gnu-clang++` with `COMPILER=clang`; C++17, statically linked output
- **QEMU user mode** — `qemu-riscv64` (or `qemu-riscv32` for `xlen=32`)
- **Optional, bare-metal flow only** — `riscv64-unknown-elf-gcc` and `objdump`, `qemu-system-riscv64`, `sail_riscv_sim`

---

## Build & Run

### From the project root

```bash
make                          # Build every extension in the default EXTENSIONS list
make EXTENSIONS="vx zvbb"     # Build a subset
make COMPILER=clang           # Build with clang
make xlen=32                  # Build for rv32
make DEBUG=1                  # Build with -O0 -g

make qemu                     # Build and run everything under QEMU user mode
make test                     # Run natively (requires a RISC-V host)
make clean
make help                     # List all targets and options
```

### A single extension or a single case

```bash
cd vx && make -j8             # Build one extension
cd vx && make qemu            # Build and run it under QEMU

qemu-riscv64 vx/vadd.vv.elf --runtime 5000 --seed 42
```

### Bare-metal flow

The bare-metal flow first fuzzes under QEMU user mode, records the semantics of each iteration, then emits a standalone program that can be compiled and executed without an OS.

```bash
make baremetal-generate EXTENSIONS=vx CASE=vadd.vv RUNTIME=100   # Emit .baremetal.c / _entry.S / .ld
make baremetal-compile  EXTENSIONS=vx CASE=vadd.vv               # Build <case>.baremetal.elf (+ .asm)
make baremetal-run      EXTENSIONS=vx CASE=vadd.vv               # Run in qemu-system-riscv64 -M virt -cpu max
make baremetal-sail     EXTENSIONS=vx CASE=vadd.vv               # Run in the Sail RISC-V model
```

Omit `CASE=` to process every test in the extension. Generated files are data-address independent (`la`), skip illegal instructions through a trap handler (`mepc += 4`), and signal completion with an `ecall`. `baremetal-run` reports the illegal-instruction and ecall counts taken from the QEMU interrupt log.

---

## Command-Line Arguments

| Argument | Description | Default |
|----------|-------------|---------|
| `--data` | Replay mode — read cases from the `.data` file instead of fuzzing | `false` |
| `--runtime`, `-r` | Number of iterations. In replay mode with no explicit value, every case in the file is run | `1000` |
| `--seed`, `-s` | Fix the random seed for reproducibility | `0` (auto) |
| `--record` | Write executed cases to the `.data` file | `false` |
| `--early-stop`, `-e` | Stop at the first mismatch instead of continuing | `false` |
| `--illegal-percent` | Fraction of illegal encodings to generate, range `[0,1]` | `-1` (unrestricted) |
| `--max-retry` | Maximum retries when generating an illegal instruction | `10` |
| `--log-level`, `-l` | `NONE` / `ERROR` / `WARN` / `INFO` / `DETAIL` / `VERBOSE` / `DEBUG` | `ERROR` |
| `--baremetal` | Emit bare-metal sources alongside the data file (implies `--record`) | `false` |

```bash
# Fuzz 5000 iterations with a fixed seed and record the cases
qemu-riscv64 vx/vadd.vv.elf --runtime 5000 --seed 42 --record

# Replay a recorded file
qemu-riscv64 vx/vadd.vv.elf --data

# Stop at the first error with detailed logging
qemu-riscv64 vx/vadd.vv.elf --runtime 10000 --early-stop --log-level INFO
```

Mismatches are reported through the `ERROR` channel together with the CSR values and vector configuration of the failing iteration; a run ends with a summary line reporting the number of expected-illegal cases and iterations executed.

---

## Execution Modes

### Fuzzing (default)

Generates a fresh encoding, configuration and operand set each iteration. Use `--seed` to make a run reproducible and `--record` to persist it for later replay.

### Replay (`--data`)

Reads previously recorded cases from `<case>.cpp.data` and re-executes them deterministically — the mode used for regression suites and bug reproduction.

### Bare-metal (`--baremetal`)

Runs the normal Linux/QEMU flow while recording enough state to reconstruct each iteration as freestanding code, then writes the bare-metal sources at exit.

---

## Data File Format

Recorded cases are stored as a JSON array. Each element carries everything needed to reproduce one iteration:

| Field | Description |
|-------|-------------|
| `CSR` | CSR values — `vtype`, `vcsr`, `vl`, `vstart`, … |
| `DATA` | Operand data — `vs1`, `vs2`, `vd`, `vm`, … as hex arrays |
| `INST` | The instruction encoding, in hex |
| `DESC` | Decoded field values of the encoding |

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

### `common/framework.h`

- **Configuration management** — `c_cfg` holds the CSR values, operand data and encoding of one iteration and serializes to/from JSON.
- **Random engine** — `std::mt19937_64` seeded from `--seed` or `std::random_device`; `get_rand<T>()` for full-range values and `get_rand_bound<T>()` for boundary-biased ones.
- **JIT execution** — `run_instruction()` copies the generated instruction sequence into `mmap`-allocated executable memory, issues `fence.i`, and executes it in place on the target.
- **Signal handling** — a `SIGILL` handler records whether the expected trap was raised.
- **Encoding generation** — an `InstField` table describes the bit layout of an instruction; `get_inst()` fills the fields randomly, honouring the requested legality and the `--illegal-percent` ratio.

### `common/v_common.h`

- **Register access** — `load_vector` / `store_vector` and their multi-register variants emit `vle`/`vse` sequences around the instruction under test.
- **CSR setup** — `vsetvli_lmul_sew()` configures length and element width; `csrrw()` programs `vstart`, `vxrm` and friends.
- **Legality checks** — the constraint set from the V specification: single-width, widening and narrowing register-group alignment, extension and reduction checks, memory-access checks, and register-group overlap rules.
- **Context save/restore** — general-purpose register state around the JIT'd sequence.

### `common/f_common.h`

Software model types for the floating-point tests: `fp16`/`bf16` and narrow integer types with defined arithmetic, NaN/Inf classification, and conversion helpers parameterized by rounding mode (`frm`) with flag reporting.

### `common/v_crypto_common.h`

Shared golden-model primitives for the crypto extensions — bit rotate, byte reverse, effective-LMUL bit accounting, and the AES/SHA/SM3/SM4/GCM round functions.

### `common/baremetal_gen.h`

Hooks the Linux fuzzing path, records the vector/scalar/memory state each iteration consumes, and emits `<case>.cpp.baremetal.c`, `<case>.cpp.baremetal_entry.S`, `<case>.cpp.baremetal.ld` and `<case>.cpp.baremetal.mk`; `baremetal-compile` turns these into `<case>.baremetal.elf`.

---

## Normative Rule Reference (`NORM/`)

- `NORM/v_norm.md` — 435 normative rules extracted from the RISC-V "V" Extension v1.0 specification, grouped by chapter, with a Chinese explanation and the instructions/CSRs each rule applies to.
- `NORM/vx.yaml` — machine-readable mapping from normative rule names to coverpoints.

---

## Adding a Test Case

1. Copy an existing `.cpp` from the closest matching extension directory.
2. Describe the encoding as an `InstField` table.
3. Implement `check_illegal()` — the operand and configuration constraints for this instruction.
4. Implement `run_self_result<Ts1, Ts2, Td>()` — the golden model, honouring `vstart` and the mask.
5. Implement `per_run()` — build the instruction sequence (`save_context` → load operands → `vsetvli_lmul_sew` → the instruction → store results → `restore_context`), call `run_instruction()`, then compare.
6. Drop the file into the extension directory; the `Makefile` wildcard picks it up, and the top-level `EXTENSIONS` list already includes the directory.

---

## License

Apache License 2.0 — see [LICENSE](LICENSE).
