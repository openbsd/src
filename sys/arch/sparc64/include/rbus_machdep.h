/*	$OpenBSD: rbus_machdep.h,v 1.1 2007/08/04 16:46:03 kettenis Exp $	*/

/*
 * Copyright (c) 2007 Mark Kettenis
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

#ifndef _SPARC64_RBUS_MACHDEP_H_
#define _SPARC64_RBUS_MACHDEP_H_

struct pci_attach_args;

rbus_tag_t rbus_pccbb_parent_io(struct device *, struct pci_attach_args *);
rbus_tag_t rbus_pccbb_parent_mem(struct device *, struct pci_attach_args *);

#define md_space_map(t, addr, size, flags, hp) \
	bus_space_map((t), (addr), (size), (flags), (hp))
#define md_space_unmap(t, h, size, addrp) \
	do { \
		*addrp = (t)->sparc_bus_addr((t), (t), (h)); \
		bus_space_unmap((t), (h), (size)); \
	} while (0)

#endif /* _SPARC64_RBUS_MACHDEP_H_ */
