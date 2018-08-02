/*	$OpenBSD: ofw_regulator.h,v 1.6 2018/08/02 09:45:17 kettenis Exp $	*/
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

#ifndef _DEV_OFW_REGULATOR_H_
#define _DEV_OFW_REGULATOR_H_

struct regulator_device {
	int	rd_node;
	void	*rd_cookie;
	uint32_t (*rd_get_voltage)(void *);
	int	(*rd_set_voltage)(void *, uint32_t);
	int	(*rd_enable)(void *, int);

	uint32_t rd_min, rd_max;
	uint32_t rd_ramp_delay;

	LIST_ENTRY(regulator_device) rd_list;
	uint32_t rd_phandle;
};

void	regulator_register(struct regulator_device *);

int	regulator_enable(uint32_t);
int	regulator_disable(uint32_t);
uint32_t regulator_get_voltage(uint32_t);
int	regulator_set_voltage(uint32_t, uint32_t);

#endif /* _DEV_OFW_REGULATOR_H_ */
