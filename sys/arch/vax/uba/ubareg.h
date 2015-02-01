/*	$OpenBSD: ubareg.h,v 1.15 2015/02/01 15:27:12 miod Exp $ */
/*	$NetBSD: ubareg.h,v 1.11 2000/01/24 02:40:36 matt Exp $ */

/*-
 * Copyright (c) 1982, 1986 The Regents of the University of California.
 * All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 *	@(#)ubareg.h	7.8 (Berkeley) 5/9/91
 */

/*
 * VAX UNIBUS adapter registers
 */

/*
 * Size of unibus memory address space in pages
 * (also number of map registers).
 */
#define UBAPAGES	496
#define UBAIOADDR	0760000	 /* start of I/O page */
#define UBAIOPAGES	16
#define	UBAIOSIZE	(UBAIOPAGES * VAX_NBPG)	/* 8K I/O space */

/* Some Qbus-specific defines */
#define	QBAPAGES	8192
#define	QBASIZE		(QBAPAGES * VAX_NBPG)
#define	QBAMAP		0x20088000
#define	QIOPAGE		0x20000000

/*
 * Q-bus control registers
 */
#define QIPCR		0x1f40		/* from start of iopage */
/* bits in QIPCR */
#define Q_DBIRQ		0x0001		/* doorbell interrupt request */
#define Q_LMEAE		0x0020		/* local mem external access enable */
#define Q_DBIIE		0x0040		/* doorbell interrupt enable */
#define Q_AUXHLT	0x0100		/* auxiliary processor halt */
#define Q_DMAQPE	0x8000		/* Q22 bus address space parity error */
