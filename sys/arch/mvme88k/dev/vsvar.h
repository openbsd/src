/*	$OpenBSD: vsvar.h,v 1.10 2004/05/21 10:24:42 miod Exp $ */
/*
 * Copyright (c) 1999 Steve Murphree, Jr.
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Van Jacobson of Lawrence Berkeley Laboratory.
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
 */
#ifndef _VSVAR_H_
#define _VSVAR_H_

/*
 * The largest single request will be MAXPHYS bytes which will require
 * at most MAXPHYS/NBPG+1 chain elements to describe, i.e. if none of
 * the buffer pages are physically contiguous (MAXPHYS/NBPG) and the
 * buffer is not page aligned (+1).
 */
#define	DMAMAXIO	(MAXPHYS/NBPG+1)
#define  LO(x) (u_short)((unsigned long)x & 0x0000FFFF)
#define  HI(x) (u_short)((unsigned long)x >> 16)
#define  OFF(x) (u_short)((vaddr_t)x - (vaddr_t)sc->sc_vsreg)

/****************     Scater/Gather Stuff                *******************/

typedef struct {
	union {
		unsigned short bytes :16;
   #define MAX_SG_BLOCK_SIZE	(1<<16)	/* the size *has* to be always *smaller* */
		struct {
			unsigned short :8;
			unsigned short gather :8;
		} scatter;
	} count;
	LONGV           address;
	unsigned short  link :1;
	unsigned short  :3;
	unsigned short  transfer_type :2;
	/* 				0x0 is reserved */
   #define SHORT_TREANSFER 		0x1
   #define LONG_TRANSFER			0x2
   #define SCATTER_GATTER_LIST_IN_SHORT_IO	0x3
	unsigned short  memory_type :2;
   #define NORMAL_TYPE			0x0
   #define BLOCK_MODE			0x1
	/*				0x2 is reserved */
	/*				0x3 is reserved */
	unsigned short  address_modifier :8;
} sg_list_element_t;

typedef sg_list_element_t * scatter_gather_list_t;

#define MAX_SG_ELEMENTS 64

struct m328_sg {
	struct m328_sg  *up;
	int                     elements;
	int                     level;
	struct m328_sg  *down[MAX_SG_ELEMENTS];
	sg_list_element_t list[MAX_SG_ELEMENTS];
};

typedef struct m328_sg *M328_SG;

typedef struct {
   struct scsi_xfer  *xs;
   M328_SG           top_sg_list;
} M328_CMD;

/**************** END Scater/Gather Stuff                *******************/

struct  vs_softc {
	struct  device sc_dev;
	struct  intrhand sc_ih_e;
	struct  intrhand sc_ih_n;
	struct  evcnt sc_intrcnt_e;
	struct  evcnt sc_intrcnt_n;
	u_short  sc_ipl;
	u_short  sc_evec;
	u_short  sc_nvec;
	struct  scsi_link sc_link;	/* proto for sub devices */
	struct   vsreg  *sc_vsreg;
};

/* sync states */
#define SYNC_START	0	/* no sync handshake started */
#define SYNC_SENT	1	/* we sent sync request, no answer yet */
#define SYNC_DONE	2	/* target accepted our (or inferior) settings,
				   or it rejected the request and we stay async */

#define IOPB_SCSI    0x20
#define IOPB_RESET   0x22
#define IOPB_INIT    0x41
#define IOPB_WQINIT  0x42
#define IOPB_DEV_RESET   0x4D

#define OPT_INTEN    0x0001
#define OPT_INTDIS   0x0000
#define OPT_SG       0x0002
#define OPT_SST      0x0004
#define OPT_SIT      0x0040
#define OPT_READ     0x0000
#define OPT_WRITE    0x0100

#define AM_S32       0x01
#define AM_S16       0x05
#define AM_16        0x0100
#define AM_32        0x0200
#define AM_SHORT     0x0300
#define AM_NORMAL    0x0000
#define AM_BLOCK     0x0400
#define AM_D64BLOCK  0x0C00

#define WQO_AE             0x0001   /* abort enable bit */
#define WQO_FOE            0x0004   /* freeze on error */
#define WQO_PE             0x0008   /* parity enable bit */
#define WQO_ARE            0x0010   /* autosense recovery enable bit */
#define WQO_RFWQ           0x0020   /* report frozen work queue bit */
#define WQO_INIT           0x8000   /* work queue init bit */

#endif /* _M328VAR_H */
