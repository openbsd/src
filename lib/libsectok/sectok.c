/* $Id: sectok.c,v 1.2 2001/06/27 22:33:36 rees Exp $ */

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
 * common card routines
 *
 * Jim Rees
 * University of Michigan CITI, July 2001
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#include "sectok.h"

#define myisprint(x) ((x) >= '!' && (x) <= 'z')

unsigned char root_fid[] = {0x3f, 0x00};

void
sectok_fmt_fid(char *fname, int f0, int f1)
{
    if (myisprint(f0) && myisprint(f1))
	sprintf(fname, "%c%c", f0, f1);
    else
	sprintf(fname, "%02x%02x", f0, f1);
}

int
sectok_selectfile(int fd, int cla, unsigned char *fid, int verbose)
{
    int n, r1, r2, code;
    unsigned char obuf[256];
    char fname[6];

    n = scrw(fd, cla, 0xa4, 0, 0, 2, fid, sizeof obuf, obuf, &r1, &r2);
    if (n < 0) {
	printf("selectfile: scwrite failed\n");
	return -2;
    }
    if (r1 == 0x90 || r1 == 0x61)
	code = 0;
    else if (r1 == 0x6a && r2 == 0x82)
	/* file not found */
	code = -1;
    else
	code = -2;
    if (verbose && n > 0)
	dump_reply(obuf, n, 0, 0);
    if (verbose || code == -2) {
	sectok_fmt_fid(fname, fid[0], fid[1]);
	printf("%s: %s\n", fname, get_r1r2s(r1, r2));
    }
    return code;
}
