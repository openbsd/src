/*	$OpenBSD: pci_machdep.h,v 1.8 1998/06/28 02:36:23 angelos Exp $	*/
/*	$NetBSD: pci_machdep.h,v 1.6 1996/11/19 04:49:21 cgd Exp $	*/

/*
 * Copyright (c) 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

/*
 * Machine-specific definitions for PCI autoconfiguration.
 */

/*
 * Types provided to machine-independent PCI code
 */
typedef struct alpha_pci_chipset *pci_chipset_tag_t;
typedef u_long pcitag_t;
typedef u_long pci_intr_handle_t;

/*
 * alpha-specific PCI structure and type definitions.
 * NOT TO BE USED DIRECTLY BY MACHINE INDEPENDENT CODE.
 */
struct alpha_pci_chipset {
	void		*pc_conf_v;
	void		(*pc_attach_hook) __P((struct device *,
			    struct device *, struct pcibus_attach_args *));
	int		(*pc_bus_maxdevs) __P((void *, int));
	pcitag_t	(*pc_make_tag) __P((void *, int, int, int));
	void		(*pc_decompose_tag) __P((void *, pcitag_t, int *,
			    int *, int *));
	pcireg_t	(*pc_conf_read) __P((void *, pcitag_t, int));
	void		(*pc_conf_write) __P((void *, pcitag_t, int, pcireg_t));

	void		*pc_intr_v;
	int		(*pc_intr_map) __P((void *, pcitag_t, int, int,
			    pci_intr_handle_t *));
	const char	*(*pc_intr_string) __P((void *, pci_intr_handle_t));
	void		*(*pc_intr_establish) __P((void *, pci_intr_handle_t,
			    int, int (*)(void *), void *, char *));
	void		(*pc_intr_disestablish) __P((void *, void *));
};

/*
 * Functions provided to machine-independent PCI code.
 */
#define	pci_attach_hook(p, s, pba)					\
    (*(pba)->pba_pc->pc_attach_hook)((p), (s), (pba))
#define	pci_bus_maxdevs(c, b)						\
    (*(c)->pc_bus_maxdevs)((c)->pc_conf_v, (b))
#define	pci_make_tag(c, b, d, f)					\
    (*(c)->pc_make_tag)((c)->pc_conf_v, (b), (d), (f))
#define	pci_decompose_tag(c, t, bp, dp, fp)				\
    (*(c)->pc_decompose_tag)((c)->pc_conf_v, (t), (bp), (dp), (fp))
#define	pci_conf_read(c, t, r)						\
    (*(c)->pc_conf_read)((c)->pc_conf_v, (t), (r))
#define	pci_conf_write(c, t, r, v)					\
    (*(c)->pc_conf_write)((c)->pc_conf_v, (t), (r), (v))
#define	pci_intr_map(c, it, ip, il, ihp)				\
    (*(c)->pc_intr_map)((c)->pc_intr_v, (it), (ip), (il), (ihp))
#define	pci_intr_string(c, ih)						\
    (*(c)->pc_intr_string)((c)->pc_intr_v, (ih))
#define	pci_intr_establish(c, ih, l, h, a, nm)				\
    (*(c)->pc_intr_establish)((c)->pc_intr_v, (ih), (l), (h), (a), (nm))
#define	pci_intr_disestablish(c, iv)					\
    (*(c)->pc_intr_disestablish)((c)->pc_intr_v, (iv))

/*
 * alpha-specific PCI functions.
 * NOT TO BE USED DIRECTLY BY MACHINE INDEPENDENT CODE.
 */
void	pci_display_console __P((bus_space_tag_t, bus_space_tag_t,
	    pci_chipset_tag_t, int, int, int));
#define alpha_pci_decompose_tag(c, t, bp, dp, fp)                       \
    (*(c)->pc_decompose_tag)((c)->pc_conf_v, (t), (bp), (dp), (fp))
#define alpha_pciide_compat_intr_establish(c, d, p, ch, f, a)           \
    ((c)->pc_pciide_compat_intr_establish == NULL ? NULL :              \
     (*(c)->pc_pciide_compat_intr_establish)((c)->pc_conf_v, (d), (p),  \
        (ch), (f), (a)))

#ifdef _KERNEL
void pci_display_console
    __P((bus_space_tag_t, bus_space_tag_t, pci_chipset_tag_t, int, int, int));
#endif /* _KERNEL */
