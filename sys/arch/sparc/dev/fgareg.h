/*	$OpenBSD: fgareg.h,v 1.2 2003/06/02 18:40:59 jason Exp $	*/

/*
 * Copyright (c) 1999 Jason L. Wright (jason@thought.net)
 * All rights reserved.
 *
 * This software was developed by Jason L. Wright under contract with
 * RTMX Incorporated (http://www.rtmx.com).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Register definitions for Force Gate Array 5000
 * Definitions from: "Force Gate Array-5000 Technical Reference Manual"
 * revision 1, April 1995, Force Computers, Inc./GmbH.
 */

/*
 * FGA Register structure.
 * The register set takes up 512 bytes and is found in to sbus slot 5's
 * address space (you can change that).
 * Most of the bit registers use a negative logic sense: ie. writing
 * a zero means "setting the bit" and writing a one means "clearing"
 * the bit.
 */
struct fga_regs {
	volatile u_int32_t	sbus_base;	/* sbus base address reg */
	volatile u_int32_t	vme_range[16];	/* master range registers */
	volatile u_int8_t	_unused0[32];
	volatile u_int8_t	vme_master_cap[16];/* master capability */
	volatile u_int8_t	_unused1[8];
	volatile u_int8_t	sbus_ssel[8];	/* sbus slot select */
	volatile u_int8_t	_unused2[15];
	volatile u_int8_t	viack_emu1;	/* vme intr ack emulation */
	volatile u_int8_t	_unused3[1];
	volatile u_int8_t	viack_emu2;
	volatile u_int8_t	_unused4[1];
	volatile u_int8_t	viack_emu3;
	volatile u_int8_t	_unused5[1];
	volatile u_int8_t	viack_emu4;
	volatile u_int8_t	_unused6[1];
	volatile u_int8_t	viack_emu5;
	volatile u_int8_t	_unused7[1];
	volatile u_int8_t	viack_emu6;
	volatile u_int8_t	_unused8[1];
	volatile u_int8_t	viack_emu7;
	volatile u_int8_t	_unused9[16];
	volatile u_int8_t	sbus_cap;	/* sbus capabilities */
	volatile u_int8_t	sbus_retry_ctrl; /* sbus retry control */
	volatile u_int8_t	sbus_rerun_ctrl; /* sbus rerun control */
	volatile u_int8_t	sbus_cap2;	/* sbus capabilities 2 */
	volatile u_int32_t	swpar;		/* sbus write post err addr */
	volatile u_int32_t	slerr;		/* sbus late err addr */
	volatile u_int32_t	_unused10;
	volatile u_int8_t	vme_base;	/* vme slave base addr */
	volatile u_int8_t	vme_ext[3];	/* vme slave addr extension */
	volatile u_int32_t	sbus_range[3];	/* vme->sbus slave addr */
	volatile u_int16_t	ibox_addr;	/* ibox address */
	volatile u_int16_t	ibox_ctrl;	/* ibox control */
	volatile u_int8_t	fmb_ctrl;	/* FMB control reg */
	volatile u_int8_t	fmb_addr;	/* FMB addresss reg */
	volatile u_int8_t	_unused11[10];
	volatile u_int8_t	vme_cap;	/* vme capabilities */
	volatile u_int8_t	vmebus_handshake; /* vme handshake mode */
	volatile u_int8_t	_unused12[2];
	volatile u_int32_t	vwpar;		/* vme write post addr */
	volatile u_int8_t	_unused13[8];
	volatile u_int16_t	dma_ctrl;	/* dma control */
	volatile u_int8_t	dma_mode;	/* dma mode */
	volatile u_int8_t	dma_stat;	/* dma status */
	volatile u_int32_t	dma_src;	/* dma source address */
	volatile u_int32_t	dma_dst;	/* dma destination address */
	volatile u_int32_t	dma_captl;	/* dma capabilities/length */
	volatile u_int8_t	_unused14[32];
	volatile u_int8_t	mbox[16];	/* mailboxes */
	volatile u_int8_t	sem[48];	/* semaphores */
	volatile u_int8_t	_unused15[32];
	volatile u_int8_t	id[4];		/* revision/id register */
	volatile u_int8_t	gcsr;		/* global control/status */
	volatile u_int8_t	_unused16[3];
	volatile u_int8_t	reset_stat;	/* reset status */
	volatile u_int8_t	_unused17[24];
	volatile u_int8_t	virq_map[7];	/* vme->sbus irq mapping */
	volatile u_int8_t	mbox_irq_map[16];
	volatile u_int8_t	acfail_irq_map;
	volatile u_int8_t	sysfail_irq_map[2];
	volatile u_int8_t	abort_irq_map;
	volatile u_int8_t	dma_irq_map;
	volatile u_int8_t	wpe_irq_map;
	volatile u_int8_t	arb_irq_map;
	volatile u_int8_t	wdt_irq_map;
	volatile u_int8_t	slerr_irq_map;
	volatile u_int8_t	fmb_irq_map[2];
	volatile u_int8_t	ibox_irq_map;
	volatile u_int8_t	_unused18[20];
	volatile u_int16_t	mbox_stat;	/* mailbox status */
	volatile u_int8_t	_unused19[2];
	volatile u_int8_t	arb_ctrl;	/* arbitration control */
	volatile u_int8_t	req_ctrl;	/* vme request control */
	volatile u_int8_t	bus_ctrl;	/* vme bus control */
	volatile u_int8_t	_unused20[1];
	volatile u_int8_t	mcsr0;		/* misc control/status */
	volatile u_int8_t	_unused21[3];
	volatile u_int8_t	mcsr1;		/* misc control/status */
	volatile u_int8_t	wdt_restart;	/* watchdog restart */
	volatile u_int8_t	_unused22[2];
	volatile u_int32_t	intr_stat;	/* interrupt status */
	volatile u_int8_t	_unused23[20];
};

/* sbus_base: sbus base address register */
#define	SBUS_BASE_RMBA		0xffffe000	/* reg map base address */
#define	SBUS_BASE_RMSS		0x00000007	/* reg map slot select bits */

/* vme_range0..15: master range registers */
#define	VME_RANGE_VMRCC		0xfff80000	/* master range compare code */
#define	VME_RANGE_VMAE		0x00078000	/* master adr extension bits */
#define	VME_RANGE_VMAT		0x00007ff8	/* master adr xlation bits */
#define	VME_RANGE_WPEN		0x00000002	/* write posting enable */
#define	VME_RANGE_DECEN		0x00000001	/* range decoding enable */

/* vme_master_map0..15: master capability registers */
#define	VME_MASTER_CAP_DATA	0xe0		/* data capabilities */
#define	VME_MASTER_CAP_D8	0x00		/* vmebus D8 */
#define	VME_MASTER_CAP_D16	0x20		/* vmebus D16 */
#define	VME_MASTER_CAP_D32	0x40		/* vmebus D32 */
#define	VME_MASTER_CAP_DBLT	0x60		/* vmebus BLT */
#define	VME_MASTER_CAP_DMBLT	0x80		/* vmebus MBLT */
#define	VME_MASTER_CAP_ADDR	0x1c		/* addr capabilities */
#define	VME_MASTER_CAP_A16	0x00		/* vmebus A16 */
#define	VME_MASTER_CAP_A24	0x04		/* vmebus A24 */
#define	VME_MASTER_CAP_A32	0x08		/* vmebus A32 */
#define	FVME_MAX_RANGES		16		/* number of ranges avail */

/* sbus_ssel0..15: sbus slot select registers */
#define	SBUS_SSEL_X		0x70		/* slot select pins range X */
#define	SBUS_SSEL_X_SLOT1	0x00		/* sbus slot 1 */
#define	SBUS_SSEL_X_SLOT2	0x10		/* sbus slot 2 */
#define	SBUS_SSEL_X_SLOT3	0x20		/* sbus slot 3 */
#define	SBUS_SSEL_X_SLOT4	0x30		/* sbus slot 4 */
#define	SBUS_SSEL_X_SLOT5x	0x40		/* sbus slot 5? */
#define	SBUS_SSEL_X_SLOT5	0x50		/* sbus slot 5 */
#define	SBUS_SSEL_Y		0x07		/* slot select pins range X+1*/
#define	SBUS_SSEL_Y_SLOT1	0x00		/* sbus slot 1 */
#define	SBUS_SSEL_Y_SLOT2	0x01		/* sbus slot 2 */
#define	SBUS_SSEL_Y_SLOT3	0x02		/* sbus slot 3 */
#define	SBUS_SSEL_Y_SLOT4	0x03		/* sbus slot 4 */
#define	SBUS_SSEL_Y_SLOT5x	0x04		/* sbus slot 5? */
#define	SBUS_SSEL_Y_SLOT5	0x05		/* sbus slot 5 */

/* viack_emu1..7: iack emulation registers */
/* bits contain d00-d07 from VMEbus interrupter */

/* sbus_cap: sbus capability register */
#define	SBUS_CAP_BURSTMASK	0xc0		/* dma burst size mask */
#define	SBUS_CAP_BURST_64	0xc0		/* 64 byte burst */
#define	SBUS_CAP_BURST_32	0x80		/* 32 byte burst */
#define	SBUS_CAP_BURST_16	0x40		/* 16 byte burst */
#define	SBUS_CAP_BURST_8	0x00		/* 8 byte burst */
#define	SBUS_CAP_READSTOPMASK	0x30		/* master read stop point */
#define	SBUS_CAP_READSTOP_64	0x30		/* stop at 64 byte boundary */
#define	SBUS_CAP_READSTOP_32	0x20		/* stop at 32 byte boundary */
#define	SBUS_CAP_READSTOP_16	0x10		/* stop at 16 byte boundary */
#define	SBUS_CAP_READSTOP_8	0x00		/* stop at 8 byte boundary */
#define	SBUS_CAP_BURSTDIS	0x08		/* disable sbus bursts */
#define	SBUS_CAP_HIDDENARBDIS	0x04		/* disable sbus hidden arb */
#define	SBUS_CAP_SPLITFLOW	0x02		/* disable flow through */

/* sbus_retry_ctrl: sbus retry register */
/* clock cycles with no acknowledge */

/* sbus_rerun_ctrl: sbus rerun limit register */
/* number of times to reruns to try on the bus */

/* swpar: sbus write posting error address register */
/* virtual sbus transfer address that was ack'd with an error */

/* slerr: sbus late error address register */
/* virtual sbus address that resulted in a late transfer */

/* vme_base: VMEbus base address register */
#define VME_BASE_RMVBA		0xfe		/* reg map base address */
#define	VME_BASE_RMACCEN	0x01		/* reg remote access enable */

/* vme_ext0..2: slave address extension registers */
/* extensions of A24 VMEbus address to 32bit sbus address (msb) */

/* sbus_range0..2: slave range registers */
#define	SBUS_RANGE_VSRCC	0xfff00000	/* slave range compare code */
#define	SBUS_RANGE_VSAT		0x000ffe00	/* slave address translation */
#define	SBUS_RANGE_A32DIS	0x00000004	/* disable A32 (enable A24) */
#define	SBUS_RANGE_WPDIS	0x00000002	/* disable write posting */
#define	SBUS_RANGE_DECDIS	0x00000001	/* disable encoding */

/* ibox_addr: IBOX address register */
/* address within VME A16 space where the IBOX is accessed */

/* ibox_ctrl: IBOX control register */
#define	IBOX_CTRL_DIS		0x01		/* disable ibox */

/* fmb_ctrl: force message broadcast control register */
#define	FMB_CTRL_SELAM		0x80		/* supervisor/user access */
#define	FMB_CTRL_DISCH1		0x40		/* disable channel 1 */
#define	FMB_CTRL_DISCH2		0x20		/* disable channel 2 */
#define	FMB_CTRL_SLOTID		0x1f		/* slot id: 1-15 */

/* vme_cap: global master capability register */
#define	VME_CAP_NPRV		0x80		/* supervisor access */

/* vmebus_handshake: VMEbus handshake configuration register */
#define	VME_HANDSHAKE_DISSGLE	0x80		/* disable glitch filter */
#define	VME_HANDSHAKE_DISASFAST	0x40		/* disable fast AS handshake */
#define	VME_HANDSHAKE_DISDS	0x20		/* disable fast data strobe */

/* vwpar: VMEbus write posting address register */
/* contains address ack'd with a BERR from the VME bus */

/* dma_ctrl: dma control register */
#define	DMA_CTRL_DMAEN		0x80		/* enable (start) dma */
#define	DMA_CTRL_DMADIS		0x40		/* disable dma transaction */
#define	DMA_CTRL_DMAHLT		0x20		/* stop dma transaction */
#define	DMA_CTRL_DMARESU	0x10		/* resume dma transaction */

/* dma_mode: dma mode register */
#define	DMA_MODE_DMASB		0x80		/* single buffer mode */
#define	DMA_MODE_DMANRTRY	0x40		/* error on retry */

/* dma_stat: dma status register */
#define	DMA_STAT_DMARUN		0x80		/* dma task is running */
#define	DMA_STAT_DMAWT		0x40		/* dma task is halted */
#define	DMA_STAT_DMANT		0x20		/* dma task successful */
#define	DMA_STAT_ERRMASK	0x18		/* dma error mask */
#define	DMA_STAT_ERR_SRC	0x00		/* error on source bus */
#define	DMA_STAT_ERR_DST	0x08		/* error on destination bus */
#define	DMA_STAT_ERR_NONE	0x10		/* no error termination */
#define	DMA_STAT_ERR_NOERROR	0x18		/* no error termination */

/* dma_src: dma source address register */
#define	DMA_SRC_ADDR		0xfffffffc	/* source address */
#define	DMA_SRC_VME		0x00000002	/* 0=vme, 1=sbus */

/* dma_dst: dma destination address register */
#define	DMA_DST_ADDR		0xfffffffc	/* destination address */
#define	DMA_DST_VME		0x00000002	/* 0=vme, 1=sbus */

/* dma_captl: dma capability/transfer length */
#define	DMA_CAPTL_SCAPD_MASK	0xe0000000	/* src data capability */
#define	DMA_CAPTL_SCAPD_D8	0x00000000	/* D8 slave */
#define	DMA_CAPTL_SCAPD_D16	0x20000000	/* D16 slave */
#define	DMA_CAPTL_SCAPD_D32	0x40000000	/* D32 slave */
#define	DMA_CAPTL_SCAPD_BLT	0x60000000	/* BLT slave */
#define	DMA_CAPTL_SCAPD_MBLT	0x80000000	/* MBLT slave */
#define	DMA_CAPTL_SCAPA_MASK	0x1c000000	/* src addr capability */
#define	DMA_CAPTL_SCAPA_A16	0x00000000	/* A16 slave */
#define	DMA_CAPTL_SCAPA_A24	0x04000000	/* A24 slave */
#define	DMA_CAPTL_SCAPA_A32	0x08000000	/* A32 slave */
#define	DMA_CAPTL_DCAPD_MASK	0x03800000	/* dst data capability */

/* mbox0..15: mailbox registers */
#define	MBOX_SEM		0x80		/* semaphore bit */

/* sem0..47: semaphore registers */
#define	SEM_SEM			0x80		/* semaphore bit */

/* gcsr: global control and status register */
#define	GCSR_SETSYSFAIL		0x80		/* assert SYSFAIL* signal */
#define	GCSR_ENSYSFAIL		0x40		/* enable SYSFAIL* output */
#define	GCSR_SYSFAIL		0x20		/* SYSFAIL* input status */
#define	GCSR_RESET		0x10		/* software reset */
#define	GCSR_ACFAIL		0x08		/* ACFAIL input status */
#define	GCSR_DISVDR		0x04		/* disable VME DTB drivers */
#define	GCSR_ENSYSCON		0x02		/* enable system controller */

/* reset_stat: reset status register */
#define	RESET_STAT_SBUS_RESET	0x80		/* sbus has been reset */
#define	RESET_STAT_VME_SYSRES	0x40		/* vmebus has been reset */
#define	RESET_STAT_WDT_RESET	0x20		/* watchdog has triggered */
#define	RESET_STAT_SYSRES_CALL	0x04		/* sysreset in mcsr0 set */
#define	RESET_STAT_RESET_CALL	0x02		/* reset in mcsr0 set */
#define	RESET_STAT_LOCRES_CALL	0x01		/* reset in ccsr set */

/* *irq_map: interrupt request mapping registers */
#define	IRQ_MAP_ENABLE		0x08		/* enable irq */
#define	IRQ_MAP_INT_MASK	0x07		/* irq mapping mask */
#define	IRQ_MAP_INT		0x00		/* NMI */
#define	IRQ_MAP_SINT1		0x01		/* sbus pri 1 */
#define	IRQ_MAP_SINT2		0x02		/* sbus pri 2 */
#define	IRQ_MAP_SINT3		0x03		/* sbus pri 3 */
#define	IRQ_MAP_SINT4		0x04		/* sbus pri 4 */
#define	IRQ_MAP_SINT5		0x05		/* sbus pri 5 */
#define	IRQ_MAP_SINT6		0x06		/* sbus pri 6 */
#define	IRQ_MAP_SINT7		0x07		/* sbus pri 7 */

/* mbox_stat: mailbox status interrupt status register */
/* 1 bit for each mailbox, 0 = interrupt pending, 1 = no interrupt pending */

/* arb_ctrl: arbitration control register */
#define	ARB_CTRL_MASK		0xc0		/* control mask */
#define	ARB_CTRL_PRIORITY	0xc0		/* priority mode */
#define	ARB_CTRL_ROBIN		0x40		/* round robin mode */
#define	ARB_CTRL_PRIROBIN	0x80		/* priority round robin mode */

/* req_ctrl: VMEbus request control register */
#define	REQ_CTRL_FM		0x80		/* 0=fair,1=demand mode */
#define	REQ_CTRL_RM_MASK	0x70		/* release mode mask */
#define	REQ_CTRL_RM_ROR		0x30		/* release on request */
#define	REQ_CTRL_RM_ROC		0x40		/* release on bus clear */
#define	REQ_CTRL_RM_RAT		0x60		/* release after timeout */
#define	REQ_CTRL_RM_RWD		0x70		/* release when done */
#define	REQ_CTRL_REC		0x08		/* 0=begin,1=end of cycle */
#define	REQ_CTRL_RL_MASK	0x06		/* request level mask */
#define	REQ_CTRL_RL_BR3		0x06		/* br3 priority */
#define	REQ_CTRL_RL_BR2		0x04		/* br2 priority */
#define	REQ_CTRL_RL_BR1		0x02		/* br1 priority */
#define	REQ_CTRL_RL_BR0		0x00		/* br0 priority */
#define	REQ_CTRL_GLFI		0x01		/* enable bbsy glitch filter */

/* bus_ctrl: VMEbus capture register */
#define	BUS_CTRL_BCAP		0x80		/* request and keep bus */
#define	BUS_CTRL_OWN		0x40		/* do we own the bus? */

/* mcsr0: miscellaneous control and status register 0 */
#define	MCSR0_ABORTSW		0x80		/* status of abort switch */
#define	MCSR0_SCON		0x40		/* status of SCON input */
#define	MCSR0_SYSRESET		0x20		/* assert SYSRESET* */
#define	MCSR0_RESET		0x10		/* software reset fga5000 */
#define	MCSR0_ENA_VME_TIMEOUT	0x08		/* enable VME timeout */
#define	MCSR0_VME_TIMEOUT_MASK	0x06		/* VME transaction timeout */
#define	MCSR0_VME_TIMEOUT_32	0x02		/* 32 usec */
#define	MCSR0_VME_TIMEOUT_128	0x04		/* 128 usec */
#define	MCSR0_VME_TIMEOUT_512	0x06		/* 512 usec */
#define	MCSR0_SYSRESET_IN	0x01		/* enable SYSRESET input */

/* mcsr1: miscellaneous control and status register 1 */
#define	MCSR1_ENAWDT		0x80		/* enable watchdog timer */
#define	MCSR1_TIMEOUT_MASK	0x70		/* watchdog timeout mask */
#define	MCSR1_TIMEOUT_408MS	0x00		/* 408 msec */
#define	MCSR1_TIMEOUT_168S	0x10		/* 1.68 sec */
#define	MCSR1_TIMEOUT_67S	0x20		/* 6.7 sec */
#define	MCSR1_TIMEOUT_268S	0x30		/* 26.8 sec */
#define	MCSR1_TIMEOUT_1M48S	0x40		/* 1 min 48 sec */
#define	MCSR1_TIMEOUT_7M9S	0x50		/* 7 min 9 sec */
#define	MCSR1_TIMEOUT_28M38S	0x60		/* 28 min 38 sec */
#define	MCSR1_TIMEOUT_1H54M	0x70		/* 1 hour 54 min */
#define	MCSR1_IRQ_FREEZE	0x08		/* freeze irq map regs */

/* wdt_restart: watchdog timer restart register */
/* any read/write resets the watchdog timer */

/* intr_stat: interrupt status register */
#define	INTR_STAT_ACFAIL	0x80000000	/* acfail pending */
#define	INTR_STAT_SYSFAILASSERT	0x40000000	/* sysfail assert pending */
#define	INTR_STAT_SYSFAILNEGATE	0x20000000	/* sysfail negate pending */
#define	INTR_STAT_ABORT		0x10000000	/* abort pending */
#define	INTR_STAT_ARBTIMEOUT	0x08000000	/* arbitration timeout */
#define	INTR_STAT_MAILBOX	0x04000000	/* mailbox intr pending */
#define	INTR_STAT_SBUS_WPERR	0x02000000	/* sbus wperr pending */
#define	INTR_STAT_VME_WPERR	0x01000000	/* vme wperr pending */
#define	INTR_STAT_DMATERM	0x00800000	/* dma finished */
#define	INTR_STAT_WDT		0x00400000	/* watchdog half timeout */
#define	INTR_STAT_SLERR		0x00200000	/* sbus late error pending */
#define	INTR_STAT_IBOX		0x00100000	/* ibox pending */
#define	INTR_STAT_FMB0		0x00080000	/* fmb channel 0 pending */
#define	INTR_STAT_FMB1		0x00040000	/* fmb channel 1 pending */
#define	INTR_STAT_VMEIRQ7	0x00000080	/* vme irq 7 pending */
#define	INTR_STAT_VMEIRQ6	0x00000040	/* vme irq 6 pending */
#define	INTR_STAT_VMEIRQ5	0x00000020	/* vme irq 5 pending */
#define	INTR_STAT_VMEIRQ4	0x00000010	/* vme irq 4 pending */
#define	INTR_STAT_VMEIRQ3	0x00000008	/* vme irq 3 pending */
#define	INTR_STAT_VMEIRQ2	0x00000004	/* vme irq 2 pending */
#define	INTR_STAT_VMEIRQ1	0x00000002	/* vme irq 1 pending */
