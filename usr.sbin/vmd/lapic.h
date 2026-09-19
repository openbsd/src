/*	$OpenBSD: lapic.h,v 1.3 2026/09/19 16:11:07 mlarkin Exp $ */

/*
 * Copyright (c) 2025 Mike Larkin <mlarkin@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN
 * NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT,
 * OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _LAPIC_H_
#define _LAPIC_H_

#include <sys/types.h>

void lapic_init(uint32_t);
void lapic_reset(uint32_t);
int lapic_mmio(uint32_t, int, paddr_t, uint8_t, uint64_t *);
int lapic_x2apic(uint32_t, int, uint32_t, uint64_t *);
int lapic_vector_irq(uint32_t, int, uint8_t, int);
int lapic_is_pending(int);
int lapic_ack(int);
int lapic_enabled(int);
int lapic_extint_enabled(int);
int lapic_timer_check(uint32_t);

#endif /* !_LAPIC_H_ */
