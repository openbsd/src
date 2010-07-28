/*	$OpenBSD: buf.h,v 1.11 2010/07/28 09:07:11 ray Exp $	*/
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
 * operations are performed on a buf structure, which is kept opaque to the
 * API user in order to avoid corruption of the fields and make sure that only
 * the internals can modify the fields.
 *
 * The first step is to allocate a new buffer using the buf_alloc()
 * function, which returns a pointer to a new buffer.
 */

#ifndef BUF_H
#define BUF_H

#include <sys/types.h>

typedef struct buf BUF;

BUF		*buf_alloc(size_t);
BUF		*buf_load(const char *);
void		 buf_free(BUF *);
void		*buf_release(BUF *);
u_char		 buf_getc(BUF *, size_t);
void		 buf_empty(BUF *);
size_t		 buf_append(BUF *, const void *, size_t);
size_t		 buf_fappend(BUF *, const char *, ...)
		     __attribute__((format(printf, 2, 3)));
void		 buf_putc(BUF *, int);
size_t		 buf_len(BUF *);
int		 buf_write_fd(BUF *, int);
int		 buf_write(BUF *, const char *, mode_t);
void		 buf_write_stmp(BUF *, char *);
u_char		*buf_get(BUF *b);
#endif	/* BUF_H */
