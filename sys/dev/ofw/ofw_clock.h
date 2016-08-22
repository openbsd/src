/*	$OpenBSD: ofw_clock.h,v 1.2 2016/08/22 11:23:54 kettenis Exp $	*/
/*
 * Copyright (c) 2016 Mark Kettenis
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

#ifndef _DEV_OFW_CLOCK_H_
#define _DEV_OFW_CLOCK_H_

struct clock_device {
	int	cd_node;
	void	*cd_cookie;
	uint32_t (*cd_get_frequency)(void *, uint32_t *);
	void	(*cd_enable)(void *, uint32_t *, int);

	LIST_ENTRY(clock_device) cd_list;
	uint32_t cd_phandle;
	uint32_t cd_cells;
};

void	clock_register(struct clock_device *);

uint32_t clock_get_frequency(int, const char *);
uint32_t clock_get_frequency_idx(int, int);
void	clock_enable(int, const char *);
void	clock_enable_idx(int, int);
void	clock_disable(int, const char *);
void	clock_disable_idx(int, int);

static inline void
clock_enable_all(int node)
{
	clock_enable_idx(node, -1);
}

static inline void
clock_disable_all(int node)
{
	clock_disable_idx(node, -1);
}

#endif /* _DEV_OFW_CLOCK_H_ */
