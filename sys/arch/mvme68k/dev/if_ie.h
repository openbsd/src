/*	$OpenBSD: if_ie.h,v 1.5 2003/06/02 05:09:14 deraadt Exp $ */

/* Copyright (c) 1998 Steve Murphree, Jr. 
 * Copyright (c) 1995 Theo de Raadt
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

/*
 * XXX where else is this from?
 * if_sunie.h
 *
 * sun's ie interface
 */

/*
 * programming notes:
 *
 * the ie chip operates in a 24 bit address space.
 *
 * most ie interfaces appear to be divided into two parts:
 *	 - generic 586 stuff
 *	 - board specific
 *
 * generic:
 *	the generic stuff of the ie chip is all done with data structures
 * 	that live in the chip's memory address space.   the chip expects
 * 	its main data structure (the sys conf ptr -- SCP) to be at a
 * 	address loaded into the chip at init.
 *
 *      the SCP points to another structure called the ISCP.
 *      the ISCP points to another structure called the SCB.
 * 	the SCB has a status field, a linked list of "commands", and
 * 	a linked list of "receive buffers".   these are data structures that
 * 	live in memory, not registers.
 *
 * board:
 * 	to get the chip to do anything, you first put a command in the
 * 	command data structure list.   then you have to signal "attention"
 * 	to the chip to get it to look at the command.   how you
 * 	signal attention depends on what board you have... on PC's
 * 	there is an i/o port number to do this, on sun's there is a
 * 	register bit you toggle.
 *
 * 	to get data from the chip you program it to interrupt...
 *
 * 	the VME boards lives in vme16 space.   only 16 and 8 bit accesses
 * 	are allowed, so functions that copy data must be aware of this.
 *
 * 	the chip is an intel chip.  this means that the byte order
 * 	on all the "short"s in the chip's data structures is wrong.
 * 	so, constants described in the intel docs are swapped for the sun.
 * 	that means that any buffer pointers you give the chip must be
 * 	swapped to intel format.   yuck.
 *
 */

/*
 * PART 1: VME/multibus defs
 */
#define IEVME_PAGESIZE 1024	/* bytes */
#define IEVME_PAGSHIFT 10	/* bits */
#define IEVME_NPAGES   256	/* number of pages on chip */
#define IEVME_MAPSZ    1024	/* number of entries in the map */

/*
 * PTE for the page map
 */
#define IEVME_SBORDR 0x8000	/* sun byte order */
#define IEVME_IBORDR 0x0000	/* intel byte ordr */

#define IEVME_P2MEM  0x2000	/* memory is on P2 */
#define IEVME_OBMEM  0x0000	/* memory is on board */

#define IEVME_PGMASK 0x0fff	/* gives the physical page frame number */

struct ievme {
	u_short pgmap[IEVME_MAPSZ];
	u_short xxx[32];	/* prom */
	u_short status;		/* see below for bits */
	u_short xxx2;		/* filler */
	u_short pectrl;		/* parity control (see below) */
	u_short peaddr;		/* low 16 bits of address */
};

/*
 * status bits
 */
#define IEVME_RESET 0x8000	/* reset board */
#define IEVME_ONAIR 0x4000	/* go out of loopback 'on-air' */
#define IEVME_ATTEN 0x2000	/* attention */
#define IEVME_IENAB 0x1000	/* interrupt enable */
#define IEVME_PEINT 0x0800	/* parity error interrupt enable */
#define IEVME_PERR  0x0200	/* parity error flag */
#define IEVME_INT   0x0100	/* interrupt flag */
#define IEVME_P2EN  0x0020	/* enable p2 bus */
#define IEVME_256K  0x0010	/* 256kb rams */
#define IEVME_HADDR 0x000f	/* mask for bits 17-20 of address */

/*
 * parity control
 */
#define IEVME_PARACK 0x0100	/* parity error ack */
#define IEVME_PARSRC 0x0080	/* parity error source */
#define IEVME_PAREND 0x0040	/* which end of the data got the error */
#define IEVME_PARADR 0x000f	/* mask to get bits 17-20 of parity address */


/*
 * PART 2: the on-board interface
 */
struct ieob {
	u_short	porthigh;
	u_short	portlow;
	u_long	attn;
};
#define IE_PORT_NEWSCPADDR	0x00000002
#define	IE_PORT_RESET		0x00000000

#define IEOB_ADBASE 0xff000000  /* KVA base addr of 24 bit address space */

/*
 * PART 3: the 3E board
 */

/*
 * not supported (yet?)
 */

