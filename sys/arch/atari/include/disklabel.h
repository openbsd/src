/*	$NetBSD: disklabel.h,v 1.4 1995/11/30 00:58:03 jtc Exp $	*/

/*
 * Copyright (c) 1995 Leo Weppelman.
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
 *      This product includes software developed by Leo Weppelman.
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
 */

#ifndef _MACHINE_DISKLABEL_H_
#define _MACHINE_DISKLABEL_H_

/*
 * On a volume, exclusively used by NetBSD, the boot block starts at
 * sector 0. To allow shared use of a volume between two or more OS's
 * the vendor specific AHDI format is supported. In this case the boot
 * block is located at the start of an AHDI partition. In any case the
 * size of the boot block is 8KB, the disk label is at offset 7KB.
 */
#define LABELSECTOR	0		/* `natural' start of boot block   */
#define LABELOFFSET	(7 * 1024)	/* offset of disklabel in bytes,
					   relative to start of boot block */
#define MAXPARTITIONS	16
#define RAW_PART	2		/* xx?c is raw partition	   */

#define MAX_TOS_ROOTS	61		/* max. # of auxilary root sectors */

struct cpu_disklabel {
	u_int32_t	cd_bblock;	/* start of NetBSD boot block      */
#define NO_BOOT_BLOCK	((u_int32_t) -1)
	u_int32_t	cd_bslst;	/* start of TOS bad sector list    */
	u_int32_t	cd_bslsize;	/* size of TOS bad sector list     */
	u_int32_t	cd_roots[MAX_TOS_ROOTS]; /* TOS root sectors       */
};

#endif /* _MACHINE_DISKLABEL_H_ */
