/* $Id: r1r2.c,v 1.9 2003/04/02 22:57:51 deraadt Exp $ */

/*
copyright 1999
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
 * Return text for a given pair of sw1/sw2 status bytes
 */

#ifdef __palmos__
#include <Common.h>
#include <System/SysAll.h>
#include <System/Unix/sys_types.h>
#include <System/Unix/unix_stdlib.h>
#include <System/Unix/unix_string.h>
#include <UI/UIAll.h>
#include "field.h"
#else
#include <stdio.h>
#endif

#include "sectok.h"

static char *scsws(int sw);

static struct r1r2s {
    int sw;
    char *s;
} r1r2s[] = {
    {0x9000, "ok"},

    /* sectok errors */
    {0x0601, "no such tty"},
    {0x0602, "out of memory"},
    {0x0603, "timeout"},
    {0x0604, "slag!"},
    {0x0605, "card type not supported"},
    {0x0606, "no card in reader"},
    {0x0607, "not implemented"},
    {0x0608, "error loading driver"},
    {0x0609, "communications error"},
    {0x060a, "reader not open"},
    {0x060c, "config conflict"},
    {0x060d, "unknown error"},

    /* card errors */
    {0x61ff, "ok; response available %x"},
    {0x6234, "no such method"},
    {0x6239, "out of memory"},
    {0x6255, "null pointer"},
    {0x6257, "array index out of bounds"},
    {0x6258, "index out of bounds"},
    {0x6281, "rec is corrupt"},
    {0x6283, "invalid file"},
    {0x6300, "auth failed"},
    {0x6381, "invalid key"},
    {0x67ff, "invalid length; should be %x"},
    {0x6980, "bad param"},
    {0x6982, "permission denied"},
    {0x6983, "auth method blocked"},
    {0x6984, "data invalid"},
    {0x6985, "no file selected"},
    {0x6987, "busy/SM missing"},
    {0x6988, "SM wrong"},
    {0x6a80, "invalid file type"},
    {0x6a81, "function not supported"},
    {0x6a82, "file not found"},
    {0x6a83, "no such rec"},
    {0x6b00, "wrong mode"},
    {0x6cff, "wrong length; should be %x"},
    {0x6d00, "unknown instruction"},
    {0x6e00, "wrong class"},
    {0x6f14, "invalid applet state"},
    {0x6f15, "invalid state"},
    {0x6f19, "applet already running"},
    {0x6fb0, "uninitialized key"},
    {0x9481, "bad state"},
    {0x0000, NULL}
};

#ifdef TEST
main(int ac, char *av[])
{
    int sw;
    char *s;

    if (ac != 2) {
	fprintf(stderr, "usage: %s sw (in hex, please)\n", av[0]);
	exit(1);
    }
    sscanf(av[1], "%x", &sw);
    sectok_print_sw(sw);
    exit(0);
}
#endif

void sectok_print_sw(int sw)
{
    printf("%s\n", sectok_get_sw(sw));
}

char *sectok_get_sw(int sw)
{
    char *s;
    static char buf[64];

    s = scsws(sw);
    if (s)
	snprintf(buf, sizeof buf, "%04x %s", sw, s);
    else
	snprintf(buf, sizeof buf, "%04x", sw);
    return buf;
}

static char *scsws(int sw)
{
    int i, r1 = sectok_r1(sw), r2 = sectok_r2(sw), tr1, tr2;
    static char buf[64];

    for (i = 0; r1r2s[i].s; i++) {
	tr1 = sectok_r1(r1r2s[i].sw);
	tr2 = sectok_r2(r1r2s[i].sw);
	if (tr1 == r1 && (tr2 == r2 || tr2 == 0xff))
	    break;
    }

    if (sectok_r2(r1r2s[i].sw) != 0xff)
	return r1r2s[i].s;
    snprintf(buf, sizeof buf, r1r2s[i].s, r2);
    return buf;
}

#ifndef __palmos__
int
sectok_fdump_reply(FILE *f, unsigned char *p, int n, int sw)
{
    int i;

    for (i = 0; i < n; i++)
	fprintf(f, "%d:%x ", i + 1, p[i]);
    if (n)
	fprintf(f, "\n");
    if (sw)
	fprintf(f, "%s\n", sectok_get_sw(sw));
    return n;
}

int
sectok_dump_reply(unsigned char *p, int n, int sw)
{
    return sectok_fdump_reply(stdout, p, n, sw);
}
#else
int
sectok_dump_reply(unsigned char *p, int n, int sw)
{
    int i;

    hidefield(printfield->id);
    for (i = 0; i < n; i++)
	palmprintf("%d:%x ", i + 1, p[i]);
    if (n)
	palmprintf("\n");
    if (sw)
	palmprintf("%s\n", sectok_get_sw(sw));
    showfield(printfield->id);
    return n;
}
#endif
