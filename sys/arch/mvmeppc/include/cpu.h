/*	$OpenBSD: cpu.h,v 1.8 2004/01/29 21:28:56 miod Exp $	*/
/*	$NetBSD: cpu.h,v 1.1 1996/09/30 16:34:21 ws Exp $	*/

/*
 * Copyright (C) 1995, 1996 Wolfgang Solfrank.
 * Copyright (C) 1995, 1996 TooLs GmbH.
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef	_MACHINE_CPU_H_
#define	_MACHINE_CPU_H_

#include <powerpc/cpu.h>

void install_extint(void (*)(void));
void nvram_map(void);

/*
 * CPU Configuration registers (in ISA space)
 */

#define	MVME_CPUCONF_REG	0x0800
#define	MVME_FEATURE_REG	0x0802
#define	MVME_STATUS_REG		0x0803
#define	MVME_SEVENSEG_REG	0x08c0

/* feature bits */
#define	MVME_FEATURE_SCC	0x40
#define	MVME_FEATURE_PMC2	0x20
#define	MVME_FEATURE_PMC1	0x10
#define	MVME_FEATURE_VME	0x08
#define	MVME_FEATURE_GFX	0x04
#define	MVME_FEATURE_LAN	0x02
#define	MVME_FEATURE_SCSI	0x01

/* status values */
#define	MVMETYPE_RESERVED	0xfa
#define	MVMETYPE_2600_712	0xfb
#define	MVMETYPE_2600_761	0xfc
#define	MVMETYPE_3600_712	0xfd
#define	MVMETYPE_3600_761	0xfe
#define	MVMETYPE_1600		0xff

#endif	/* _MACHINE_CPU_H_ */
