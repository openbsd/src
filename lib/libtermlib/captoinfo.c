/*	$OpenBSD: captoinfo.c,v 1.2 1996/06/02 23:47:01 tholo Exp $	*/

/*
 * Copyright (c) 1996 SigmaSoft, Th. Lockert <tholo@sigmasoft.com>
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by SigmaSoft, Th.  Lockert.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef lint
static char rcsid[] = "$OpenBSD: captoinfo.c,v 1.2 1996/06/02 23:47:01 tholo Exp $";
#endif

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "term.private.h"

#define	STKSIZ	64

static int	 stack[STKSIZ];
static int	 stkidx;
static int	 nflag;
static int	 rflag;
static int	 param;
static int	 onstack;
static char	*d;

static void
pop()
{
    if (stkidx > 0)
	onstack = stack[--stkidx];
    else
	onstack = 0;
    param++;
}

static void
push()
{
    if (stkidx < STKSIZ)
	stack[stkidx++] = onstack;
}

static void
getparam(p, n)
    int p;
    int n;
{
    if (rflag)
	if (p == 1)
	    p++;
	else if (p == 2)
	    p--;
    if (onstack == p) {
	if (n > 1) {
	    *d++ = '%';
	    *d++ = 'P';
	    *d++ = 'a';
	    while (n--) {
		*d++ = '%';
		*d++ = 'g';
		*d++ = 'a';
	    }
	}
	return;
    }
    if (onstack)
	push();
    onstack = p;
    while (n--) {
	*d++ = '%';
	*d++ = 'p';
	*d++ = '0' + p;
    }
    if (nflag && p < 3) {
	*d++ = '%';
	*d++ = '{';
	*d++ = '9';
	*d++ = '6';
	*d++ = '}';
	*d++ = '%';
	*d++ = '^';
    }
}

static int
cvtchar(p)
    const char *p;
{
    unsigned char ch = 0;
    int len;

    switch (*p) {
	case '\\':
	    switch (*++p) {
		case '\'':
		case '$':
		case '\\':
		case '%':
		    ch = *p;
		    len = 2;
		    break;
		case '\0':
		    ch = '\\';
		    len = 1;
		    break;
		case '0':
		case '1':
		case '2':
		case '3':
		    len = 1;
		    while (isdigit(*p)) {
			ch = ch * 8 + (*p++ - '0');
			len++;
		    }
		    break;
		default:
		    ch = *p;
		    len = 2;
		    break;
	    }
	    break;
	case '^':
	    ch = (*++p & 0x1F);
	    len = 2;
	    break;
	default:
	    ch = *p;
	    len = 1;
	    break;
    }

    if (isgraph(ch) && ch != ',' && ch != '\'' && ch != '\\' && ch != ':') {
	*d++ = '%';
	*d++ = '\'';
	*d++ = ch;
	*d++ = '\'';
    }
    else {
	*d++ = '%';
	*d++ = '{';
	if (ch > 99)
	    *d++ = ch / 100 + '0';
	if (ch > 9)
	    *d++ = ((int)ch / 10) % 10 + '0';
	*d++ = ch % 10 + '0';
	*d++ = '}';
    }
    return len;
}

char *
_ti_captoinfo(cap)
    const char *cap;
{
    char ch, new[4096];
    const char *cost;

    if (cap == NULL)
	return NULL;
    stkidx = 0;
    onstack = 0;
    nflag = 0;
    rflag = 0;
    param = 1;
    d = new;
    cost = NULL;
    if (isdigit(*cap))
	for (cost = cap; isdigit(*cap) || *cap == '*' || *cap == '.'; cap++)
	    ;

    while (*cap) {
	switch (*cap) {
	    case '%':
		cap++;
		switch (ch = *cap++) {
		    case '%':
			*d++ = '%';
			break;
		    case 'r':
			rflag++;
			break;
		    case 'n':
			nflag++;
			break;
		    case 'i':
			*d++ = '%';
			*d++ = 'i';
			break;
		    case '6':
		    case 'B':
			getparam(param, 2);
			*d++ = '%';
			*d++ = '{';
			*d++ = '6';
			*d++ = '}';
			*d++ = '%';
			*d++ = '*';
			*d++ = '%';
			*d++ = '+';
			break;
		    case '8':
		    case 'D':
			getparam(param, 2);
			*d++ = '%';
			*d++ = '{';
			*d++ = '2';
			*d++ = '}';
			*d++ = '%';
			*d++ = '*';
			*d++ = '%';
			*d++ = '-';
			break;
		    case '>':
			getparam(param, 2);
			*d++ = '%';
			*d++ = '?';
			cap += cvtchar(cap);
			*d++ = '%';
			*d++ = '>';
			*d++ = '%';
			*d++ = 't';
			cap += cvtchar(cap);
			*d++ = '%';
			*d++ = '+';
			*d++ = '%';
			*d++ = ';';
			break;
		    case 'a':
			getparam(param, 1);
			cap += cvtchar(cap);
			*d++ = '%';
			*d++ = '+';
			break;
		    case 'd':
		    case 's':
			getparam(param, 1);
			*d++ = '%';
			*d++ = ch;
			pop();
			break;
		    case '+':
		    case '-':
			getparam(param, 1);
			cap += cvtchar(cap);
			*d++ = '%';
			*d++ = ch;
			*d++ = '%';
			*d++ = 'c';
			pop();
			break;
		    case '.':
			getparam(param, 1);
			*d++ = '%';
			*d++ = 'c';
			pop();
			break;
		    case '2':
		    case '3':
			getparam(param, 1);
			*d++ = '%';
			*d++ = ch;
			*d++ = 'd';
			pop();
			break;
		    case '\\':
			*d++ = '%';
			*d++ = '\\';
			break;
		    default:
			*d++ = '%';
			cap--;
			break;
		}
		break;
	    default:
		*d++ = *cap++;
		break;
	}
    }

    if (cost) {
	*d++ = '$';
	*d++ = '<';
	for (cap = cost;; cap++)
	    if (isdigit(*cap) || *cap == '*' || *cap == '.')
		*d++ = *cap;
	    else
		break;
	*d++ = '/';
	*d++ = '>';
    }

    *d = '\0';

    return strdup(new);
}
