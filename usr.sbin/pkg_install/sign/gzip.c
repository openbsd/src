/* $OpenBSD: gzip.c,v 1.2 1999/10/01 01:14:38 espie Exp $ */
/*-
 * Copyright (c) 1999 Marc Espie.
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
 *	This product includes software developed by Marc Espie for the OpenBSD
 * Project.
 *
 * THIS SOFTWARE IS PROVIDED BY THE OPENBSD PROJECT AND CONTRIBUTORS 
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR 
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OPENBSD
 * PROJECT OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <string.h>
#include "stand.h"
#include "gzip.h"
#include "pgp.h"

/* For now, signatures follow a hardcoded format
   (endianess was chosen to conform to gzip header format)
 */
static char tagsign[] = 
	{'S', 'i', 'g', 'P', 'G', 'P', 
	    (char)(SIGNSIZE /256), (char)(SIGNSIZE & 255) };

/* retrieve a gzip header, including PGP signatures */
int 
gzip_read_header(f, h, sign)
	FILE *f;
	struct mygzip_header *h;
	char sign[];
{
	{
		int c, d;

		c = fgetc(f);
		d = fgetc(f);
		if ((unsigned char)c != (unsigned char)GZIP_MAGIC0 
			 || (unsigned char)d != (unsigned char)GZIP_MAGIC1)	
			return GZIP_NOT_GZIP;
	}
	{
		int method, flags;
		
		method = fgetc(f);
		flags = fgetc(f);

		if (method == EOF || flags == EOF || fread(h->stamp, 1, 6, f) != 6)
			return GZIP_NOT_GZIP;
		h->method = (char)method;
		h->flags = (char)flags;
	}

	if ((h->flags & CONTINUATION) != 0)
		if (fread(h->part, 1, 2, f) != 2)
			return GZIP_NOT_GZIP;
	if ((h->flags & EXTRA_FIELD) != 0) {
		char match[sizeof(tagsign)];
		unsigned int len;
		int c;

		c = fgetc(f);
		if (c == EOF)
			return GZIP_NOT_PGPSIGNED;
		len = (unsigned)c;
		c = fgetc(f);
		if (c == EOF)
			return GZIP_NOT_PGPSIGNED;
		len |= ((unsigned) c) << 8;
		if (len != sizeof(tagsign) + SIGNSIZE)
			return GZIP_NOT_PGPSIGNED;
		if (fread(match, 1, sizeof(match), f) != sizeof(match) ||
			memcmp(match, tagsign, sizeof(match)) != 0)
			return GZIP_NOT_PGPSIGNED;
		if (sign != NULL) {
			if (fread(sign, 1, SIGNSIZE, f) == SIGNSIZE)
				return GZIP_SIGNED;
			else
				return GZIP_NOT_PGPSIGNED;
		} else {
			if (fseek(f, SIGNSIZE, SEEK_CUR) != -1)
				return GZIP_SIGNED;
			else
				return GZIP_NOT_PGPSIGNED;
		}
	} else
		return GZIP_UNSIGNED;
}

/* write a gzip header, including PGP signature */
int 
gzip_write_header(f, h, sign)
	FILE *f;
	const struct mygzip_header *h;
	const char sign[];
{
	char flags;

	flags = h->flags;

	if (sign != NULL)
		flags |= EXTRA_FIELD;
	else
		flags &= ~EXTRA_FIELD;
	if (fputc(GZIP_MAGIC0, f) == EOF ||
	    fputc(GZIP_MAGIC1, f) == EOF ||
	    fputc(h->method, f) == EOF ||
	    fputc(flags, f) == EOF || 
		 fwrite(h->stamp, 1, 6, f) != 6)
		 return 0;
	if ((h->flags & CONTINUATION) != 0)
		if (fwrite(h->part, 1, 2, f) != 2)
			return 0;
	if (sign != NULL) {
		unsigned short len = sizeof(tagsign) + SIGNSIZE;
		if (fputc(len & 255, f) == EOF ||
			fputc(len/256, f) == EOF ||
			fwrite(tagsign, 1, sizeof(tagsign), f) != sizeof(tagsign) ||
		    fwrite(sign, 1, SIGNSIZE, f) != SIGNSIZE)
			 return 0;
	}
	return 1;
}
