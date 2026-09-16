/*	$OpenBSD: hpet.h,v 1.1 2026/09/16 00:26:19 mlarkin Exp $ */

/*
 * Copyright (c) 2026 Mike Larkin <mlarkin@openbsd.org>
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

#ifndef _HPET_H_
#define _HPET_H_

#include <sys/types.h>

#define VMD_HPET_BASE		0xfed00000ULL
#define VMD_HPET_SIZE		0x400
#define VMD_HPET_FREQUENCY	100000000ULL
#define VMD_HPET_PERIOD_FS	10000000ULL
#define VMD_HPET_NUM_TIMERS	3

/* Lower 32 bits of the General Capabilities and ID register. */
#define VMD_HPET_CAP_ID		0x80862201U

void	 hpet_init(uint32_t);
void	 hpet_stop(void);
void	 hpet_start(void);
int	 hpet_mmio(uint32_t, int, paddr_t, uint8_t, uint64_t *);

#endif /* !_HPET_H_ */
