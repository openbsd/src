/* $Id: input.c,v 1.8 2001/08/02 17:02:05 rees Exp $ */

/*
copyright 2001
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
 * turn text into hex in a flexible way
 *
 * Jim Rees, University of Michigan, July 2000
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
#include <string.h>
#endif
#include <ctype.h>

#include "sectok.h"

#ifdef TEST
main(ac, av)
int ac;
char *av[];
{
    int i, n;
    unsigned char obuf[256];

    while (1) {
	n = sectok_get_input(stdin, obuf, 1, sizeof obuf);
	if (!n)
	    break;
	for (i = 0; i < n; i++)
	    printf("%02x ", obuf[i]);
	printf("\n");
    }
    exit(0);
}
#endif

#ifndef __palmos__
int
sectok_get_input(FILE *f, unsigned char *obuf, int omin, int olen)
{
    int n = 0;
    char ibuf[1024];

    while (n < omin && fgets(ibuf, sizeof ibuf, f) != NULL)
	n += sectok_parse_input(ibuf, obuf + n, olen - n);
    return n;
}
#endif

int
sectok_parse_input(char *ibuf, unsigned char *obuf, int olen)
{
    char *cp;
    unsigned char *up;
    int its_hex, nhex, ntext, n, ndig;

    if (!strncmp(ibuf, "0x", 2)) {
	/* If it starts with '0x' it's hex */
	ibuf += 2;
	its_hex = 1;
    } else if (ibuf[0] == '\"') {
	/* If it starts with " it's text */
	ibuf++;
	its_hex = 0;
    } else {
	/* Count hex and non-hex characters */
	nhex = ntext = 0;
	for (cp = ibuf; *cp; cp++) {
	    if (isxdigit(*cp))
		nhex++;
	    if (!isspace(*cp) && *cp != '.')
		ntext++;
	}

	/*
	 * 1. Two characters is always text (scfs file names, for example)
	 * 2. Any non-space, non-hexdigit chars means it's text
	 */
	if (ntext == 2 || ntext > nhex)
	    its_hex = 0;
	else
	    its_hex = 1;
    }

    if (its_hex) {
	for (cp = ibuf, up = obuf, n = ndig = 0; *cp && (up - obuf < olen); cp++) {
	    if (isxdigit(*cp)) {
		n <<= 4;
		n += isdigit(*cp) ? (*cp - '0') : ((*cp & 0x5f) - 'A' + 10);
	    }
	    if (ndig >= 1) {
		*up++ = n;
		n = 0;
		ndig = 0;
	    } else if (isxdigit(*cp))
		ndig++;
	}
    } else {
	/* It's text */
	for (cp = ibuf, up = obuf; *cp && (up - obuf < olen); cp++) {
	    if (isprint(*cp))
		*up++ = *cp;
	}
    }

    return (up - obuf);
}

void
sectok_parse_fname(char *buf, unsigned char *fid)
{
    fid[1] = 0;

    if (buf[0] == '/' || sectok_parse_input(buf, fid, 2) < 1) {
	/* root */
	fid[0] = 0x3f;
    }
}
