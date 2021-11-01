/*	$OpenBSD: fdpass.h,v 1.4 2021/11/01 14:43:25 ratchov Exp $	*/
/*
 * Copyright (c) 2015 Alexandre Ratchov <alex@caoua.org>
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
#ifndef FDPASS_H
#define FDPASS_H

struct fileops;

struct fdpass *fdpass_new(int sock, struct fileops *ops);
void fdpass_close(struct fdpass *f);

extern struct fileops worker_fileops, helper_fileops;
extern struct fdpass *fdpass_peer;

struct sio_hdl *fdpass_sio_open(int, unsigned int);
struct mio_hdl *fdpass_mio_open(int, unsigned int);
struct sioctl_hdl *fdpass_sioctl_open(int, unsigned int);

#endif /* !defined(FDPASS_H) */
