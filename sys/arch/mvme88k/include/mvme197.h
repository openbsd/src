/*	$OpenBSD: mvme197.h,v 1.10 2009/02/16 23:03:33 miod Exp $ */
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
#ifndef __MACHINE_MVME197_H__
#define __MACHINE_MVME197_H__

#define	FLASH_START	0xff800000	/* start of flash memory area */
#define	FLASH_SIZE	0x00400000
#define BUG197_START	0xfff80000	/* start of BUG PROM (in OBIO) */
#define BUG197_SIZE	0x00040000
#define OBIO197_START	0xfff00000	/* start of local IO */
#define OBIO197_SIZE	0x000d0000	/* size of obio space */

#define M197_ILEVEL	0xfff00064	/* interrupt priority level */
#define M197_IMASK	0xfff00065	/* interrupt mask level */
#define M197_ISRC	0xfff0006f	/* interrupt SRC */
#define M197_IACK	0xfff00100	/* interrupt ACK base */

#ifdef _KERNEL
void	m197_broadcast_complex_ipi(int, u_int32_t, u_int32_t);
void	m197_send_complex_ipi(int, cpuid_t, u_int32_t, u_int32_t);
void	m197_send_ipi(int, cpuid_t);
#endif

#endif	/* __MACHINE_MVME197_H__ */
