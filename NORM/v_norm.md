# RISC-V V 扩展 (向量扩展) Normative Rules 汇总表

> 本文档从 `SPEC/v-st-ext.adoc` (RISC-V "V" Standard Extension for Vector Operations, Version 1.0) 中提取全部 normative rules，
> 按原文档章节结构分组整理。共计 **435** 条 normative rules。

---

## 目录

1. [Implementation-defined Constant Parameters](#Implementation-defined-Constant-Parameters) (2 条)
2. [Vector Extension Programmer's Model](#Vector-Extension-Programmers-Model) (52 条)
3. [Mapping of Vector Elements to Vector Register State](#Mapping-of-Vector-Elements-to-Vector-Register-State) (5 条)
4. [Vector Instruction Formats](#Vector-Instruction-Formats) (23 条)
5. [Configuration-Setting Instructions (`vsetvli`/`vsetivli`/`vsetvl`)](#Configuration-Setting-Instructions-vsetvli-vsetivli-vsetvl) (14 条)
6. [Vector Loads and Stores](#Vector-Loads-and-Stores) (62 条)
7. [Vector Memory Alignment Constraints](#Vector-Memory-Alignment-Constraints) (3 条)
8. [Vector Memory Consistency Model](#Vector-Memory-Consistency-Model) (7 条)
9. [Vector Arithmetic Instruction Formats](#Vector-Arithmetic-Instruction-Formats) (16 条)
10. [Vector Integer Arithmetic Instructions](#Vector-Integer-Arithmetic-Instructions) (38 条)
11. [Vector Fixed-Point Arithmetic Instructions](#Vector-Fixed-Point-Arithmetic-Instructions) (14 条)
12. [Vector Floating-Point Instructions](#Vector-Floating-Point-Instructions) (47 条)
13. [Vector Reduction Operations](#Vector-Reduction-Operations) (22 条)
14. [Vector Mask Instructions](#Vector-Mask-Instructions) (46 条)
15. [Vector Permutation Instructions](#Vector-Permutation-Instructions) (48 条)
16. [Exception Handling](#Exception-Handling) (1 条)
17. [Standard Vector Extensions](#Standard-Vector-Extensions) (29 条)
18. [Vector Element Groups](#Vector-Element-Groups) (6 条)

---

## 1. Implementation-defined Constant Parameters (2 条)

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 1 | `elen` | The maximum size in bits of a vector element that any operation can produce or consume, _ELEN_  8, which must be a power of 2. | 任何操作可产生或消费的向量元素的最大位宽 ELEN≥8，且必须是2的幂 |  |
| 2 | `vlen` | The number of bits in a single vector register, _VLEN_  ELEN, which must be a power of 2, and must be no greater than 216. | 单个向量寄存器的位数 VLEN≥ELEN，必须是2的幂，且不超过2^16 |  |

## 2. Vector Extension Programmer's Model (52 条)

### Vector Registers

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 3 | `vreg_count` | The vector extension adds 32 architectural vector registers, v0-v31 to the base scalar RISC-V ISA. | 向量扩展增加32个架构向量寄存器 v0-v31 |  |

### Vector Context Status in `mstatus`

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 4 | `mstatus_vs_sstatus_vs_op` | A vector context status field, VS, is added to mstatus[10:9] and shadowed in sstatus[10:9].  It is defined analogously to the floating-point context status field, FS. | 向量上下文状态字段 VS 添加到 mstatus[10:9] 并映射到 sstatus[10:9] | `MSTATUS` |
| 5 | `mstatus_vs_op_off` | Attempts to execute any vector instruction, or to access the vector CSRs, raise an illegal-instruction exception when mstatus.VS is set to Off. | mstatus.VS=Off 时，执行向量指令或访问向量CSR引发非法指令异常 | `MSTATUS` |
| 6 | `mstatus_vs_op_initial_clean` | When mstatus.VS is set to Initial or Clean, executing any instruction that changes vector state, including the vector CSRs, will change mstatus.VS to Dirty. Implementations may also change mstatus.... | mstatus.VS=Initial/Clean 时，改变向量状态的指令将其改为 Dirty | `MSTATUS` |
| 7 | `mstatus_sd_op2` | If mstatus.VS is Dirty, mstatus.SD is 1; otherwise, mstatus.SD is set in accordance with existing specifications. | 实现可在不改变向量状态时将 mstatus.VS 从 Initial/Clean 改为 Dirty | `MSTATUS` |
| 8 | `mutable_misa_v` | Implementations may have a writable misa.V field. | 实现可以有可写的 misa.V 字段 | `MSTATUS` |
| 9 | `mstatus_vs_exists` | Analogous to the way in which the floating-point unit is handled, the mstatus.VS field may exist even if misa.V is clear. | misa.V 被清除时，VS 字段行为与 FS 在 misa.F 被清除时相同 | `MSTATUS` |

### Vector Context Status in `vsstatus`

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 10 | `vsstatus_vs_sz_acc` | When the hypervisor extension is present, a vector context status field, VS, is added to vsstatus[10:9]. It is defined analogously to the floating-point context status field, FS. | V=1 时，vsstatus.VS 字段行为与 mstatus.VS 类似 | `VSSTATUS` |
| 11 | `vsstatus_vs_mstatus_vs_op_off` | When V=1, both vsstatus.VS and mstatus.VS are in effect: attempts to execute any vector instruction, or to access the vector CSRs, raise an illegal-instruction exception when either field is set to... | V=1 且 vsstatus.VS=Off 时，执行向量指令引发虚拟指令异常 | `VSSTATUS` |
| 12 | `vsstatus_vs_mstatus_vs_op_active` | When V=1 and neither vsstatus.VS nor mstatus.VS is set to Off, executing any instruction that changes vector state, including the vector CSRs, will change both mstatus.VS and vsstatus.VS to Dirty. | V=1 且两者都不为 Off 时，改变向量状态将两者都改为 Dirty | `VSSTATUS` |
| 13 | `hw_mstatus_vs_dirty_update` | Implementations may also change mstatus.VS or vsstatus.VS from Initial or Clean to Dirty at any time, even when there is no change in vector state. | 实现可在不改变向量状态时将 mstatus.VS/vsstatus.VS 改为 Dirty | `MSTATUS` |
| 14 | `vsstatus_sd_op_vs` | If vsstatus.VS is Dirty, vsstatus.SD is 1; otherwise, vsstatus.SD is set in accordance with existing specifications. | vsstatus.VS=Dirty 时，vsstatus.SD 被设置 | `VSSTATUS` |
| 15 | `mstatus_sd_op_vs` | If mstatus.VS is Dirty, mstatus.SD is 1; otherwise, mstatus.SD is set in accordance with existing specifications. | mstatus.VS=Dirty 时，mstatus.SD 被设置 | `MSTATUS` |
| 16 | `vsstatus_vs_exists` | For implementations with a writable misa.V field, the vsstatus.VS field may exist even if misa.V is clear. | Hypervisor 扩展未实现时，vsstatus 不存在 | `VSSTATUS` |

### Vector Type (`vtype`) Register

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 17 | `vtype_sz_acc_op` | The read-only XLEN-wide _vector_ _type_ CSR, vtype provides the default type used to interpret the contents of the vector register file, and can only be updated by vsetvl instructions. | 只读 XLEN 位宽 vtype CSR 提供向量指令执行的默认数据类型 | `VTYPE` |
| 18 | `vtype_fields_sz` | The vtype register has five fields, vill, vma, vta, vsew[2:0], and vlmul[2:0].  Bits vtype[XLEN-2:8] should be written with zero, and non-zero values in this field are reserved. | vtype 有五个字段：vill、vma、vta、vsew[2:0]、vlmul[2:0] | `VTYPE` |
| 19 | `vill_implicit_encoding` | A small implementation supporting ELEN=32 requires only seven bits of state in vtype: two bits for ma and ta, two bits for vsew[1:0] and three bits for vlmul[2:0].  The illegal value represented by... | 小型实现(ELEN=32)只需 vtype 中7位状态，vill 可用非法编码隐式表示 | `VTYPE` |
| 20 | `vtype_vsew_op` | The value in vsew sets the dynamic _selected_ _element_ _width_ (SEW).  By default, a vector register is viewed as being divided into VLEN/SEW elements. | vsew 设置动态选择的元素宽度(SEW)，寄存器划分为 VLEN/SEW 个元素 | vsetvli/vsetivli/vsetvl |
| 21 | `vtype_vsew_rsv` | While it is anticipated the larger vsew[2:0] encodings (100-111) will be used to encode larger SEW, the encodings are formally _reserved_ at this point. | 较大的 vsew[2:0] 编码(100-111)目前正式保留 | vsetvli/vsetivli/vsetvl |
| 22 | `vtype_lmul_val` | Implementations must support LMUL integer values of 1, 2, 4, and 8. | 实现必须支持 LMUL 整数值 1、2、4 和 8 | `VTYPE` |
| 23 | `vtype_lmul_fval` | Implementations must provide fractional LMUL settings that allow the narrowest supported type to occupy a fraction of a vector register corresponding to the ratio of the narrowest supported type's ... | 实现必须提供分数 LMUL 设置，要求 LMUL≥SEW_MIN/ELEN | `VTYPE` |
| 24 | `vtype_sew_val` | For a given supported fractional LMUL setting, implementations must support SEW settings between SEW~MIN~ and LMUL * ELEN, inclusive. | 对于给定分数 LMUL，实现必须支持 SEW_MIN 到 LMUL*ELEN 之间的 SEW | `VTYPE` |
| 25 | `vtype_lmul_fval_rsv` | The use of vtype encodings with LMUL < SEW~MIN~/ELEN is __reserved__, but implementations can set vill if they do not support these configurations. | LMUL<SEW_MIN/ELEN 的 vtype 编码保留，实现可设置 vill | vsetvli/vsetivli/vsetvl |
| 26 | `lmul` | LMUL is set by the signed vlmul field in vtype (i.e., LMUL = 2vlmul[2:0]). | LMUL 由 vtype 中有符号 vlmul 字段设置，LMUL=2^vlmul[2:0] | `VTYPE` |
| 27 | `vlmax` | The derived value VLMAX = LMUL*VLEN/SEW represents the maximum number of elements that can be operated on with a single vector instruction given the current SEW and LMUL settings as shown in the ta... | VLMAX=LMUL*VLEN/SEW，单条向量指令可操作的最大元素数 | `VTYPE` |
| 28 | `vreg_offgroup_lmul2_rsv` | Instructions specifying an LMUL=2 vector register group with an odd-numbered vector register are reserved. | LMUL=2 时使用奇数编号向量寄存器的指令保留 | `VTYPE` |
| 29 | `vreg_offgroup_lmul4_rsv` | instructions specifying an LMUL=4 vector register group using vector register numbers that are not multiples of four are reserved. | LMUL=4 时使用非4倍数寄存器编号的指令保留 | `VTYPE` |
| 30 | `vreg_offgroup_lmul8_rsv` | instructions specifying an LMUL=8 vector register group using register numbers that are not multiples of eight are reserved. | LMUL=8 时使用非8倍数寄存器编号的指令保留 | `VTYPE` |
| 31 | `vreg_mask_lmul_indp` | Mask registers are always contained in a single vector register, regardless of LMUL. | 掩码寄存器始终在单个向量寄存器中，与 LMUL 无关 | `VTYPE` |
| 32 | `vtype_vta-vma_op` | These two bits modify the behavior of destination tail elements and destination inactive masked-off elements respectively during the execution of vector instructions.  The tail and inactive sets co... | vta/vma 两位分别修改目标尾部元素和非活跃掩码元素的行为 | vsetvli/vsetivli/vsetvl |
| 33 | `vtype_vta-vma_val` | All systems must support all four options: | 所有系统必须支持全部四种 vta/vma 选项组合 | vsetvli/vsetivli/vsetvl |
| 34 | `vreg_mask_tail_agn` | Mask destination tail elements are always treated as tail-agnostic, regardless of the setting of vta. | 掩码目标尾部元素始终被视为 tail-agnostic，与 vta 无关 | `VTYPE` |
| 35 | `vreg_mask_op` | When a set is marked undisturbed, the corresponding set of destination elements in a vector register group retain the value they previously held. | 标记为 undisturbed 的元素保持先前值 | `VTYPE` |
| 36 | `vreg_agnostic_op` | When a set is marked agnostic, the corresponding set of destination elements in any vector destination operand can either retain the value they previously held, or are overwritten with 1s.  Within ... | 标记为 agnostic 的元素可保持原值或被1覆盖，模式不要求确定性 | `VTYPE` |
| 37 | `vreg_mask_tail_op` | In addition, except for mask load instructions, any element in the tail of a mask result can also be written with the value the mask-producing operation would have calculated with vl=VLMAX. Further... | 除掩码加载外，掩码结果尾部可用 vl=VLMAX 时的计算值写入 | `VTYPE` |
| 38 | `vtype_vill_op` | If the vill bit is set, then any attempt to execute a vector instruction that depends upon vtype will raise an illegal-instruction exception. | vill 被设置时，执行依赖 vtype 的向量指令引发非法指令异常 | `VTYPE` |

### Vector Length (`vl`) Register

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 39 | `vl_acc` | The _XLEN_-bit-wide read-only vl CSR can only be updated by the vset{i}vl{i} instructions, and the _fault-only-first_ vector load instruction variants. | 只读 vl CSR 只能由 vsetvli/vsetivli/vsetvl 和 fault-only-first 加载更新 | vsetvli/vsetivli/vsetvl |
| 40 | `vl_op` | The vl register holds an unsigned integer specifying the number of elements to be updated with results from a vector instruction, as further detailed in <<sec-inactive-defs>>. | vl 保存无符号整数，指定向量指令要更新结果的元素数量 | `VL` |

### Vector Byte Length (`vlenb`) Register

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 41 | `vlenb_acc_op` | The _XLEN_-bit-wide read-only CSR vlenb holds the value VLEN/8, i.e., the vector register length in bytes. | 只读 CSR vlenb 保存 VLEN/8，即向量寄存器长度(字节) | `VLENB` |

### Vector Start Index (`vstart`) Register

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 42 | `vstart_acc_sz` | The _XLEN_-bit-wide read-write vstart CSR specifies the index of the first element to be executed by a vector instruction, as described in <<sec-inactive-defs>>. | 读写 vstart CSR 指定向量指令要执行的第一个元素索引 | `VSTART` |
| 43 | `vstart_op` | All vector instructions are defined to begin execution with the element number given in the vstart CSR, leaving earlier elements in the destination vector undisturbed | 所有向量指令从 vstart 给出的元素编号开始执行，保持较早元素不变 | `VSTART` |
| 44 | `vstart_update` | reset the vstart CSR to zero at the end of execution. | 执行结束时将 vstart CSR 重置为零 | `VSTART` |
| 45 | `vstart_unmodified` | vstart is not modified by vector instructions that raise illegal-instruction exceptions. | 引发非法指令异常的向量指令不修改 vstart | `VSTART` |
| 46 | `vstart_sz_writable` | The vstart CSR is defined to have only enough writable bits to hold the largest element index (one less than the maximum VLMAX). | vstart CSR 仅有足够可写位保存最大元素索引(VLMAX-1) | `VSTART` |
| 47 | `vstart_val_rsv` | The use of vstart values greater than the largest element index for the current vtype setting is reserved. | 使用大于当前 vtype 最大元素索引的 vstart 值保留 | `VSTART` |
| 48 | `vstart_vtype_dep` | Implementations are permitted to raise illegal-instruction exceptions when attempting to execute a vector instruction with a value of vstart that the implementation can never produce when executing... | 实现可在 vstart 值不可能由该指令产生时引发非法指令异常 | `VSTART` |

### Vector Fixed-Point Rounding Mode (`vxrm`) Register

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 49 | `vxrm_val_sz_acc` | The vector fixed-point rounding-mode register holds a two-bit read-write rounding-mode field in the least-significant bits (vxrm[1:0]).  The upper bits, vxrm[XLEN-1:2], should be written as zeros. | 向量定点舍入模式寄存器最低有效位保存两位读写舍入模式 vxrm[1:0] | `VXRM` |
| 50 | `vcsr_vxrm_op` | The vector fixed-point rounding-mode is given a separate CSR address to allow independent access, but is also reflected as a field in vcsr. | 向量定点舍入模式有单独 CSR 地址，也作为 vcsr 字段反映 | `VXRM` |
| 51 | `vxrm_op` | The fixed-point rounding algorithm is specified as follows. Suppose the pre-rounding result is v, and d bits of that result are to be rounded off. Then the rounded result is (v >> d) + r, where r d... | 定点舍入算法：舍入结果为 (v>>d)+r，r 取决于舍入模式 | `VXRM` |

### Vector Fixed-Point Saturation Flag (`vxsat`)

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 52 | `vxsat_op_acc_sz` | The vxsat CSR has a single read-write least-significant bit (vxsat[0]) that indicates if a fixed-point instruction has had to saturate an output value to fit into a destination format. Bits vxsat[X... | vxsat CSR 的 vxsat[0] 指示定点指令是否饱和了输出值 | `VXSAT` |
| 53 | `vcsr_vxsat_op` | The vxsat bit is mirrored in vcsr. | vxsat 位在 vcsr 中镜像 | `VXSAT` |

### Vector Control and Status (`vcsr`) Register

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 54 | `vcsr_vxrm-vxsat_acc` | The vxrm and vxsat separate CSRs can also be accessed via fields in the _XLEN_-bit-wide vector control and status CSR, vcsr. | vxrm 和 vxsat 也可通过 vcsr 中的字段访问 | `VCSR` |

## 3. Mapping of Vector Elements to Vector Register State (5 条)

### Mapping for LMUL = 1

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 55 | `vreg_lmul1_op` | When LMUL=1, elements are simply packed in order from the least-significant to most-significant bits of the vector register. | LMUL=1 时，元素按字节地址递增顺序映射到向量寄存器 | `VREG` |

### Mapping for LMUL < 1

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 56 | `vreg_flmul_op` | When LMUL < 1, only the first LMUL*VLEN/SEW elements in the vector register are used.  The remaining space in the vector register is treated as part of the tail, and hence must obey the vta setting. | LMUL<1 时，只使用向量寄存器中 VLEN*LMUL 个最低有效位 | `VREG` |

### Mapping for LMUL > 1

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 57 | `vreg_lmulge2_op` | When vector registers are grouped, the elements of the vector register group are packed contiguously in element order beginning with the lowest-numbered vector register and moving to the next-highe... | LMUL>1 时，元素按寄存器编号递增、字节地址递增顺序映射 | `VREG` |

### Mask Register Layout

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 58 | `vreg_mask_vtype_indp` | A vector mask occupies only one vector register regardless of SEW and LMUL. | 掩码寄存器布局与当前 SEW 和 LMUL 设置无关 | `VREG` |
| 59 | `vreg_mask_sz` | Each element is allocated a single mask bit in a mask vector register. The mask bit for element _i_ is located in bit _i_ of the mask register, independent of SEW or LMUL. | 掩码寄存器每元素占1位，单个向量寄存器可保存 VLEN 个掩码元素 | `VREG` |

## 4. Vector Instruction Formats (23 条)

### Scalar Operands

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 60 | `vreg_scalar_lmul_indp` | Any vector register can be used to hold a scalar regardless of the current LMUL setting. | 任何向量寄存器都可保存标量，与 LMUL 无关 |  |

### Vector Operands

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 61 | `eew_emul` | Each vector operand has an _effective_ _element_ _width_ (EEW) and an _effective_ LMUL (EMUL) that is used to determine the size and location of all the elements within a vector register group.  By... | 每个向量操作数有有效元素宽度(EEW)和有效LMUL(EMUL) |  |
| 62 | `eew_emul_sew_lmul_dep` | EEW/EMUL = SEW/LMUL. | EEW/EMUL = SEW/LMUL |  |
| 63 | `vnarrowing_eew_emul` | Narrowing instructions have a source operand that has EEW=2*SEW and EMUL=2*LMUL but with a destination where EEW=SEW and EMUL=LMUL. | 窄化指令源操作数 EEW=2*SEW，EMUL=2*LMUL |  |
| 64 | `emul_offgroup_rsv` | Using other than the lowest-numbered vector register to specify a vector register group is a reserved encoding. | 使用非最低编号寄存器作为 EMUL>1 操作数保留 |  |
| 65 | `vreg_source_eew_rsv` | A vector register cannot be used to provide source operands with more than one EEW for a single instruction.  A mask register source is considered to have EEW=1 for this constraint.  An encoding th... | 源 EEW 不是支持的宽度时指令编码保留 |  |
| 66 | `vreg_overlap_legal` | A destination vector register group can overlap a source vector register group only if one of the following holds: | 源和目标寄存器组重叠且 EMUL 不等时指令编码保留(除特定条件外) |  |
| 67 | `vreg_mask_overlap` | For the purpose of determining register group overlap constraints, mask elements have EEW=1. | 掩码寄存器可与任何源或目标向量寄存器组重叠 |  |
| 68 | `vreg_overlap_rsv` | Any instruction encoding that violates the overlap constraints is reserved. | 其他向量操作数与掩码操作数重叠且 EMUL>1 时指令编码保留 |  |
| 69 | `vreg_overlap_agn` | When source and destination registers overlap and have different EEW, the instruction is mask- and tail-agnostic, regardless of the setting of the vta and vma bits in vtype. | 目标 EEW≠源 EEW 且重叠时，非活跃和尾部元素必须用 agnostic 策略 |  |
| 70 | `emul_rsv` | The largest vector register group used by an instruction can not be greater than 8 vector registers (i.e., EMUL{le}8), and if a vector instruction would require greater than 8 vector registers in a... | EMUL 不在 [1/8,8] 范围内时指令编码保留 |  |
| 71 | `vreg_scalar_emul` | Widened scalar values, e.g., input and output to a widening reduction operation, are held in the first element of a vector register and have EMUL=1. | 标量操作数的 EMUL 始终为1 |  |

### Vector Masking

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 72 | `vmask_inactive_op` | Element operations that are masked off (inactive) never generate exceptions. | 被掩码关闭的元素操作永远不产生异常 |  |
| 73 | `vmask_agn_op` | The destination vector register elements corresponding to masked-off elements are handled with either a mask-undisturbed or mask-agnostic policy depending on the setting of the vma bit in vtype | 非活跃元素目标值取决于 vtype.vma 设置 |  |
| 74 | `vreg_vmask` | The mask value used to control execution of a masked vector instruction is always supplied by vector register v0. | 向量掩码使用 v0 寄存器 |  |
| 75 | `vreg_vmask_rsv` | The destination vector register group for a masked vector instruction cannot overlap the source mask register (v0), unless the destination vector register is being written with a mask value (e.g., ... | vm=0 但不支持掩码的指令编码保留 |  |
| 76 | `vmask_vm_enc` | Where available, masking is encoded in a single-bit vm field in the instruction (inst[25]). | vm=1 不使用掩码，vm=0 使用 v0 作为掩码 |  |

### Prestart, Active, Inactive, Body, and Tail Element Definitions

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 77 | `vector_prestart_element` | The prestart elements do not raise exceptions and do not update the destination vector register. | 预启动元素不引发异常，不更新目标向量寄存器 |  |
| 78 | `vector_active_element` | The active elements can raise exceptions and update the destination vector register group. | 活跃元素可引发异常并更新目标向量寄存器组 |  |
| 79 | `vector_inactive_element` | The inactive elements do not raise exceptions and do not update any destination vector register group unless masked agnostic is specified (vtype.vma=1), in which case inactive elements may be overw... | 非活跃元素不引发异常，不更新目标(除非 vma=1 时可被1覆盖) |  |
| 80 | `vector_tail_element` | The tail elements do not raise exceptions, and do not update any destination vector register group unless tail agnostic is specified (vtype.vta=1), in which case tail elements may be overwritten wi... | 尾部元素不引发异常，不更新目标(除非 vta=1 时可被1或结果覆盖) |  |
| 81 | `vstart_vl_dep` | When vstart {ge} vl, there are no body elements, and no elements are updated in any destination vector register group, including that no tail elements are updated with agnostic values. | 向量指令行为取决于预启动、主体和尾部元素 |  |
| 82 | `vstart_vl_scalar_indp` | Instructions that write an x register or f register do so even when vstart {ge} vl, including when vl=0. | 不使用 vl 的向量指令不受 vl 影响 | vsetvli/vsetivli/vsetvl |

## 5. Configuration-Setting Instructions (`vsetvli`/`vsetivli`/`vsetvl`) (14 条)

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 83 | `vset_op` | The vsetvl instructions set the vtype and vl CSRs based on their arguments, and write the new value of vl into rd. | vsetvli/vsetivli/vsetvl 设置 vl 和 vtype 以匹配应用需求 | vsetvli/vsetivli/vsetvl |

### `vtype` encoding

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 84 | `vtype_acc` | The new vtype value is encoded in the immediate fields of vsetvli and vsetivli, and in the rs2 register for vsetvl. | vtype CSR 只能通过 vsetvli/vsetivli/vsetvl 写入 | vsetvli/vsetivli/vsetvl |
| 85 | `vtype_vill_val` | If the vtype value is not supported by the implementation, then the vill bit is set in vtype, the remaining bits in vtype are set to zero | vtype 值不被支持时 vill 被设置，其余位设为零 | vsetvli/vsetivli/vsetvl |
| 86 | `vtype_vstart_op` | the vl register is also set to zero. | vtype 值不被支持时 vl 也被设为零 | vsetvli/vsetivli/vsetvl |
| 87 | `vtype_vill_val_vill` | A vtype value with vill set is treated as an unsupported configuration. | vill 被设置时 vtype 其余位应为零 | vsetvli/vsetivli/vsetvl |
| 88 | `vtype_vill_all_bits` | Implementations must consider all bits of the vtype value to determine if the configuration is supported.  An unsupported value in any location within the vtype value must result in vill being set. | vill 被设置时 vtype 中除 vill 外所有位都应为零 | vsetvli/vsetivli/vsetvl |

### AVL encoding

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 89 | `vsetvl_op` | When _rs1_ is not x0, the AVL is an unsigned integer held in the x register specified by _rs1_, and the new vl value is also written to the x register specified by _rd_. | rs1≠x0 时 AVL 取自 rs1 的值，结果写入 vl 和 rd | vsetvli/vsetivli/vsetvl |
| 90 | `vsetvl_op_rs1_x0_rd_nx0` | When _rs1_=x0 but _rd_≠x0, the maximum unsigned integer value (~0) is used as the AVL, and the resulting VLMAX is written to vl and also to the x register specified by rd. | rs1=x0 且 rd≠x0 时使用 VLMAX 作为 AVL | vsetvli/vsetivli/vsetvl |
| 91 | `vsetvl_op_rs1_x0_rd_x0` | When _rs1_=x0 and _rd_=x0, the instructions operate as if the current vector length in vl is used as the AVL, and the resulting value is written to vl, but not to a destination register.  This form... | rs1=x0 且 rd=x0 时使用当前 vl 作为 AVL，仅在 VLMAX 不变时使用 | vsetvli/vsetivli/vsetvl |
| 92 | `vtype_vset_rsv` | Use of the instructions with a new SEW/LMUL ratio that would result in a change of VLMAX is reserved. Use of the instructions is also reserved if vill was 1 beforehand. | 会导致 VLMAX 改变的 vsetvl(rs1=x0,rd=x0) 保留 | vsetvli/vsetivli/vsetvl |
| 93 | `reserved_vill_set` | Implementations may set vill in either case. | 保留情况下实现可设置 vill | vsetvli/vsetivli/vsetvl |
| 94 | `vsetivli_op` | For the vsetivli instruction, the AVL is encoded as a 5-bit zero-extended immediate (0--31) in the rs1 field. | vsetivli 使用5位零扩展立即数作为 AVL | vsetivli |

### Constraints on Setting `vl`

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 95 | `vl_val_lead-in` | The vset{i}vl{i} instructions first set VLMAX according to their vtype argument, then set vl obeying the following constraints: | vl 的设置值满足以下约束条件 | vsetvli/vsetivli/vsetvl |
| 96 | `vl_val_list` | . vl = AVL if AVL {le} VLMAX . ceil(AVL / 2) {le} vl {le} VLMAX if AVL < (2 * VLMAX) . vl = VLMAX if AVL {ge} (2 * VLMAX) . Deterministic on any given implementation for same input AVL and VLMAX va... | vl 设置规则：AVL≤VLMAX则vl=AVL；AVL<2*VLMAX则ceil(AVL/2)≤vl≤VLMAX；AVL≥2*VLMAX则vl=VLMAX | vsetvli/vsetivli/vsetvl |

## 6. Vector Loads and Stores (62 条)

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 97 | `vector_load_store_semantics` | Vector loads and stores move values between vector registers and memory. | 向量加载和存储在向量寄存器和内存之间移动值 | vle/vse/vlse/vsse/vlxei/vsxei |
| 98 | `vector_masked_memory_access` | Vector loads and stores can be masked, and they only access memory or raise exceptions for active elements. | 向量加载/存储可被掩码，只访问活跃元素对应的内存 | vle/vse/vlse/vsse/vlxei/vsxei |
| 99 | `vector_masked_inactive_behavior` | Masked vector loads do not update inactive elements in the destination vector register group, unless masked agnostic is specified (vtype.vma=1). | 掩码向量加载不更新目标中的非活跃元素，除非掩码不可知(vma=1) | vle/vse/vlse/vsse/vlxei/vsxei |
| 100 | `vector_ls_vstart` | All vector loads and stores may generate and accept a non-zero vstart value. | 向量加载/存储受 vstart 影响，不访问 vstart 之前的元素 | vle/vse/vlse/vsse/vlxei/vsxei |

### Vector Load/Store Instruction Encoding

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 101 | `vector_ls_strided_eew` | Vector memory unit-stride and constant-stride operations directly encode EEW of the data to be transferred statically in the instruction to reduce the number of vtype changes when accessing memory ... | 单位步长和常量步长操作直接编码数据传输的 EEW | vle/vse/vlse/vsse |
| 102 | `vector_ls_indexed_eew` | Indexed operations use the explicit EEW encoding in the instruction to set the size of the indices used, and use SEW/LMUL to specify the data width. | 索引操作使用指令中的 EEW 编码设置索引大小 | vlxei/vsxei |

### Vector Load/Store Addressing Modes

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 103 | `vector_ls_base_stride_regtype` | Vector load/store base registers and strides are taken from the GPR x registers. | 向量加载/存储的基地址和步长取自 GPR x 寄存器 | vle/vse/vlse/vsse/vlxei/vsxei |
| 104 | `vector_ls_base` | The base effective address for all vector accesses is given by the contents of the x register named in rs1. | 向量加载/存储的基地址由标量寄存器 rs1 提供 | vle/vse/vlse/vsse/vlxei/vsxei |
| 105 | `vector_ls_unit_stride_op` | Vector unit-stride operations access elements stored contiguously in memory starting from the base effective address. | 单位步长加载/存储访问连续的内存元素 | vle/vse |
| 106 | `vector_ls_constant_stride_op` | Vector constant-stride operations access the first memory element at the base effective address, and then access subsequent elements at address increments given by the byte offset contained in the ... | 常量步长加载/存储以固定字节间隔访问内存，步长由 rs2 提供 | vlse/vsse |
| 107 | `vector_ls_indexed_op` | Vector indexed operations add the contents of each element of the vector offset operand specified by vs2 to the base effective address to give the effective address of each element.  The data vecto... | 索引加载/存储使用向量寄存器中的偏移量作为地址索引 | vlxei/vsxei |
| 108 | `vector_ls_bytewise` | The vector offset operand is treated as a vector of byte-address offsets. | 向量加载/存储以字节为单位进行内存访问 | vle/vse/vlse/vsse/vlxei/vsxei |
| 109 | `vector_ls_xlen_dep` | If the vector offset elements are narrower than XLEN, they are zero-extended to XLEN before adding to the base effective address.  If the vector offset elements are wider than XLEN, the least-signi... | 向量加载/存储的有效地址计算依赖于 XLEN | vle/vse/vlse/vsse/vlxei/vsxei |
| 110 | `vector_ls_eew_rsv` | If the implementation does not support the EEW of the offset elements, the instruction is reserved. | EEW 不是支持的宽度时指令编码保留 | vle/vse/vlse/vsse/vlxei/vsxei |
| 111 | `vector_ls_stride_ordered_op` | Vector unit-stride and constant-stride memory accesses do not guarantee ordering between individual element accesses.  The vector indexed load and store memory operations have two forms, ordered an... | 单位步长和常量步长的向量存储按元素顺序执行内存访问 | vse/vsse |
| 112 | `vector_ls_stride_unordered_op` | For unordered instructions (mop[1:0]!=11) there is no guarantee on element access order.  If the accesses are to a strongly ordered IO region, the element accesses can be initiated in any order. | 单位步长和常量步长的向量加载可以任意顺序访问内存 | vle/vlse |
| 113 | `vector_ls_stride_unordered_precise` | For implementations with precise vector traps, exceptions on indexed-unordered stores must also be precise. | 即使加载无序，异常仍然精确 | vle/vlse |
| 114 | `vector_ls_nf_op` | The nf[2:0] field encodes the number of fields in each segment.  For regular vector loads and stores, nf=0, indicating that a single value is moved between a vector register group and memory at eac... | nf[2:0] 编码每段字段数量减1(NFIELDS-1) | vlseg/vsseg |
| 115 | `vector_wholels_nf_op` | The nf[2:0] field also encodes the number of whole vector registers to transfer for the whole vector register load/store instructions. | 整个寄存器加载/存储中 nf 编码寄存器数量减1 | vl1r/vl2r/vl4r/vl8r/vs1r/vs2r/vs4r/vs8r |

### Vector Load/Store Width Encoding

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 116 | `vector_ls_eew_emul` | Vector loads and stores have an EEW encoded directly in the instruction.  The corresponding EMUL is calculated as EMUL = (EEW/SEW)*LMUL. | 向量加载/存储的 EEW 直接编码在指令中，对应 EMUL 由 EEW/SEW*LMUL 计算 | vle/vse/vlse/vsse/vlxei/vsxei |
| 117 | `vector_ls_emul_rsv` | If the EMUL would be out of range (EMUL>8 or EMUL<1/8), the instruction encoding is reserved. | EMUL 超出范围(>8或<1/8)时指令编码保留 | vle/vse/vlse/vsse/vlxei/vsxei |
| 118 | `vector_ls_emul_offgroup_rsv` | The vector register groups must have legal register specifiers for the selected EMUL, otherwise the instruction encoding is reserved. | 向量寄存器组必须有合法的寄存器指定符，否则指令编码保留 | vle/vse/vlse/vsse/vlxei/vsxei |
| 119 | `vector_ls_indexed_eew_emul` | Vector unit-stride and constant-stride use the EEW/EMUL encoded in the instruction for the data values, while vector indexed loads and stores use the EEW/EMUL encoded in the instruction for the ind... | 索引加载/存储中索引向量 EEW 由宽度字段编码，EMUL=EEW/SEW*LMUL | vlxei/vsxei |
| 120 | `vector_ls_ins_req` | Implementations must provide vector loads and stores with EEWs corresponding to all supported SEW settings. | 实现必须提供与所有支持的 SEW 设置对应的 EEW 的向量加载/存储 | vle/vse/vlse/vsse/vlxei/vsxei |
| 121 | `vector_ls_ins_rsv` | Vector load/store encodings for unsupported EEW widths are reserved. | 不支持的 EEW 宽度的向量加载/存储编码保留 | vle/vse/vlse/vsse/vlxei/vsxei |
| 122 | `vector_ls_mew_rsv` | The mew bit (inst[28]) when set is expected to be used to encode expanded memory sizes of 128 bits and above, but these encodings are currently reserved. | mew 位(inst[28])当前必须为0，否则指令编码保留 | vle/vse/vlse/vsse/vlxei/vsxei |

### Vector Unit-Stride Instructions

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 123 | `vector_ls_unit_stride_mask` | Additional unit-stride mask load and store instructions are provided to transfer mask values to/from memory.  These operate similarly to unmasked byte loads or stores (EEW=8), except that the effec... | 单位步长掩码加载/存储使用 EEW=8、EMUL=1 传输掩码值 | vlm.v/vsm.v |

### Vector Constant-Stride Instructions

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 124 | `vector_ls_neg_zero_stride` | Negative and zero strides are supported. | 负数和零步长在常量步长加载/存储中合法 | vlse/vsse |
| 125 | `vector_ls_constant_stride_unordered` | Element accesses within a constant-stride instruction are unordered with respect to each other. | 常量步长加载可以任意顺序访问内存 | vlse |
| 126 | `vector_ls_constant_stride_x0` | When rs2=x0, then an implementation is allowed, but not required, to perform fewer memory operations than the number of active elements, and may perform different numbers of memory operations acros... | rs2=x0 时常量步长加载/存储步长为0 | vlse/vsse |

### Unit-stride Fault-Only-First Loads

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 127 | `vector_ff_trigger` | The unit-stride fault-only-first load instructions are used to vectorize loops with data-dependent exit conditions ("while" loops). These instructions execute as a regular load except that they wil... | 单位步长 fault-only-first 加载用于向量化具有数据依赖退出条件的循环 | vle8ff/vle16ff/vle32ff/vle64ff |
| 128 | `vector_ff_op` | If element 0 raises an exception, vl is not modified, and the trap is taken.  If an element > 0 raises an exception, the corresponding trap is not taken, and the vector length vl is reduced to the ... | 元素0引发异常时 vl 不修改并触发陷阱；元素>0引发异常时截断 vl 到该元素 | vle8ff/vle16ff/vle32ff/vle64ff |
| 129 | `vector_ls_overwrite_past_trap` | Load instructions may overwrite active destination vector register group elements past the element index at which the trap is reported. | 加载指令可能覆盖引发陷阱的元素之后的活跃目标元素 | vle/vlse/vlxei |
| 130 | `vector_ff_past_trap` | Similarly, fault-only-first load instructions may update active destination elements past the element that causes trimming of the vector length (but not past the original vector length).  The value... | fault-only-first 加载可能更新引发陷阱的元素之后的活跃目标元素 | vle8ff/vle16ff/vle32ff/vle64ff |
| 131 | `vector_ff_no_exception` | Even when an exception is not raised, implementations are permitted to process fewer than vl elements and reduce vl accordingly, but if vstart=0 and vl>0, then at least one element must be processed. | fault-only-first 加载仅在第一个活跃元素上报告异常，后续异常被抑制并截断 vl | vle8ff/vle16ff/vle32ff/vle64ff |
| 132 | `vector_ff_interrupt_behavior` | When the fault-only-first instruction takes a trap due to an interrupt, implementations should not reduce vl and should instead set a vstart value. | fault-only-first 加载期间中断时实现可将 vl 截断到中断点 | vle8ff/vle16ff/vle32ff/vle64ff |

### Vector Load/Store Segment Instructions

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 133 | `nfields` | The three-bit nf field in the vector instruction encoding is an unsigned integer that contains one less than the number of fields per segment, _NFIELDS_. | NFIELDS 是段加载/存储中每段字段数量，由 nf+1 给出 | vlseg/vsseg |
| 134 | `emul_nfields_rsv` | The EMUL setting must be such that EMUL * NFIELDS {le} 8, otherwise the instruction encoding is reserved. | EMUL*NFIELDS>8 时指令编码保留 | vlseg/vsseg |
| 135 | `nfields_op` | Each field will be held in successively numbered vector register groups.  When EMUL>1, each field will occupy a vector register group held in multiple successively numbered vector registers, and th... | 段加载/存储将连续 NFIELDS 个向量寄存器组作为操作数 | vlseg/vsseg |
| 136 | `vector_ls_seg_rsv` | If the vector register numbers accessed by the segment load or store would increment past 31, then the instruction encoding is reserved. | 段加载/存储寄存器编号加 EMUL*NFIELDS 超过32时指令编码保留 | vlseg/vsseg |
| 137 | `vector_ls_seg_op` | The vl register gives the number of segments to move, which is equal to the number of elements transferred to each vector register group.  Masking is also applied at the level of whole segments. | 段加载/存储按段访问内存，每段包含 NFIELDS 个字段 | vlseg/vsseg |
| 138 | `vector_ls_seg_unordered` | For segment loads and stores, the individual memory accesses used to access fields within each segment are unordered with respect to each other even for ordered indexed segment loads and stores. | 段加载可以任意顺序访问段，但段内字段按顺序 | vlseg |
| 139 | `vector_ls_seg_vstart_dep` | The vstart value is in units of whole segments. | vstart 值以整个段为单位 | vlseg/vsseg |
| 140 | `vector_ls_seg_partial_access` | If a trap occurs during access to a segment, it is implementation-defined whether a subset of the faulting segment's accesses are performed before the trap is taken. | 段访问期间发生陷阱时，是否已访问段的子集由实现定义 | vlseg/vsseg |
| 141 | `vector_ls_seg_unit_stride_op` | The vector unit-stride load and store segment instructions move packed contiguous segments into multiple destination vector register groups. | 单位步长段加载/存储的段在内存中连续排列 | vlseg/vsseg |
| 142 | `vector_ls_seg_unit_stride_vd_vs3` | For loads, the vd register will hold the first field loaded from the segment.  For stores, the vs3 register is read to provide the first field to be stored to each segment. | 段加载目标和段存储源使用连续向量寄存器组 | vlseg/vsseg |
| 143 | `vector_ls__seg_ff_unit-stride_op` | For fault-only-first segment loads, if an exception is detected partway through accessing the zeroth segment, the trap is taken. If an exception is detected partway through accessing a subsequent s... | 单位步长段 fault-only-first 加载仅在第一个活跃段的第一个字段上报告异常 | vlseg*ff |
| 144 | `vector_ff_seg_partial_access` | In both cases, it is implementation-defined whether a subset of the segment is loaded. | fault-only-first 段加载在段中间异常时之前字段可能已被写入 | vlseg*ff |
| 145 | `vector_ls_seg_ff_overload` | These instructions may overwrite destination vector register group elements past the point at which a trap is reported or past the point at which vector length is trimmed. | fault-only-first 段加载可能覆盖已加载的段数据 | vlseg*ff |
| 146 | `vector_ls_seg_constant_stride_op` | Vector constant-stride segment loads and stores move contiguous segments where each segment is separated by the byte-stride offset given in the rs2 GPR argument. | 常量步长段加载/存储以固定字节间隔访问段 | vlsseg/vssseg |
| 147 | `vector_ls_seg_constant_stride_unordered` | Accesses to the fields within each segment can occur in any order, including the case where the byte stride is such that segments overlap in memory. | 常量步长段加载可以任意顺序访问段 | vlsseg |
| 148 | `vector_ls_seg_indexed_op` | Vector indexed segment loads and stores move contiguous segments where each segment is located at an address given by adding the scalar base address in the rs1 field to byte offsets in vector regis... | 索引段加载/存储移动连续段，每段位于索引向量给出的地址 | vlxseg/vsxseg |
| 149 | `vector_ls_seg_indexed_unordered` | However, even for the ordered form, accesses to the fields within an individual segment are not ordered with respect to each other. | 即使有序形式，单个段内字段的访问也不保证有序 | vlxseg/vsxseg |
| 150 | `vector_ls_seg_indexed_eew_emul_op` | The data vector register group has EEW=SEW, EMUL=LMUL, while the index vector register group has EEW encoded in the instruction with EMUL=(EEW/SEW)*LMUL. | 索引段加载/存储使用向量寄存器偏移量作为段地址索引 | vlxseg/vsxseg |
| 151 | `vector_ls_seg_indexed_emul_nfields_val` | The EMUL * NFIELDS {le} 8 constraint applies to the data vector register group. | 索引段加载/存储中索引 EMUL 由 EEW/SEW*LMUL 计算 | vlxseg/vsxseg |
| 152 | `vector_ls_seg_indexed_vreg_rsv` | For vector indexed segment loads, the destination vector register groups cannot overlap the source vector register group (specified by vs2), else the instruction encoding is reserved. | 索引段加载/存储数据和索引寄存器组重叠时指令编码保留 | vlxseg/vsxseg |

### Vector Load/Store Whole Register Instructions

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 153 | `vector_ls_seg_wholereg_eew` | The load instructions have an EEW encoded in the mew and width fields following the pattern of regular unit-stride loads. | 整个寄存器加载/存储的 EEW 为8位 | vl1r/vl2r/vl4r/vl8r/vs1r/vs2r/vs4r/vs8r |
| 154 | `vector_ls_seg_wholereg_op` | NFIELDS indicates the number of vector registers to transfer, numbered successively after the base. | NFIELDS 指示要传输的向量寄存器数量，从基址之后连续编号 | vl1r/vl2r/vl4r/vl8r/vs1r/vs2r/vs4r/vs8r |
| 155 | `vector_ls_seg_wholereg_nf_rsv` | Only NFIELDS values of 1, 2, 4, 8 are supported, with other values reserved. | NFIELDS 仅支持 1/2/4/8，其他值保留 | vl1r/vl2r/vl4r/vl8r/vs1r/vs2r/vs4r/vs8r |
| 156 | `vector_ls_seg_wholereg_op_cont` | When multiple registers are transferred, the lowest-numbered vector register is held in the lowest-numbered memory addresses and successive vector register numbers are placed contiguously in memory. | 传输多个寄存器时，最低编号寄存器在最低内存地址 | vl1r/vl2r/vl4r/vl8r/vs1r/vs2r/vs4r/vs8r |
| 157 | `vector_ls_seg_wholereg_evl` | The instructions operate with an effective vector length, evl=NFIELDS*VLEN/EEW, regardless of current settings in vtype and vl.  The usual property that no elements are written if vstart {ge} vl do... | 整个寄存器加载/存储 EVL=NFIELDS*VLEN/EEW | vl1r/vl2r/vl4r/vl8r/vs1r/vs2r/vs4r/vs8r |
| 158 | `vector_ls_wholereg_missaligned_exception` | Implementations are allowed to raise a misaligned address exception on whole register loads and stores if the base address is not naturally aligned to the larger of the size of the encoded EEW in b... | 整个寄存器加载/存储基地址未对齐时可能引发异常 | vl1r/vl2r/vl4r/vl8r/vs1r/vs2r/vs4r/vs8r |

## 7. Vector Memory Alignment Constraints (3 条)

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 159 | `vector_ls_missaligned_exception` | If an element accessed by a vector memory instruction is not naturally aligned to the size of the element, either the element is transferred successfully or an address-misaligned exception is raise... | 向量内存访问地址未自然对齐时实现可处理未对齐或引发异常 |  |
| 160 | `vector_ls_scalar_missaligned_independence` | Support for misaligned vector memory accesses is independent of an implementation's support for misaligned scalar memory accesses. | 向量内存访问对齐要求独立于标量内存访问 |  |
| 161 | `vector_ls_scalar_missaligned_dependence` | Vector misaligned memory accesses follow the same rules for atomicity as scalar misaligned memory accesses. | 标量访问支持未对齐则向量访问也必须支持 |  |

## 8. Vector Memory Consistency Model (7 条)

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 162 | `vector_ls_program_order` | Vector memory instructions appear to execute in program order on the local hart. | 向量加载和存储遵循 RVWMO 内存一致性模型 |  |
| 163 | `vector_ls_rvwmo` | Vector memory instructions follow RVWMO at the instruction level. | 向量内存指令在指令级别遵循 RVWMO |  |
| 164 | `vector_ls_rvtso` | If the Ztso extension is implemented, vector memory instructions additionally follow RVTSO at the instruction level. | 如果实现了 Ztso 扩展，向量内存指令还在指令级别遵循 RVTSO |  |
| 165 | `vector_ls_indexed_ordered_ordered` | Except for vector indexed-ordered loads and stores, element operations are unordered within the instruction. | 有序索引加载/存储按元素顺序执行内存访问 | vlxei/vsxei (ordered) |
| 166 | `vector_ls_indexed_ordered_rvwmo` | Vector indexed-ordered loads and stores read and write elements from/to memory in element order respectively, obeying RVWMO at the element level. | 有序索引加载/存储在 RVWMO 下按元素顺序排序 | vlxei/vsxei (ordered) |
| 167 | `vl_control_dependency` | Instructions affected by the vector length register vl have a control dependency on vl, rather than a data dependency. | vl 寄存器值对后续向量指令产生控制依赖 |  |
| 168 | `vmask_control_dependency` | Similarly, masked vector instructions have a control dependency on the source mask register, rather than a data dependency. | 掩码寄存器值对后续向量指令产生控制依赖 |  |

## 9. Vector Arithmetic Instruction Formats (16 条)

### Vector Arithmetic Instruction encoding

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 169 | `V_fp_frm` | All vector floating-point operations use the dynamic rounding mode in the frm register. | 所有向量浮点操作使用 frm 寄存器中的动态舍入模式 |  |
| 170 | `V_inv_frm_rsv` | Use of the frm field when it contains an invalid rounding mode by any vector floating-point instruction--even those that do not depend on the rounding mode, or when vl=0, or when vstart  vl--is res... | frm 包含无效舍入模式时，任何向量浮点指令使用 frm 是保留的 |  |
| 171 | `vop-vv_vreg_vs2_vs1` | Vector-vector operations take two vectors of operands from vector register groups specified by vs2 and vs1 respectively. | 向量-向量算术指令中 vs2 是第一个源，vs1 是第二个源 |  |
| 172 | `vop-vx_vop-vi_vreg_vs2` | the vector register group operand is specified by vs2. | 向量寄存器组操作数由 vs2 指定 |  |
| 173 | `vop-vi_imm_5bit` | For integer operations, the scalar can be a 5-bit immediate, imm[4:0], encoded in the rs1 field.  The value is sign-extended to SEW bits, unless otherwise specified. | 整数操作中标量可以是编码在 rs1 字段的5位立即数 imm[4:0] |  |
| 174 | `vop-vx_xreg_rs1` | For integer operations, the scalar can be taken from the scalar x register specified by rs1. | 整数操作中标量可取自 rs1 指定的标量 x 寄存器 |  |
| 175 | `vop-vx_rs1_trunc_lsb_sewbits` | If XLEN>SEW, the least-significant SEW bits of the x register are used, unless otherwise specified. | XLEN>SEW 时使用 x 寄存器的最低有效 SEW 位(除非另有规定) |  |
| 176 | `vop-vx_rs1_sext_sewbits` | If XLEN<SEW, the value from the x register is sign-extended to SEW bits. | XLEN<SEW 时 x 寄存器的值被符号扩展到 SEW 位 |  |
| 177 | `vfop_freg` | For floating-point operations, the scalar can be taken from a scalar f register. | 浮点操作中标量可取自标量 f 寄存器 |  |
| 178 | `vfop_freg_NaNbox_lsb_sewbits` | If FLEN > SEW, the value in the f registers is checked for a valid NaN-boxed value, in which case the least-significant SEW bits of the f register are used, else the canonical NaN value is used. | FLEN>SEW 时检查 f 寄存器中的值是否为有效 NaN-boxed 值 |  |
| 179 | `V_fp_eew_rsv` | Vector instructions where any floating-point vector operand's EEW is not a supported floating-point type width (which includes when FLEN < SEW) are reserved. | 任何浮点向量操作数的 EEW 不是支持的浮点类型时指令编码保留 |  |
| 180 | `V_Zinx_fp_scalar` | When adding a vector extension to the Zfinx/Zdinx/Zhinx extensions, floating-point scalar arguments are taken from the x registers. NaN-boxing is not supported in these extensions, and so operands ... | 向量扩展添加到 Zfinx/Zdinx/Zhinx 时，浮点标量参数取自 x 寄存器 |  |
| 181 | `V_masked` | Vector arithmetic instructions are masked under control of the vm field. | 向量算术指令在 vm 字段控制下被掩码 |  |

### Widening Vector Arithmetic Instructions

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 182 | `vwop_vd_eew_emul` | A few vector arithmetic instructions are defined to be __widening__ operations where the destination vector register group has EEW=2*SEW and EMUL=2*LMUL. | 加宽操作中目标 EEW=2*SEW，EMUL=2*LMUL |  |

### Narrowing Vector Arithmetic Instructions

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 183 | `vnop_vd_vs2_eew_emul` | A few instructions are provided to convert double-width source vectors into single-width destination vectors.  These instructions convert a vector register group specified by vs2 with EEW/EMUL=2*SE... | 窄化指令将双宽度源向量转换为单宽度目标向量 |  |
| 184 | `vnop_vs1_eew_emul` | Where there is a second source vector register group (specified by vs1), this has the same (narrower) width as the result (i.e., EEW=SEW). | 窄化指令中第二个源向量寄存器组(vs1)具有与目标相同的窄 EEW 和 EMUL |  |

## 10. Vector Integer Arithmetic Instructions (38 条)

### Vector Single-Width Integer Add and Subtract

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 185 | `vadd_vsub_op` | Vector integer add and subtract are provided. | 提供向量整数加法和减法指令 | vadd/vsub |
| 186 | `vrsub_op` | Reverse-subtract instructions are also provided for the vector-scalar forms. | 还提供向量-标量形式的反向减法指令 | vrsub |

### Vector Widening Integer Add/Subtract

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 187 | `vwaddu_vwadd_vwsubu_vwsub_op` | The widening add/subtract instructions are provided in both signed and unsigned variants, depending on whether the narrower source operands are first sign- or zero-extended before forming the doubl... | 加宽加法/减法指令提供有符号和无符号变体 | vwaddu/vwadd/vwsubu/vwsub |

### Vector Integer Extension

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 188 | `vsext_vzext_op` | The vector integer extension instructions zero- or sign-extend a source vector integer operand with EEW less than SEW to fill SEW-sized elements in the destination. | 向量整数扩展指令将窄源操作数零扩展或符号扩展 | vzext/vsext |
| 189 | `vsext_vzext_vs_eew_emul` | The EEW of the source is 1/2, 1/4, or 1/8 of SEW, while EMUL of the source is (EEW/SEW)*LMUL. | 源 EEW 为 SEW 的 1/2、1/4 或 1/8，源 EMUL=(EEW/SEW)*LMUL | vzext/vsext |
| 190 | `vsext_vzext_vd_eew_emul` | The destination has EEW equal to SEW and EMUL equal to LMUL. | 目标 EEW 等于 SEW，EMUL 等于 LMUL | vzext/vsext |
| 191 | `vsext_vzext_ill_eew_emul_rsv` | If the source EEW is not a supported width, or source EMUL would be below the minimum legal LMUL, the instruction encoding is reserved. | 源 EEW 不支持或源 EMUL 低于最小合法 LMUL 时指令编码保留 | vzext/vsext |

### Vector Integer Add-with-Carry / Subtract-with-Borrow Instructions

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 192 | `vmadc_vmsbc_vadc_vsbc_carry_v0` | The carry inputs and outputs are represented using the mask register layout as described in <<sec-mask-register-layout>>.  Due to encoding constraints, the carry input must come from the implicit v... | 进位输入和输出使用掩码寄存器布局表示 | vadc/vsbc/vmadc/vmsbc |
| 193 | `vadc_vsbc_op` | vadc and vsbc add or subtract the source operands and the carry-in or borrow-in, and write the result to vector register vd. | 带进位加法/带借位减法使用 v0 作为进位/借位输入 | vadc/vsbc |
| 194 | `vadc_vsbc_masked_write_all_elem` | These instructions are encoded as masked instructions (vm=0), but they operate on and write back all body elements. | vadc/vsbc 编码为掩码指令(vm=0)，但操作并写回所有主体元素 | vadc/vsbc |
| 195 | `vadc_vsbc_unmasked_rsv` | Encodings corresponding to the unmasked versions (vm=1) are reserved. | vadc/vsbc 的无掩码版本(vm=1)编码保留 | vadc/vsbc |
| 196 | `vmadc_vmsbc_op_masked` | vmadc and vmsbc add or subtract the source operands, optionally add the carry-in or subtract the borrow-in if masked (vm=0), and write the resulting carry-out or borrow-out back to mask register vd. | vmadc/vmsbc 加减源操作数，可选地加进位或减借位，将结果写入掩码寄存器 | vmadc/vmsbc |
| 197 | `vmadc_vmsbc_op_unmasked` | If unmasked (vm=1), there is no carry-in or borrow-in. | vmadc/vmsbc 无掩码(vm=1)时没有进位/借位输入 | vmadc/vmsbc |
| 198 | `vmadc_vmsbc_masked_write_all_elem` | These instructions operate on and write back all body elements, even if masked. | vmadc/vmsbc 操作并写回所有主体元素，即使被掩码 | vmadc/vmsbc |
| 199 | `vmadc_vmsbc_tail_agnostic` | Because these instructions produce a mask value, they always operate with a tail-agnostic policy. | vmadc/vmsbc 产生掩码值，始终以 tail-agnostic 策略操作 | vmadc/vmsbc |
| 200 | `vmsbc_borrow_neg` | For vmsbc, the borrow is defined to be 1 iff the difference, prior to truncation, is negative. | vmsbc 中借位定义为：截断前差值为负时借位为1 | vmsbc |
| 201 | `vadc_vsbc_vd_v0_rsv` | For vadc and vsbc, the instruction encoding is reserved if the destination vector register is v0. | vadc/vsbc 目标不能是 v0，否则指令编码保留 | vadc/vsbc |

### Vector Bitwise Logical Instructions

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 202 | `vand_vor_vxor_op` | # Bitwise logical operations. vand.vv vd, vs2, vs1, vm   # Vector-vector vand.vx vd, vs2, rs1, vm   # vector-scalar vand.vi vd, vs2, imm, vm   # vector-immediate  vor.vv vd, vs2, vs1, vm    # Vecto... | 向量按位逻辑运算：AND、OR、XOR | vand/vor/vxor |

### Vector Single-Width Shift Instructions

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 203 | `vsll_vsrl_vsra_op` | A full set of vector shift instructions are provided, including logical shift left (sll), and logical (zero-extending srl) and arithmetic (sign-extending sra) shift right.  The data to be shifted i... | 向量移位：逻辑左移、逻辑右移、算术右移 | vsll/vsrl/vsra |
| 204 | `vsll_vsrl_vsra_shamt` | Only the low lg2(SEW) bits of the shift-amount value are used to control the shift amount. | 仅使用移位量值的低 lg2(SEW) 位控制移位量 | vsll/vsrl/vsra |

### Vector Narrowing Integer Right Shift Instructions

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 205 | `vnsrl_vnsra_op` | The narrowing right shifts extract a smaller field from a wider operand and have both zero-extending (srl) and sign-extending (sra) forms.  The shift amount can come from a vector register group, o... | 窄化右移将 2*SEW 源右移后截断为 SEW | vnsrl/vnsra |
| 206 | `vnsrl_vnsra_shamt` | The low lg2(2*SEW) bits of the shift-amount value are used (e.g., the low 6 bits for a SEW=64-bit to SEW=32-bit narrowing operation). | 使用移位量值的低 lg2(2*SEW) 位 | vnsrl/vnsra |

### Vector Integer Compare Instructions

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 207 | `vmseq_vmsne_vmsltu_vmslt_vmsleu_vmsle_vmsgtu_vmsgt_op` | The following integer compare instructions write 1 to the destination mask register element if the comparison evaluates to true, and 0 otherwise.  The destination mask vector is always held in a si... | 整数比较指令在条件满足时向目标掩码寄存器元素写入1 | vmseq/vmsne/vmsltu/vmslt/vmsleu/vmsle/vmsgtu/vmsgt |
| 208 | `vmseq_vmsne_vmsltu_vmslt_vmsleu_vmsle_vmsgtu_vmsgt_vd_v0_legal` | The destination mask vector register may be the same as the source vector mask register (v0). | 目标掩码向量寄存器可与源向量掩码寄存器(v0)相同 | vmseq/vmsne/vmsltu/vmslt/vmsgtu/vmsgt |
| 209 | `vmseq_vmsne_vmsltu_vmslt_vmsleu_vmsle_vmsgtu_vmsgt_maskundisturbed` | Compares effectively AND in the mask under a mask-undisturbed policy if the destination register is v0, | 比较指令在目标寄存器为 v0 时，mask-undisturbed 策略下有效地 AND 掩码 | vmseq/vmsne/vmsltu/vmslt/vmsgtu/vmsgt |
| 210 | `vmseq_vmsne_vmsltu_vmslt_vmsleu_vmsle_vmsgtu_vmsgt_tail_agnostic` | Compares write mask registers, and so always operate under a tail-agnostic policy. | 比较指令写入掩码寄存器，始终以 tail-agnostic 策略操作 | vmseq/vmsne/vmsltu/vmslt/vmsgtu/vmsgt |

### Vector Integer Min/Max Instructions

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 211 | `vminu_vmin_vmaxu_vmax_op` | Signed and unsigned integer minimum and maximum instructions are supported. | 向量整数最小值/最大值指令 | vminu/vmin/vmaxu/vmax |

### Vector Single-Width Integer Multiply Instructions

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 212 | `vmul_vmulh_vmulhu_vmulhsu_op` | The single-width multiply instructions perform a SEW-bit*SEW-bit multiply to generate a 2*SEW-bit product, then return one half of the product in the SEW-bit-wide destination.  The *mul* versions w... | 向量整数乘法(低位和高位结果) | vmul/vmulh/vmulhu/vmulhsu |

### Vector Integer Divide Instructions

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 213 | `vdivu_vdiv_vremu_vrem_op` | The divide and remainder instructions are equivalent to the RISC-V standard scalar integer multiply/divides, with the same results for extreme inputs. | 向量整数除法和取余指令 | vdivu/vdiv/vremu/vrem |

### Vector Widening Integer Multiply Instructions

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 214 | `vwmul_wmulu_vwmulsu_op` | The widening integer multiply instructions return the full 2*SEW-bit product from an SEW-bit*SEW-bit multiply. | 加宽整数乘法返回 SEW*SEW 的完整 2*SEW 位乘积 | vwmul/vwmulu/vwmulsu |

### Vector Single-Width Integer Multiply-Add Instructions

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 215 | `vmacc_vnmsac_vmadd_vnmsub_op` | The integer multiply-add instructions are destructive and are provided in two forms, one that overwrites the addend or minuend (vmacc, vnmsac) and one that overwrites the first multiplicand (vmadd,... | 向量整数乘加指令 | vmacc/vnmsac/vmadd/vnmsub |
| 216 | `vmacc_vnmsac_vmadd_vnmsub_op_lowhalf` | The low half of the product is added or subtracted from the third operand. | 乘积的低半部分与第三个操作数相加或相减 | vmacc/vnmsac/vmadd/vnmsub |

### Vector Widening Integer Multiply-Add Instructions

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 217 | `vwmaccu_vwmacc_vwmaccsu_vwmaccus_op` | The widening integer multiply-add instructions add the full 2*SEW-bit product from a SEW-bit*SEW-bit multiply to a 2*SEW-bit value and produce a 2*SEW-bit result. | 加宽整数乘加将 SEW*SEW 的完整 2*SEW 位乘积加到累加器 | vwmacc/vwmaccu/vwmaccsu/vwmaccus |

### Vector Integer Merge Instructions

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 218 | `vmerge_op` | The vector integer merge instructions combine two source operands based on a mask. | 向量合并根据掩码从两个源中选择元素 | vmerge |
| 219 | `vmerge_all_elem` | Unlike regular arithmetic instructions, the merge operates on all body elements (i.e., the set of elements from vstart up to the current vector length in vl). | 与常规算术指令不同，合并操作对所有主体元素操作 | vmerge |
| 220 | `vmerge_op_mask` | The vmerge instructions are encoded as masked instructions (vm=0). The instructions combine two sources as follows.  At elements where the mask value is zero, the first operand is copied to the des... | vmerge 编码为掩码指令(vm=0)，根据掩码从两个源中选择元素 | vmerge |

### Vector Integer Move Instructions

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 221 | `vmv_op` | The vector integer move instructions copy a source operand to a vector register group. The vmv.v.v variant copies a vector register group, whereas the vmv.v.x and vmv.v.i variants __splat__ a scala... | 向量移动将源复制到目标向量寄存器 | vmv.v.v/vmv.v.x/vmv.v.i |
| 222 | `vmv_vs2_nv0_rsv` | The first operand specifier (vs2) must contain v0, and any other vector register number in vs2 is _reserved_. | vmv 的第一个操作数(vs2)必须为 v0，其他值保留 | vmv.v.v/vmv.v.x/vmv.v.i |

## 11. Vector Fixed-Point Arithmetic Instructions (14 条)

### Vector Single-Width Saturating Add and Subtract

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 223 | `vsaddu_vsadd_vssubu_vssub_op` | Saturating forms of integer add and subtract are provided, for both signed and unsigned integers. | 向量饱和加法/减法，溢出时饱和到最大/最小值 | vsaddu/vsadd/vssubu/vssub |
| 224 | `vsaddu_vsadd_vssubu_vssub_op_overflow_vxsat_op_vsaddsub` | If the result would overflow the destination, the result is replaced with the closest representable value, and the vxsat bit is set. | 结果溢出时替换为最接近的可表示值，并设置 vxsat | vsaddu/vsadd/vssubu/vssub |

### Vector Single-Width Averaging Add and Subtract

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 225 | `vaaddu_vaadd_vasubu_vasub_op` | The averaging add and subtract instructions right shift the result by one bit and round off the result according to the setting in vxrm. Computation is performed in infinite precision before roundi... | 向量平均加法/减法，结果右移1位并使用 vxrm 舍入 | vaaddu/vaadd/vasubu/vasub |
| 226 | `vasub_vasubu_op_overflow` | For vasub and vasubu, overflow is ignored and the result wraps around. | vasub/vasubu 忽略溢出，结果环绕 | vasub/vasubu |

### Vector Single-Width Fractional Multiply with Rounding and Saturation

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 227 | `vsmul_op` | The signed fractional multiply instruction produces a 2*SEW product of the two SEW inputs, then shifts the result right by SEW-1 bits, rounding these bits according to vxrm, then saturates the resu... | 向量分数乘法，结果右移 SEW-1 位并饱和 | vsmul |
| 228 | `vxsat_op_vsmul` | If the result causes saturation, the vxsat bit is set. | vsmul 结果饱和时设置 vxsat 位 | vsmul |

### Vector Single-Width Scaling Shift Instructions

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 229 | `vssrl_vssra_op` | These instructions shift the input value right, and round off the shifted out bits according to vxrm.  The scaling right shifts have both zero-extending (vssrl) and sign-extending (vssra) forms.  T... | 向量缩放移位，右移后使用 vxrm 舍入 | vssrl/vssra |
| 230 | `vssrl_vssra_shamt` | Only the low lg2(SEW) bits of the shift-amount value are used to control the shift amount. | 仅使用移位量值的低 lg2(SEW) 位控制移位量 | vssrl/vssra |

### Vector Narrowing Fixed-Point Clip Instructions

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 231 | `vnclipu_vnclip_op` | The vnclip instructions are used to pack a fixed-point value into a narrower destination.  The instructions support rounding, scaling, and saturation into the final destination format.  The source ... | vnclip 指令用于将定点值打包到更窄的目标中 | vnclip/vnclipu |
| 232 | `vnclipu_vnclip_shamt` | The low lg2(2*SEW) bits of the vector or scalar shift-amount value (e.g., the low 6 bits for a SEW=64-bit to SEW=32-bit narrowing operation) are used to control the right shift amount, which provid... | 使用向量或标量移位量值的低 lg2(2*SEW) 位 | vnclip/vnclipu |
| 233 | `vnclipu_vnclip_rounding` | For vnclipu/vnclip, the rounding mode is specified in the vxrm CSR.  Rounding occurs around the least-significant bit of the destination and before saturation. | vnclipu/vnclip 的舍入模式由 vxrm CSR 指定 | vnclip/vnclipu |
| 234 | `vnclipu_overflow` | For vnclipu, the shifted rounded source value is treated as an unsigned integer and saturates if the result would overflow the destination viewed as an unsigned integer. | vnclipu 将移位舍入后的源值视为无符号整数，超出范围时饱和 | vnclipu |
| 235 | `vnclip_overflow` | For vnclip, the shifted rounded source value is treated as a signed integer and saturates if the result would overflow the destination viewed as a signed integer. | vnclip 将移位舍入后的源值视为有符号整数，超出范围时饱和 | vnclip |
| 236 | `vxsat_op_vnclip_u` | If any destination element is saturated, the vxsat bit is set in the vxsat register. | 任何目标元素饱和时在 vxsat 寄存器中设置 vxsat 位 | vnclip/vnclipu |

## 12. Vector Floating-Point Instructions (47 条)

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 237 | `V_fp_EEW_IEEE_nsupported_rsv` | If the EEW of a vector floating-point operand does not correspond to a supported IEEE floating-point type, the instruction encoding is reserved. | 浮点操作数的 EEW 不对应支持的 IEEE 浮点类型时指令编码保留 |  |
| 238 | `Vf_requrires_Vx` | Vector floating-point instructions require the presence of base scalar floating-point extensions corresponding to the supported vector floating-point element widths. | 向量浮点指令要求存在对应的基础标量浮点扩展 |  |
| 239 | `mstatus_FS_off_V_fp_ill` | If the floating-point unit status field mstatus.FS is Off then any attempt to execute a vector floating-point instruction will raise an illegal-instruction exception. | mstatus.FS=Off 时执行向量浮点指令引发非法指令异常 |  |
| 240 | `mstatus_FS_dirty_V_fp` | Any vector floating-point instruction that modifies any floating-point extension state (i.e., floating-point CSRs or f registers) must set mstatus.FS to Dirty. | 修改浮点扩展状态的向量浮点指令将 mstatus.FS 设为 Dirty |  |
| 241 | `vsstatus_mstatus_FS_off_hypervisor_V_fp_ill` | If the hypervisor extension is implemented and V=1, the vsstatus.FS field is additionally in effect for vector floating-point instructions.  If vsstatus.FS or mstatus.FS is Off then any attempt to ... | V=1 时 vsstatus.FS 也生效，vsstatus.FS=Off 时引发虚拟指令异常 |  |
| 242 | `vsstatus_mstatus_FS_dirty_hypervisor_V_fp` | Any vector floating-point instruction that modifies any floating-point extension state (i.e., floating-point CSRs or f registers) must set both mstatus.FS and vsstatus.FS to Dirty. | V=1 时修改浮点状态的向量浮点指令将 vsstatus.FS 和 mstatus.FS 都设为 Dirty |  |

### Vector Floating-Point Exception Flags

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 243 | `fflags_op_V_fp` | A vector floating-point exception at any active floating-point element sets the standard FP exception flags in the fflags register.  Inactive elements do not set FP exception flags. | 任何活跃浮点元素的向量浮点异常设置标准 FP 异常标志 |  |

### Vector Single-Width Floating-Point Add/Subtract Instructions

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 244 | `vfadd_vfsub_vfrsub_op` | # Floating-point add vfadd.vv vd, vs2, vs1, vm   # Vector-vector vfadd.vf vd, vs2, rs1, vm   # vector-scalar  # Floating-point subtract vfsub.vv vd, vs2, vs1, vm   # Vector-vector vfsub.vf vd, vs2,... | 向量浮点加法/减法/反向减法指令 | vfadd/vfsub/vfrsub |

### Vector Widening Floating-Point Add/Subtract Instructions

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 245 | `vfwadd_op` | # Widening FP add/subtract, 2*SEW = SEW +/- SEW vfwadd.vv vd, vs2, vs1, vm  # vector-vector vfwadd.vf vd, vs2, rs1, vm  # vector-scalar vfwsub.vv vd, vs2, vs1, vm  # vector-vector vfwsub.vf vd, vs2... | 加宽浮点加法/减法，结果宽度 2*SEW | vfwadd/vfwsub |

### Vector Single-Width Floating-Point Multiply/Divide Instructions

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 246 | `vfmul_vfdiv_vfrdiv_op` | # Floating-point multiply vfmul.vv vd, vs2, vs1, vm   # Vector-vector vfmul.vf vd, vs2, rs1, vm   # vector-scalar  # Floating-point divide vfdiv.vv vd, vs2, vs1, vm   # Vector-vector vfdiv.vf vd, v... | 向量浮点乘法/除法/反向除法指令 | vfmul/vfdiv/vfrdiv |

### Vector Widening Floating-Point Multiply

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 247 | `vfwmul_op` | # Widening floating-point multiply vfwmul.vv    vd, vs2, vs1, vm # vector-vector vfwmul.vf    vd, vs2, rs1, vm # vector-scalar | 加宽浮点乘法，结果宽度 2*SEW | vfwmul |

### Vector Single-Width Floating-Point Fused Multiply-Add Instructions

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 248 | `vfmacc_vfnmacc_vfmsac_vfnmsac_vfmadd_vfnmadd_vfmsub_vfnmsub_op` | All four varieties of fused multiply-add are provided, and in two destructive forms that overwrite one of the operands, either the addend or the first multiplicand. | 向量浮点融合乘加指令 | vfmacc/vfnmacc/vfmsac/vfnmsac/vfmadd/vfnmadd/vfmsub/vfnmsub |

### Vector Widening Floating-Point Fused Multiply-Add Instructions

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 249 | `vfwmacc_vfwnmacc_vfwmsac_vfwnmsac_op` | The widening floating-point fused multiply-add instructions all overwrite the wide addend with the result.  The multiplier inputs are all SEW wide, while the addend and destination is 2*SEW bits wide. | 加宽浮点融合乘加，累加到 2*SEW 目标 | vfwmacc/vfwnmacc/vfwmsac/vfwnmsac |

### Vector Floating-Point Square-Root Instruction

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 250 | `vfsqrt_op` | # Floating-point square root vfsqrt.v vd, vs2, vm   # Vector-vector square root | 向量浮点平方根指令 | vfsqrt |

### Vector Floating-Point Reciprocal Square-Root Estimate Instruction

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 251 | `vfrsqrt7_op` | This is a unary vector-vector instruction that returns an estimate of 1/sqrt(x) accurate to 7 bits. | 向量浮点倒数平方根估计(7位精度) | vfrsqrt7 |
| 252 | `vfrsqrt7_op_unex` | For the non-exceptional cases, the low bit of the exponent and the six high bits of significand (after the leading one) are concatenated and used to address the following table. The output of the t... | vfrsqrt7 非异常情况下使用指数低位和尾数高6位查表 | vfrsqrt7 |
| 253 | `vfrsqrt7_op_precise` | More precisely, the result is computed as follows. Let the normalized input exponent be equal to the input exponent if the input is normal, or 0 minus the number of leading zeros in the significand... | vfrsqrt7 精确计算：归一化输入指数和尾数通过查表得到输出 | vfrsqrt7 |

### Vector Floating-Point Reciprocal Estimate Instruction

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 254 | `vfrec7_op` | This is a unary vector-vector instruction that returns an estimate of 1/x accurate to 7 bits. | 向量浮点倒数估计(7位精度) | vfrec7 |
| 255 | `vfrec7_op_unex` | For the non-exceptional cases, the seven high bits of significand (after the leading one) are used to address the following table. The output of the table becomes the seven high bits of the result ... | vfrec7 非异常情况下使用尾数高7位查表 | vfrec7 |
| 256 | `vfrec7_op_precise` | More precisely, the result is computed as follows. Let the normalized input exponent be equal to the input exponent if the input is normal, or 0 minus the number of leading zeros in the significand... | vfrec7 精确计算：归一化输入指数和尾数通过查表得到输出 | vfrec7 |
| 257 | `vfrec7_op_subnorm` | If the input is subnormal, the normalized input significand is given by shifting the input significand left by 1 minus the normalized input exponent, discarding the leading 1 bit. Otherwise, the no... | vfrec7 输入为次正规数时，通过移位输入尾数获得归一化输入尾数 | vfrec7 |
| 258 | `vfrec7_op_output` | If the normalized output exponent is 0 or -1, the result is subnormal: the output exponent is 0, and the output significand is given by concatenating a 1 bit to the left of the normalized output si... | vfrec7 归一化输出指数为0或-1时结果为次正规数 | vfrec7 |

### Vector Floating-Point MIN/MAX Instructions

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 259 | `vfmin_vfmax_op` | The vector floating-point vfmin and vfmax instructions have the same behavior as the corresponding scalar floating-point instructions in version 2.2 of the RISC-V F/D/Q extension: they perform the ... | 向量浮点最小值/最大值，遵循 IEEE 754-2019 规则 | vfmin/vfmax |

### Vector Floating-Point Sign-Injection Instructions

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 260 | `vfsgnj_vfsgnjn_vfsgnjx_op` | Vector versions of the scalar sign-injection instructions.  The result takes all bits except the sign bit from the vector vs2 operands. | 向量浮点符号注入指令 | vfsgnj/vfsgnjn/vfsgnjx |

### Vector Floating-Point Compare Instructions

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 261 | `vmfeq_vmfne_vmflt_vmfle_vmfgt_vmfge_op` | These vector FP compare instructions compare two source operands and write the comparison result to a mask register. | 向量浮点比较将结果写入掩码寄存器 | vmfeq/vmfne/vmflt/vmfle/vmfgt/vmfge |
| 262 | `vmfeq_vmfne_vmflt_vmfle_vmfgt_vmfge_vd_single_vreg` | The destination mask vector is always held in a single vector register, with a layout of elements as described in <<sec-mask-register-layout>>. | 目标掩码向量始终在单个向量寄存器中 | vmfeq/vmfne/vmflt/vmfle/vmfgt/vmfge |
| 263 | `vmfeq_vmfne_vmflt_vmfle_vmfgt_vmfge_vd_eq_v0` | The destination mask vector register may be the same as the source vector mask register (v0). | 目标掩码向量寄存器可与源向量掩码寄存器(v0)相同 | vmfeq/vmfne/vmflt/vmfle/vmfgt/vmfge |
| 264 | `vmfeq_vmfne_vmflt_vmfle_vmfgt_vmfge_tail_agnostic` | Compares write mask registers, and so always operate under a tail-agnostic policy. | 浮点比较写入掩码寄存器，始终以 tail-agnostic 策略操作 | vmfeq/vmfne/vmflt/vmfle/vmfgt/vmfge |
| 265 | `vmfeq_vmfne_sNaN_invalid` | vmfeq and vmfne raise the invalid operation exception only on signaling NaN inputs. | vmfeq/vmfne 仅对信号 NaN 输入引发无效操作异常 | vmfeq/vmfne |
| 266 | `vmflt_vmfle_vmfgt_vmfge_sqNaN_invalid` | vmflt, vmfle, vmfgt, and vmfge raise the invalid operation exception on both signaling and quiet NaN inputs. | vmflt/vmfle/vmfgt/vmfge 对信号和安静 NaN 都引发无效操作异常 | vmflt/vmfle/vmfgt/vmfge |
| 267 | `vmfne_vdval1_NaN` | vmfne writes 1 to the destination element when either operand is NaN, | vmfne 在任一操作数为 NaN 时向目标元素写入1 | vmfne |
| 268 | `vmfeq_vmflt_vmfle_vmfgt_vmfge_vdval0_NaN` | whereas the other compares write 0 when either operand is NaN. | 其他比较指令在任一操作数为 NaN 时写入0 | vmfeq/vmflt/vmfle/vmfgt/vmfge |

### Vector Floating-Point Classify Instruction

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 269 | `vfclass_op` | This is a unary vector-vector instruction that operates in the same way as the scalar classify instruction. | 向量浮点分类，将元素分类为10种浮点类别之一 | vfclass |
| 270 | `vfclass_op_result` | The 10-bit mask produced by this instruction is placed in the least-significant bits of the result elements.  The upper (SEW-10) bits of the result are filled with zeros. | vfclass 产生的10位掩码放在结果元素的最低有效位 | vfclass |
| 271 | `vfclass_SEWge16` | The instruction is only defined for SEW=16b and above, so the result will always fit in the destination elements. | vfclass 仅定义于 SEW≥16b，结果始终能放入目标元素 | vfclass |

### Vector Floating-Point Merge Instruction

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 272 | `vfmerge_op` | A vector-scalar floating-point merge instruction is provided, | 向量浮点合并根据掩码从向量和标量源选择元素 | vfmerge |
| 273 | `vfmerge_all_elem` | operates on all body elements from vstart up to the current vector length in vl regardless of mask value. | vfmerge 对从 vstart 到 vl 的所有主体元素操作，不受掩码值影响 | vfmerge |
| 274 | `vfmerge_op_mask` | At elements where the mask value is zero, the first vector operand is copied to the destination element, otherwise a scalar floating-point register value is copied to the destination element. | vfmerge 在掩码值为0的元素处复制第一个向量操作数到目标 | vfmerge |

### Vector Floating-Point Move Instruction

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 275 | `vfmv_op` | The vector floating-point move instruction __splats__ a floating-point scalar operand to a vector register group.  The instruction copies a scalar f register value to all active elements of a vecto... | 向量浮点移动将标量浮点值广播到所有目标元素 | vfmv.v.f |
| 276 | `vfmv_vs2_nv0_rsv` | The instruction must have the vs2 field set to v0, with all other values for vs2 reserved. | vfmv 的 vs2 字段必须设为 v0，其他值保留 | vfmv.v.f |

### Single-Width Floating-Point/Integer Type-Convert Instructions

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 277 | `vfcvt_op` | Conversion operations are provided to convert to and from floating-point values and unsigned and signed integers, where both source and destination are SEW wide. | 向量浮点/整数类型转换指令 | vfcvt |
| 278 | `vfcvt_op_exceptions` | The conversions follow the same rules on exceptional conditions as the scalar conversion instructions. | 转换遵循与标量转换指令相同的异常条件规则 | vfcvt |
| 279 | `vfcvt_op_frm` | The conversions use the dynamic rounding mode in frm, except for the rtz variants, which round towards zero. | 转换使用 frm 中的动态舍入模式，rtz 变体除外 | vfcvt |

### Widening Floating-Point/Integer Type-Convert Instructions

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 280 | `vfwcvt_op` | A set of conversion instructions is provided to convert between narrower integer and floating-point datatypes to a type of twice the width. | 加宽浮点/整数类型转换，结果宽度 2*SEW | vfwcvt |
| 281 | `vfwcvt_vreg_constr` | These instructions have the same constraints on vector register overlap as other widening instructions (see <<sec-widening>>). | 加宽类型转换的向量寄存器重叠约束与其他加宽指令相同 | vfwcvt |

### Narrowing Floating-Point/Integer Type-Convert Instructions

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 282 | `vfncvt_op` | A set of conversion instructions is provided to convert wider integer and floating-point datatypes to a type of half the width. | 窄化浮点/整数类型转换，将 2*SEW 转为 SEW | vfncvt |
| 283 | `vfncvt_vreg_constr` | These instructions have the same constraints on vector register overlap as other narrowing instructions (see <<sec-narrowing>>). | 窄化类型转换的向量寄存器重叠约束与其他窄化指令相同 | vfncvt |

## 13. Vector Reduction Operations (22 条)

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 284 | `vreduction_scalar_def` | Vector reduction operations take a vector register group of elements and a scalar held in element 0 of a vector register, and perform a reduction using some binary operator, to produce a scalar res... | 向量归约取向量寄存器组元素和标量初始值进行归约 |  |
| 285 | `vreduction_scalar_disregard_LMUL` | The scalar input and output operands are held in element 0 of a single vector register, not a vector register group, so any vector register can be the scalar source or destination of a vector reduc... | 标量输入和输出在单个向量寄存器的元素0中，与 LMUL 无关 |  |
| 286 | `vreduction_vd_overlap_vs` | The destination vector register can overlap the source operands, including the mask register. | 目标向量寄存器可与源操作数重叠(包括掩码寄存器) |  |
| 287 | `vreduction_scalar_disregard_maskval` | Inactive elements from the source vector register group are excluded from the reduction, but the scalar operand is always included regardless of the mask values. | 非活跃元素从归约中排除 |  |
| 288 | `vreduction_tail_policy` | The other elements in the destination vector register ( 0 < index < VLEN/SEW) are considered the tail and are managed with the current tail agnostic/undisturbed policy. | 目标向量寄存器中其他元素(0<index<VLEN/SEW)按尾部不可知策略处理 |  |
| 289 | `vreduction_vl_0` | If vl=0, no operation is performed and the destination register is not updated. | vl=0 时不执行操作，目标寄存器不更新 |  |
| 290 | `vreduction_trap` | Traps on vector reduction instructions are always reported with a vstart of 0. | 向量归约指令的陷阱始终以 vstart=0 报告 |  |
| 291 | `vreduction_vstart_n0_ill` | Vector reduction operations raise an illegal-instruction exception if vstart is non-zero. | 向量归约操作在 vstart≠0 时引发非法指令异常 |  |

### Vector Single-Width Integer Reduction Instructions

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 292 | `vredsum_vredmaxu_vredmax_vredminu_vredmin_vredand_vredor_vredxor_op` | All operands and results of single-width reduction instructions have the same SEW width. | 单宽度整数归约指令，所有操作数和结果具有相同 SEW | vredsum/vredmaxu/vredmax/vredminu/vredmin/vredand/vredor/vredxor |
| 293 | `vredsum_vredmaxu_vredmax_vredminu_vredmin_vredand_vredor_vredxor_overflow` | Overflows wrap around on arithmetic sums. | 算术求和溢出时环绕 | vredsum |

### Vector Widening Integer Reduction Instructions

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 294 | `vwredsumu_op` | The unsigned vwredsumu.vs instruction zero-extends the SEW-wide vector elements before summing them, then adds the 2*SEW-width scalar element, and stores the result in a 2*SEW-width scalar element. | 无符号加宽归约求和：将 SEW 宽元素零扩展后累加到 2*SEW 累加器 | vwredsumu |
| 295 | `vwredsum_op` | The vwredsum.vs instruction sign-extends the SEW-wide vector elements before summing them. | 有符号加宽归约求和：将 SEW 宽元素符号扩展后累加到 2*SEW 累加器 | vwredsum |
| 296 | `vwredsumu_vwredsum_op_overflow` | For both vwredsumu.vs and vwredsum.vs, overflows wrap around. | vwredsumu 和 vwredsum 溢出时环绕 | vwredsumu/vwredsum |

### Vector Single-Width Floating-Point Reduction Instructions

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 297 | `vfredosum_op` | The vfredosum instruction must sum the floating-point values in element order, starting with the scalar in vs1[0] | vfredosum 必须按元素顺序求和，从 vs1[0] 开始 | vfredosum |
| 298 | `vfredosum_op_exceptions` | where each addition operates identically to the scalar floating-point instructions in terms of raising exception flags and generating or propagating special values. | vfredosum 每次加法与标量浮点加法指令行为相同 | vfredosum |
| 299 | `vfredosum_maskoff` | When the operation is masked (vm=0), the masked-off elements do not affect the result or the exception flags. | 掩码关闭的元素不影响 vfredosum 的结果 | vfredosum |
| 300 | `vfredusum_op` | The implementation must produce a result equivalent to a reduction tree composed of binary operator nodes, with the inputs being elements from the source vector register group (vs2) and the source ... | vfredusum 实现必须产生等价于归约树的结果 | vfredusum |
| 301 | `vfredusum_additive_impl` | The additive identity is +0.0 when rounding down (towards -) or -0.0 for all other rounding modes. | vfredusum 的加法恒等元素：向负无穷舍入时为+0.0，其他模式为-0.0 | vfredusum |
| 302 | `vfredusum_redtree` | The reduction tree structure must be deterministic for a given value in vtype and vl. | vfredusum 归约树结构对给定 vtype 和 vl 值必须确定性 | vfredusum |
| 303 | `vfredmin_vfredmax_op` | The vfredmin and vfredmax instructions reduce the scalar argument in vs1[0] and active elements in vs2 using the minimumNumber and maximumNumber operations, respectively. | vfredmin/vfredmax 对标量参数和向量元素执行浮点最小/最大归约 | vfredmin/vfredmax |

### Vector Widening Floating-Point Reduction Instructions

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 304 | `vfwredosum_vfwredusum_op` | Widening forms of the sum reductions are provided that read and write a double-width reduction result. | 加宽浮点归约求和读写双倍宽度累加器 | vfwredosum/vfwredusum |
| 305 | `vfwredosum_vfwredusum_op_reduction` | The reduction of the SEW-width elements is performed as in the single-width reduction case, with the elements in vs2 promoted to 2*SEW bits before adding to the 2*SEW-bit accumulator. | 加宽浮点归约的 SEW 宽元素归约方式与单宽度归约相同 | vfwredosum/vfwredusum |

## 14. Vector Mask Instructions (46 条)

### Vector Mask-Register Logical Instructions

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 306 | `vmask_maskreg_def` | Vector mask-register logical operations operate on mask registers. Each element in a mask register is a single bit, | 向量掩码寄存器逻辑操作对掩码寄存器操作，每个元素占1位 | vmand/vmnand/vmandn/vmxor/vmor/vmnor/vmorn/vmxnor |
| 307 | `instrgrp_vmask_disregard_vlmul` | so these instructions all operate on single vector registers regardless of the setting of the vlmul field in vtype.  They do not change the value of vlmul. | 掩码逻辑指令操作单个向量寄存器，与 LMUL 无关 | vmand/vmnand/vmandn/vmxor/vmor/vmnor/vmorn/vmxnor |
| 308 | `vmask_vd_overlap_vs` | The destination vector register may be the same as either source vector register. | 目标向量寄存器可与任一源向量寄存器相同 | vmand/vmnand/vmandn/vmxor/vmor/vmnor/vmorn/vmxnor |
| 309 | `vmask_vstart` | As with other vector instructions, the elements with indices less than vstart are unchanged, and vstart is reset to zero after execution. | 与其他向量指令一样，索引小于 vstart 的元素不变 | vmand/vmnand/vmandn/vmxor/vmor/vmnor/vmorn/vmxnor |
| 310 | `vmasklogical_unmasked` | Vector mask logical instructions are always unmasked, | 向量掩码逻辑指令始终不使用掩码 | vmand/vmnand/vmandn/vmxor/vmor/vmnor/vmorn/vmxnor |
| 311 | `vmasklogical_masked_rsv` | so there are no inactive elements, and the encodings with vm=0 are reserved. | 掩码逻辑指令没有非活跃元素，vm=0 编码保留 | vmand/vmnand/vmandn/vmxor/vmor/vmnor/vmorn/vmxnor |
| 312 | `vmasklogical_tail_agnostic` | Mask elements past vl, the tail elements, are always updated with a tail-agnostic policy. | 掩码逻辑指令尾部元素始终用 tail-agnostic 策略更新 | vmand/vmnand/vmandn/vmxor/vmor/vmnor/vmorn/vmxnor |
| 313 | `vmand_vmnand_vmandn_vmxor_vmor_vmnor_vmorn_vmxnor_op` | vmand.mm vd, vs2, vs1   # vd.mask[i] =   vs2.mask[i] &&  vs1.mask[i] vmnand.mm vd, vs2, vs1  # vd.mask[i] = !(vs2.mask[i] &&  vs1.mask[i]) vmandn.mm vd, vs2, vs1  # vd.mask[i] =   vs2.mask[i] && !v... | 向量掩码寄存器逻辑运算指令(AND/NAND/ANDN/XOR/OR/NOR/ORN/XNOR) | vmand/vmnand/vmandn/vmxor/vmor/vmnor/vmorn/vmxnor |

### Vector count population in mask `vcpop.m`

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 314 | `vcpop_vs_single_vreg` | The source operand is a single vector register holding mask register values as described in <<sec-mask-register-layout>>. | vcpop 源操作数是保存掩码值的单个向量寄存器 | vcpop |
| 315 | `vcpop_op` | The vcpop.m instruction counts the number of mask elements of the active elements of the vector source mask register that have the value 1 and writes the result to a scalar x register. | vcpop.m 计算活跃元素中设置的掩码位数量 | vcpop |
| 316 | `vcpop_op_mask` | The operation can be performed under a mask, in which case only the masked elements are counted. | vcpop 可在掩码下执行，只计算掩码启用的元素 | vcpop |
| 317 | `vcpop_vl0` | The vcpop.m instruction writes x[rd] even if vl=0 (with the value 0, since no mask elements are active). | vcpop.m 即使 vl=0 也写入 x[rd](值为0) | vcpop |
| 318 | `vcpop_trap` | Traps on vcpop.m are always reported with a vstart of 0. | vcpop.m 的陷阱始终以 vstart=0 报告 | vcpop |
| 319 | `vcpop_vstart_n0_ill` | The vcpop.m instruction will raise an illegal-instruction exception if vstart is non-zero. | vcpop.m 在 vstart≠0 时引发非法指令异常 | vcpop |

### `vfirst` find-first-set mask bit

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 320 | `vfirst_op` | The vfirst instruction finds the lowest-numbered active element of the source mask vector that has the value 1 and writes that element's index to a GPR.  If no active element has the value 1, -1 is... | vfirst 找到源掩码中最低编号的活跃且设置的元素 | vfirst |
| 321 | `vfirst_vl0` | The vfirst.m instruction writes x[rd] even if vl=0 (with the value -1, since no mask elements are active). | vfirst.m 即使 vl=0 也写入 x[rd](值为-1) | vfirst |
| 322 | `vfirst_trap` | Traps on vfirst are always reported with a vstart of 0. | vfirst 的陷阱始终以 vstart=0 报告 | vfirst |
| 323 | `vfirst_vstart_n0_ill` | The vfirst instruction will raise an illegal-instruction exception if vstart is non-zero. | vfirst 在 vstart≠0 时引发非法指令异常 | vfirst |

### `vmsbf.m` set-before-first mask bit

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 324 | `vmsbf_op` | The vmsbf.m instruction takes a mask register as input and writes results to a mask register.  The instruction writes a 1 to all active mask elements before the first active source element that is ... | vmsbf.m 取掩码输入，在第一个设置位之前的所有位设为1 | vmsbf |
| 325 | `vmsbf_tail_agnostic` | The tail elements in the destination mask register are updated under a tail-agnostic policy. | vmsbf 目标掩码尾部元素用 tail-agnostic 策略更新 | vmsbf |
| 326 | `vmsbf_trap` | Traps on vmsbf.m are always reported with a vstart of 0. | vmsbf.m 的陷阱始终以 vstart=0 报告 | vmsbf |
| 327 | `vmsbf_vstart_n0_ill` | The vmsbf instruction will raise an illegal-instruction exception if vstart is non-zero. | vmsbf 在 vstart≠0 时引发非法指令异常 | vmsbf |
| 328 | `vmsbf_vreg_constr` | The destination register cannot overlap the source register and, if masked, cannot overlap the mask register ('v0'). | vmsbf 目标不能与源重叠，掩码时也不能与 v0 重叠 | vmsbf |

### `vmsif.m` set-including-first mask bit

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 329 | `vmsif_op` | The vector mask set-including-first instruction is similar to set-before-first, except it also includes the element with a set bit. | vmsif.m 类似 vmsbf，但包含第一个设置位本身 | vmsif |
| 330 | `vmsif_tail_agnostic` | The tail elements in the destination mask register are updated under a tail-agnostic policy. | vmsif 目标掩码尾部元素用 tail-agnostic 策略更新 | vmsif |
| 331 | `vmsif_trap` | Traps on vmsif.m are always reported with a vstart of 0. | vmsif.m 的陷阱始终以 vstart=0 报告 | vmsif |
| 332 | `vmsif_vstart_n0_ill` | The vmsif instruction will raise an illegal-instruction exception if vstart is non-zero. | vmsif 在 vstart≠0 时引发非法指令异常 | vmsif |
| 333 | `vmsif_vreg_constr` | The destination register cannot overlap the source register and, if masked, cannot overlap the mask register ('v0'). | vmsif 目标不能与源重叠，掩码时也不能与 v0 重叠 | vmsif |

### `vmsof.m` set-only-first mask bit

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 334 | `vmsof_op` | The vector mask set-only-first instruction is similar to set-before-first, except it only sets the first element with a bit set, if any. | vmsof.m 类似 vmsbf，但只设置第一个设置位对应位置 | vmsof |
| 335 | `vmsof_tail_agnostic` | The tail elements in the destination mask register are updated under a tail-agnostic policy. | vmsof 目标掩码尾部元素用 tail-agnostic 策略更新 | vmsof |
| 336 | `vmsof_trap` | Traps on vmsof.m are always reported with a vstart of 0. | vmsof.m 的陷阱始终以 vstart=0 报告 | vmsof |
| 337 | `vmsof_vstart_n0_ill` | The vmsof instruction will raise an illegal-instruction exception if vstart is non-zero. | vmsof 在 vstart≠0 时引发非法指令异常 | vmsof |
| 338 | `vmsof_vreg_constr` | The destination register cannot overlap the source register and, if masked, cannot overlap the mask register ('v0'). | vmsof 目标不能与源重叠，掩码时也不能与 v0 重叠 | vmsof |

### Vector Iota Instruction

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 339 | `viota_op` | The viota.m instruction reads a source vector mask register and writes to each element of the destination vector register group the sum of all the bits of elements in the mask register whose index ... | viota.m 读取源掩码，将每个元素位置之前的设置位计数写入目标 | viota |
| 340 | `viota_op_masked` | This instruction can be masked, in which case only the enabled elements contribute to the sum. | viota 可被掩码，只有启用的元素参与计数 | viota |
| 341 | `viota_op_zext` | The result value is zero-extended to fill the destination element if SEW is wider than the result. | viota 结果值零扩展以填充目标元素(如果 SEW 更宽) | viota |
| 342 | `viota_op_overflow` | If the result value would overflow the destination SEW, the least-significant SEW bits are retained. | viota 结果溢出目标 SEW 时保留最低有效 SEW 位 | viota |
| 343 | `viota_trap` | Traps on viota.m are always reported with a vstart of 0, | viota.m 的陷阱始终以 vstart=0 报告 | viota |
| 344 | `viota_restart` | execution is always restarted from the beginning when resuming after a trap handler. | viota 从陷阱恢复时始终从头重新开始执行 | viota |
| 345 | `viota_vstart_n0_ill` | An illegal-instruction exception is raised if vstart is non-zero. | viota 在 vstart≠0 时引发非法指令异常 | viota |
| 346 | `viota_vreg_constr` | The destination register group cannot overlap the source register and, if masked, cannot overlap the mask register (v0). | viota 目标寄存器组不能与源重叠，掩码时也不能与 v0 重叠 | viota |

### Vector Element Index Instruction

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 347 | `vid_op` | The vid.v instruction writes each element's index to the destination vector register group, from 0 to vl-1. | vid.v 将每个元素的索引写入目标向量寄存器 | vid |
| 348 | `vid_op_mask` | The instruction can be masked.  Masking does not change the index value written to active elements. | vid 可被掩码，掩码不改变写入的索引值 | vid |
| 349 | `vid_vs2_nv0_rsv` | The vs2 field of the instruction must be set to v0, otherwise the encoding is _reserved_. | vid 的 vs2 字段必须设为 v0，否则编码保留 | vid |
| 350 | `vid_op_zext` | The result value is zero-extended to fill the destination element if SEW is wider than the result. | vid 结果值零扩展以填充目标元素 | vid |
| 351 | `vid_op_overflow` | If the result value would overflow the destination SEW, the least-significant SEW bits are retained. | vid 结果溢出目标 SEW 时保留最低有效 SEW 位 | vid |

## 15. Vector Permutation Instructions (48 条)

### Integer Scalar Move Instructions

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 352 | `vmv-x-s_vmv-s-x_ignoreLMUL` | The instructions ignore LMUL and vector register groups. | vmv.x.s/vmv.s.x 忽略 LMUL 和向量寄存器组 | vmv.x.s/vmv.s.x |
| 353 | `vmv-x-s_op` | The vmv.x.s instruction copies a single SEW-wide element from index 0 of the source vector register to a destination integer register.  If SEW > XLEN, the least-significant XLEN bits are transferre... | vmv.x.s 将源向量寄存器索引0的 SEW 宽元素复制到标量寄存器 | vmv.x.s |
| 354 | `vmv-x-s_vstartgevl_vl0` | vmv.x.s performs its operation even if vstart  vl or vl=0. | vmv.x.s 即使 vstart≥vl 或 vl=0 也执行操作 | vmv.x.s |
| 355 | `vmv-s-x_op` | The vmv.s.x instruction copies the scalar integer register to element 0 of the destination vector register.  If SEW < XLEN, the least-significant bits are copied and the upper XLEN-SEW bits are ign... | vmv.s.x 将标量整数寄存器复制到目标向量寄存器的元素0 | vmv.s.x |
| 356 | `vmv-s-x_vstart_ge_vl` | If vstart  vl, no operation is performed and the destination register is not updated. | vmv.s.x 在 vstart≥vl 时不执行操作，目标不更新 | vmv.s.x |
| 357 | `vmv-s-x_vmv-x-s_masked_rsv` | The encodings corresponding to the masked versions (vm=0) of vmv.x.s and vmv.s.x are reserved. | vmv.x.s 和 vmv.s.x 的掩码版本(vm=0)编码保留 | vmv.x.s/vmv.s.x |

### Floating-Point Scalar Move Instructions

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 358 | `vfmv-f-s_vfmv-s-f_ignoreLMUL` | The instructions ignore LMUL and vector register groups. | vfmv.f.s/vfmv.s.f 忽略 LMUL 和向量寄存器组 | vfmv.f.s/vfmv.s.f |
| 359 | `vfmv-f-s_op` | The vfmv.f.s instruction copies a single SEW-wide element from index 0 of the source vector register to a destination scalar floating-point register. | vfmv.f.s 将源向量寄存器索引0的 SEW 宽元素复制到浮点寄存器 | vfmv.f.s |
| 360 | `vfmv-s-f_op` | The vfmv.s.f instruction copies the scalar floating-point register to element 0 of the destination vector register.  The other elements in the destination vector register ( 0 < index < VLEN/SEW) ar... | vfmv.s.f 将标量浮点寄存器复制到目标向量寄存器的元素0 | vfmv.s.f |
| 361 | `vfmv-s-f_vstart_ge_vl` | If vstart  vl, no operation is performed and the destination register is not updated. | vfmv.s.f 在 vstart≥vl 时不执行操作，目标不更新 | vfmv.s.f |
| 362 | `vfmv-s-f_masked_rsv` | The encodings corresponding to the masked versions (vm=0) of vfmv.f.s and vfmv.s.f are reserved. | vfmv.f.s 和 vfmv.s.f 的掩码版本(vm=0)编码保留 | vfmv.f.s/vfmv.s.f |

### Vector Slide Instructions

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 363 | `vslide_vstart_ge_vl` | For all of the vslideup, vslidedown, v[f]slide1up, and v[f]slide1down instructions, if vstart  vl, the instruction performs no operation and leaves the destination vector register unchanged. | 所有 slide 指令在 vstart≥vl 时不执行操作 | vslideup/vslidedown/vslide1up/vslide1down/vfslide1up/vfslide1down |
| 364 | `vslide_mask` | The slide instructions may be masked, with mask element _i_ controlling whether _destination_ element _i_ is written.  The mask undisturbed/agnostic policy is followed for inactive elements. | slide 指令可被掩码，掩码元素 i 控制是否写入目标元素 i | vslideup/vslidedown/vslide1up/vslide1down |
| 365 | `vslideup_op` | For vslideup, the value in vl specifies the maximum number of destination elements that are written.  The start index (_OFFSET_) for the destination can be either specified using an unsigned intege... | vslideup 中 vl 指定最大目标元素数，OFFSET 之前的元素不变 | vslideup |
| 366 | `vslideup_vreg_constr` | The destination vector register group for vslideup cannot overlap the source vector register group, otherwise the instruction encoding is reserved. | vslideup 目标向量寄存器组不能与源重叠 | vslideup |
| 367 | `vslidedown_op` | For vslidedown, the value in vl specifies the maximum number of destination elements that are written.  The remaining elements past vl are handled according to the current tail policy (<<sec-agnost... | vslidedown 中 vl 指定最大目标元素数，从源的 OFFSET 位置开始复制 | vslidedown |
| 368 | `vslidedown_op_src` | The start index (_OFFSET_) for the source can be either specified using an unsigned integer in the x register specified by rs1, or a 5-bit immediate, zero-extended to XLEN bits. If XLEN > SEW, _OFF... | vslidedown 的起始索引(OFFSET)可由无符号整数或5位立即数指定 | vslidedown |
| 369 | `vslide1up-vx_op` | The vslide1up instruction places the x register argument at location 0 of the destination vector register group, provided that element 0 is active, otherwise the destination element update follows ... | vslide1up 将标量寄存器值放在目标位置0 | vslide1up |
| 370 | `vslide1up-vx_op_rem_elem` | The remaining active vl-1 elements are copied over from index _i_ in the source vector register group to index _i_+1 in the destination vector register group. | vslide1up 剩余 vl-1 个活跃元素从源索引 i 复制到目标索引 i+1 | vslide1up |
| 371 | `vslide1up-vx_op_vl` | The vl register specifies the maximum number of destination vector register elements updated with source values, and remaining elements past vl are handled according to the current tail policy (<<s... | vslide1up 中 vl 指定最大目标元素数 | vslide1up |
| 372 | `vslide1up-vx_vreg_constr` | The vslide1up instruction requires that the destination vector register group does not overlap the source vector register group. Otherwise, the instruction encoding is reserved. | vslide1up 目标向量寄存器组不能与源重叠 | vslide1up |
| 373 | `vslide1up-vf_op` | The vfslide1up instruction is defined analogously to vslide1up, but sources its scalar argument from an f register. | vfslide1up 类似 vslide1up，但源为浮点标量寄存器 | vfslide1up |
| 374 | `vslide1down-vx_op` | The vslide1down instruction copies the first vl-1 active elements values from index _i_+1 in the source vector register group to index _i_ in the destination vector register group. | vslide1down 将前 vl-1 个活跃元素从源索引 i+1 复制到目标索引 i | vslide1down |
| 375 | `vslide1down-vx_op_vl` | The vl register specifies the maximum number of destination vector register elements written with source values, and remaining elements past vl are handled according to the current tail policy (<<s... | vslide1down 中 vl 指定最大目标元素数 | vslide1down |
| 376 | `vslide1down-vx_op_details` | The vslide1down instruction places the x register argument at location vl-1 in the destination vector register, provided that element vl-1 is active, otherwise the destination element update follow... | vslide1down 将标量寄存器值放在目标位置 vl-1 | vslide1down |
| 377 | `vslide1down-vf_op` | The vfslide1down instruction is defined analogously to vslide1down, but sources its scalar argument from an f register. | vfslide1down 类似 vslide1down，但源为浮点标量寄存器 | vfslide1down |

### Vector Register Gather Instructions

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 378 | `vrgather_vrgatherei16_vs2_uint` | The index values in the second vector are treated as unsigned integers. | vrgather 中索引值被视为无符号整数 | vrgather/vrgatherei16 |
| 379 | `vrgather_vrgatherei16_vs_ignore_vl` | The source vector can be read at any index < VLMAX regardless of vl. | vrgather 源向量可在任何 index<VLMAX 处读取，不受 vl 限制 | vrgather/vrgatherei16 |
| 380 | `vrgather_vrgatherei16_vl` | The maximum number of elements to write to the destination register is given by vl, | vrgather 写入目标寄存器的最大元素数由 vl 给出 | vrgather/vrgatherei16 |
| 381 | `vrgather_vrgatherei16_tail` | and the remaining elements past vl are handled according to the current tail policy (<<sec-agnostic>>). | vrgather 超过 vl 的元素按当前尾部策略处理 | vrgather/vrgatherei16 |
| 382 | `vrgather_vrgatherei16_mask` | The operation can be masked, and the mask undisturbed/agnostic policy is followed for inactive elements. | vrgather 可被掩码，非活跃元素按掩码 undisturbed/agnostic 策略处理 | vrgather/vrgatherei16 |
| 383 | `vrgather-vv_op_vrgatherei16-vv_op` | vrgather.vv vd, vs2, vs1, vm     # vd[i] = (vs1[i] >= VLMAX) ? 0 : vs2[vs1[i]]; vrgatherei16.vv vd, vs2, vs1, vm # vd[i] = (vs1[i] >= VLMAX) ? 0 : vs2[vs1[i]]; | vrgather.vv/vrgatherei16.vv 按索引从源向量收集元素到目标 | vrgather/vrgatherei16 |
| 384 | `vrgather-vv_sew_lmul` | The vrgather.vv form uses SEW/LMUL for both the data and indices. | vrgather.vv 对数据和索引都使用 SEW/LMUL | vrgather |
| 385 | `vrgatherei16-vv_sew_lmul` | The vrgatherei16.vv form uses SEW/LMUL for the data in vs2 but EEW=16 and EMUL = (16/SEW)*LMUL for the indices in vs1. | vrgatherei16.vv 数据使用 SEW/LMUL，索引使用 EEW=16 | vrgatherei16 |
| 386 | `vrgather_vrgatherei16_id_ge_VLMAX` | If an element index is out of range ( vs1[i]  VLMAX ) then zero is returned for the element value. | vrgather 索引超出范围(≥VLMAX)时返回零 | vrgather/vrgatherei16 |
| 387 | `vrgather-vx_vrgather-vi_op` | Vector-scalar and vector-immediate forms of the register gather are also provided.  These read one element from the source vector at the given index, and write this value to the active elements of ... | vrgather 也提供向量-标量和向量-立即数形式 | vrgather |
| 388 | `vrgather_vrgatherei16_vreg_constr` | For any vrgather instruction, the destination vector register group cannot overlap with the source vector register groups, otherwise the instruction encoding is reserved. | vrgather 目标向量寄存器组不能与任何源操作数重叠 | vrgather/vrgatherei16 |

### Vector Compress Instruction

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 389 | `vcompress_op` | The vector mask register specified by vs1 indicates which of the first vl elements of vector register group vs2 should be extracted and packed into contiguous elements at the beginning of vector re... | vcompress 根据掩码将源向量中选中的元素压缩到目标的连续位置 | vcompress |
| 390 | `vcompress_enc` | Tvcompress is encoded as an unmasked instruction (vm=1). | vcompress 编码为无掩码指令(vm=1) | vcompress |
| 391 | `vcompress_masked_rsv` | The equivalent masked instruction (vm=0) is reserved. | vcompress 的掩码版本(vm=0)编码保留 | vcompress |
| 392 | `vcompress_vreg_constr` | The destination vector register group cannot overlap the source vector register group or the source mask register, otherwise the instruction encoding is reserved. | vcompress 目标不能与源向量寄存器组或掩码操作数重叠 | vcompress |
| 393 | `vcompress_trap` | A trap on a vcompress instruction is always reported with a vstart of 0. | vcompress 的陷阱始终以 vstart=0 报告 | vcompress |
| 394 | `vcompress_vstart_n0_ill` | Executing a vcompress instruction with a non-zero vstart raises an illegal-instruction exception. | vcompress 在 vstart≠0 时引发非法指令异常 | vcompress |

### Whole Vector Register Move

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 395 | `vmv-nr-r_op` | The vmv<nr>r.v instructions copy whole vector registers (i.e., all VLEN bits) and can copy whole vector register groups.  The nr value in the opcode is the number of individual vector registers, NR... | vmv<nr>r.v 复制整个向量寄存器(所有 VLEN 位)，不受 vtype 影响 | vmv1r/vmv2r/vmv4r/vmv8r |
| 396 | `vmv-nr-r_enc` | The instruction is encoded as an OPIVI instruction.  The number of vector registers to copy is encoded in the low three bits of the simm field (simm[2:0]) using the same encoding as the nf[2:0] fie... | vmv<nr>r.v 编码为 OPIVI 指令，寄存器数量由 simm[4:0] 编码 | vmv1r/vmv2r/vmv4r/vmv8r |
| 397 | `vmv-nr-r_nreg_rsv` | The value of NREG must be 1, 2, 4, or 8, and values of simm[4:0] other than 0, 1, 3, and 7 are reserved. | NREG 必须为 1/2/4/8，其他 simm[4:0] 值保留 | vmv1r/vmv2r/vmv4r/vmv8r |
| 398 | `vmv-nr-r_vreg_constr` | The source and destination vector register numbers must be aligned appropriately for the vector register group size, | vmv<nr>r.v 源和目标向量寄存器编号必须适当对齐 | vmv1r/vmv2r/vmv4r/vmv8r |
| 399 | `vmv-nr-r_unaligned_rsv` | encodings with other vector register numbers are reserved. | vmv<nr>r.v 使用未对齐的向量寄存器编号时编码保留 | vmv1r/vmv2r/vmv4r/vmv8r |

## 16. Exception Handling (1 条)

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 400 | `epc_vstart_op_V_trap` | On a trap during a vector instruction (caused by either a synchronous exception or an asynchronous interrupt), the existing *epc CSR is written with a pointer to the trapping vector instruction, wh... | 向量指令陷阱时 epc 指向陷阱指令，vstart 包含要恢复的元素索引 |  |

## 17. Standard Vector Extensions (29 条)

### Zve*: Vector Extensions for Embedded Processors

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 401 | `Zve_XLEN` | Any of these extensions can be added to base ISAs with XLEN=32 or XLEN=64. | 任何 Zve* 扩展可添加到 XLEN=32 或 XLEN=64 的基础 ISA |  |
| 402 | `Zve32f_Zve64x_dependent_Zve32x` | The Zve32f and Zve64x extensions depend on the Zve32x extension. | Zve32f 和 Zve64x 扩展依赖 Zve32x 扩展 |  |
| 403 | `Zve64f_dependent_Zve32f_Zve64x` | The Zve64f extension depends on the Zve32f and Zve64x extensions. | Zve64f 扩展依赖 Zve32f 和 Zve64x 扩展 |  |
| 404 | `Zve64d_dependent_Zve64f` | The Zve64d extension depends on the Zve64f extension. | Zve64d 扩展依赖 Zve64f 扩展 |  |
| 405 | `Zve_precise_traps` | All Zve* extensions have precise traps. | 所有 Zve* 扩展具有精确陷阱 |  |
| 406 | `Zve_eew` | All Zve* extensions provide support for EEW of 8, 16, and 32, and Zve64* extensions also support EEW of 64. | 所有 Zve* 扩展支持 EEW 8/16/32，Zve64* 还支持 EEW 64 |  |
| 407 | `Zve_nsupport_eew64_xlen32` | All Zve* extensions support all vector load and store instructions (<<sec-vector-memory>>), except Zve64* extensions do not support EEW=64 for index values when XLEN=32. | 所有 Zve* 扩展支持所有向量加载/存储指令 |  |
| 408 | `Zve64_eew64_nsupport_vmulh` | All Zve* extensions support all vector integer instructions (<<sec-vector-integer>>), except that the vmulh integer multiply variants that return the high word of the product (vmulh.vv, vmulh.vx, v... | 所有 Zve* 扩展支持所有向量整数指令 |  |
| 409 | `Zve64_eew64_nsupport_vsmul` | All Zve* extensions support all vector fixed-point arithmetic instructions (<<sec-vector-fixed-point>>), except that vsmul.vv and vsmul.vx are not included in EEW=64 in Zve64*. | 所有 Zve* 扩展支持所有向量定点算术指令 |  |
| 410 | `Zve32x_Zve64x_nsupport_freg` | All Zve* extensions support all vector permutation instructions (<<sec-vector-permute>>), except that Zve32x and Zve64x do not include those with floating-point operands, and Zve64f does not includ... | 所有 Zve* 扩展支持所有向量置换指令 |  |
| 411 | `Zve32x_dependent_Zicsr` | The Zve32x extension depends on the Zicsr extension. | Zve32x 扩展依赖 Zicsr 扩展 |  |
| 412 | `Zve32f_Zve64f_dependent_F` | The Zve32f and Zve64f extensions depend upon the F extension, and implement all vector floating-point instructions (<<sec-vector-float>>) for floating-point operands with EEW=32. Vector single-widt... | Zve32f/Zve64f 依赖 F 扩展，实现所有单精度向量浮点指令 |  |

### V: Vector Extension for Application Processors

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 413 | `misa_v_op` | The misa.v bit is set for implementations providing misa and supporting V. | 提供 misa 且支持 V 的实现设置 misa.v 位 |  |
| 414 | `V_precise_traps` | The V vector extension has precise traps. | V 向量扩展具有精确陷阱 |  |
| 415 | `V_dependent_Zvl128b_Zve64d` | The V vector extension depends upon the Zvl128b and Zve64d extensions. | V 扩展依赖 Zvl128b 和 Zve64d 扩展 |  |
| 416 | `V_supported_eew` | The V extension supports EEW of 8, 16, and 32, and 64. | V 扩展支持 EEW 8/16/32/64 |  |
| 417 | `V_instr_config` | The V extension supports the vector configuration instructions (<<sec-vector-config>>). | V 扩展支持向量配置指令 | vsetvli/vsetivli/vsetvl |
| 418 | `V_instr_ls_eew64_nsupported_xlen32` | The V extension supports all vector load and store instructions (<<sec-vector-memory>>), except the V extension does not support EEW=64 for index values when XLEN=32. | V 扩展支持所有向量加载/存储指令 |  |
| 419 | `V_instr_int` | The V extension supports all vector integer instructions (<<sec-vector-integer>>). | V 扩展支持所有向量整数指令 |  |
| 420 | `V_instr_fixedpt` | The V extension supports all vector fixed-point arithmetic instructions (<<sec-vector-fixed-point>>). | V 扩展支持所有向量定点算术指令 |  |
| 421 | `V_instr_red` | The V extension supports all vector integer single-width and widening reduction operations (<<sec-vector-integer-reduce>>, <<sec-vector-integer-reduce-widen>>). | V 扩展支持所有向量整数单宽度和加宽归约指令 |  |
| 422 | `V_instr_mask` | The V extension supports all vector mask instructions (<<sec-vector-mask>>). | V 扩展支持所有向量掩码指令 |  |
| 423 | `V_instr_perm` | The V extension supports all vector permutation instructions (<<sec-vector-permute>>). | V 扩展支持所有向量置换指令 |  |
| 424 | `V_dependent_F_D` | The V extension depends upon the F and D extensions, and implements all vector floating-point instructions (<<sec-vector-float>>) for floating-point operands with EEW=32 or EEW=64 (including wideni... | V 扩展依赖 F 和 D 扩展，实现所有向量浮点指令 |  |

### Zvfhmin: Vector Extension for Minimal Half-Precision Floating-Point

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 425 | `Zvfhmin_dependent_Zve32f` | The Zvfhmin extension depends on the Zve32f extension. | Zvfhmin 扩展依赖 Zve32f 扩展 |  |

### Zvfh: Vector Extension for Half-Precision Floating-Point

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 426 | `Zvfh_instr` | When the Zvfh extension is implemented, all instructions in <<sec-vector-float>>, <<sec-vector-float-reduce>>, <<sec-vector-float-reduce-widen>>, <<sec-vector-float-move>>, <<sec-vfslide1up>>, and ... | Zvfh 实现时，所有向量浮点指令支持 EEW=16 操作数 |  |
| 427 | `Zvfh_eew16` | The EEW=16 floating-point operands of these instructions use the binary16 format. | Zvfh 的 EEW=16 浮点操作数使用 binary16 格式 |  |
| 428 | `Zvfh_instr_cvt` | Additionally, conversions between 8-bit integers and binary16 values are provided.  The floating-point-to-integer narrowing conversions (vfncvt[.rtz].x[u].f.w) and integer-to-floating-point widenin... | Zvfh 还提供8位整数和 binary16 值之间的转换 |  |
| 429 | `Zvfh_dependent_Zve32f_Zfhmin` | The Zvfh extension depends on the Zve32f and Zfhmin extensions. | Zvfh 扩展依赖 Zve32f 和 Zfhmin 扩展 |  |

## 18. Vector Element Groups (6 条)

### Element Group Size

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 430 | `egs_ge_vlmax_rsv` | Vector instructions with EGS > VLMAX are reserved. | EGS>VLMAX 的向量指令保留 |  |

### Setting `vl`

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 431 | `egs_vl_rsv` | When an operand is a vector of element groups, the vl setting must correspond to an integer multiple of the element group size, with other values of vl reserved. | 元素组指令的 vl 设置必须对应整数个元素组 |  |
| 432 | `egs_vl_avl` | When element group instructions are present, an additional constraint is placed on the setting of vl based on an AVL value (augmenting <<constraints-on-setting-vl>>). EGSMAX is the largest EGS supp... | 元素组指令存在时，AVL 约束增加：vl 必须是 EGS 的整数倍 |  |

### Determining EEW

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 433 | `egs_sew_eew` | The vtype SEW can be used to indicate or calculate the effective element size (EEW) of one or more operands of an element group instruction.  Where the operand is an element group, SEW and EEW refe... | vtype SEW 可用于指示或计算有效元素大小(EEW) |  |

### Determining EMUL

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 434 | `egs_lmul_emul` | The vtype LMUL setting can be used to indicate or calculate the effective length multiplier (EMUL) for one or more operands.  Element group instructions tend to exhibit a much wider range of relati... | vtype LMUL 可用于指示或计算有效长度乘数(EMUL) |  |

### Element Group Width

| # | Norm ID | Original Text | 中文解释 | 关联指令/CSR |
|------|---------|---------------|----------|----------|
| 435 | `egs_egw` | The _element_ _group_ _width_ (EGW) is the number of bits in the element group as a whole. For example, the SHA-256 instructions in the Zvknha extension operate on an EGW of 128, with EGS=4 and EEW... | 元素组宽度(EGW)是元素组中的位数，等于 EGS*EEW |  |
