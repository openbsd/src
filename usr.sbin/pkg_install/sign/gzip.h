/* $OpenBSD: gzip.h,v 1.1 1999/09/27 21:40:04 espie Exp $ */
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

#define GZIP_MAGIC0	'\037'
#define GZIP_MAGIC1	'\213'
/* flags values */
#define CONTINUATION	0x02
#define EXTRA_FIELD 	0x04

/* meaningful fields in a gzip header, see gzip proper for details.
   This structure should not be fiddled with outside of gzip_read_header
   and gzip_write_header 
 */
struct mygzip_header
	{
	char method;
	char flags;
	char stamp[6];
	char part[2];
	};
	
/* returns from gzip_read_header */
#define GZIP_UNSIGNED 		0	/* gzip file, no signature */
#define GZIP_SIGNED 		1	/* gzip file, signature parsed ok */
#define GZIP_NOT_GZIP 		2	/* not a proper gzip file */
#define GZIP_NOT_PGPSIGNED 	3	/* gzip file, unknown extension */
extern int gzip_read_header 
	__P((FILE *f, /*@out@*/struct mygzip_header *h, /*@null@*/char sign[]));
/* gzip_write_header returns 1 for success */
extern int gzip_write_header
	__P((FILE *f, const struct mygzip_header *h, /*@null@*/const char sign[]));

#ifndef GZCAT
#define GZCAT "/usr/bin/gzcat"
#endif

