/* Return text for a given pair of sw1/sw2 status bytes */
static char *rcsid = "$Id: r1r2.c,v 1.1 2001/05/22 15:35:58 rees Exp $";

#ifdef __palmos__
#define NULL 0
#define printf palmprintf
#else
#include <stdio.h>
#endif

#include "sectok.h"

struct r1r2s {
    int r1, r2;
    char *s;
} r1r2s[] = {
    {0x90, 0x00, "ok"},
    {0x61, 0xff, "ok; response available %x"},
    {0x62, 0x34, "no such method"},
    {0x62, 0x39, "out of memory"},
    {0x62, 0x55, "null pointer"},
    {0x62, 0x57, "array index out of bounds"},
    {0x62, 0x58, "index out of bounds"},
    {0x62, 0x81, "rec is corrupt"},
    {0x62, 0x83, "invalid file"},
    {0x63, 0x00, "auth failed"},
    {0x63, 0x81, "invalid key"},
    {0x67, 0xff, "invalid length; should be %x"},
    {0x69, 0x80, "bad param"},
    {0x69, 0x82, "unreadable"},
    {0x69, 0x83, "auth method blocked"},
    {0x69, 0x84, "data invalid"},
    {0x69, 0x85, "no file selected"},
    {0x69, 0x87, "busy/SM missing"},
    {0x69, 0x88, "SM wrong"},
    {0x6a, 0x80, "invalid file type"},
    {0x6a, 0x81, "function not supported"},
    {0x6a, 0x82, "file not found"},
    {0x6a, 0x83, "no such rec"},
    {0x6b, 0x00, "wrong mode"},
    {0x6c, 0xff, "wrong length; should be %x"},
    {0x6d, 0x00, "unknown instruction"},
    {0x6e, 0x00, "wrong class"},
    {0x6f, 0x14, "invalid applet state"},
    {0x6f, 0x15, "invalid state"},
    {0x6f, 0x19, "applet already running"},
    {0x6f, 0xb0, "uninitialized key"},
    {0x94, 0x81, "bad state"},
    {0x00, 0x00, NULL}
};

#ifdef TEST
main(int ac, char *av[])
{
    int r1, r2;
    char *s;

    if (ac != 3) {
	fprintf(stderr, "usage: %s sw1 sw2 (in hex, please)\n", av[0]);
	exit(1);
    }
    sscanf(av[1], "%x", &r1);
    sscanf(av[2], "%x", &r2);
    print_r1r2(r1, r2);
    exit(0);
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
    char *s;
    static char buf[64];

    s = scr1r2s(r1, r2);
    if (s)
	sprintf(buf, "%02x %02x %s", r1, r2, s);
    else
	sprintf(buf, "%02x %02x", r1, r2);
    return buf;
}

char *
scr1r2s(int r1, int r2)
{
    int i;
    char *s;
    static char buf[64];

    for (i = 0; r1r2s[i].s; i++)
	if (r1r2s[i].r1 == r1 && (r1r2s[i].r2 == r2 || r1r2s[i].r2 == 0xff))
	    break;
    if (r1r2s[i].r2 != 0xff)
	return r1r2s[i].s;
    sprintf(buf, r1r2s[i].s, r2);
    return buf;
}

#ifndef __palmos__
int
fdump_reply(FILE *f, unsigned char *p, int n, int r1, int r2)
{
    int i;

    for (i = 0; i < n; i++)
	fprintf(f, "%d:%x ", i + 1, p[i]);
    if (n)
	fprintf(f, "\n");
    if (r1)
	fprintf(f, "%s\n", get_r1r2s(r1, r2));
    return n;
}

int
dump_reply(unsigned char *p, int n, int r1, int r2)
{
    fdump_reply(stdout, p, n, r1, r2);
}
#endif

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
