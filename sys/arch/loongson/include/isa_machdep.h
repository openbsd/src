/*	$OpenBSD: isa_machdep.h,v 1.1.1.1 2009/11/25 19:44:27 miod Exp $	*/

/*
 * Copyright (c) 2007 Miodrag Vallat.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice, this permission notice, and the disclaimer below
 * appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef	_MACHINE_ISA_MACHDEP_H_
#define	_MACHINE_ISA_MACHDEP_H_

#include <machine/bus.h>

#define	__NO_ISA_INTR_CHECK

typedef	void *isa_chipset_tag_t;

void	 isa_attach_hook(struct device *, struct device *,
	    struct isabus_attach_args *);
void	*isa_intr_establish(void *, int, int, int, int (*)(void *), void *,
	    char *);
void	 isa_intr_disestablish(void *, void *);

#endif
