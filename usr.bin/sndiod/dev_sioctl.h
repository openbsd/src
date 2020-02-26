/*	$OpenBSD: dev_sioctl.h,v 1.1 2020/02/26 13:53:58 ratchov Exp $	*/
/*
 * Copyright (c) 2014-2020 Alexandre Ratchov <alex@caoua.org>
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
#ifndef DEV_SIOCTL_H
#define DEV_SIOCTL_H

#include "file.h"

struct dev;

struct dev_sioctl {
	struct sioctl_hdl *hdl;
	struct file *file;
};

void dev_sioctl_open(struct dev *);
void dev_sioctl_close(struct dev *);

#endif /* !defined(DEV_SIOCTL_H) */
