/*	$OpenBSD: rbus_machdep.h,v 1.2 2011/03/23 16:54:36 pirofti Exp $	*/

/*
 * Copyright (c) 2009 Miodrag Vallat.
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

#ifndef	_MACHINE_RBUS_MACHDEP_H_
#define	_MACHINE_RBUS_MACHDEP_H_

/*
 * RBUS mapping routines
 */

struct rb_md_fnptr {
	int	(*rbus_md_space_map)(bus_space_tag_t, bus_addr_t, bus_size_t,
		    int, bus_space_handle_t *);
	void	(*rbus_md_space_unmap)(bus_space_tag_t, bus_space_handle_t,
		    bus_size_t, bus_addr_t *);
};

static __inline__ int
md_space_map(rbus_tag_t rbt, u_long addr, bus_size_t size, int flags,
    bus_space_handle_t *bshp)
{
	struct rb_md_fnptr *fn = (struct rb_md_fnptr *)rbt->rb_md;

	return (*fn->rbus_md_space_map)(rbt->rb_bt, (bus_addr_t)addr, size,
	    flags, bshp);
}

static __inline__ void
md_space_unmap(rbus_tag_t rbt, bus_space_handle_t h, bus_size_t size,
    bus_addr_t *addrp)
{
	struct rb_md_fnptr *fn = (struct rb_md_fnptr *)rbt->rb_md;

	(*fn->rbus_md_space_unmap)(rbt->rb_bt, h, size, addrp);
}

/*
 * PCCBB RBUS allocation routines (rbus_pccbb_parent_io, rbus_pccbb_parent_mem)
 * are implemented in pci_machdep.h.
 */

#define	pccbb_attach_hook(parent, self, paa)	\
	do { /* nothing */} while (/*CONSTCOND*/0)

#endif	/* _MACHINE_RBUS_MACHDEP_H_ */
