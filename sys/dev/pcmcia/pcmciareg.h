/*	$Id: pcmciareg.h,v 1.1 1996/04/29 14:17:35 hvozda Exp $	*/
/*
 * This file was apparently first written by Stefan Grefen, although it
 * contained no copyright notice at the time.
 */
#ifndef __PCMCIAREG_H__
#define __PCMCIAREG_H__

/*
 * Configuration Registers
 *
 * These are the registers required by Release 2.0 of the standard
 * (Section 4.15)
 */

/* Offsets for register ordering */
#define PCMCIA_COR	0x00	/* Configuration and Option Register */
#define PCMCIA_CCSR	0x02	/* Card Configuration and Status Register */
#define PCMCIA_PIR	0x04	/* Pin Replacement Register */
#define PCMCIA_SCR	0x06	/* Socket and Copy Register */

/* Now register bits, ordered by reg # */

/* For Configuration and Option Register (PCMCIA_COR) */
#define PCMCIA_MEMIO	0x01	/* Use I/O Space */
#define PCMCIA_CNFG	0x0e	/* I/O decoding configuration */
#define PCMCIA_CNFGMASK 0x3f	/* Use template */
#define PCMCIA_LVLREQ	0x40	/* Generate level mode interrupts */
#define PCMCIA_SRESET	0x80	/* Reset Card */

/* For Card Configuration and Status Register (PCMCIA_CCSR) */
#define PCMCIA_INTR		0x02	/* Interrupt Pending */
#define PCMCIA_POWER_DOWN	0x04
#define PCMCIA_AUDIO_ENA	0x08
#define PCMCIA_IOIS8		0x20
#define PCMCIA_SIGCHG_ENA	0x40
#define PCMCIA_CHANGED		0x80

/* Pin Replacement Register (PCMCIA_PIR) */
#define PCMCIA_WP_STATUS	   0x01
#define PCMCIA_READY_STATUS	   0x02
#define PCMCIA_BVD2_STATUS	   0x04
#define PCMCIA_BVD1_STATUS	   0x08
#define PCMCIA_WP_EVENT		   0x10
#define PCMCIA_READY_EVENT	   0x20
#define PCMCIA_BVD2_EVENT	   0x40
#define PCMCIA_BVD1_EVENT	   0x80


/* For Socket and Copy Register (PCMCIA_SCR) */
#define PCMCIA_SOCKNUM	0x0f	/* Which socket I'm sitting in */
#define PCMCIA_COPNUM	0x70	/* Which instance I am. */

/*
 * CIS Tuple defines
 */
#define CIS_MAXSIZE	512

/* Define tuple types */
#define CIS_NULL	0x00	/* null tuple */
#define CIS_DEVICE	0x01	/* Device descriptor, common mem */
#define CIS_DEVICE_A	0x17	/* Device descriptor, attribute mem */
#define		CIS_DEVICE_TYPE		0xf0	/* type mask */
#define		CIS_DEVICE_TYPE_SHIFT	4	/* type offset */
#define		CIS_DEVICE_WPS		0x08	/* WPS mask */
#define		CIS_DEVICE_SPEED	0x07	/* speed mask */
#define		CIS_DEVICE_ADDRS	0xf8	/* # addr units */
#define		CIS_DEVICE_ADDRS_SHIFT	3	/* # addr units offset */
#define		CIS_DEVICE_SIZE		0x07
#define CIS_CSUM	0x10	/* Checksum field */
#define CIS_NOLINK	0x14	/* No Link */
#define CIS_VER1	0x15	/* Level 1 Version/Product info */
#define CIS_CFG_INFO	0x1a	/* Configuration info map */
#define		TPCC_RASZ		0x03	/* size of regaddr */
#define		TPCC_RASZ_SHIFT		0
#define		TPCC_RMSZ		0x3c	/* size of regmask */
#define		TPCC_RMSZ_SHIFT		2
#define		TPCC_LAST		0x3f	/* last con entry idx */
#define		TPCC_LAST_SHIFT		0
#define CIS_CFG_ENT	0x1b	/* Configuration info entry */
#define		TPCE_INDX_ENTRY		0x3f	/* config entry # */
#define		TPCE_INDX_DEF		0x40	/* default bit */
#define		TPCE_INDX_INT		0x80	/* interface bit */
#define		TPCE_IF_TYPE		0x0f	/* interface type */
#define		TPCE_IF_BVD		0x10	/* BVD active bit */
#define		TPCE_IF_WP		0x20	/* WP active bit */
#define		TPCE_IF_RDYBSY		0x40	/* RdyBsy active bit */
#define		TPCE_IF_MWAIT		0x80	/* Wait Sig req. bit */
#define		TPCE_FS_PWR		0x03	/* Power */
#define			TPCE_FS_PWR_VCC		0x01	/* Vcc struct */
#define			TPCE_FS_PWR_VPP		0x02	/* Vpp struct */
#define		TPCE_FS_TD		0x04	/* Timing */
#define			TPCE_FS_TD_WAIT		0x03	/* wait scale */
#define			TPCE_FS_TD_RDY		0x1c	/* rdy/bsy scale */
#define			TPCE_FS_TD_RDY_SHIFT	2
#define			TPCE_FS_TD_RSV		0xe0	/* reserved scale */
#define			TPCE_FS_TD_RSV_SHIFT	5
#define		TPCE_FS_IO		0x08	/* I/O Space */
#define			TPCE_FS_IO_LINES	0x1f	/* IO addr lines */
#define			TPCE_FS_IO_BUS8		0x20	/* bus 8 bit */
#define			TPCE_FS_IO_BUS16	0x40	/* bus 16 bit */
#define			TPCE_FS_IO_RANGE	0x80	/* range bit */
#define			TPCE_FS_IO_LEN		0xc0	/* block len size */
#define			TPCE_FS_IO_LEN_SHIFT	6
#define			TPCE_FS_IO_SIZE		0x30	/* block size size */
#define			TPCE_FS_IO_SIZE_SHIFT	4
#define			TPCE_FS_IO_NUM		0x0f	/* # of blocks */
#define		TPCE_FS_IRQ		0x10	/* IRQ */
#define			TPCE_FS_IRQ_SHARE	0x80	/* int sharing */
#define			TPCE_FS_IRQ_PULSE	0x40	/* pulse request */
#define			TPCE_FS_IRQ_LEVEL	0x20	/* level-trig int */
#define			TPCE_FS_IRQ_MASK	0x10	/* irq mask bit */
#define			TPCE_FS_IRQ_IRQN	0x0f	/* irqn mask */
#define			TPCE_FS_IRQ_VEND	0x08	/* vendor sig */
#define			TPCE_FS_IRQ_BERR	0x04	/* bus error */
#define			TPCE_FS_IRQ_IOCK	0x02	/* io check */
#define			TPCE_FS_IRQ_NMI		0x01	/* nmi */
#define		TPCE_FS_MEM		0x60	/* Mem Space */
#define		TPCE_FS_MEM_SHIFT	5
#define			TPCE_FS_MEM_HOST	0x80
#define			TPCE_FS_MEM_ADDR	0x60
#define			TPCE_FS_MEM_ADDR_SHIFT	5
#define			TPCE_FS_MEM_LEN		0x18
#define			TPCE_FS_MEM_LEN_SHIFT	3
#define			TPCE_FS_MEM_WINS	0x07
#define		TPCE_FS_MISC		0x80	/* Misc */
#define CIS_MFG		0x20	/* Manufacturer's ID */
#define CIS_FUNC	0x21	/* Function ID */
#define CIS_FUNE	0x22	/* Function Extension */
#define CIS_DRIVER	0x77	/* Driver ID */
#define CIS_END		0xff	/* Last Entry */

#define splpcmcia spltty
#define IPL_PCMCIA IPL_TTY

#endif /* __PCMCIAREG_H__ */
