/*	$OpenBSD: fmt.c,v 1.9 2008/03/24 20:32:04 jasper Exp $	*/

/*-
 * Copyright (c) 1990, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/user.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/proc.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <vis.h>
#include "ps.h"

void
fmt_puts(const char *s, int *leftp)
{
	static char *v = NULL, *nv;
	static int maxlen = 0;
	int len;

	if (*leftp == 0)
		return;
	len = strlen(s) * 4 + 1;
	if (len > maxlen) {
		size_t newmaxlen = maxlen;

		if (newmaxlen == 0)
			newmaxlen = getpagesize();
		while (len > newmaxlen)
			newmaxlen *= 2;
		nv = realloc(v, newmaxlen);
		if (nv == 0) {
			free(v);
			v = NULL;
			maxlen = 0;
			return;
		}
		maxlen = newmaxlen;
		v = nv;
	}
	strvis(v, s, VIS_TAB | VIS_NL | VIS_CSTYLE);
	if (*leftp != -1) {
		len = strlen(v);
		if (len > *leftp) {
			v[*leftp] = '\0';
			*leftp = 0;
		} else
			*leftp -= len;
	}
	printf("%s", v);
}

void
fmt_putc(int c, int *leftp)
{

	if (*leftp == 0)
		return;
	if (*leftp != -1)
		*leftp -= 1;
	putchar(c);
}
