/*	$NetBSD: podule.h,v 1.2 1996/03/14 23:11:31 mark Exp $	*/

/*
 * Copyright (C) 1994 Wolfgang Solfrank.
 * Copyright (C) 1994 TooLs GmbH.
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

#ifndef	_ARM32_PODULE_H_
#define	_ARM32_PODULE_H_

/*
 * Generic defines for Acorn Podules
 */
#define	IO_SPACE_PHYS	0x03000000		/* physical I/O space location */
#define	IO_SPACE_VIRT	0xf8000000		/* virtual I/O space location */
#define	IO_SPACE_SIZE	0x01000000		/* size of I/O space */

#define	PODULE_SLOW_MEM	((volatile void *)(IO_SPACE_VIRT + 0x240000))
#define	PODULE_MED_MEM	((volatile void *)(IO_SPACE_VIRT + 0x2c0000))
#define	PODULE_FAST_MEM	((volatile void *)(IO_SPACE_VIRT + 0x340000))
#define	PODULE_SYNC_MEM	((volatile void *)(IO_SPACE_VIRT + 0x3c0000))

#define	PODULE_SLOT2MEM(slot,mem)	((slot)*0x4000 + (mem))

/*
 * Access 8 bit wide memory on podule
 */
#define	PODULE_GET_BYTE(sp,off)		(((volatile u_char *)(sp))[(off) * 4])
#define	PODULE_SET_BYTE(sp,off,byte)	(((volatile u_int *)(sp))[(off)] \
					 = ((u_char)(byte) << 24) \
					   | ((u_char)(byte) << 16) \
					   | ((u_char)(byte) << 8) \
					   | (u_char)(byte))
/*
 * Access 16 bit wide memory on podule
 */
#define	PODULE_GET_SHORT(sp,off)	(((volatile u_short *)(sp))[(off)])
#define	PODULE_SET_SHORT(sp,off,word)	(((volatile u_int *)(sp))[(off) / 2] \
					 = ((u_short)(word) << 16) \
					   | (u_short)(word))

/*
 * Known offsets in any podule
 */
#define	PODULE_IRQ_BYTE		0
#define	PODULE_IRQ_PEND		1
#define	PODULE_FIQ_PEND		2

#define	PODULE_ID_BYTE		3
#define	PODULE_ID_SCSI		2
#define	PODULE_ID_ETHER		3

extern void podule_get_bytes __P((volatile void *sp, void *dp, int cnt));
extern void podule_set_bytes __P((void *sp, volatile void *dp, int cnt));

extern void podule_get_shorts __P((volatile void *sp, void *dp, int cnt));
extern void podule_set_shorts __P((void *sp, volatile void *dp, int cnt));

/* XXX Shouldn't this be somewhere else? */
#define	offsetof(type, member)	((size_t)&((type *)0)->member)

#endif	/* _ARM_PODULE_H_ */
