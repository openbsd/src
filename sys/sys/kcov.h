/*	$OpenBSD: kcov.h,v 1.3 2018/12/27 19:33:08 anton Exp $	*/

/*
 * Copyright (c) 2018 Anton Lindqvist <anton@openbsd.org>
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

#ifndef _SYS_KCOV_H_
#define _SYS_KCOV_H_

#include <sys/ioccom.h>

#define KIOSETBUFSIZE	_IOW('K', 1, unsigned long)
#define KIOENABLE	_IOW('K', 2, int)
#define KIODISABLE	_IO('K', 3)

#define KCOV_MODE_NONE		0
#define KCOV_MODE_TRACE_PC	1

#ifdef _KERNEL

#define KCOV_BUF_MAX_NMEMB	(256 << 10)

void kcov_exit(struct proc *);

#endif /* _KERNEL */

#endif /* !_SYS_KCOV_H_ */
