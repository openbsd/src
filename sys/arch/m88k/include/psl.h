/*	$OpenBSD: psl.h,v 1.1 2004/04/26 12:34:05 miod Exp $ */
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

#ifndef __M88K_PSL_H__
#define __M88K_PSL_H__

/*
 * 88100 control registers
 */

/*
 * processor identification register (PID)
 */
#define PID_ARN		0x0000FF00U	/* architectural revision number */
#define PID_VN		0x000000FEU	/* version number */
#define PID_MC		0x00000001U	/* master/checker */

/*
 * processor status register
 */

#define PSR_MODE	0x80000000U	/* supervisor/user mode */
#define PSR_BO		0x40000000U	/* byte-ordering 0:big 1:little */
#define PSR_SER		0x20000000U	/* 88110 serial mode */
#define PSR_C		0x10000000U	/* carry */
#define PSR_SGN		0x04000000U	/* 88110 Signed Immediate mode */
#define PSR_SRM		0x02000000U	/* 88110 Serialize Memory */
#define PSR_TRACE	0x00800000U	/* 88110 hardware trace */
#define PSR_SFD		0x000003E0U	/* SFU disable */
#define PSR_SFD2	0x00000010U	/* 88110 SFU2 (Graphics) disable */
#define PSR_SFD1	0x00000008U	/* SFU1 (FPU) disable */
#define PSR_MXM		0x00000004U	/* misaligned access enable */
#define PSR_IND		0x00000002U	/* interrupt disable */
#define PSR_SFRZ	0x00000001U	/* shadow freeze */

#define PSR_SUPERVISOR	(PSR_MODE | PSR_SFD)
#define PSR_USER	(PSR_SFD)
#define PSR_SET_BY_USER	(PSR_BO | PSR_SER | PSR_C | PSR_MXM)

#define FIP_V		0x00000002U	/* valid */
#define FIP_E		0x00000001U	/* exception */
#define FIP_ADDR	0xFFFFFFFCU	/* address mask */
#define NIP_V		0x00000002U	/* valid */
#define NIP_E		0x00000001U	/* exception */
#define NIP_ADDR	0xFFFFFFFCU	/* address mask */
#define XIP_V		0x00000002U	/* valid */
#define XIP_E		0x00000001U	/* exception */
#define XIP_ADDR	0xFFFFFFFCU	/* address mask */

#endif /* __M88K_PSL_H__ */

