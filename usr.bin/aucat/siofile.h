/*	$OpenBSD: siofile.h,v 1.5 2010/05/02 11:54:26 ratchov Exp $	*/
/*
 * Copyright (c) 2008 Alexandre Ratchov <alex@caoua.org>
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
#ifndef SIOFILE_H
#define SIOFILE_H

struct fileops;
struct siofile;
struct aparams;
struct aproc;

struct siofile *siofile_new(struct fileops *, char *, unsigned *,
    struct aparams *, struct aparams *, unsigned *, unsigned *);
struct aproc *rsio_new(struct file *f);
struct aproc *wsio_new(struct file *f);

extern struct fileops siofile_ops;

#endif /* !defined(SIOFILE_H) */
