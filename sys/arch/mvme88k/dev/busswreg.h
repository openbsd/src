/*	$OpenBSD: busswreg.h,v 1.5 2004/04/14 20:17:21 miod Exp $ */

/*
 * Memory map for BusSwitch chip found in mvme197 boards.
 */
#ifndef BUSSWREG_H
#define BUSSWREG_H
#define BS_BASE	0xFFF00000

struct bussw_reg {
	volatile u_long		bs_gcsr;
	volatile u_short	bs_iodata;
	volatile u_short	bs_iodir;
	volatile u_short	bs_psar1;
	volatile u_short	bs_pear1;
	volatile u_short	bs_psar2;
	volatile u_short	bs_pear2;
	volatile u_short	bs_psar3;
	volatile u_short	bs_pear3;
	volatile u_short	bs_psar4;
	volatile u_short	bs_pear4;
	volatile u_short	bs_ptr1;
	volatile u_short	bs_ptsr1;
	volatile u_short	bs_ptr2;
	volatile u_short	bs_ptsr2;
	volatile u_short	bs_ptr3;
	volatile u_short	bs_ptsr3;
	volatile u_short	bs_ptr4;
	volatile u_short	bs_ptsr4;
	volatile u_short	bs_ssar1;
	volatile u_short	bs_sear1;
	volatile u_short	bs_ssar2;
	volatile u_short	bs_sear2;
	volatile u_short	bs_ssar3;
	volatile u_short	bs_sear3;
	volatile u_short	bs_ssar4;
	volatile u_short	bs_sear4;
	volatile u_short	bs_str1;
	volatile u_short	bs_stsr1;
	volatile u_short	bs_str2;
	volatile u_short	bs_stsr2;
	volatile u_short	bs_str3;
	volatile u_short	bs_stsr3;
	volatile u_short	bs_str4;
	volatile u_short	bs_stsr4;
	volatile u_long		bs_par;
	volatile u_long		bs_sar;
	volatile u_long		bs_btimer;
	volatile u_long		bs_pal;
	volatile u_long		bs_wppa;
	volatile u_long		bs_wp;
	volatile u_long		bs_romcr;
	volatile u_long		bs_lmi;
	volatile u_long		bs_intr1;
	volatile u_long		bs_intr2;
	volatile u_long		bs_tcomp1;
	volatile u_long		bs_tcount1;
	volatile u_long		bs_tcomp2;
	volatile u_long		bs_tcount2;
	volatile u_long		bs_gpr1;
	volatile u_long		bs_gpr2;
	volatile u_long		bs_gpr3;
	volatile u_long		bs_gpr4;
	volatile u_long		bs_xctags;
	volatile u_int8_t	bs_res[(0x100 - 0x94)];
	volatile u_long		bs_xccr;
	volatile u_long		bs_vec1;
	volatile u_long		bs_vec2;
	volatile u_long		bs_vec3;
	volatile u_long		bs_vec4;
	volatile u_long		bs_vec5;
	volatile u_long		bs_vec6;
	volatile u_long		bs_vec7;
};

/* GCSR bit definitions */
#define BS_CHIPID(x)	(((x)->bs_gcsr & 0xFF000000) >> 16)
#define BS_CHIPREV(x)	(((x)->bs_gcsr & 0x00FF0000) >> 16)
#define BS_GCSR_APRI0	0x00000001	/* Bus Request 0 Priority indicator (CPU0)*/
#define BS_GCSR_APRI1	0x00000002	/* Bus Request 1 Priority indicator (CPU1)*/
#define BS_GCSR_APRI2	0x00000003	/* Bus Request 2 Priority indicator (mc88410)*/
#define BS_GCSR_AMOD	0x00000004	/* Arbitration Mode */
#define BS_GCSR_BREN	0x00000008	/* Bus Request Enable */
#define BS_GCSR_CPUID	0x00000010	/* CPU ID */
#define BS_GCSR_B410	0x00000020	/* BUS410 indicator */
#define BS_GCSR_INVD	0x00000040	/* Invalidate Decoder */
#define BS_GCSR_USR	0x00000080	/* User Access Enable */
#define BS_GCSR_XIPL	0x00000100	/* External IPL Enable */
#define BS_GCSR_TCPU1	0x00000200	/* Test CPU 1 Registers */
#define BS_GCSR_XCC	0x00000400	/* External Cache Controller */
#define BS_GCSR_INCB	0x00000800	/* Increment On Burst */
#define BS_GCSR_TDPR	0x00002000	/* Test Dual Processor Registers */
#define BS_GCSR_TBB	0x00004000	/* Test Bus Busy */
#define BS_GCSR_POR	0x00008000	/* Power On Reset */

/* System Attribute Registers bit definitions */
#define BS_SAR_DEN	0x01	/* Decode Enable */
#define BS_SAR_INVR	0x04	/* Invalidate On Reads */
#define BS_SAR_GBL	0x08	/* Global Access */

/* Bus Timer Register bit definitions */
#define BS_BTIMER_PBT8		0x00	/* Processor Bus Timout, 8 usec */
#define BS_BTIMER_PBT64		0x01	/* Processor Bus Timout, 64 usec */
#define BS_BTIMER_PBT256	0x02	/* Processor Bus Timout, 256 usec */
#define BS_BTIMER_PBTD		0x03	/* Processor Bus Timout, disable */
#define BS_BTIMER_SBT8		(0x00 << 2)	/* System Bus Timout, 8 usec */
#define BS_BTIMER_SBT64		(0x01 << 2)	/* System Bus Timout, 64 usec */
#define BS_BTIMER_SBT256	(0x02 << 2)	/* System Bus Timout, 256 usec */
#define BS_BTIMER_SBTD		(0x03 << 2)	/* System Bus Timout, disable */

/* Prescaler Adjust values */
#define BS_PADJUST_50	0xCE	/* 50 MHz clock */
#define BS_PADJUST_40	0xD8	/* 40 MHz clock */
#define BS_PADJUST_33	0xDF	/* 33 MHz clock */
#define BS_PADJUST_25	0xE7	/* 25 MHz clock */

/* ROM Control Register bit definitions */
#define BS_ROMCR_WEN0	0x01000000
#define BS_ROMCR_WEN1   0x02000000
#define BS_ROMCR_SGLB	0x04000000
#define BS_ROMCR_ROM0	0x80000000

/* External Cache Control Register bit definitions */
#define BS_XCC_F0	0x00000001
#define BS_XCC_F1	0x00000002
#define BS_XCC_FBSY	0x00000004
#define BS_XCC_DIAG	0x00000008

/*
 * INTR1 - Abort Control Register
 * Cross Processor Interrupt Register
 * Timer Interrupt 1 Register
 * Timer Interrupt 2 Register
 */
#define BS_INTR1_ABORT_ICLR	0x08000000	/* abort interrupt clear */
#define BS_INTR1_ABORT_IEN	0x10000000	/* abort interrupt enable */
#define BS_INTR1_ABORT_INT	0x20000000	/* abort interrupt received */
#define BS_INTR1_ABORT_ABT	0x40000000	/* abort interrupt asserted */

#define BS_INTR1_CPI_ICLR	0x00080000	/* cpi interrupt clear */
#define BS_INTR1_CPI_IEN	0x00100000	/* cpi interrupt enable */
#define BS_INTR1_CPI_INT	0x00200000	/* cpi interrupt received */
#define BS_INTR1_CPI_STAT	0x00400000	/* cpi interrupt status */
#define BS_INTR1_CPI_SCPI	0x00800000	/* send cross proc interrupt */

#define BS_INTR1_TINT1_ICLR	0x00000800	/* timer 1 interrupt clear */
#define BS_INTR1_TINT1_IEN	0x00001000	/* timer 1 interrupt enable */
#define BS_INTR1_TINT1_INT	0x00002000	/* timer 1 interrupt received */
#define BS_INTR1_TINT1_LM	0x00000700	/* timer 1 level mask */
#define BS_INTR1_TINT1_LEVEL(x)	((x << 8) & BS_INTR1_TINT1_LM)

#define BS_INTR1_TINT2_ICLR	0x00000008	/* timer 1 interrupt clear */
#define BS_INTR1_TINT2_IEN	0x00000010	/* timer 1 interrupt enable */
#define BS_INTR1_TINT2_INT	0x00000020	/* timer 1 interrupt received */
#define BS_INTR1_TINT2_LM	0x00000007	/* timer 1 level mask */
#define BS_INTR1_TINT2_LEVEL(x)	(x & BS_INTR1_TINT2_LM)

/* Vector Base Register (A read upon an interrupt reveals the source) */
#define BS_VBASE_SRC_TMR1	0x0
#define BS_VBASE_SRC_TMR2	0x1
#define BS_VBASE_SRC_WPE	0x2
#define BS_VBASE_SRC_PAL	0x3
#define BS_VBASE_SRC_EXT	0x4	/* external interrupt */
#define BS_VBASE_SRC_SPUR	0x7	/* spurious interrupt */

/*
 * INTR2 - Write Post Control Register
 * Processor Address Log Interrupt Register
 * External Interrupt Register
 * Vector Base
 */
#define BS_INTR2_WPINT_ICLR	0x08000000	/* WPINT interrupt clear */
#define BS_INTR2_WPINT_IEN	0x10000000	/* WPINT interrupt enable */
#define BS_INTR2_WPINT_INT	0x20000000	/* WPINT interrupt received */
#define BS_INTR2_WPINT_LM	0x07000000	/* WPINT level mask */
#define BS_INTR2_WPINT_LEVEL(x)	((x << 24) & BS_INTR2_WPINT_LM)

#define BS_INTR2_PALINT_ICLR	0x00080000	/* PALINT interrupt clear */
#define BS_INTR2_PALINT_IEN	0x00100000	/* PALINT interrupt enable */
#define BS_INTR2_PALINT_INT	0x00200000	/* PALINT interrupt received */
#define BS_INTR2_PALINT_PLTY	0x00800000	/* PALINT polarity */
#define BS_INTR2_PALINT_LM	0x00070000	/* PALINT level mask */
#define BS_INTR2_PALINT_LEVEL(x)	((x << 16) & BS_INTR2_PALINT_LM)

#define BS_INTR2_XINT_ICLR	0x00000800	/* XINT interrupt clear */
#define BS_INTR2_XINT_IEN	0x00001000	/* XINT interrupt enable */
#define BS_INTR2_XINT_INT	0x00002000	/* XINT interrupt received */
#define BS_INTR2_XINT_EL	0x00004000	/* XINT edge/level */
#define BS_INTR2_XINT_PLTY	0x00008000	/* XINT polarity */
#define BS_INTR2_XINT_LM	0x00000700	/* XINT level mask */
#define BS_INTR2_XINT_LEVEL(x)	((x << 24) & BS_INTR2_XINT_LM)

/* We lock off BusSwitch vectors at 0x40 */
#define BS_VECBASE	0x40
#define BS_NVEC		16

/* Bottom 4 bits of the vector returned during IACK cycle */
#define BS_TMR1IRQ	0x01	/* lowest */
#define BS_TMR2IRQ	0x02
#define BS_ABORTIRQ	0x03

/* Define the Abort vector */
#define BS_ABORTVEC  	(BS_VECBASE | BS_ABORTIRQ)

#endif /* BUSSWREG_H */
