/*	$NetBSD: disklabel.h,v 1.3 1995/08/05 20:24:42 leo Exp $	*/

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

#define LABELSECTOR	0		/* start of boot block		*/
#define LABELOFFSET	(7 * 1024)	/* offset of disklabel in bytes,
					   relative to start of boot block */
#define MAXPARTITIONS	16
#define RAW_PART	 2		/* xx?c is raw partition	*/

#define MK_PARTID(x,y,z)	(   ((u_int32_t)(x) << 16)	\
				  | ((u_int32_t)(y) << 8)	\
				  | ((u_int32_t)(z))		\
				)
/*
 * Various `well known' AHDI partition identifiers.
 */
#define	CPU_PID_XGM	MK_PARTID('X','G','M')
#define	CPU_PID_BGM	MK_PARTID('B','G','M')
#define	CPU_PID_GEM	MK_PARTID('G','E','M')
#define	CPU_PID_RAW	MK_PARTID('R','A','W')
#define	CPU_PID_SWP	MK_PARTID('S','W','P')
#define	CPU_PID_NBD	MK_PARTID('N','B','D')
#define	CPU_PID_NBR	MK_PARTID('N','B','R')
#define	CPU_PID_NBS	MK_PARTID('N','B','S')
#define	CPU_PID_NBU	MK_PARTID('N','B','U')

struct cpu_partition {			/* AHDI partition descriptor:	*/
	u_int32_t	cp_id;		/* identifier (see above)	*/
	u_int32_t	cp_st;		/* start and size in		*/
	u_int32_t	cp_size;	/*  512 byte blocks		*/
};

struct cpu_disklabel {
   u_int32_t		cd_bslst;	/* start of AHDI bad sector list */
   u_int32_t		cd_bslsize;	/* size of AHDI bad sector list  */
   u_int32_t		cd_npartitions;	/* number of AHDI partitions     */
   struct cpu_partition	*cd_partitions;	/* list of AHDI partitions       */
   struct cpu_partition	*cd_labelpart;	/* AHDI partition with disklabel */
};

#endif /* _MACHINE_DISKLABEL_H_ */
