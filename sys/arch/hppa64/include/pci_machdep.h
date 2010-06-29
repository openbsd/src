/*	$OpenBSD: pci_machdep.h,v 1.6 2010/06/29 22:08:28 jordan Exp $	*/

/*
 * Copyright (c) 2005 Michael Shalayeff
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef	_MACHINE_PCI_MACHDEP_H_
#define	_MACHINE_PCI_MACHDEP_H_

/*
 * Types provided to machine-independent PCI code
 */
typedef struct hppa64_pci_chipset_tag *pci_chipset_tag_t;
typedef u_int pcitag_t;
typedef u_long pci_intr_handle_t;

struct pci_attach_args;

struct hppa64_pci_chipset_tag {
	void		*_cookie;
	void		(*pc_attach_hook)(struct device *,
			    struct device *, struct pcibus_attach_args *);
	int		(*pc_bus_maxdevs)(void *, int);
	pcitag_t	(*pc_make_tag)(void *, int, int, int);
	void		(*pc_decompose_tag)(void *, pcitag_t, int *,
			    int *, int *);
	pcireg_t	(*pc_conf_read)(void *, pcitag_t, int);
	void		(*pc_conf_write)(void *, pcitag_t, int, pcireg_t);

	int		(*pc_intr_map)(struct pci_attach_args *,
			    pci_intr_handle_t *);
	const char	*(*pc_intr_string)(void *, pci_intr_handle_t);
	void		*(*pc_intr_establish)(void *, pci_intr_handle_t,
			    int, int (*)(void *), void *, const char *);
	void		(*pc_intr_disestablish)(void *, void *);

	void		*(*pc_alloc_parent)(struct device *,
			    struct pci_attach_args *, int);
};

/*
 * Functions provided to machine-independent PCI code.
 */
#define	pci_attach_hook(p, s, pba)					\
    (*(pba)->pba_pc->pc_attach_hook)((p), (s), (pba))
#define	pci_bus_maxdevs(c, b)						\
    (*(c)->pc_bus_maxdevs)((c)->_cookie, (b))
#define	pci_make_tag(c, b, d, f)					\
    (*(c)->pc_make_tag)((c)->_cookie, (b), (d), (f))
#define	pci_decompose_tag(c, t, bp, dp, fp)				\
    (*(c)->pc_decompose_tag)((c)->_cookie, (t), (bp), (dp), (fp))
#define	pci_conf_read(c, t, r)						\
    (*(c)->pc_conf_read)((c)->_cookie, (t), (r))
#define	pci_conf_write(c, t, r, v)					\
    (*(c)->pc_conf_write)((c)->_cookie, (t), (r), (v))
#define	pci_intr_map(p, ihp)						\
    (*(p)->pa_pc->pc_intr_map)((p), (ihp))
#define	pci_intr_line(c, ih)	(ih)
#define	pci_intr_string(c, ih)						\
    (*(c)->pc_intr_string)((c)->_cookie, (ih))
#define	pci_intr_establish(c, ih, l, h, a, nm)				\
    (*(c)->pc_intr_establish)((c)->_cookie, (ih), (l), (h), (a), (nm))
#define	pci_intr_disestablish(c, iv)					\
    (*(c)->pc_intr_disestablish)((c)->_cookie, (iv))

#define	pciide_machdep_compat_intr_establish(a, b, c, d, e)	(NULL)
#define	pciide_machdep_compat_intr_disestablish(a, b)	((void)(a), (void)(b))

#define	pci_dev_postattach(a, b)

#endif /* _MACHINE_PCI_MACHDEP_H_ */
