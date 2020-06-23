/*	$OpenBSD: intrmap.h,v 1.3 2020/06/23 01:40:03 dlg Exp $ */

/*
 * Copyright (c) 2020 David Gwynne <dlg@openbsd.org>
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

#ifndef _SYS_INTRMAP_H_
#define _SYS_INTRMAP_H_

struct intrmap;

#define INTRMAP_POWEROF2	(1 << 0)

struct intrmap	*intrmap_create(const struct device *,
		     unsigned int, unsigned int, unsigned int);
void		 intrmap_destroy(struct intrmap *);

void		 intrmap_match(const struct device *,
		     struct intrmap *, struct intrmap *);
void		 intrmap_align(const struct device *,
		     struct intrmap *, struct intrmap *);

unsigned int	 intrmap_count(const struct intrmap *);
struct cpu_info	*intrmap_cpu(const struct intrmap *, unsigned int);
struct cpu_info	*intrmap_one(const struct device *);

#endif /* _SYS_INTRMAP_H_ */
