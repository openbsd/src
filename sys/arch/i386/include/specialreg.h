/*	$OpenBSD: specialreg.h,v 1.21 2004/02/19 22:33:29 grange Exp $	*/
/*	$NetBSD: specialreg.h,v 1.7 1994/10/27 04:16:26 cgd Exp $	*/

/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)specialreg.h	7.1 (Berkeley) 5/9/91
 */

/*
 * Bits in 386 special registers:
 */
#define	CR0_PE	0x00000001	/* Protected mode Enable */
#define	CR0_MP	0x00000002	/* "Math" Present (NPX or NPX emulator) */
#define	CR0_EM	0x00000004	/* EMulate non-NPX coproc. (trap ESC only) */
#define	CR0_TS	0x00000008	/* Task Switched (if MP, trap ESC and WAIT) */
#define	CR0_ET	0x00000010	/* Extension Type (387 (if set) vs 287) */
#define	CR0_PG	0x80000000	/* PaGing enable */

/*
 * Bits in 486 special registers:
 */
#define CR0_NE	0x00000020	/* Numeric Error enable (EX16 vs IRQ13) */
#define CR0_WP	0x00010000	/* Write Protect (honor PG_RW in all modes) */
#define CR0_AM	0x00040000	/* Alignment Mask (set to enable AC flag) */
#define	CR0_NW	0x20000000	/* Not Write-through */
#define	CR0_CD	0x40000000	/* Cache Disable */

/*
 * Cyrix 486 DLC special registers, accessable as IO ports.
 */
#define CCR0	0xc0		/* configuration control register 0 */
#define CCR0_NC0	0x01	/* first 64K of each 1M memory region is non-cacheable */
#define CCR0_NC1	0x02	/* 640K-1M region is non-cacheable */
#define CCR0_A20M	0x04	/* enables A20M# input pin */
#define CCR0_KEN	0x08	/* enables KEN# input pin */
#define CCR0_FLUSH	0x10	/* enables FLUSH# input pin */
#define CCR0_BARB	0x20	/* flushes internal cache when entering hold state */
#define CCR0_CO		0x40	/* cache org: 1=direct mapped, 0=2x set assoc */
#define CCR0_SUSPEND	0x80	/* enables SUSP# and SUSPA# pins */

#define CCR1	0xc1		/* configuration control register 1 */
#define CCR1_RPL	0x01	/* enables RPLSET and RPLVAL# pins */
/* the remaining 7 bits of this register are reserved */

/*
 * bits in the pentiums %cr4 register:
 */

#define CR4_VME	0x00000001	/* virtual 8086 mode extension enable */
#define CR4_PVI 0x00000002	/* protected mode virtual interrupt enable */
#define CR4_TSD 0x00000004	/* restrict RDTSC instruction to cpl 0 only */
#define CR4_DE	0x00000008	/* debugging extension */
#define CR4_PSE	0x00000010	/* large (4MB) page size enable */
#define CR4_PAE 0x00000020	/* physical address extension enable */
#define CR4_MCE	0x00000040	/* machine check enable */
#define CR4_PGE	0x00000080	/* page global enable */
#define CR4_PCE	0x00000100	/* enable RDPMC instruction for all cpls */
#define CR4_OSFXSR	0x00000200	/* enable SSE instructions (P6 & later) */
#define CR4_OSXMMEXCPT	0x00000400	/* enable SSE instructions (P6 & later) */

/*
 * CPUID "features" (and "extended features") bits:
 */

#define CPUID_FPU	0x00000001	/* processor has an FPU? */
#define CPUID_VME	0x00000002	/* has virtual mode (%cr4's VME/PVI) */
#define CPUID_DE	0x00000004	/* has debugging extension */
#define CPUID_PSE	0x00000008	/* has 4MB page size extension */
#define CPUID_TSC	0x00000010	/* has time stamp counter */
#define CPUID_MSR	0x00000020	/* has mode specific registers */
#define CPUID_PAE	0x00000040	/* has phys address extension */
#define CPUID_MCE	0x00000080	/* has machine check exception */
#define CPUID_CX8	0x00000100	/* has CMPXCHG8B instruction */
#define CPUID_APIC	0x00000200	/* has enabled APIC */
#define CPUID_SYS1	0x00000400	/* has SYSCALL/SYSRET inst. (Cyrix) */
#define CPUID_SEP	0x00000800	/* has SYSCALL/SYSRET inst. (AMD/Intel) */
#define CPUID_MTRR	0x00001000	/* has memory type range register */
#define CPUID_PGE	0x00002000	/* has page global extension */
#define CPUID_MCA	0x00004000	/* has machine check architecture */
#define CPUID_CMOV	0x00008000	/* has CMOVcc instruction */
#define CPUID_PAT	0x00010000	/* has page attribute table */
#define CPUID_PSE36	0x00020000	/* has 36bit page size extension */
#define CPUID_SER	0x00040000	/* has processor serial number */
#define CPUID_CFLUSH	0x00080000	/* CFLUSH insn supported */
#define CPUID_B20	0x00100000	/* reserved */
#define CPUID_DS	0x00200000	/* Debug Store */
#define CPUID_ACPI	0x00400000	/* ACPI performance modulation regs */
#define CPUID_MMX	0x00800000	/* has MMX instructions */
#define CPUID_FXSR	0x01000000	/* has FXRSTOR instruction (Intel) */
#define CPUID_EMMX	0x01000000	/* has extended MMX (Cyrix; obsolete) */
#define CPUID_SSE	0x02000000	/* has SSE instructions */
#define CPUID_SSE2	0x04000000	/* has SSE2 instructions  */
#define CPUID_SS	0x08000000	/* self-snoop */
#define CPUID_HTT	0x10000000	/* hyper-threading tech */
#define CPUID_TM	0x20000000	/* thermal monitor (TCC) */
#define CPUID_B30	0x40000000	/* reserved */
#define CPUID_SBF	0x80000000	/* signal break on FERR */

/*
 * Note: The 3DNOW flag does not really belong in this feature set since it is
 * returned by the cpuid instruction when called with 0x80000001 in eax rather
 * than 0x00000001, but cyrix3_cpu_setup() moves it to a reserved bit of the
 * feature set for simplicity
 */
#define CPUID_3DNOW	0x40000000	/* has 3DNow! instructions (AMD) */

#define CPUIDECX_PNI	0x00000001	/* Prescott New Instructions */
#define CPUIDECX_MWAIT	0x00000008	/* Monitor/Mwait */
#define CPUIDECX_EST	0x00000080	/* enhanced SpeedStep */
#define CPUIDECX_TM2	0x00000100	/* thermal monitor 2 */
#define CPUIDECX_CNXTID	0x00000400	/* Context ID */

/*
 * Model-specific registers for the i386 family
 */
#define MSR_P5_MC_ADDR		0x000
#define MSR_P5_MC_TYPE		0x001
#define MSR_TSC			0x010
#define	P5MSR_CTRSEL		0x011	/* P5 only (trap on P6) */
#define	P5MSR_CTR0		0x012	/* P5 only (trap on P6) */
#define	P5MSR_CTR1		0x013	/* P5 only (trap on P6) */
#define MSR_APICBASE		0x01b
#define MSR_EBL_CR_POWERON	0x02a
#define MSR_EBC_FREQUENCY_ID	0x02c	/* Pentium 4 only */
#define	MSR_TEST_CTL		0x033
#define MSR_BIOS_UPDT_TRIG	0x079
#define	MSR_BBL_CR_D0		0x088	/* PII+ only */
#define	MSR_BBL_CR_D1		0x089	/* PII+ only */
#define	MSR_BBL_CR_D2		0x08a	/* PII+ only */
#define MSR_BIOS_SIGN		0x08b
#define P6MSR_CTR0		0x0c1
#define P6MSR_CTR1		0x0c2
#define MSR_MTRRcap		0x0fe
#define	MSR_BBL_CR_ADDR		0x116	/* PII+ only */
#define	MSR_BBL_CR_DECC		0x118	/* PII+ only */
#define	MSR_BBL_CR_CTL		0x119	/* PII+ only */
#define	MSR_BBL_CR_TRIG		0x11a	/* PII+ only */
#define	MSR_BBL_CR_BUSY		0x11b	/* PII+ only */
#define	MSR_BBL_CR_CTR3		0x11e	/* PII+ only */
#define MSR_SYSENTER_CS		0x174
#define MSR_SYSENTER_ESP	0x175
#define MSR_SYSENTER_EIP	0x176
#define MSR_MCG_CAP		0x179
#define MSR_MCG_STATUS		0x17a
#define MSR_MCG_CTL		0x17b
#define P6MSR_CTRSEL0		0x186
#define P6MSR_CTRSEL1		0x187
#define MSR_PERF_STATUS		0x198	/* Pentium M */
#define MSR_PERF_CTL		0x199	/* Pentium M */
#define MSR_THERM_CONTROL	0x19a
#define MSR_THERM_INTERRUPT	0x19b
#define MSR_THERM_STATUS	0x19c
#define MSR_THERM2_CTL		0x19d	/* Pentium M */
#define MSR_MISC_ENABLE		0x1a0
#define MSR_DEBUGCTLMSR		0x1d9
#define MSR_LASTBRANCHFROMIP	0x1db
#define MSR_LASTBRANCHTOIP	0x1dc
#define MSR_LASTINTFROMIP	0x1dd
#define MSR_LASTINTTOIP		0x1de
#define MSR_ROB_CR_BKUPTMPDR6	0x1e0
#define MSR_MTRRVarBase		0x200
#define	MSR_MTRRphysMask0	0x201
#define	MSR_MTRRphysBase1	0x202
#define	MSR_MTRRphysMask1	0x203
#define	MSR_MTRRphysBase2	0x204
#define	MSR_MTRRphysMask2	0x205
#define	MSR_MTRRphysBase3	0x206
#define	MSR_MTRRphysMask3	0x207
#define	MSR_MTRRphysBase4	0x208
#define	MSR_MTRRphysMask4	0x209
#define	MSR_MTRRphysBase5	0x20a
#define	MSR_MTRRphysMask5	0x20b
#define	MSR_MTRRphysBase6	0x20c
#define	MSR_MTRRphysMask6	0x20d
#define	MSR_MTRRphysBase7	0x20e
#define	MSR_MTRRphysMask7	0x20f
#define MSR_MTRR64kBase		0x250
#define MSR_MTRR16kBase		0x258
#define	MSR_MTRRfix16K_A0000	0x259
#define MSR_MTRR4kBase		0x268
#define	MSR_MTRRfix4K_C8000	0x269
#define	MSR_MTRRfix4K_D0000	0x26a
#define	MSR_MTRRfix4K_D8000	0x26b
#define	MSR_MTRRfix4K_E0000	0x26c
#define	MSR_MTRRfix4K_E8000	0x26d
#define	MSR_MTRRfix4K_F0000	0x26e
#define	MSR_MTRRfix4K_F8000	0x26f
#define MSR_MTRRdefType		0x2ff
#define MSR_MC0_CTL		0x400
#define MSR_MC0_STATUS		0x401
#define MSR_MC0_ADDR		0x402
#define MSR_MC0_MISC		0x403
#define MSR_MC1_CTL		0x404
#define MSR_MC1_STATUS		0x405
#define MSR_MC1_ADDR		0x406
#define MSR_MC1_MISC		0x407
#define MSR_MC2_CTL		0x408
#define MSR_MC2_STATUS		0x409
#define MSR_MC2_ADDR		0x40a
#define MSR_MC2_MISC		0x40b
#define MSR_MC4_CTL		0x40c
#define MSR_MC4_STATUS		0x40d
#define MSR_MC4_ADDR		0x40e
#define MSR_MC4_MISC		0x40f
#define MSR_MC3_CTL		0x410
#define MSR_MC3_STATUS		0x411
#define MSR_MC3_ADDR		0x412
#define MSR_MC3_MISC		0x413

/*
 * Constants related to MTRRs
 */
#define MTRR_N64K		8	/* numbers of fixed-size entries */
#define MTRR_N16K		16
#define MTRR_N4K		64

/*
 * the following four 3-byte registers control the non-cacheable regions.
 * These registers must be written as three separate bytes.
 *
 * NCRx+0: A31-A24 of starting address
 * NCRx+1: A23-A16 of starting address
 * NCRx+2: A15-A12 of starting address | NCR_SIZE_xx.
 * 
 * The non-cacheable region's starting address must be aligned to the
 * size indicated by the NCR_SIZE_xx field.
 */
#define NCR1	0xc4
#define NCR2	0xc7
#define NCR3	0xca
#define NCR4	0xcd

#define NCR_SIZE_0K	0
#define NCR_SIZE_4K	1
#define NCR_SIZE_8K	2
#define NCR_SIZE_16K	3
#define NCR_SIZE_32K	4
#define NCR_SIZE_64K	5
#define NCR_SIZE_128K	6
#define NCR_SIZE_256K	7
#define NCR_SIZE_512K	8
#define NCR_SIZE_1M	9
#define NCR_SIZE_2M	10
#define NCR_SIZE_4M	11
#define NCR_SIZE_8M	12
#define NCR_SIZE_16M	13
#define NCR_SIZE_32M	14
#define NCR_SIZE_4G	15

/*
 * Performance monitor events.
 *
 * Note that 586-class and 686-class CPUs have different performance
 * monitors available, and they are accessed differently:
 *
 *	686-class: `rdpmc' instruction
 *	586-class: `rdmsr' instruction, CESR MSR
 *
 * The descriptions of these events are too lenghy to include here.
 * See Appendix A of "Intel Architecture Software Developer's
 * Manual, Volume 3: System Programming" for more information.
 */

/*
 * 586-class CESR MSR format.  Lower 16 bits is CTR0, upper 16 bits
 * is CTR1.
 */

#define	PMC5_CESR_EVENT			0x003f
#define	PMC5_CESR_OS			0x0040
#define	PMC5_CESR_USR			0x0080
#define	PMC5_CESR_E			0x0100
#define	PMC5_CESR_P			0x0200

/*
 * 686-class Event Selector MSR format.
 */

#define	PMC6_EVTSEL_EVENT		0x000000ff
#define	PMC6_EVTSEL_UNIT		0x0000ff00
#define	PMC6_EVTSEL_UNIT_SHIFT		8
#define	PMC6_EVTSEL_USR			(1 << 16)
#define	PMC6_EVTSEL_OS			(1 << 17)
#define	PMC6_EVTSEL_E			(1 << 18)
#define	PMC6_EVTSEL_PC			(1 << 19)
#define	PMC6_EVTSEL_INT			(1 << 20)
#define	PMC6_EVTSEL_EN			(1 << 22)	/* PerfEvtSel0 only */
#define	PMC6_EVTSEL_INV			(1 << 23)
#define	PMC6_EVTSEL_COUNTER_MASK	0xff000000
#define	PMC6_EVTSEL_COUNTER_MASK_SHIFT	24

/* Data Cache Unit */
#define	PMC6_DATA_MEM_REFS		0x43
#define	PMC6_DCU_LINES_IN		0x45
#define	PMC6_DCU_M_LINES_IN		0x46
#define	PMC6_DCU_M_LINES_OUT		0x47
#define	PMC6_DCU_MISS_OUTSTANDING	0x48

/* Instruction Fetch Unit */
#define	PMC6_IFU_IFETCH			0x80
#define	PMC6_IFU_IFETCH_MISS		0x81
#define	PMC6_ITLB_MISS			0x85
#define	PMC6_IFU_MEM_STALL		0x86
#define	PMC6_ILD_STALL			0x87

/* L2 Cache */
#define	PMC6_L2_IFETCH			0x28
#define	PMC6_L2_LD			0x29
#define	PMC6_L2_ST			0x2a
#define	PMC6_L2_LINES_IN		0x24
#define	PMC6_L2_LINES_OUT		0x26
#define	PMC6_L2_M_LINES_INM		0x25
#define	PMC6_L2_M_LINES_OUTM		0x27
#define	PMC6_L2_RQSTS			0x2e
#define	PMC6_L2_ADS			0x21
#define	PMC6_L2_DBUS_BUSY		0x22
#define	PMC6_L2_DBUS_BUSY_RD		0x23

/* External Bus Logic */
#define	PMC6_BUS_DRDY_CLOCKS		0x62
#define	PMC6_BUS_LOCK_CLOCKS		0x63
#define	PMC6_BUS_REQ_OUTSTANDING	0x60
#define	PMC6_BUS_TRAN_BRD		0x65
#define	PMC6_BUS_TRAN_RFO		0x66
#define	PMC6_BUS_TRANS_WB		0x67
#define	PMC6_BUS_TRAN_IFETCH		0x68
#define	PMC6_BUS_TRAN_INVAL		0x69
#define	PMC6_BUS_TRAN_PWR		0x6a
#define	PMC6_BUS_TRANS_P		0x6b
#define	PMC6_BUS_TRANS_IO		0x6c
#define	PMC6_BUS_TRAN_DEF		0x6d
#define	PMC6_BUS_TRAN_BURST		0x6e
#define	PMC6_BUS_TRAN_ANY		0x70
#define	PMC6_BUS_TRAN_MEM		0x6f
#define	PMC6_BUS_DATA_RCV		0x64
#define	PMC6_BUS_BNR_DRV		0x61
#define	PMC6_BUS_HIT_DRV		0x7a
#define	PMC6_BUS_HITM_DRDV		0x7b
#define	PMC6_BUS_SNOOP_STALL		0x7e

/* Floating Point Unit */
#define	PMC6_FLOPS			0xc1
#define	PMC6_FP_COMP_OPS_EXE		0x10
#define	PMC6_FP_ASSIST			0x11
#define	PMC6_MUL			0x12
#define	PMC6_DIV			0x12
#define	PMC6_CYCLES_DIV_BUSY		0x14

/* Memory Ordering */
#define	PMC6_LD_BLOCKS			0x03
#define	PMC6_SB_DRAINS			0x04
#define	PMC6_MISALIGN_MEM_REF		0x05
#define	PMC6_EMON_KNI_PREF_DISPATCHED	0x07	/* P-III only */
#define	PMC6_EMON_KNI_PREF_MISS		0x4b	/* P-III only */

/* Instruction Decoding and Retirement */
#define	PMC6_INST_RETIRED		0xc0
#define	PMC6_UOPS_RETIRED		0xc2
#define	PMC6_INST_DECODED		0xd0
#define	PMC6_EMON_KNI_INST_RETIRED	0xd8
#define	PMC6_EMON_KNI_COMP_INST_RET	0xd9

/* Interrupts */
#define	PMC6_HW_INT_RX			0xc8
#define	PMC6_CYCLES_INT_MASKED		0xc6
#define	PMC6_CYCLES_INT_PENDING_AND_MASKED 0xc7

/* Branches */
#define	PMC6_BR_INST_RETIRED		0xc4
#define	PMC6_BR_MISS_PRED_RETIRED	0xc5
#define	PMC6_BR_TAKEN_RETIRED		0xc9
#define	PMC6_BR_MISS_PRED_TAKEN_RET	0xca
#define	PMC6_BR_INST_DECODED		0xe0
#define	PMC6_BTB_MISSES			0xe2
#define	PMC6_BR_BOGUS			0xe4
#define	PMC6_BACLEARS			0xe6

/* Stalls */
#define	PMC6_RESOURCE_STALLS		0xa2
#define	PMC6_PARTIAL_RAT_STALLS		0xd2

/* Segment Register Loads */
#define	PMC6_SEGMENT_REG_LOADS		0x06

/* Clocks */
#define	PMC6_CPU_CLK_UNHALTED		0x79

/* MMX Unit */
#define	PMC6_MMX_INSTR_EXEC		0xb0	/* Celeron, P-II, P-IIX only */
#define	PMC6_MMX_SAT_INSTR_EXEC		0xb1	/* P-II and P-III only */
#define	PMC6_MMX_UOPS_EXEC		0xb2	/* P-II and P-III only */
#define	PMC6_MMX_INSTR_TYPE_EXEC	0xb3	/* P-II and P-III only */
#define	PMC6_FP_MMX_TRANS		0xcc	/* P-II and P-III only */
#define	PMC6_MMX_ASSIST			0xcd	/* P-II and P-III only */
#define	PMC6_MMX_INSTR_RET		0xc3	/* P-II only */

/* Segment Register Renaming */
#define	PMC6_SEG_RENAME_STALLS		0xd4	/* P-II and P-III only */
#define	PMC6_SEG_REG_RENAMES		0xd5	/* P-II and P-III only */
#define	PMC6_RET_SEG_RENAMES		0xd6	/* P-II and P-III only */

/* VIA C3 xcrypt-* instruction context control options */
#define	C3_CRYPT_CWLO_ROUND_M		0x0000000f
#define	C3_CRYPT_CWLO_ALG_M		0x00000070
#define	C3_CRYPT_CWLO_ALG_AES		0x00000000
#define	C3_CRYPT_CWLO_KEYGEN_M		0x00000080
#define	C3_CRYPT_CWLO_KEYGEN_HW		0x00000000
#define	C3_CRYPT_CWLO_KEYGEN_SW		0x00000080
#define	C3_CRYPT_CWLO_NORMAL		0x00000000
#define	C3_CRYPT_CWLO_INTERMEDIATE	0x00000100
#define	C3_CRYPT_CWLO_ENCRYPT		0x00000000
#define	C3_CRYPT_CWLO_DECRYPT		0x00000200
#define	C3_CRYPT_CWLO_KEY128		0x0000000a	/* 128bit, 10 rds */
#define	C3_CRYPT_CWLO_KEY192		0x0000040c	/* 192bit, 12 rds */
#define	C3_CRYPT_CWLO_KEY256		0x0000080e	/* 256bit, 15 rds */

/* VIA C3 xcrypt-* opcodes */
#define	VIAC3_CRYPTOP_RNG	0xc0		/* rng */
#define	VIAC3_CRYPTOP_ECB	0xc8		/* aes-ecb */
#define	VIAC3_CRYPTOP_CBC	0xd0		/* aes-cbc */
#define	VIAC3_CRYPTOP_CFB	0xe0		/* aes-cfb */
#define	VIAC3_CRYPTOP_OFB	0xe8		/* aes-ofb */
