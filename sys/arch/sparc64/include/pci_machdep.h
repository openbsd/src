/*	$OpenBSD: pci_machdep.h,v 1.13 2004/12/02 02:41:02 brad Exp $	*/
/* $NetBSD: pci_machdep.h,v 1.7 2001/07/20 00:07:14 eeh Exp $ */

/*
 * Copyright (c) 1999 Matthew R. Green
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _MACHINE_PCI_MACHDEP_H_
#define _MACHINE_PCI_MACHDEP_H_

/*
 * We want to control both device & function probe order.
 */
#define		__PCI_BUS_DEVORDER
#define		__PCI_DEV_FUNCORDER

/*
 * Forward declarations.
 */
struct pci_attach_args;

/*
 * define some bits used to glue into the common PCI code.
 */

typedef struct sparc_pci_chipset *pci_chipset_tag_t;
typedef u_int pci_intr_handle_t;

/* 
 * The stuuuuuuupid allegedly MI PCI code expects pcitag_t to be a
 * scalar type.  But we really need to store both the OFW node and
 * the bus/device/function info in it.  (We'd like to store more, 
 * like all the ofw properties, but we don't need to.)  Luckily,
 * both are 32-bit values, so we can squeeze them into a u_int64_t
 * with a little help from some macros.
 */

#define	PCITAG_NODE(x)		(int)(((x)>>32)&0xffffffff)
#define	PCITAG_OFFSET(x)	((x)&0xffffffff)
#define	PCITAG_BUS(t)		((PCITAG_OFFSET(t)>>16)&0xff)
#define	PCITAG_DEV(t)		((PCITAG_OFFSET(t)>>11)&0x1f)
#define	PCITAG_FUN(t)		((PCITAG_OFFSET(t)>>8)&0x7)
#define	PCITAG_CREATE(n,b,d,f)	(((u_int64_t)(n)<<32)|((b)<<16)|((d)<<11)|((f)<<8))
#define	PCITAG_SETNODE(t,n)	((t)&0xffffffff)|(((n)<<32)
typedef u_int64_t pcitag_t; 

struct sparc_pci_chipset {
	void			*cookie;	/* psycho_pbm, but sssh! */
	bus_space_tag_t		bustag;
	bus_space_handle_t	bushandle;
	int			rootnode;	/* PCI controller */
	int			curnode;	/* Current OFW node */
	int (*intr_map)(struct pci_attach_args *, pci_intr_handle_t *);
};

void		pci_attach_hook(struct device *, struct device *,
				     struct pcibus_attach_args *);
#ifdef __PCI_BUS_DEVORDER
int		pci_bus_devorder(pci_chipset_tag_t, int, char *);
#endif
#ifdef __PCI_DEV_FUNCORDER
int		pci_dev_funcorder(pci_chipset_tag_t, int, int, char *);
#endif
int		pci_bus_maxdevs(pci_chipset_tag_t, int);
pcitag_t	pci_make_tag(pci_chipset_tag_t, int, int, int);
void		pci_decompose_tag(pci_chipset_tag_t, pcitag_t, int *, int *,
		    int *);
pcireg_t	pci_conf_read(pci_chipset_tag_t, pcitag_t, int);
void		pci_conf_write(pci_chipset_tag_t, pcitag_t, int,
				    pcireg_t);
int		pci_intr_map(struct pci_attach_args *, pci_intr_handle_t *);
const char	*pci_intr_string(pci_chipset_tag_t, pci_intr_handle_t);
const struct evcnt *pci_intr_evcnt(pci_chipset_tag_t, pci_intr_handle_t);
void		*pci_intr_establish(pci_chipset_tag_t, pci_intr_handle_t,
				 int, int (*)(void *), void *, char *);
void		pci_intr_disestablish(pci_chipset_tag_t, void *);

#define pciide_machdep_compat_intr_establish(a, b, c, d, e) (NULL)
#define pciide_machdep_compat_intr_disestablish(a, b) do { } while (0)

#endif /* _MACHINE_PCI_MACHDEP_H_ */
