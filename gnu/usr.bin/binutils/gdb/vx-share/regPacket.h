/*  regPacket.h - register packet definitions for rdb */

/* Copyright 1992-1993 Wind River Systems, Inc. */

/*
modification history
--------------------
01d,30nov93,pad  Added Am29K target definitions.
01c,14jun93,maf  additional definitions for documentation purposes.
		 fixed reversal of MIPS_R_LO and MIPS_R_HI.
01b,08feb93,scy  added SPARC target definitions. changed to WRS code convetion.
01a,20feb92,j_w  created.
*/

#ifndef __INCregPacketh
#define __INCregPacketh


/* MC68K */

#define MC68K_GREG_SIZE		0x04	/* size of general-purpose reg */
#define MC68K_GREG_PLEN		0x48	/* size of general-purpose reg block */

/* offsets into general-purpose register block */

#define MC68K_R_D0		0x00	/* d0; d1 - d7 follow in sequence */
#define MC68K_R_A0		0x20	/* a0; a1 - a7 follow in sequence */
#define MC68K_R_SR		0x40	/* sr (represented as a 4-byte val) */
#define MC68K_R_PC		0x44	/* pc */

#define MC68K_FPREG_SIZE	0x0c	/* size of floating-point data reg */
#define MC68K_FPREG_PLEN  	0x6c	/* size of floating-point reg block */

/* offsets into floating-point register block */

#define MC68K_R_FP0		0x00	/* fp0; fp1 - fp7 follow in sequence */
#define MC68K_R_FPCR		0x60	/* fpcr */
#define MC68K_R_FPSR		0x64	/* fpsr */
#define MC68K_R_FPIAR		0x68	/* fpiar */


/* I960 */

#define I960_GREG_SIZE		0x04	/* size of general-purpose reg */
#define I960_GREG_PLEN		0x8c	/* size of general-purpose reg block */

/* offsets into general-purpose register block */

#define I960_R_R0		0x00	/* r0; r1 - r15 follow in sequence */
#define I960_R_G0		0x40	/* g0; g1 - g15 follow in sequence */
#define I960_R_PCW		0x80	/* pcw */
#define I960_R_ACW		0x84	/* acw */
#define I960_R_TCW		0x88	/* tcw */

#define I960_FPREG_SIZE		0x10	/* size of floating-point reg */
#define I960_FPREG_PLEN		0x28	/* size of floating-point reg block */

/* offsets  into floating-point register block */

#define I960_R_FP0		0x00	/* fp0; fp1 - fp3 follow in sequence */


/* SPARC */

#define SPARC_GREG_SIZE 	0x04	/* size of general-purpose reg */
#define SPARC_GREG_PLEN 	0x98	/* size of general-purpose reg block */

/* offsets into general-purpose register block */

#define SPARC_R_G0		0x00	/* g0; g1 - g7 follow in sequence */
#define SPARC_R_O0		0x20	/* o0; o1 - o7 follow in sequence */
#define SPARC_R_L0		0x40	/* l0; l1 - l7 follow in sequence */
#define SPARC_R_I0		0x60	/* i0; i1 - i7 follow in sequence */
#define SPARC_R_Y		0x80	/* y */
#define SPARC_R_PSR		0x84	/* psr */
#define SPARC_R_WIM		0x88	/* wim */
#define SPARC_R_TBR		0x8c	/* tbr */
#define SPARC_R_PC		0x90	/* pc */
#define SPARC_R_NPC		0x94	/* npc */

#define SPARC_FPREG_SIZE 	0x04	/* size of floating-point reg */
#define SPARC_FPREG_PLEN 	0x84	/* size of floating-point reg block */

/* offsets into floating-point register block */

#define SPARC_R_FP0		0x00	/* f0; f1 - f31 follow in sequence */
#define SPARC_R_FSR		0x80	/* fsr */


/* MIPS */

#define MIPS_GREG_SIZE		0x04	/* size of general-purpose reg */
#define MIPS_GREG_PLEN		0x90	/* size of general-purpose reg block */

/* offsets into general-purpose register block */

#define MIPS_R_GP0		0x00	/* gp0 (zero) */
#define MIPS_R_AT		0x04	/* at */
#define MIPS_R_V0		0x08	/* v0 */
#define MIPS_R_V1		0x0c	/* v1 */
#define MIPS_R_A0		0x10	/* a0 */
#define MIPS_R_A1		0x14	/* a1 */
#define MIPS_R_A2		0x18	/* a2 */
#define MIPS_R_A3		0x1c	/* a3 */
#define MIPS_R_T0		0x20	/* t0 */
#define MIPS_R_T1		0x24	/* t1 */
#define MIPS_R_T2		0x28	/* t2 */
#define MIPS_R_T3		0x2c	/* t3 */
#define MIPS_R_T4		0x30	/* t4 */
#define MIPS_R_T5		0x34	/* t5 */
#define MIPS_R_T6		0x38	/* t6 */
#define MIPS_R_T7		0x3c	/* t7 */
#define MIPS_R_S0		0x40	/* s0 */
#define MIPS_R_S1		0x44	/* s1 */
#define MIPS_R_S2		0x48	/* s2 */
#define MIPS_R_S3		0x4c	/* s3 */
#define MIPS_R_S4		0x50	/* s4 */
#define MIPS_R_S5		0x54	/* s5 */
#define MIPS_R_S6		0x58	/* s6 */
#define MIPS_R_S7		0x5c	/* s7 */
#define MIPS_R_T8		0x60	/* t8 */
#define MIPS_R_T9		0x64	/* t9 */
#define MIPS_R_K0		0x68	/* k0 */
#define MIPS_R_K1		0x6c	/* k1 */
#define MIPS_R_GP		0x70	/* gp */
#define MIPS_R_SP		0x74	/* sp */
#define MIPS_R_S8		0x78	/* s8 */
#define MIPS_R_LO		0x80	/* lo */
#define MIPS_R_HI		0x84	/* hi */
#define MIPS_R_SR		0x88	/* sr */
#define MIPS_R_PC		0x8c	/* pc */
	
#define MIPS_FPREG_SIZE		0x04	/* size of floating-point data reg */
#define MIPS_FPREG_PLEN		0x84	/* size of floating-point reg block */

/* offsets into floating-point register block */

#define MIPS_R_FP0		0x00	/* f0; f1 - f31 follow in sequence */
#define MIPS_R_FPCSR		0x80 	/* offset of fpcsr in reg block */


/* General registers for the Am29k */

#define AM29K_GREG_SIZE         0x04
#define AM29K_GREG_PLEN         0x2d4

#define AM29K_R_GR96            0x0
#define AM29K_R_VAB             0x280
#define AM29K_R_INTE            0x2bc
#define AM29K_R_RSP             0x2c0

/* Floating Point registers for the Am29k */

#define AM29K_FPREG_SIZE        0x04
#define AM29K_FPREG_PLEN        0x8

#define AM29K_R_FPE             0x0
#define AM29K_R_FPS             0x4

#endif /* __INCregPacketh */
