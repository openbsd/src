/* $OpenBSD: signify.h,v 1.2 2019/03/23 07:10:06 tedu Exp $ */
/*
 * Copyright (c) 2016 Marc Espie <espie@openbsd.org>
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

/* common interface to signify.c/zsig.c */
#ifndef signify_h
#define signify_h
extern void zverify(const char *, const char *, const char *, const char *);
extern void zsign(const char *, const char *, const char *, int);

extern void *xmalloc(size_t);
extern void writeall(int, const void *, size_t, const char *);
extern int xopen(const char *, int, mode_t);
extern void *verifyzdata(uint8_t *, unsigned long long,
    const char *, const char *, const char *);
extern uint8_t *createsig(const char *, const char *, uint8_t *,
    unsigned long long);


#endif
