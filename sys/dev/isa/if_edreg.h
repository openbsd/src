/*    $OpenBSD: if_edreg.h,v 1.4 1998/03/16 10:41:42 downsj Exp $      */
/*    $NetBSD: if_edreg.h,v 1.15 1996/01/10 16:49:22 chuck Exp $      */

/*
 * National Semiconductor DS8390 NIC register definitions.
 *
 * Copyright (C) 1993, David Greenman.  This software may be used, modified,
 * copied, distributed, and sold, in both source and binary form provided that
 * the above copyright and these terms are retained.  Under no circumstances is
 * the author responsible for the proper functioning of this software, nor does
 * the author assume any responsibility for damages incurred with its use.
 */

/*
 * Vendor types
 */
#define ED_VENDOR_WD_SMC	0x00	/* Western Digital/SMC */
#define ED_VENDOR_3COM		0x01	/* 3Com */
#define ED_VENDOR_NOVELL	0x02	/* Novell */

/*
 * Compile-time config flags
 */
/*
 * This sets the default for enabling/disablng the tranceiver.
 */
#define ED_FLAGS_DISABLE_TRANCEIVER	0x0001

/*
 * This forces the board to be used in 8/16-bit mode even if it autoconfigs
 * differently.
 */
#define ED_FLAGS_FORCE_8BIT_MODE	0x0002
#define ED_FLAGS_FORCE_16BIT_MODE	0x0004

/*
 * This disables the use of double transmit buffers.
 */
#define ED_FLAGS_NO_MULTI_BUFFERING	0x0008

/*
 * This forces all operations with the NIC memory to use Programmed I/O (i.e.
 * not via shared memory).
 */
#define ED_FLAGS_FORCE_PIO		0x0010

/*
 *		Definitions for Western digital/SMC WD80x3 series ASIC
 */
/*
 * Memory Select Register (MSR)
 */
#define ED_WD_MSR	0

/* next three definitions for Toshiba */
#define	ED_WD_MSR_POW	0x02	/* 0 = power save, 1 = normal (R/W) */
#define	ED_WD_MSR_BSY	0x04	/* gate array busy (R) */
#define	ED_WD_MSR_LEN	0x20	/* 0 = 16-bit, 1 = 8-bit (R/W) */

#define ED_WD_MSR_ADDR	0x3f	/* Memory decode bits 18-13 */
#define ED_WD_MSR_MENB	0x40	/* Memory enable */
#define ED_WD_MSR_RST	0x80	/* Reset board */

/*
 * Interface Configuration Register (ICR)
 */
#define ED_WD_ICR	1

#define ED_WD_ICR_16BIT	0x01	/* 16-bit interface */
#define ED_WD_ICR_OAR	0x02	/* select register (0=BIO 1=EAR) */
#define ED_WD_ICR_IR2	0x04	/* high order bit of encoded IRQ */
#define ED_WD_ICR_MSZ	0x08	/* memory size (0=8k 1=32k) */
#define ED_WD_ICR_RLA	0x10	/* recall LAN address */
#define ED_WD_ICR_RX7	0x20	/* recall all but i/o and LAN address */
#define	ED_WD_ICR_RIO	0x40	/* recall i/o address */
#define ED_WD_ICR_STO	0x80	/* store to non-volatile memory */
#ifdef TOSH_ETHER
#define	ED_WD_ICR_MEM	0xe0	/* shared mem address A15-A13 (R/W) */
#define	ED_WD_ICR_MSZ1	0x0f	/* memory size, 0x08 = 64K, 0x04 = 32K,
				   0x02 = 16K, 0x01 = 8K */
				/* 64K can only be used if mem address
				   above 1MB */
				/* IAR holds address A23-A16 (R/W) */
#endif

/*
 * IO Address Register (IAR)
 */
#define ED_WD_IAR	2

/*
 * EEROM Address Register
 */
#define ED_WD_EAR	3

/*
 * Interrupt Request Register (IRR)
 */
#define ED_WD_IRR	4

#define	ED_WD_IRR_0WS	0x01	/* use 0 wait-states on 8 bit bus */
#define ED_WD_IRR_OUT1	0x02	/* WD83C584 pin 1 output */
#define ED_WD_IRR_OUT2	0x04	/* WD83C584 pin 2 output */
#define ED_WD_IRR_OUT3	0x08	/* WD83C584 pin 3 output */
#define ED_WD_IRR_FLASH	0x10	/* Flash RAM is in the ROM socket */

/*
 * The three bits of the encoded IRQ are decoded as follows:
 *
 * IR2 IR1 IR0  IRQ
 *  0   0   0   2/9
 *  0   0   1   3
 *  0   1   0   5
 *  0   1   1   7
 *  1   0   0   10
 *  1   0   1   11
 *  1   1   0   15
 *  1   1   1   4
 */
#define ED_WD_IRR_IR0	0x20	/* bit 0 of encoded IRQ */
#define ED_WD_IRR_IR1	0x40	/* bit 1 of encoded IRQ */
#define ED_WD_IRR_IEN	0x80	/* Interrupt enable */

/*
 * LA Address Register (LAAR)
 */
#define ED_WD_LAAR	5

#define ED_WD_LAAR_ADDRHI	0x1f	/* bits 23-19 of RAM address */
#define ED_WD_LAAR_0WS16	0x20	/* enable 0 wait-states on 16 bit bus */
#define ED_WD_LAAR_L16EN	0x40	/* enable 16-bit operation */
#define ED_WD_LAAR_M16EN	0x80	/* enable 16-bit memory access */

/* i/o base offset to station address/card-ID PROM */
#define ED_WD_PROM	8

/*
 *	83C790 specific registers
 */
/*
 * Hardware Support Register (HWR) ('790)
 */
#define	ED_WD790_HWR	4

#define	ED_WD790_HWR_RST	0x10	/* hardware reset */
#define	ED_WD790_HWR_LPRM	0x40	/* LAN PROM select */
#define	ED_WD790_HWR_SWH	0x80	/* switch register set */

/*
 * ICR790 Interrupt Control Register for the 83C790
 */
#define	ED_WD790_ICR	6

#define	ED_WD790_ICR_EIL	0x01	/* enable interrupts */

/*
 * REV/IOPA Revision / I/O Pipe register for the 83C79X
 */
#define ED_WD790_REV	7

#define ED_WD790	0x20		/* and 0x21... */
#define ED_WD795	0x40		/* and 0x41... */

/*
 * PIO mode register for the 83C795
 */
#define ED_WD795_PIO	8

/*
 * 79X RAM Address Register (RAR)
 *      Enabled with SWH bit=1 in HWR register
 */

#define ED_WD790_RAR	0x0b

#define ED_WD790_RAR_SZ8	0x00	/* 8k memory buffer */
#define ED_WD790_RAR_SZ16	0x10	/* 16k memory buffer */
#define ED_WD790_RAR_SZ32	0x20	/* 32k memory buffer */
#define ED_WD790_RAR_SZ64	0x30	/* 64k memory buffer */

/*
 * General Control Register (GCR)
 * Eanbled with SWH bit == 1 in HWR register
 */
#define	ED_WD790_GCR	0x0d

#define	ED_WD790_GCR_IR0	0x04	/* bit 0 of encoded IRQ */
#define	ED_WD790_GCR_IR1	0x08	/* bit 1 of encoded IRQ */
#define	ED_WD790_GCR_ZWSEN	0x20	/* zero wait state enable */
#define	ED_WD790_GCR_IR2	0x40	/* bit 2 of encoded IRQ */
/*
 * The three bits of the encoded IRQ are decoded as follows:
 *
 * IR2 IR1 IR0  IRQ
 *  0   0   0   none
 *  0   0   1   9
 *  0   1   0   3
 *  0   1   1   5
 *  1   0   0   7
 *  1   0   1   10
 *  1   1   0   11
 *  1   1   1   15
 */

/* i/o base offset to CARD ID */
#define ED_WD_CARD_ID	ED_WD_PROM+6

/* Board type codes in card ID */
#define ED_TYPE_WD8003S		0x02
#define ED_TYPE_WD8003E		0x03
#define ED_TYPE_WD8013EBT	0x05
#define	ED_TYPE_TOSHIBA1	0x11	/* named PCETA1 */
#define	ED_TYPE_TOSHIBA2	0x12	/* named PCETA2 */
#define	ED_TYPE_TOSHIBA3	0x13	/* named PCETB */
#define	ED_TYPE_TOSHIBA4	0x14	/* named PCETC */
#define	ED_TYPE_WD8003W		0x24
#define	ED_TYPE_WD8003EB	0x25
#define	ED_TYPE_WD8013W		0x26
#define ED_TYPE_WD8013EP	0x27
#define ED_TYPE_WD8013WC	0x28
#define ED_TYPE_WD8013EPC	0x29
#define	ED_TYPE_SMC8216T	0x2a
#define	ED_TYPE_SMC8216C	0x2b
#define ED_TYPE_WD8013EBP	0x2c

/* Bit definitions in card ID */
#define	ED_WD_REV_MASK		0x1f	/* Revision mask */
#define	ED_WD_SOFTCONFIG	0x20	/* Soft config */
#define	ED_WD_LARGERAM		0x40	/* Large RAM */
#define	ED_MICROCHANEL		0x80	/* Microchannel bus (vs. isa) */

/*
 * Checksum total.  All 8 bytes in station address PROM will add up to this.
 */
#ifdef TOSH_ETHER
#define ED_WD_ROM_CHECKSUM_TOTAL	0xA5
#else
#define ED_WD_ROM_CHECKSUM_TOTAL	0xFF
#endif

#define ED_WD_NIC_OFFSET	0x10	/* I/O base offset to NIC */
#define ED_WD_ASIC_OFFSET	0	/* I/O base offset to ASIC */
#define ED_WD_IO_PORTS		32	/* # of i/o addresses used */

#define ED_WD_PAGE_OFFSET	0	/* page offset for NIC access to mem */

/*
 *			Definitions for 3Com 3c503
 */
#define ED_3COM_NIC_OFFSET	0
#define ED_3COM_ASIC_OFFSET	0x400	/* offset to nic i/o regs */

/*
 * XXX - The I/O address range is fragmented in the 3c503; this is the
 *	number of regs at iobase.
 */
#define ED_3COM_IO_PORTS	16	/* # of i/o addresses used */

/* tx memory starts in second bank on 8bit cards */
#define ED_3COM_TX_PAGE_OFFSET_8BIT	0x20

/* tx memory starts in first bank on 16bit cards */
#define ED_3COM_TX_PAGE_OFFSET_16BIT	0x0

/* ...and rx memory starts in second bank */
#define ED_3COM_RX_PAGE_OFFSET_16BIT	0x20


/*
 * Page Start Register.  Must match PSTART in NIC.
 */
#define ED_3COM_PSTR		0

/*
 * Page Stop Register.  Must match PSTOP in NIC.
 */
#define ED_3COM_PSPR		1

/*
 * DrQ Timer Register.  Determines number of bytes to be transfered during a
 * DMA burst.
 */
#define ED_3COM_DQTR		2

/*
 * Base Configuration Register.  Read-only register which contains the
 * board-configured I/O base address of the adapter.  Bit encoded.
 */
#define ED_3COM_BCFR		3

/*
 * EPROM Configuration Register.  Read-only register which contains the
 * board-configured memory base address.  Bit encoded.
 */
#define ED_3COM_PCFR		4

/*
 * GA Configuration Register.  Gate-Array Configuration Register.
 *
 * mbs2  mbs1  mbs0	start address
 *  0     0     0	0x0000
 *  0     0     1	0x2000
 *  0     1     0	0x4000
 *  0     1     1	0x6000
 *
 * Note that with adapters with only 8K, the setting for 0x2000 must always be
 * used.
 */
#define ED_3COM_GACFR		5

#define ED_3COM_GACFR_MBS0	0x01
#define ED_3COM_GACFR_MBS1	0x02
#define ED_3COM_GACFR_MBS2	0x04

#define ED_3COM_GACFR_RSEL	0x08	/* enable shared memory */
#define ED_3COM_GACFR_TEST	0x10	/* for GA testing */
#define ED_3COM_GACFR_OWS	0x20	/* select 0WS access to GA */
#define ED_3COM_GACFR_TCM	0x40	/* Mask DMA interrupts */
#define ED_3COM_GACFR_NIM	0x80	/* Mask NIC interrupts */

/*
 * Control Register.  Miscellaneous control functions.
 */
#define ED_3COM_CR		6

#define ED_3COM_CR_RST		0x01	/* Reset GA and NIC */
#define ED_3COM_CR_XSEL		0x02	/* Transceiver select.  BNC=1(def) AUI=0 */
#define ED_3COM_CR_EALO		0x04	/* window EA PROM 0-15 to I/O base */
#define ED_3COM_CR_EAHI		0x08	/* window EA PROM 16-31 to I/O base */
#define ED_3COM_CR_SHARE	0x10	/* select interrupt sharing option */
#define ED_3COM_CR_DBSEL	0x20	/* Double buffer select */
#define ED_3COM_CR_DDIR		0x40	/* DMA direction select */
#define ED_3COM_CR_START	0x80	/* Start DMA controller */

/*
 * Status Register.  Miscellaneous status information.
 */
#define ED_3COM_STREG		7

#define ED_3COM_STREG_REV	0x07	/* GA revision */
#define ED_3COM_STREG_DIP	0x08	/* DMA in progress */
#define ED_3COM_STREG_DTC	0x10	/* DMA terminal count */
#define ED_3COM_STREG_OFLW	0x20	/* Overflow */
#define ED_3COM_STREG_UFLW	0x40	/* Underflow */
#define ED_3COM_STREG_DPRDY	0x80	/* Data port ready */

/*
 * Interrupt/DMA Configuration Register
 */
#define ED_3COM_IDCFR		8

#define ED_3COM_IDCFR_DRQ	0x07	/* DMA request */
#define ED_3COM_IDCFR_UNUSED	0x08	/* not used */
#if 0
#define ED_3COM_IDCFR_IRQ	0xF0	/* Interrupt request */
#else
#define ED_3COM_IDCFR_IRQ2	0x10	/* Interrupt request 2 select */
#define ED_3COM_IDCFR_IRQ3	0x20	/* Interrupt request 3 select */
#define ED_3COM_IDCFR_IRQ4	0x40	/* Interrupt request 4 select */
#define ED_3COM_IDCFR_IRQ5	0x80	/* Interrupt request 5 select */
#endif

/*
 * DMA Address Register MSB
 */
#define ED_3COM_DAMSB		9

/*
 * DMA Address Register LSB
 */
#define ED_3COM_DALSB		0x0a

/*
 * Vector Pointer Register 2
 */
#define ED_3COM_VPTR2		0x0b

/*
 * Vector Pointer Register 1
 */
#define ED_3COM_VPTR1		0x0c

/*
 * Vector Pointer Register 0
 */
#define ED_3COM_VPTR0		0x0d

/*
 * Register File Access MSB
 */
#define ED_3COM_RFMSB		0x0e

/*
 * Register File Access LSB
 */
#define ED_3COM_RFLSB		0x0f

/*
 *		 Definitions for Novell NE1000/2000 boards
 */

/*
 * Board type codes
 */
#define ED_TYPE_NE1000		0x01
#define ED_TYPE_NE2000		0x02

/*
 * Register offsets/total
 */
#define ED_NOVELL_NIC_OFFSET	0x00
#define ED_NOVELL_ASIC_OFFSET	0x10
#define ED_NOVELL_IO_PORTS	32

/*
 * Remote DMA data register; for reading or writing to the NIC mem via
 * programmed I/O (offset from ASIC base).
 */
#define ED_NOVELL_DATA		0x00

/*
 * Reset register; reading from this register causes a board reset.
 */
#define ED_NOVELL_RESET		0x0f
