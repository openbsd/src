/*	$NetBSD: cia_pci.c,v 1.1 1995/11/23 02:37:29 cgd Exp $	*/

/*
 * Copyright (c) 1995 Carnegie-Mellon University.
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <vm/vm.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <alpha/pci/ciareg.h>
#include <alpha/pci/ciavar.h>

pci_confreg_t	cia_conf_read __P((void *, pci_conftag_t, pci_confoffset_t));
void		cia_conf_write __P((void *, pci_conftag_t,
		    pci_confoffset_t, pci_confreg_t));
int		cia_find_io __P((void *, pci_conftag_t,
		    pci_confoffset_t, pci_iooffset_t *, pci_iosize_t *));
int		cia_find_mem __P((void *, pci_conftag_t,
		    pci_confoffset_t, pci_moffset_t *, pci_msize_t *, int *));

__const struct pci_conf_fns cia_conf_fns = {
	cia_conf_read,
	cia_conf_write,
	cia_find_io,
	cia_find_mem,
};

pci_confreg_t
cia_conf_read(cpv, tag, offset)
	void *cpv;
	pci_conftag_t tag;
	pci_confoffset_t offset;
{
	struct cia_config *acp = cpv;
	pci_confreg_t *datap, data;
	int s, secondary, ba;
	int32_t old_haxr2;					/* XXX */

	secondary = PCI_TAG_BUS(tag) != 0;
	if (secondary) {
		s = splhigh();
		old_haxr2 = REGVAL(CIA_CSRS + 0x480);		/* XXX */
		wbflush();
		REGVAL(CIA_CSRS + 0x480) = old_haxr2 | 0x1;	/* XXX */
		wbflush();
	}

	datap = (pci_confreg_t *)phystok0seg(CIA_PCI_CONF |
	    tag << 5UL |					/* XXX */
	    (offset & ~0x03) << 5 |				/* XXX */
	    0 << 5 |						/* XXX */
	    0x3 << 3);						/* XXX */
	data = (pci_confreg_t)-1;
	if (!(ba = badaddr(datap, sizeof *datap)))
		data = *datap;

	if (secondary) {
		wbflush();
		REGVAL(CIA_CSRS + 0x480) = old_haxr2;		/* XXX */
		wbflush();
		splx(s);
	}

#if 0
	printf("cia_conf_read: tag 0x%lx, reg 0x%lx -> %x @ %p%s\n", tag, reg,
	    data, datap, ba ? " (badaddr)" : "");
#endif

	return data;
}

void
cia_conf_write(cpv, tag, offset, data)
	void *cpv;
	pci_conftag_t tag;
	pci_confoffset_t offset;
	pci_confreg_t data;
{
	struct cia_config *acp = cpv;
	pci_confreg_t *datap;
	int s, secondary;
	int32_t old_haxr2;					/* XXX */

	secondary = PCI_TAG_BUS(tag) != 0;
	if (secondary) {
		s = splhigh();
		old_haxr2 = REGVAL(CIA_CSRS + 0x480);		/* XXX */
		wbflush();
		REGVAL(CIA_CSRS + 0x480) = old_haxr2 | 0x1;	/* XXX */
		wbflush();
	}

	datap = (pci_confreg_t *)phystok0seg(CIA_PCI_CONF |
	    tag << 5UL |					/* XXX */
	    (offset & ~0x03) << 5 |				/* XXX */
	    0 << 5 |						/* XXX */
	    0x3 << 3);						/* XXX */
	*datap = data;

	if (secondary) {
		wbflush();
		REGVAL(CIA_CSRS + 0x480) = old_haxr2;		/* XXX */
		wbflush();
		splx(s);
	}

#if 0
	printf("cia_conf_write: tag 0x%lx, reg 0x%lx -> 0x%x @ %p\n", tag,
	    reg, data, datap);
#endif
}

int
cia_find_io(cpv, tag, reg, iobasep, sizep)
	void *cpv;
	pci_conftag_t tag;
	pci_confoffset_t reg;
	pci_iooffset_t *iobasep;
	pci_iosize_t *sizep;
{
	struct cia_config *acp = cpv;
	pci_confreg_t addrdata, sizedata;
	pci_iooffset_t pci_iobase;

	if (reg < PCI_MAPREG_START || reg >= PCI_MAPREG_END || (reg & 3))
		panic("cia_map_io: bad request");

	addrdata = PCI_CONF_READ(acp->cc_conffns, acp->cc_confarg, tag, reg);

	PCI_CONF_WRITE(acp->cc_conffns, acp->cc_confarg, tag, reg, 0xffffffff);
	sizedata = PCI_CONF_READ(acp->cc_conffns, acp->cc_confarg, tag, reg);
	PCI_CONF_WRITE(acp->cc_conffns, acp->cc_confarg, tag, reg, addrdata);

	if (PCI_MAPREG_TYPE(addrdata) == PCI_MAPREG_TYPE_MEM)
		panic("cia_map_io: attempt to I/O map an memory region");

	if (iobasep != NULL)
		*iobasep = PCI_MAPREG_IO_ADDRESS(addrdata);
	if (sizep != NULL)
		*sizep = ~PCI_MAPREG_IO_ADDRESS(sizedata) + 1;

	return (0);
}

int
cia_find_mem(cpv, tag, reg, paddrp, sizep, cacheablep)
	void *cpv;
	pci_conftag_t tag;
	pci_confoffset_t reg;
	pci_moffset_t *paddrp;
	pci_msize_t *sizep;
	int *cacheablep;
{
	struct cia_config *acp = cpv;
	pci_confreg_t addrdata, sizedata;

	if (reg < PCI_MAPREG_START || reg >= PCI_MAPREG_END || (reg & 3))
		panic("cia_map_mem: bad request");

	/*
	 * The PROM has mapped the device for us.  We take the address
	 * that's been assigned to the register, and figure out what
	 * physical and virtual addresses go with it...
	 */
	addrdata = PCI_CONF_READ(acp->cc_conffns, acp->cc_confarg, tag, reg);

	PCI_CONF_WRITE(acp->cc_conffns, acp->cc_confarg, tag, reg, 0xffffffff);
	sizedata = PCI_CONF_READ(acp->cc_conffns, acp->cc_confarg, tag, reg);
	PCI_CONF_WRITE(acp->cc_conffns, acp->cc_confarg, tag, reg, addrdata);

	if (PCI_MAPREG_TYPE(addrdata) == PCI_MAPREG_TYPE_IO)
		panic("cia_map_mem: attempt to memory map an I/O region");

	switch (PCI_MAPREG_MEM_TYPE(addrdata)) {
	case PCI_MAPREG_MEM_TYPE_32BIT:
	case PCI_MAPREG_MEM_TYPE_32BIT_1M:
		break;
	case PCI_MAPREG_MEM_TYPE_64BIT:
/* XXX */	printf("cia_map_mem: attempt to map 64-bit region\n");
/* XXX */	break;
	default:
		printf("cia_map_mem: reserved mapping type\n");
		return EINVAL;
	}

	if (paddrp != NULL)
		*paddrp = PCI_MAPREG_MEM_ADDRESS(addrdata);	/* PCI addr */
	if (sizep != NULL)
		*sizep = ~PCI_MAPREG_MEM_ADDRESS(sizedata) + 1;
	if (cacheablep != NULL)
		*cacheablep = PCI_MAPREG_MEM_CACHEABLE(addrdata);

	return 0;
}
