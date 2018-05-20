/*	$OpenBSD: ofw_power.h,v 1.1 2018/05/02 15:16:31 patrick Exp $	*/
/*
 * Copyright (c) 2016 Mark Kettenis
 * Copyright (c) 2018 Patrick Wildt <patrick@blueri.se>
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

#ifndef _DEV_OFW_POWER_H_
#define _DEV_OFW_POWER_H_

struct power_domain_device {
	int	pd_node;
	void	*pd_cookie;
	void	(*pd_enable)(void *, uint32_t *, int);

	LIST_ENTRY(power_domain_device) pd_list;
	uint32_t pd_phandle;
	uint32_t pd_cells;
};

void	power_domain_register(struct power_domain_device *);
void	power_domain_enable(int);
void	power_domain_disable(int);

#endif /* _DEV_OFW_POWER_H_ */
