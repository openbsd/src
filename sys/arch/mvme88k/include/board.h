/*	$OpenBSD: board.h,v 1.4 1999/02/09 06:36:25 smurph Exp $ */
/*
 * Copyright (c) 1996 Nivas Madhur
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Nivas Madhur.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
/*
 * Mach Operating System
 * Copyright (c) 1991 Carnegie Mellon University
 * Copyright (c) 1991 OMRON Corporation
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 */
#ifndef _MACHINE_BOARD_H
#define _MACHINE_BOARD_H
/*
 *      VME187 CPU board constants - derived from Luna88k
 */

/*
 * Something to put append a 'U' to a long constant if it's C so that
 * it'll be unsigned in both ANSI and traditional.
 */
#if defined(ASSEMBLER)
#	define U(num)	num
#else
#  if defined(__STDC__)
#	define U(num)	num ## U
#  else
#	define U(num)	num/**/U
#  endif
#endif

#define	MAX_CPUS	1		/* no. of CPUs */
#define MAX_CMMUS	2		/* 2 CMMUs - 1 data and 1 code */

#define	SYSV_BASE	U(0x00000000) 	/* system virtual base */

#define	MAXU_ADDR	U(0x40000000) 	/* size of user virtual space */
#define MAXPHYSMEM	U(0x10000000) 	/* max physical memory */

#define BUGROM_START	U(0xFF800000)	/* start of BUG PROM */
#define BUGROM_SIZE	U(0x003FFFFF)	/* size of BUG PROM */
#define SRAM_START	U(0xFFE00000)	/* start of sram used by bug */
#define SRAM_SIZE	U(0x0001FFFF)	/* size of sram */
#define OBIO_START	U(0xFFF00000)	/* start of local IO */
#define OBIO_SIZE	U(0x000EFFFF)	/* size of obio space */

#define INT_PRI_LEVEL	U(0xFFF4203E)	/* interrupt priority level */
#define INT_MASK_LEVEL	U(0xFFF4203F)	/* interrupt mask level */

#define LOCAL_IO_DEVS	U(0xFFF00000)	/* local IO devices */
#define VMEA16		U(0xFFFF0000)	/* VMEbus A16 */

#define	PCC2_ADDR	U(0xFFF42000)	/* PCCchip2 Regs */
#define	MEM_CTLR	U(0xFFF43000)	/* MEMC040 mem controller */
#define SCC_ADDR	U(0xFFF45000) 	/* Cirrus Chip */
#define LANCE_ADDR	U(0xFFF46000) 	/* 82596CA */
#define SCSI_ADDR	U(0xFFF47000) 	/* NCR 710 address */
#define NCR710_SIZE	U(0x00000040) 	/* NCR 710 size */
#define MK48T08_ADDR	U(0xFFFC0000) 	/* BBRAM, TOD */

#define	TOD_CAL_CTL	U(0xFFFC1FF8) 	/* calendar control register */
#define TOD_CAL_SEC	U(0xFFFC1FF9) 	/* seconds */
#define TOD_CAL_MIN	U(0xFFFC1FFA) 	/* minutes */
#define TOD_CAL_HOUR	U(0xFFFC1FFB) 	/* hours */
#define TOD_CAL_DOW	U(0xFFFC1FFC) 	/* Day Of the Week */
#define TOD_CAL_DAY	U(0xFFFC1FFD) 	/* days */
#define TOD_CAL_MON	U(0xFFFC1FFE) 	/* months */
#define TOD_CAL_YEAR	U(0xFFFC1FFF) 	/* years */

#define CMMU_I		U(0xFFF77000) 	/* CMMU instruction  */
#define CMMU_D		U(0xFFF7F000) 	/* CMMU data */
#define CMMU_SIZE	0x1000

#if 0
/* interrupt vectors */

#define PPBSY		0x50		/* printer port busy */
#define PPPE		0x51		/* printer port PE   */
#define PPSEL		0x52		/* printer port select */
#define PPFLT		0x53		/* printer port fault */
#define PPACK		0x54		/* printer port ack */
#define SCSIIRQ		0x55		/* SCSI IRQ */
#define LANCERR		0x56		/* LANC ERR */
#define LANCIRQ		0x57		/* LANC IRQ */
#define TIMER2IRQ	0x58		/* Tick Timer 2 vec */
#define TIMER1IRQ	0x59		/* Tick Timer 1 vec */
#define GPIOIRQ		0x5A		/* GPIO IRQ */
#define SRXEXIRQ	0x5C		/* Serial RX Exception IRQ */
#define SRMIRQ		0x5D		/* Serial Modem IRQ */
#define STXIRQ		0x5E		/* Serial TX IRQ */
#define SRXIRQ		0x5F		/* Serial RX IRQ */
#endif /* 0 */

#endif /* _MACHINE_BOARD_H */
