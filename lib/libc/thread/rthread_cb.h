/*	$OpenBSD: rthread_cb.h,v 1.3 2021/01/06 19:54:17 otto Exp $ */
/*
 * Copyright (c) 2016 Philip Guenther <guenther@openbsd.org>
 * All Rights Reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdio.h>

__BEGIN_HIDDEN_DECLS
void	_thread_flockfile(FILE *);
int	_thread_ftrylockfile(FILE *);
void	_thread_funlockfile(FILE *);
void	_thread_malloc_lock(int);
void	_thread_malloc_unlock(int);
void	_thread_atexit_lock(void);
void	_thread_atexit_unlock(void);
void	_thread_atfork_lock(void);
void	_thread_atfork_unlock(void);
void	_thread_arc4_lock(void);
void	_thread_arc4_unlock(void);
void	_thread_mutex_lock(void **);
void	_thread_mutex_unlock(void **);
void	_thread_mutex_destroy(void **);
void	_thread_tag_lock(void **);
void	_thread_tag_unlock(void **);
void	*_thread_tag_storage(void **, void *, size_t, void (*)(void*), void *);
__END_HIDDEN_DECLS
