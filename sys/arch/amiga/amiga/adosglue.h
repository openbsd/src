/*	$NetBSD: adosglue.h,v 1.4 1994/12/28 09:27:45 chopps Exp $	*/

/*
 * Copyright (c) 1994 Christian E. Hopps
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
 *      This product includes software developed by Christian E. Hopps.
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
#ifndef _ADOSGLUE_H_
#define _ADOSGLUE_H_

/*
 * Dos types for identifying file systems
 * bsd file systems will be 'N','B',x,y where y is the fstype found in
 * disklabel.h (for DOST_DOS it will be the version number)
 */
#define DOST_XXXBSD	0x42534400	/* Old type back compat*/
#define DOST_NBR	0x4e425200	/* 'NBRx' Netbsd root partition */
#define DOST_NBS	0x4e425300	/* 'NBS0' Netbsd swap partition */
#define DOST_NBU	0x4e425500	/* 'NBUx' Netbsd user partition */
#define DOST_DOS	0x444f5300	/* 'DOSx' AmigaDos partition */
#define DOST_AMIX	0x554e4900	/* 'UNIx' AmigaDos partition */
#define DOST_MUFS	0x6d754600	/* 'muFx' AmigaDos partition (muFS) */

struct adostype {
	u_char archtype;	/* see ADT_xxx below */
	u_char fstype;		/* byte 3 from amiga dostype */
};

/* archtypes */
#define ADT_UNKNOWN	0
#define ADT_AMIGADOS	1
#define ADT_NETBSDROOT	2
#define ADT_NETBSDSWAP	3
#define ADT_NETBSDUSER	4
#define ADT_AMIX	5

#define ISFSARCH_NETBSD(adt) \
	((adt).archtype >= ADT_NETBSDROOT && (adt).archtype <= ADT_NETBSDUSER)

#endif /* _ADOSGLUE_H_ */
