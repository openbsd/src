/*
 * OS independent part
 *
 * Jim Rees, University of Michigan, October 1997
 */
static char *rcsid = "$Id: scrw.c,v 1.1 2001/05/22 15:35:57 rees Exp $";

#ifdef __palmos__
#include <Common.h>
#include <System/SysAll.h>
#include <System/MemoryMgr.h>
#include <System/Unix/unix_stdlib.h>
#include <System/Unix/sys_socket.h>
#else
#include <stdio.h>
#endif
#include <string.h>
#ifdef SCPERF 
#define SCPERF_FIRST_APPEARANCE
#endif /* SCPERF */
#include "todos_scrw.h"

/* Global interface bytes */
#define TA1 (tpb[0][0])
#define TB1 (tpb[0][1])
#define TC1 (tpb[0][2])
#define TD1 (tpb[0][3])
#define TC2 (tpb[1][2])
#define TA2 (tpb[1][0])

/* external variable */
#ifdef BYTECOUNT
extern int num_getc, num_putc;
#endif /* BYTECOUNT */

struct scparam scparam[4];

#ifndef NO_T_EQ_1
int todos_scioT1(int ttyn, int cla, int ins, int p1, int p2, int ilen, unsigned char *ibuf, int olen, unsigned char *obuf, int *sw1p, int *sw2p);
#endif

/* reset the card, and return answer to reset (atr) */

int
todos_scxreset(int ttyn, int flags, unsigned char *atr, int *ep)
{
    unsigned char *ap, buf[33];
    int n, err;

    if (ep)
	*ep = SCEOK;
    else
	ep = &err;

    if (!todos_sccardpresent(ttyn)) {
	*ep = SCENOCARD;
	return 0;
    }

    if (!atr)
	atr = buf;

    if (!(flags & SCRTODOS) && (todos_scsetflags(ttyn, 0, 0) & SCOXDTR))
	flags |= SCRTODOS;

    todos_scsetspeed(ttyn, 9600);
    todos_scsetflags(ttyn, 0, SCOINVRT);

    if (todos_scdtr(ttyn, (flags & SCRTODOS)) < 0) {
	*ep = SCENOCARD;
	return 0;
    }

    /* 7816-3 sec 5.2 says >= 40000 clock cycles, ~=12 msec */
    todos_scsleep(20);

    todos_scdtr(ttyn, !(flags & SCRTODOS));

    n = todos_get_atr(ttyn, flags, atr, &scparam[ttyn]);
    if (!n && ep)
	*ep = SCESLAG;
    if (scparam[ttyn].t < 0 && ep)
	*ep = SCENOSUPP;

    return n;
}

static int
todos_scioproc(int ttyn, int io, unsigned char *cp)
{
    int code;

    /* Wait extra guard time if needed */
    if (!io && scparam[ttyn].n)
	todos_scsleep(((scparam[ttyn].n * scparam[ttyn].etu) + 999) / 1000);

    code = (!io ? todos_scputc(ttyn, *cp) : todos_scgetc(ttyn, cp, scparam[ttyn].cwt));

    return code;
}

/* This does the real work of transferring data to or from the card.  Since the ack
   handling is the same for both send and receive, we do it all here. */

static int
todos_scioT0(int ttyn, int io, int cla, int ins, int p1, int p2, int p3, unsigned char *buf, int *sw1p, int *sw2p)
{
    int n = 0, ack, ackxins;
    unsigned char *bp = buf, c;
#ifdef BYTECOUNT
    int tmp_num_getc, tmp_num_putc;
#endif /* BYTECOUNT */
	
#ifdef BYTECOUNT
    tmp_num_getc = num_getc;
    tmp_num_putc = num_putc;
#endif /* BYTECOUNT */

#ifdef SCPERF
    SetTime("SCIO starts");
#endif /* SCPERF */

#ifdef DEBUG
    printf("scioT0 %3s %02x %02x %02x %02x %02x\n", io ? "out" : "in", cla, ins, p1, p2, p3);
#endif
  
    c = cla; todos_scioproc(ttyn, 0, &c);
    c = ins; todos_scioproc(ttyn, 0, &c);
    c = p1;  todos_scioproc(ttyn, 0, &c);
    c = p2;  todos_scioproc(ttyn, 0, &c);
    c = p3;  todos_scioproc(ttyn, 0, &c);

#ifdef SCPERF
    SetTime("Finish sending APDU");
#endif /* SCPERF */

    while (1) {
	/* read ack byte; see 7816-3 8.2.2 */
	if (todos_scgetc(ttyn, &c, scparam[ttyn].cwt) != SCEOK) {
#ifdef DEBUG
	    printf("%d ms timeout reading ack\n", scparam[ttyn].cwt);
#endif
	    return -1;
	}
	ack = c;

	if (ack == 0x60) {
	    /* null (no-op but resets timer) */
#ifdef DEBUG
	    printf("got 0x60 (null) ack; reset timer\n");
#endif
	    continue;
	}

	if ((ack & 0xf0) == 0x60 || (ack & 0xf0) == 0x90) {
	    /* SW1; get SW2 and return */
	    *sw1p = ack;
	    if (todos_scgetc(ttyn, &c, scparam[ttyn].cwt) != SCEOK) {
#ifdef DEBUG
		printf("%d ms timeout reading sw2\n", scparam[ttyn].cwt);
#endif
		return -1;
	    }
	    *sw2p = (c & 0xff);
	    break;
	}

	/* we would set VPP here if the interface supported it */

	ackxins = (ack ^ ins) & 0xfe;
	if (ackxins == 0xfe) {
	    /* xfer next data byte */
	    if (todos_scioproc(ttyn, io, bp++) != SCEOK) {
#ifdef DEBUG
		printf("%d ms timeout reading next data byte\n", scparam[ttyn].cwt);
#endif
		return -1;
	    }
	    n++;

	} else if (ackxins == 0) {
	    /* xfer all remaining data bytes */
	    while (n < p3) {
		if (todos_scioproc(ttyn, io, bp++) != SCEOK) {
#ifdef DEBUG
		    printf("%d ms timeout reading all remaining data bytes\n", scparam[ttyn].cwt);
#endif
		    return -1;
		}
		n++;
	    }
#ifdef SCPERF
	    SetTime("Finish sending or receiving DATA");
#endif /* SCPERF */
	} else {
	    /* ?? unknown ack byte */
#ifdef DEBUG
	    printf("unknown ack %x\n", ack);
	    continue;
#else
	    return -1;
#endif
	}
    }

#ifdef SCPERF
    SetTime("Finish scio");
#endif /* SCPERF */

#ifdef BYTECOUNT
    tmp_num_putc = num_putc - tmp_num_putc;
    tmp_num_getc = num_getc - tmp_num_getc - tmp_num_putc;
    MESSAGE3("#getc=%d, #putc=%d\n", tmp_num_getc, tmp_num_putc); 
#endif /* BYTECOUNT */

    return n;
}

int
todos_scrw(int ttyn, int cla, int ins, int p1, int p2, int ilen, unsigned char *ibuf, int olen, unsigned char *obuf, int *sw1p, int *sw2p)
{
    int r;

    if (scparam[ttyn].t == 0) {
	if (ilen > 0 && ibuf) {
	    /* Send "in" data */
	    r = (todos_scioT0(ttyn, 0, cla, ins, p1, p2, ilen, ibuf, sw1p, sw2p) >= 0) ? 0 : -1;
	    if (r >= 0 && *sw1p == 0x61 && olen >= *sw2p && obuf) {
		/* Response available; get it */
		r = todos_scioT0(ttyn, 1, cla, 0xc0, 0, 0, *sw2p, obuf, sw1p, sw2p);
	    }
	} else {
	    /* Get "out" data */
	    r = todos_scioT0(ttyn, 1, cla, ins, p1, p2, olen, obuf, sw1p, sw2p);
	}
#ifndef NO_T_EQ_1
    } else if (scparam[ttyn].t == 1) {
	r = todos_scioT1(ttyn, cla, ins, p1, p2, ilen, ibuf, olen, obuf, sw1p, sw2p);
#endif
    } else
	r = -1;

    return r;
}

/*
copyright 1997, 1999, 2000
the regents of the university of michigan
all rights reserved

permission is granted to use, copy, create derivative works 
and redistribute this software and such derivative works 
for any purpose, so long as the name of the university of 
michigan is not used in any advertising or publicity 
pertaining to the use or distribution of this software 
without specific, written prior authorization.  if the 
above copyright notice or any other identification of the 
university of michigan is included in any copy of any 
portion of this software, then the disclaimer below must 
also be included.

this software is provided as is, without representation 
from the university of michigan as to its fitness for any 
purpose, and without warranty by the university of 
michigan of any kind, either express or implied, including 
without limitation the implied warranties of 
merchantability and fitness for a particular purpose. the 
regents of the university of michigan shall not be liable 
for any damages, including special, indirect, incidental, or 
consequential damages, with respect to any claim arising 
out of or in connection with the use of the software, even 
if it has been or is hereafter advised of the possibility of 
such damages.
*/
