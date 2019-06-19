/*	$OpenBSD: ofw_misc.h,v 1.3 2018/04/02 17:42:15 patrick Exp $	*/
/*
 * Copyright (c) 2017 Mark Kettenis
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

#ifndef _DEV_OFW_MISC_H_
#define _DEV_OFW_MISC_H_

void	regmap_register(int, bus_space_tag_t, bus_space_handle_t, bus_size_t);

struct regmap;
struct regmap *regmap_bycompatible(char *);
struct regmap *regmap_bynode(int);
struct regmap *regmap_byphandle(uint32_t);

uint32_t regmap_read_4(struct regmap *, bus_size_t);
void	regmap_write_4(struct regmap *, bus_size_t, uint32_t);

#endif /* _DEV_OFW_MISC_H_ */
