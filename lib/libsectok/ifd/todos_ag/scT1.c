/*
 * T=1 protocol engine
 *
 * Jim Rees, University of Michigan, October 1997
 */
static char *rcsid = "$Id: scT1.c,v 1.1 2001/05/22 15:35:57 rees Exp $";

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
#ifdef TIMING
#include <sys/time.h>
#endif

#include "todos_scrw.h"

int
todos_scioT1(int ttyn, int cla, int ins, int p1, int p2, int ilen, unsigned char *ibuf, int olen, unsigned char *obuf, int *sw1p, int *sw2p)
{
    int i, len, n;
    unsigned char *bp, *obp, tbuf[256];

    /* Send 3 bytes prologue (7816-3 sec 9.4.1 fig 16) */

    bp = tbuf;
    len = 5 + ilen;

    /* Send APDU */

    *bp++ = cla;
    *bp++ = ins;
    *bp++ = p1;
    *bp++ = p2;
    *bp++ = ilen;

    /* Send data */

    for (i = 0; i < ilen; i++)
	*bp++ = ibuf[i];
#ifndef HUH
    if (ilen) {
	/* I don't understand this, but Visa card wants it */
	*bp++ = 0;
	len++;
    }
#endif

    obp = obuf ? obuf : tbuf;

    n = todos_scioT1Iblk(ttyn, len, tbuf, obp);

    if (n >= 2) {
	*sw1p = obp[n-2];
	*sw2p = obp[n-1];
	n -= 2;
    }
    return n;
}

int
todos_scioT1Iblk(int ttyn, int ilen, unsigned char *ibuf, unsigned char *obuf)
{
    int n;
    unsigned char tbuf[256];
    static unsigned char ssn;

    tbuf[0] = 0;
    tbuf[1] = ssn;
    ssn ^= 0x40;
    tbuf[2] = ilen;
    memcpy(&tbuf[3], ibuf, ilen);
    n = todos_scioT1pkt(ttyn, tbuf, tbuf);
    if (n < 0)
	return n;
    memcpy(obuf, &tbuf[3], tbuf[2]);
    return tbuf[2];
}

int
todos_scioT1pkt(int ttyn, unsigned char *ibuf, unsigned char *obuf)
{
    int i, len;
    unsigned char edc, *bp;

    len = ibuf[2] + 3;

    /* Calculate checksum */

    for (i = 0, edc = 0; i < len; i++)
	edc ^= ibuf[i];
    ibuf[len++] = edc;

    /* Wait BGT = 22 etu */

    todos_scsleep(scparam[ttyn].etu * 22 / 1000 + 1);

    /* Send the packet */

    todos_scputblk(ttyn, ibuf, len);

    /* Read return packet */

    bp = obuf;

    /* Read three byte header */
    for (i = 0; i < 3; i++) {
	if (todos_scgetc(ttyn, bp++, (i == 0) ? scparam[ttyn].bwt : scparam[ttyn].cwt) != SCEOK) {
#ifdef DEBUG
	    printf("T=1 header read timeout\n");
#endif
	    return -1;
	}
    }
    len = obuf[2];

    /* Read data and edc */
    for (i = 0; i < len + 1; i++) {
	if (todos_scgetc(ttyn, bp++, scparam[ttyn].cwt) != SCEOK) {
#ifdef DEBUG
	    printf("T=1 data read timeout\n");
#endif
	    return -1;
	}
    }

    /* Check edc */
    for (i = 0, edc = 0; i < len + 3; i++)
	edc ^= obuf[i];
#ifdef DEBUG
    if (edc != obuf[len + 3])
	printf("edc mismatch, %02x != %02x\n", edc, obuf[len + 3]);
#endif

    return len;
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
