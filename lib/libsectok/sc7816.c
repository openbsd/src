/* $Id: sc7816.c,v 1.9 2003/04/02 22:57:51 deraadt Exp $ */

/*
copyright 2000
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

/*
 * sc7816 library for use with pc/sc ifd drivers
 *
 * Jim Rees
 * Mukesh Agrawal
 * University of Michigan CITI, August 2000
 */

#ifdef __palmos__
#include <Common.h>
#include <System/SysAll.h>
#include <System/Unix/unix_stdlib.h>
#include <System/Unix/unix_string.h>
#include <UI/UIAll.h>
#include "field.h"
#else
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#endif

#ifdef SCPERF
#define SCPERF_FIRST_APPEARANCE
#endif /* SCPERF */

#include "sectok.h"
#include "sc7816.h"

char *scerrtab[] = {
    "ok",
    "no such tty",
    "out of memory",
    "timeout",
    "slag!",
    "card type not supported",
    "no card in reader",
    "not implemented",
    "error loading driver",
    "communications error",
    "reader not open",
    "unknown error",
};

int
scxopen(int rn, int flags, int *ep, char *config_path, char *driver_path)
{
    int r, sw;

    flags ^= STONOWAIT;

    r = sectok_xopen(rn, flags, config_path, driver_path, &sw);

    if (ep) {
	if (sectok_r1(sw) == 0x06)
	    *ep = sectok_r2(sw);
	else
	    *ep = SCECOMM;
    }

    return r;
}

int
scopen(int rn, int flags, int *ep)
{
    return scxopen(rn, flags, ep, NULL, NULL);
}

int
scclose(int fd)
{
    return sectok_close(fd);
}

int
sccardpresent(int fd)
{
    return sectok_cardpresent(fd);
}

int
scxreset(int fd, int flags, unsigned char *atr, int *ep)
{
    int r, sw;

    r = sectok_reset(fd, flags, atr, &sw);

    if (ep) {
	if (sectok_swOK(sw))
	    *ep = SCEOK;
	else if (sectok_r1(sw) == 0x06)
	    *ep = sectok_r2(sw);
	else
	    *ep = SCESLAG;
    }

    return r;
}

int
screset(int fd, unsigned char *atr, int *ep)
{
    return scxreset(fd, 0, atr, ep);
}

int
scrw(int fd, int cla, int ins, int p1, int p2, int ilen, unsigned char *ibuf, int olen, unsigned char *obuf, int *sw1p, int *sw2p)
{
    int n, sw;

    n = sectok_apdu(fd, cla, ins, p1, p2, ilen, ibuf, olen, obuf, &sw);
    *sw1p = sectok_r1(sw);
    *sw2p = sectok_r2(sw);
    return n;
}

int
scwrite(int fd, int cla, int ins, int p1, int p2, int p3, unsigned char *buf, int *sw1p, int *sw2p)
{
    int rv;
#ifdef SCPERF
    char *scperf_buf;

    scperf_buf = malloc (64);

    snprintf (scperf_buf, 64, "scwrite (ins %02x, p3 %02x) start", ins, p3);
    SetTime(scperf_buf);
#endif /* SCPERF */
    rv = scrw(fd, cla, ins, p1, p2, p3, buf, 0, NULL, sw1p, sw2p);

#ifdef SCPERF
    SetTime("scwrite() end");
#endif /* SCPERF */
    return (rv >= 0) ? p3 : -1;
}

int
scread(int fd, int cla, int ins, int p1, int p2, int p3, unsigned char *buf, int *sw1p, int *sw2p)
{
    int rv;
#ifdef SCPERF
    char *scperf_buf;

    scperf_buf = malloc (64);

    snprintf (scperf_buf, 64, "scread (ins %02x, p3 %02x) start", ins, p3);
    SetTime(scperf_buf);
#endif /* SCPERF */
    rv = scrw(fd, cla, ins, p1, p2, 0, NULL, p3, buf, sw1p, sw2p);

#ifdef SCPERF
    SetTime("scread() end");
#endif /* SCPERF */
    return rv;
}

#ifndef __palmos__
int
parse_input(char *ibuf, unsigned char *obuf, int olen)
{
    return sectok_parse_input(ibuf, obuf, olen);
}

int
get_input(FILE *f, unsigned char *obuf, int omin, int olen)
{
    return sectok_get_input(f, obuf, omin, olen);
}
#endif

void
print_r1r2(int r1, int r2)
{
    printf("%s\n", get_r1r2s(r1, r2));
}

char *
get_r1r2s(int r1, int r2)
{
    return sectok_get_sw(sectok_mksw(r1, r2));
}

#ifndef __palmos__
int
fdump_reply(FILE *f, unsigned char *p, int n, int r1, int r2)
{
    return sectok_fdump_reply(f, p, n, sectok_mksw(r1, r2));
}

int
dump_reply(unsigned char *p, int n, int r1, int r2)
{
    return sectok_fdump_reply(stdout, p, n, sectok_mksw(r1, r2));
}
#endif
