/*	$OpenBSD: param.h,v 1.36 2004/04/26 12:34:05 miod Exp $ */
/*
 * Copyright (c) 1999 Steve Murphree, Jr.
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1982, 1986, 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 * from: Utah $Hdr: machparam.h 1.11 89/08/14$
 *
 *	@(#)param.h	7.8 (Berkeley) 6/28/91
 */

#ifndef _MACHINE_PARAM_H_
#define _MACHINE_PARAM_H_

#define  _MACHINE       mvme88k
#define  MACHINE        "mvme88k"

#include <m88k/param.h>

/*
 * The Bug uses the bottom 64KB. The kernel will allocate PTEs to map this
 * space, but the kernel must be linked with a start address past these 64KB.
 */
#define KERNBASE	0x00000000	/* start of kernel virtual */
#define	KERNTEXTOFF	0x00010000	/* start of kernel text */

/*
 * IO space
 */

#define	IOMAP_SIZE	0x01000000	/* roughly 0xff000000 - 0xffffffff */

#ifdef   _KERNEL
extern int brdtyp;
extern int cpumod;
#endif

/*
 * Values for the brdtyp variable.
 */
#define BRD_187		0x187
#define BRD_188		0x188
#define BRD_197		0x197
#define BRD_8120	0x8120

/*
 * Values for the cpumod variable.
 */
#define MOD_LE		0x01
#define MOD_SP		0x02
#define MOD_DP		0x03

#endif /* !_MACHINE_PARAM_H_ */
