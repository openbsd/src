/*	$OpenBSD: mvme1x7.h,v 1.11 2004/08/02 08:35:00 miod Exp $ */
/*
 * Copyright (c) 1996 Nivas Madhur
 * Copyright (c) 1999 Steve Murphree, Jr.
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
#ifndef __MACHINE_MVME1X7_H__
#define __MACHINE_MVME1X7_H__

#define OBIO_START	0xFFF00000	/* start of local IO */
#define OBIO_SIZE	0x000EFFFF	/* size of obio space */

#define INT_PRI_LEVEL	0xFFF4203E	/* interrupt priority level */
#define INT_MASK_LEVEL	0xFFF4203F	/* interrupt mask level */

#define LOCAL_IO_DEVS	0xFFF00000	/* local IO devices */

#define MEM_CTLR	0xFFF43000	/* MEMC040 mem controller */
#define SCC_ADDR	0xFFF45000 	/* Cirrus Chip */
#define LANCE_ADDR	0xFFF46000 	/* 82596CA */
#define SCSI_ADDR	0xFFF47000 	/* NCR 710 address */
#define NCR710_SIZE	0x00000040 	/* NCR 710 size */
#define MK48T08_ADDR	0xFFFC0000 	/* BBRAM, TOD */

#define TOD_CAL_CTL	0xFFFC1FF8 	/* calendar control register */
#define TOD_CAL_SEC	0xFFFC1FF9 	/* seconds */
#define TOD_CAL_MIN	0xFFFC1FFA 	/* minutes */
#define TOD_CAL_HOUR	0xFFFC1FFB 	/* hours */
#define TOD_CAL_DOW	0xFFFC1FFC 	/* Day Of the Week */
#define TOD_CAL_DAY	0xFFFC1FFD 	/* days */
#define TOD_CAL_MON	0xFFFC1FFE 	/* months */
#define TOD_CAL_YEAR	0xFFFC1FFF 	/* years */

#endif	/* __MACHINE_MVME1X7_H__ */
