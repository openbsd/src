/*	$OpenBSD: internat.c,v 1.3 2003/03/11 21:26:26 ian Exp $	*/

/*
 * Copyright (c) Ian F. Darwin 1986-1995.
 * Software written by Ian F. Darwin and others;
 * maintained 1995-present by Christos Zoulas and others.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by Ian F. Darwin and others.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *  
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#include "file.h"

#define F 0
#define T 1

/*
 * List of characters that look "reasonable" in international
 * language texts.  That's almost all characters :), except a
 * few in the control range of ASCII (all the known international
 * charactersets share the bottom half with ASCII).
 */
static char maybe_internat[256] = {
	F, F, F, F, F, F, F, F, T, T, T, T, T, T, F, F,  /* 0x0X */
	F, F, F, F, F, F, F, F, F, F, F, T, F, F, F, F,  /* 0x1X */
	T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T,  /* 0x2X */
	T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T,  /* 0x3X */
	T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T,  /* 0x4X */
	T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T,  /* 0x5X */
	T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T,  /* 0x6X */
	T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, F,  /* 0x7X */
	T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T,  /* 0x8X */
	T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T,  /* 0x9X */
	T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T,  /* 0xaX */
	T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T,  /* 0xbX */
	T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T,  /* 0xcX */
	T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T,  /* 0xdX */
	T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T,  /* 0xeX */
	T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T   /* 0xfX */
};

/* Maximal length of a line we consider "reasonable". */
#define MAXLINELEN 300

int
internatmagic(buf, nbytes)
	unsigned char *buf;
	int nbytes;
{
	int i;
	unsigned char *cp;

	nbytes--;

	/* First, look whether there are "unreasonable" characters. */
	for (i = 0, cp = buf; i < nbytes; i++, cp++)
		if (!maybe_internat[*cp])
			return 0;

	/*
	 * Now, look whether the file consists of lines of
	 * "reasonable" length.
	 */

	for (i = 0; i < nbytes;) {
		cp = memchr(buf, '\n', nbytes - i);
		if (cp == NULL) {
			/* Don't fail if we hit the end of buffer. */
			if (i + MAXLINELEN >= nbytes)
				break;
			else
				return 0;
		}
		if (cp - buf > MAXLINELEN)
			return 0;
		i += (cp - buf + 1);
		buf = cp + 1;
	}
	ckfputs("International language text", stdout);
	return 1;
}
