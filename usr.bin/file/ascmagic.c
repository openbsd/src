/*	$OpenBSD: ascmagic.c,v 1.6 2003/03/11 21:26:26 ian Exp $	*/

/*
 * ASCII magic -- file types that we know based on keywords
 * that can appear anywhere in the file.
 *
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
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <err.h>
#include "file.h"
#include "names.h"

#ifndef	lint
static char *moduleid = "$OpenBSD: ascmagic.c,v 1.6 2003/03/11 21:26:26 ian Exp $";
#endif	/* lint */

			/* an optimisation over plain strcmp() */
#define	STREQ(a, b)	(*(a) == *(b) && strcmp((a), (b)) == 0)

int
ascmagic(buf, nbytes)
unsigned char *buf;
int nbytes;	/* size actually read */
{
	int i, has_escapes = 0;
	unsigned char *s;
	char nbuf[HOWMANY+1];	/* one extra for terminating '\0' */
	char *token;
	struct names *p;

	/*
	 * Do the tar test first, because if the first file in the tar
	 * archive starts with a dot, we can confuse it with an nroff file.
	 */
	switch (is_tar(buf, nbytes)) {
	case 1:
		ckfputs("tar archive", stdout);
		return 1;
	case 2:
		ckfputs("POSIX tar archive", stdout);
		return 1;
	}

	/*
	 * for troff, look for . + letter + letter or .\";
	 * this must be done to disambiguate tar archives' ./file
	 * and other trash from real troff input.
	 */
	if (*buf == '.') {
		unsigned char *tp = buf + 1;

		while (isascii(*tp) && isspace(*tp))
			++tp;	/* skip leading whitespace */
		if ((isascii(*tp) && (isalnum(*tp) || *tp=='\\') &&
		    isascii(tp[1]) && (isalnum(tp[1]) || tp[1] == '"'))) {
			ckfputs("troff or preprocessor input text", stdout);
			return 1;
		}
	}
	if ((*buf == 'c' || *buf == 'C') && 
	    isascii(buf[1]) && isspace(buf[1])) {
		ckfputs("fortran program text", stdout);
		return 1;
	}


	/* Make sure we are dealing with ascii text before looking for tokens */
	for (i = 0; i < nbytes; i++) {
		if (!isascii(buf[i]))
			return 0;	/* not all ASCII */
	}

	/* look for tokens from names.h - this is expensive! */
	/* make a copy of the buffer here because strtok() will destroy it */
	s = (unsigned char*) memcpy(nbuf, buf, nbytes);
	s[nbytes] = '\0';
	has_escapes = (memchr(s, '\033', nbytes) != NULL);
	while ((token = strtok((char *) s, " \t\n\r\f")) != NULL) {
		s = NULL;	/* make strtok() keep on tokin' */
		for (p = names; p < names + NNAMES; p++) {
			if (STREQ(p->name, token)) {
				ckfputs(types[p->type], stdout);
				if (has_escapes)
					ckfputs(" (with escape sequences)", 
						stdout);
				return 1;
			}
		}
	}

	/* all else fails, but it is ASCII... */
	ckfputs("ASCII text", stdout);
	if (has_escapes) {
		ckfputs(" (with escape sequences)", stdout);
	}
	return 1;
}


