/*	$NetBSD: tospart.h,v 1.3 1995/11/30 00:58:05 jtc Exp $	*/

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

#ifndef _MACHINE_TOSPART_H
#define _MACHINE_TOSPART_H

#include <machine/disklabel.h>

#define	TOS_BSIZE	512		/* TOS blocksize	*/
#define	TOS_BBLOCK	0		/* TOS bootblock	*/

#define MK_PARTID(x,y,z)	(   ((u_int32_t)(x) << 16)	\
				  | ((u_int32_t)(y) << 8)	\
				  | ((u_int32_t)(z))		\
				)
/*
 * Various `well known' AHDI partition identifiers.
 */
#define	PID_XGM		MK_PARTID('X','G','M')
#define	PID_BGM		MK_PARTID('B','G','M')
#define	PID_GEM		MK_PARTID('G','E','M')
#define	PID_RAW		MK_PARTID('R','A','W')
#define	PID_SWP		MK_PARTID('S','W','P')
#define	PID_NBD		MK_PARTID('N','B','D')
#define	PID_NBR		MK_PARTID('N','B','R')
#define	PID_NBS		MK_PARTID('N','B','S')
#define	PID_NBU		MK_PARTID('N','B','U')

/*
 * Format of TOS boot block.
 */
#define	NTOS_PARTS	4		/* Max. # of entries in TOS bootblock */

struct tos_part {
	u_char		tp_flg;		/* bit 0 is in-use flag            */
	u_char		tp_id[3];	/* id: GEM, BGM, XGM, UNX, MIX     */
	u_int32_t	tp_st;		/* block where partition starts    */
	u_int32_t	tp_size;	/* partition size in blocks        */
};
struct tos_root {
	u_char		tr_fill[0x1c2];	/* filler, can be boot code        */
	u_int32_t	tr_hdsize;	/* size of entire volume in blocks */
	struct tos_part	tr_parts[NTOS_PARTS]; /* partition table           */
	u_int32_t	tr_bslst;	/* start of bad-sector list        */
	u_int32_t	tr_bslsize;	/* # of blocks in bad-sector list  */
};

/*
 * TOS partition table.
 */
#define MAX_TOS_PARTS	(MAX_TOS_ROOTS + 3) /* Max. # of TOS partitions    */

struct tos_table {
	struct cpu_disklabel	*tt_cdl;
	u_int32_t		tt_nroots;   /* # of auxilary root sectors */
	u_int32_t		tt_nparts;   /* # of TOS partitions        */
	struct tos_part		tt_parts[MAX_TOS_PARTS];
#define tt_roots	tt_cdl->cd_roots
#define tt_bblock	tt_cdl->cd_bblock
#define tt_bslst	tt_cdl->cd_bslst
#define tt_bslsize	tt_cdl->cd_bslsize
};

#endif /* _MACHINE_TOSPART_H */
