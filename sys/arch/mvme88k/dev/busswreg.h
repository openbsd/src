/*	$OpenBSD: busswreg.h,v 1.13 2009/02/14 17:39:51 miod Exp $ */

/*
 * Memory map for BusSwitch chip found in mvme197 boards.
 */
#ifndef	BUSSWREG_H
#define	BUSSWREG_H

#define BS_BASE		0xfff00000
#define	BS_SIZE		0x00000120

#define	BS_CHIPID	0x0000
#define	BS_CHIPREV	0x0001
#define	BS_GCSR		0x0002
#define	BS_IODATA	0x0004
#define	BS_IODIR	0x0006
#define	BS_PSAR1	0x0008
#define	BS_PEAR1	0x000a
#define	BS_PSAR2	0x000c
#define	BS_PEAR2	0x000e
#define	BS_PSAR3	0x0010
#define	BS_PEAR3	0x0012
#define	BS_PSAR4	0x0014
#define	BS_PEAR4	0x0016
#define	BS_PTR1		0x0018
#define	BS_PTSR1	0x001a
#define	BS_PTR2		0x001c
#define	BS_PTSR2	0x001e
#define	BS_PTR3		0x0020
#define	BS_PTSR3	0x0022
#define	BS_PTR4		0x0024
#define	BS_PTSR4	0x0026
#define	BS_SSAR1	0x0028
#define	BS_SEAR1	0x002a
#define	BS_SSAR2	0x002c
#define	BS_SEAR2	0x002e
#define	BS_SSAR3	0x0030
#define	BS_SEAR3	0x0032
#define	BS_SSAR4	0x0034
#define	BS_SEAR4	0x0036
#define	BS_STR1		0x0038
#define	BS_STSR1	0x003a
#define	BS_STR2		0x003c
#define	BS_STSR2	0x003e
#define	BS_STR3		0x0040
#define	BS_STSR3	0x0042
#define	BS_STR4		0x0044
#define	BS_STSR4	0x0046
#define	BS_PAR		0x0048
#define	BS_SAR		0x004c
#define	BS_BTIMER	0x0051
#define	BS_PADJUST	0x0052
#define	BS_PCOUNT	0x0053
#define	BS_PAL		0x0054
#define	BS_WPPA		0x0058
#define	BS_WPTPA	0x005c
#define	BS_WPPAT	0x005e
#define	BS_ROMCR	0x0060
#define	BS_TCTRL1	0x0062
#define	BS_TCTRL2	0x0063
#define	BS_LEVEL	0x0064
#define	BS_MASK		0x0065
#define	BS_ISEL0	0x0066	/* do not access on 197LE!!! */
#define	BS_ISEL1	0x0067	/* do not access on 197LE!!! */
#define	BS_ABORT	0x0068
#define	BS_CPINT	0x0069
#define	BS_TINT1	0x006a
#define	BS_TINT2	0x006b
#define	BS_WPINT	0x006c
#define	BS_PALINT	0x006d
#define	BS_XINT		0x006e
#define	BS_VBASE	0x006f
#define	BS_TCOMP1	0x0070
#define	BS_TCOUNT1	0x0074
#define	BS_TCOMP2	0x0078
#define	BS_TCOUNT2	0x007c
#define	BS_GPR		0x0080
#define	BS_XCTAGS	0x0090
#define	BS_XCCR		0x0100
#define	BS_VEC		0x0104

#define	BUSSWITCH_ID	0x21	/* value at CHIPID */

/* GCSR bit definitions */
#define BS_GCSR_APRI0	0x0001	/* Bus Request 0 Priority indicator (CPU0)*/
#define BS_GCSR_APRI1	0x0002	/* Bus Request 1 Priority indicator (CPU1)*/
#define BS_GCSR_APRI2	0x0003	/* Bus Request 2 Priority indicator (mc88410)*/
#define BS_GCSR_AMOD	0x0004	/* Arbitration Mode */
#define BS_GCSR_BREN	0x0008	/* Bus Request Enable */
#define BS_GCSR_CPUID	0x0010	/* CPU ID */
#define BS_GCSR_B410	0x0020	/* BUS410 indicator */
#define BS_GCSR_INVD	0x0040	/* Invalidate Decoder */
#define BS_GCSR_USR	0x0080	/* User Access Enable */
#define BS_GCSR_XIPL	0x0100	/* External IPL Enable */
#define BS_GCSR_TCPU1	0x0200	/* Test CPU 1 Registers */
#define BS_GCSR_XCC	0x0400	/* External Cache Controller */
#define BS_GCSR_INCB	0x0800	/* Increment On Burst */
#define BS_GCSR_TDPR	0x2000	/* Test Dual Processor Registers */
#define BS_GCSR_TBB	0x4000	/* Test Bus Busy */
#define BS_GCSR_POR	0x8000	/* Power On Reset */

/* Processor Attribute Registers bit definitions */
#define BS_PAR_DEN	0x01	/* Decode Enable */
#define BS_PAR_WPEN	0x02	/* Write Post Enable */

/* System Attribute Registers bit definitions */
#define BS_SAR_DEN	0x01	/* Decode Enable */
#define BS_SAR_INVR	0x04	/* Invalidate On Reads */
#define BS_SAR_GBL	0x08	/* Global Access */

/* Bus Timer Register bit definitions */
#define BS_BTIMER_PBT8		0x00	/* Processor Bus Timout, 8 usec */
#define BS_BTIMER_PBT64		0x01	/* Processor Bus Timout, 64 usec */
#define BS_BTIMER_PBT256	0x02	/* Processor Bus Timout, 256 usec */
#define BS_BTIMER_PBTD		0x03	/* Processor Bus Timout, disable */
#define	BS_BTIMER_PBT_MASK	0x03
#define BS_BTIMER_SBT8		0x00	/* System Bus Timout, 8 usec */
#define BS_BTIMER_SBT64		0x04	/* System Bus Timout, 64 usec */
#define BS_BTIMER_SBT256	0x08	/* System Bus Timout, 256 usec */
#define BS_BTIMER_SBTD		0x0c	/* System Bus Timout, disable */
#define	BS_BTIMER_SBT_MASK	0x0c

/* ROM Control Register bit definitions */
#define BS_ROMCR_WEN0	0x0100
#define BS_ROMCR_WEN1   0x0200
#define BS_ROMCR_SGLB	0x0400
#define BS_ROMCR_ROM0	0x8000

/* External Cache Control Register bit definitions */
#define BS_XCC_F0	0x00000001
#define BS_XCC_F1	0x00000002
#define BS_XCC_FBSY	0x00000004
#define BS_XCC_DIAG	0x00000008

/* Abort Control Register */
#define BS_ABORT_ICLR	0x08	/* abort interrupt clear */
#define BS_ABORT_IEN	0x10	/* abort interrupt enable */
#define BS_ABORT_INT	0x20	/* abort interrupt received */
#define BS_ABORT_ABT	0x40	/* abort interrupt asserted */

/* Cross Processor Interrupt Register */
#define BS_CPI_ICLR	0x08	/* cpi interrupt clear */
#define BS_CPI_IEN	0x10	/* cpi interrupt enable */
#define BS_CPI_INT	0x20	/* cpi interrupt received */
#define BS_CPI_STAT	0x40	/* cpi interrupt status */
#define BS_CPI_SCPI	0x80	/* send cross proc interrupt */

/* Timer Control Register */
#define BS_TCTRL_CEN	0x01	/* counter enable */
#define BS_TCTRL_COC	0x02	/* clear on compare */
#define BS_TCTRL_COVF	0x04	/* clear overflow counter */
#define BS_TCTRL_OVF(x)		((x) >> 4)	/* overflow counter */

/* Timer Interrupt Register */
#define BS_TINT_ICLR	0x08	/* timer interrupt clear */
#define BS_TINT_IEN	0x10	/* timer interrupt enable */
#define BS_TINT_INT	0x20	/* timer interrupt received */
#define BS_TINT_LM	0x07	/* timer level mask */
#define BS_TINT_LEVEL(x)	(x & BS_TINT_LM)

/* Write Post Control Register */
#define BS_WPINT_ICLR	0x08	/* WPINT interrupt clear */
#define BS_WPINT_IEN	0x10	/* WPINT interrupt enable */
#define BS_WPINT_INT	0x20	/* WPINT interrupt received */
#define BS_WPINT_LM	0x07	/* WPINT level mask */
#define BS_WPINT_LEVEL(x)	(x & BS_WPINT_LM)

/* Processor Address Log Interrupt Register */
#define BS_PALINT_ICLR	0x08	/* PALINT interrupt clear */
#define BS_PALINT_IEN	0x10	/* PALINT interrupt enable */
#define BS_PALINT_INT	0x20	/* PALINT interrupt received */
#define BS_PALINT_PLTY	0x80	/* PALINT polarity */
#define BS_PALINT_LM	0x07	/* PALINT level mask */
#define BS_PALINT_LEVEL(x)	(x & BS_PALINT_LM)

/* External Interrupt Register */
#define BS_XINT_ICLR	0x08	/* XINT interrupt clear */
#define BS_XINT_IEN	0x10	/* XINT interrupt enable */
#define BS_XINT_INT	0x20	/* XINT interrupt received */
#define BS_XINT_EL	0x40	/* XINT edge/level */
#define BS_XINT_PLTY	0x80	/* XINT polarity */
#define BS_XINT_LM	0x07	/* XINT level mask */
#define BS_XINT_LEVEL(x)	(x & BS_XINT_LM)

/* Vector Base Register (A read upon an interrupt reveals the source) */
#define BS_VBASE_SRC_TMR1	0x0
#define BS_VBASE_SRC_TMR2	0x1
#define BS_VBASE_SRC_WPE	0x2
#define BS_VBASE_SRC_PAL	0x3
#define BS_VBASE_SRC_EXT	0x4	/* external interrupt */
#define BS_VBASE_SRC_SPUR	0x7	/* spurious interrupt */

/*
 * BusSwitch wired interrupt vectors
 */

#define BS_VECBASE	0x40		/* vector base */
#define BS_NVEC		0x10

#define BS_TMR1IRQ	0x00		/* timer1 */
#define BS_TMR2IRQ	0x01		/* timer2 */
#define	BS_WPEIRQ	0x02		/* write post error */
#define	BS_PALIRQ	0x03		/* processor address log interrupt */
#define	BS_EXTIRQ	0x04		/* external interrupt */
#define	BS_SPURIRQ	0x07		/* spurious interrupt */

#endif	/* BUSSWREG_H */
