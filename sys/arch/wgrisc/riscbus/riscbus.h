/*	$OpenBSD: riscbus.h,v 1.2 1997/05/11 16:26:03 pefo Exp $ */

/*
 * Copyright (c) 1996 Per Fogelstrom
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed under OpenBSD by
 *	Per Fogelstrom.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#ifndef	RISCBUS_H
#define	RISCBUS_H 1

#define RISC_PHYS_MIN		0x00000000	/* 256 Meg */
#define RISC_PHYS_MAX		0x01ffffff

/*
 * Memory map
 */

#define RISC_PHYS_MEMORY_START	0x00000000
#define RISC_PHYS_MEMORY_END	0x01ffffff	/* 256 Meg in 8 slots */

#define	RISC_SRAM_START		0xbfb00000	/* Static ram */

/*
 * I/O map
 */

#define	RISC_ISA_IO_BASE	0xac000000	/* ISA I/O base adderss */
#define	RISC_ISA_MEM_BASE	0xad000000	/* ISA MEM base adderss */
#define	RISC_LOCAL_IO_BASE	0xae000000	/* I/O Base address */

#define	RISC_COM0		0xae400000	/* 16550 */
#define	RISC_COM1		0xae410000	/* 16550 */
#define	RISC_COM2		0xae420000	/* 16550 */
#define	RISC_COM3		0xae430000	/* 16550 */
#define	RISC_SCSI		0xae440000	/* AMD53C94 */
#define	RISC_RTC		0xae450000	/* DP8571 */
#define	RISC_SONIC		0xae460000	/* DP83932 */
#define	RISC_SONIC_SRAM		0xae470000	/* 2 * 32KB 70ns sram */

#define	RISC_CTRL1		0xae800000	/* Watchdog control */
#define	RISC_CTRL2		0xae810000	/* System control */
#define	RISC_STATUS		0xae820000	/* System status */
#define	RISC_FLASH_CTRL		0xae840000	/* Flash */
#define	RISC_FLASH_WRITE	0xae850000	/* Flash */
#define	RISC_FLASH_READ		0xae860000	/* Flash */
#define	RISC_LEDS		0xae870000	/* System LEDS */

/*
 * I/O map of R3715 chip.
 */
#define	R3715_ROM_CONF		0xbd000000	/* Rom config reg */
#define	R3715_PIO_VAL		0xbd000040	/* PIO value reg */
#define	R3715_PIO_CTRL		0xbd000044	/* PIO control reg */
#define	R3715_PIO_READ		0xbd00005c	/* PIO read (pin) reg */
#define	R3715_TC_VAL		0xbd000048	/* Timer/Counter value reg */
#define	R3715_TC_CTRL		0xbd00004c	/* Timer/Counter control reg */
#define	R3715_INT_CAUSE		0xbd000050	/* R3715 Interrupt cause */
#define	R3715_INT_MASK		0xbd000054	/* R3715 Interrupt mask */
#define	R3715_INT_WRITE		0xbd000060	/* R3715 Interrupt write */
#define	R3715_DRAM_CTRL		0xbd000058	/* R3715 DRAM Control */
#define	R3715_DMA_ADR0		0xbd000080	/* R3715 DMA Address 0 */
#define	R3715_DMA_ADR1		0xbd000084	/* R3715 DMA Address 1 */
#define	R3715_DMA_CNT0		0xbd000090	/* R3715 DMA Size count 0 */
#define	R3715_DMA_CNT1		0xbd000044	/* R3715 DMA Size count 1 */
#define	R3715_IO_TIMING		0xbd0000a0	/* R3715 I/O Timing control */

/*
 * R3715 Interrupt control register bits.
 */
#define	R3715_ISAIRQ_11		0x00100000
#define	R3715_ISAIRQ_7		0x00200000
#define	R3715_ISAIRQ_6		0x00400000
#define	R3715_ISAIRQ_9		0x00800000
#define	R3715_ISAIRQ_10		0x01000000
#define	R3715_DSPINT		0x02000000
#define	R3715_TIMINT		0x00000010
#define	R3715_DMA_INT0		0x00000020
#define	R3715_DMA_INT1		0x00000040

#ifndef _LOCORE
/*
 *  Interrupt vector descriptor for device on risc bus.
 */
struct riscbus_int_desc {
	int		int_mask;	/* Mask to call handler at */
	intr_handler_t	int_hand;	/* Interrupt handler */
	void		*param;		/* Parameter to send to handler */
	int		spl_mask;	/* Spl mask for interrupt */
};

int	riscbus_intrnull __P((void *));
#endif	/* _LOCORE */
#endif	/* RISCBUS_H */
