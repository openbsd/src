/*	$OpenBSD: buf.h,v 1.1 2006/04/26 02:55:13 joris Exp $	*/
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
 *
 * Buffer management
 * -----------------
 *
 * This code provides an API to generic memory buffer management.  All
 * operations are performed on a rcs_buf structure, which is kept opaque to the
 * API user in order to avoid corruption of the fields and make sure that only
 * the internals can modify the fields.
 *
 * The first step is to allocate a new buffer using the rcs_buf_create()
 * function, which returns a pointer to a new buffer.
 */

#ifndef BUF_H
#define BUF_H

/* flags */
#define BUF_AUTOEXT	1	/* autoextend on append */

typedef struct rcs_buf BUF;

BUF		*rcs_buf_alloc(size_t, u_int);
BUF		*rcs_buf_load(const char *, u_int);
void		 rcs_buf_free(BUF *);
void		*rcs_buf_release(BUF *);
u_char		 rcs_buf_getc(BUF *, size_t);
void		 rcs_buf_empty(BUF *);
ssize_t		 rcs_buf_set(BUF *, const void *, size_t, size_t);
ssize_t		 rcs_buf_append(BUF *, const void *, size_t);
ssize_t		 rcs_buf_fappend(BUF *, const char *, ...)
		     __attribute__((format(printf, 2, 3)));
void		 rcs_buf_putc(BUF *, int);
size_t		 rcs_buf_len(BUF *);
int		 rcs_buf_write_fd(BUF *, int);
int		 rcs_buf_write(BUF *, const char *, mode_t);
void		 rcs_buf_write_stmp(BUF *, char *, mode_t);

#define rcs_buf_get(b)	rcs_buf_peek(b, 0)

#endif	/* BUF_H */
