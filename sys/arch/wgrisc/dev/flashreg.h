/*	$OpenBSD: flashreg.h,v 1.1 1997/02/23 21:59:28 pefo Exp $ */

/*
 * Copyright (c) 1997 Per Fogelstrom
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
 *	This product includes software developed under OpenBSD by
 *	Per Fogelstrom for Willowglen Singapore.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/*
 *	Definitions for the Samsung KM29N16000 2M/8 NAND Flash Memory.
 */

/* Commands */

#define	FL_SEQDI	0x80	/* Sequential data input	*/
#define	FL_READ1	0x00	/* Read 1 (normal read		*/
#define	FL_READ2	0x50	/* Read 2 (extended read	*/
#define	FL_READID	0x90	/* Read chip id			*/
#define	FL_RESET	0xff	/* Chip control reset		*/
#define	FL_PGPROG	0x10	/* Page program			*/
#define	FL_BLERASE	0x60	/* Block erase			*/
#define	FL_SUERASE	0xb0	/* Suspend erase		*/
#define	FL_REERASE	0xd0	/* Resume erase			*/
#define	FL_READSTAT	0x70	/* Read status			*/
#define	FL_READREG	0xe0	/* Read register		*/

/* Status */
#define	FLST_ERROR	0x01	/* Error in Program/Erase	*/
#define	FLST_ESUSP	0x20	/* Erase suspended		*/
#define	FLST_RDY	0x40	/* Ready			*/
#define	FLST_UNPROT	0x80	/* Memory not wr protected	*/

/* Handy macros */
#define	FLC_ALE		0x80	/* Control reg ALE bit		*/
#define	FLC_CLE		0x40	/* Control reg CLE bit		*/
#define	FLC_NCS		0x3f	/* CS bits			*/

#define WAIT_FL_RDY						\
	while((inb(RISC_STATUS) & 0x10) == 0)

#define OUT_FL_CTRL(x, cs)					\
	do {							\
	    outb(RISC_FLASH_CTRL, (x) | (FLC_NCS ^ (cs)));	\
	    wbflush();						\
	} while(0)

#define OUT_FL_DATA(x)						\
	outb(RISC_FLASH_WRITE, (x))

#define IN_FL_DATA						\
	inb(RISC_FLASH_READ)

#define	OUT_FL_CLE(cmd, cs)					\
	do {							\
	    OUT_FL_CTRL(FLC_CLE, (cs));				\
	    OUT_FL_DATA(cmd);					\
	    OUT_FL_CTRL(0, cs);					\
	} while(0);

#define OUT_FL_ALE1(addr, cs)					\
	do {							\
	    register int _a = addr;				\
	    OUT_FL_CTRL(FLC_ALE, (cs));				\
	    OUT_FL_DATA(_a);					\
	    OUT_FL_CTRL(0, cs);					\
	} while(0);

#define OUT_FL_ALE2(addr, cs)					\
	do {							\
	    register int _a = addr;				\
	    OUT_FL_CTRL(FLC_ALE, (cs));				\
	    OUT_FL_DATA(_a >> 8);				\
	    OUT_FL_DATA(_a >> 16);				\
	    OUT_FL_CTRL(0, cs);					\
	} while(0);

#define OUT_FL_ALE3(addr, cs)					\
	do {							\
	    register int _a = addr;				\
	    OUT_FL_CTRL(FLC_ALE, (cs));				\
	    OUT_FL_DATA(_a);					\
	    OUT_FL_DATA(_a >> 8);				\
	    OUT_FL_DATA(_a >> 16);				\
	    OUT_FL_CTRL(0, cs);					\
	} while(0);

