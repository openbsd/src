/*	$OpenBSD: tparm.c,v 1.6 1997/12/16 03:10:05 millert Exp $	*/

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
static char rcsid[] = "$OpenBSD: tparm.c,v 1.6 1997/12/16 03:10:05 millert Exp $";
#endif

#include <stdio.h>
#include <stdlib.h>
#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif
#include <string.h>
#include <ctype.h>

#ifdef MAX
#undef MAX
#endif

#define	MAX(a, b)	((a) < (b) ? (b) : (a))

#define	STKSIZ		32
#define	MAXRETURNSIZE	256

static __inline void push __P((int));
static __inline int popnum __P((void));
static __inline char *popstr __P((void));

static char *_tparm __P((const char *, char *, size_t, va_list));

static union {
    unsigned int	num;
    char		*str;
} stack[STKSIZ];

static int stackidx;

static __inline void
push(value)
     int value;
{
    if (stackidx < STKSIZ)
	stack[stackidx++].num = value;
}

static __inline int
popnum()
{
    return stackidx > 0 ? stack[--stackidx].num : 0;
}

static __inline char *
popstr()
{
    return stackidx > 0 ? stack[--stackidx].str : NULL;
}

/*
 * This function is identical to the GNU tparam() but we don't
 * advertise it as such since it confuses emacs (and perhaps others).
 */
static char *
_tparm(str, buf, siz, ap)
     const char *str;
     char *buf;
     size_t siz;
     va_list ap;
{
    int param[10], variable[26];
    int pops, num, i, level;
    char scratch[64];
    char *bufp, len;
    const char *p;

    if (str == NULL)
	return NULL;

    for (p = str, pops = 0, num = 0; *p != '\0'; p++)
	if (*p == '%' && *(p + 1) != '\0') {
	    switch (p[1]) {
		case '%':
		    p++;
		    break;
		case 'i':
		    if (pops < 2)
			pops = 2;
		    break;
		case 'p':
		    p++;
		    if (isdigit(p[1])) {
			int n = p[1] - '0';

			if (n > pops)
			    pops = n;
		    }
		    break;
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
		case 'c':
		case 'd':
		case 's':
		    num++;
		    break;
	    }
	}

    for (i = 0; i < MAX(pops, num); i++)
	param[i] = va_arg(ap, int);	/* XXX  arg size might be different than int */

    stackidx = 0;
    bufp = buf;

    while (*str) {
	if (*str != '%') {
	    if (bufp >= buf + siz)
		return(NULL);
	    *bufp++ = *str;
	}
	else {
	    switch (*++str) {
		case '%':
		    if (bufp >= buf + siz)
			return(NULL);
		    *bufp++ = '%';
		    break;
		case 'd':
		    sprintf(scratch, "%d", popnum());
		    if (bufp + strlen(scratch) >= buf + siz)
			return(NULL);
		    strcpy(bufp, scratch);
		    bufp += strlen(bufp);
		    break;
		case '0':
		    len = *++str;
		    if (len == '2' || len == '3') {
			if (*++str == 'd') {
			    if (len == '2')
				sprintf(scratch, "%02d", popnum());
			    else
				sprintf(scratch, "%03d", popnum());
			    if (bufp + strlen(scratch) >= buf + siz)
				return(NULL);
			    strcpy(bufp, scratch);
			    bufp += strlen(bufp);
			}
			else if (*str == 'x') {
			    if (len == '2')
				sprintf(scratch, "%02x", popnum());
			    else
				sprintf(scratch, "%03x", popnum());
			    if (bufp + strlen(scratch) >= buf + siz)
				return(NULL);
			    strcpy(bufp, scratch);
			    bufp += strlen(bufp);
			}
		    }
		    break;
		case '2':
		    if (*++str == 'd') {
			sprintf(scratch, "%2d", popnum());
			if (bufp + strlen(scratch) >= buf + siz)
			    return(NULL);
			strcpy(bufp, scratch);
			bufp += strlen(bufp);
		    }
		    else if (*str == 'x') {
			sprintf(scratch, "%2x", popnum());
			if (bufp + strlen(scratch) >= buf + siz)
			    return(NULL);
			strcpy(bufp, scratch);
			bufp += strlen(bufp);
		    }
		    break;
		case '3':
		    if (*++str == 'd') {
			sprintf(scratch, "%3d", popnum());
			if (bufp + strlen(scratch) >= buf + siz)
			    return(NULL);
			strcpy(bufp, scratch);
			bufp += strlen(bufp);
		    }
		    else if (*str == 'x') {
			sprintf(scratch, "%3x", popnum());
			if (bufp + strlen(scratch) >= buf + siz)
			    return(NULL);
			strcpy(bufp, scratch);
			bufp += strlen(bufp);
		    }
		    break;
		case 'c':
		    if (bufp >= buf + siz)
			return(NULL);
		    *bufp++ = (char)popnum();
		    break;
		case 's':
		    if (bufp + strlen(p = popstr()) >= buf + siz)
			return(NULL);
		    strcpy(bufp, p);
		    bufp += strlen(bufp);
		    break;
		case 'p':
		    str++;
		    if (*str != '0' && isdigit(*str))
			push(param[*str - '1']);
		    break;
		case 'P':
		    str++;
		    if (islower(*str))
			variable[*str - 'a'] = popnum();
		    break;
		case 'g':
		    str++;
		    if (islower(*str))
			push(variable[*str - 'a']);
		    break;
		case '\'':
		    push(*++str & 0xFF);
		    str++;
		    break;
		case '{':
		    num = 0;
		    str++;
		    while (isdigit(*str))
			num = num * 10 + (*str++ - '0');
		    push(num);
		    break;
		case '+':
		    push(popnum() + popnum());
		    break;
		case '-':
		    num = popnum();
		    push(popnum() - num);
		    break;
		case '*':
		    push(popnum() * popnum());
		    break;
		case '/':
		    num = popnum();
		    push(popnum() / num);
		    break;
		case 'm':
		    num = popnum();
		    push(popnum() % num);
		    break;
		case 'A':
		    num = popnum();
		    push(popnum() && num);
		    break;
		case 'O':
		    num = popnum();
		    push(popnum() || num);
		    break;
		case '&':
		    push(popnum() & popnum());
		    break;
		case '|':
		    push(popnum() | popnum());
		    break;
		case '^':
		    push(popnum() ^ popnum());
		    break;
		case '=':
		    push(popnum() == popnum());
		    break;
		case '<':
		    push(popnum() > popnum());
		    break;
		case '>':
		    push(popnum() < popnum());
		    break;
		case '!':
		    push(!popnum());
		    break;
		case '~':
		    push(~popnum());
		    break;
		case 'i':
		    param[0]++;
		    param[1]++;
		    break;
		case '?':
		    break;
		case 't':
		    if (!popnum()) {
			str++;
			level = 0;
			while (*str) {
			    if (*str == '%') {
				if (*++str == '?')
				    level++;
				else if (*str == ';') {
				    if (level > 0)
					level--;
				    else
					break;
				}
				else if (*str == 'e' && level == 0)
				    break;
			    }
			    if (*str)
				str++;
			}
		    }
		    break;
		case 'e':
		    str++;
		    level = 0;
		    while (*str) {
			if (*str == '%') {
			    if (*++str == '?')
				level++;
			    else if (*str == ';') {
				if (level > 0)
				    level--;
				else
				    break;
			    }
			}
			if (*str)
			    str++;
		    }
		    break;
		case ';':
		    break;
		default:
		    break;
	    }
	}
	if (*str != '\0')
	    str++;
    }

    if (bufp >= buf + siz)
	return(NULL);
    *bufp = '\0';
    return(buf);
}

char *
#ifdef __STDC__
tparm(const char *str, ...)
#else
tparm(va_alist)
     va_dcl
#endif
{
    static char buf[MAXRETURNSIZE];
    va_list ap;
    char *p;
#ifndef __STDC__
    const char *str;

    va_start(ap);
    str = va_arg(ap, const char *);
#else
    /* LINTED pointer casts may be troublesome */
    va_start(ap, str);
#endif
    p = _tparm(str, buf, sizeof(buf), ap);
    /* LINTED expression has no effect */
    va_end(ap);
    return(p);
}
