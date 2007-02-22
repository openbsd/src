/*	$OpenBSD: buf.h,v 1.22 2007/02/22 06:42:09 otto Exp $	*/
/*
 * Copyright (c) 2003 Jean-Francois Brousseau <jfb@openbsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL  DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef BUF_H
#define BUF_H

#include <sys/types.h>

/* flags */
#define BUF_AUTOEXT	1	/* autoextend on append */

typedef struct cvs_buf BUF;

BUF		*cvs_buf_alloc(size_t, u_int);
BUF		*cvs_buf_load(const char *, u_int);
BUF		*cvs_buf_load_fd(int, u_int);
void		 cvs_buf_free(BUF *);
u_char		*cvs_buf_release(BUF *);
u_char		 cvs_buf_getc(BUF *, size_t);
void		 cvs_buf_empty(BUF *);
ssize_t		 cvs_buf_append(BUF *, const void *, size_t);
ssize_t		 cvs_buf_fappend(BUF *, const char *, ...)
		     __attribute__((format(printf, 2, 3)));
void		 cvs_buf_putc(BUF *, int);
size_t		 cvs_buf_len(BUF *);
int		 cvs_buf_write_fd(BUF *, int);
int		 cvs_buf_write(BUF *, const char *, mode_t);
int		 cvs_buf_differ(const BUF *, const BUF *);
void		 cvs_buf_write_stmp(BUF *, char *, struct timeval *);

ssize_t		 cvs_buf_copy(BUF *, size_t, void *, size_t);
const u_char	*cvs_buf_peek(BUF *, size_t);

#define cvs_buf_get(b)	cvs_buf_peek(b, 0)

#endif	/* BUF_H */
