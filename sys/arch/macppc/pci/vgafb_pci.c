/*	$OpenBSD: vgafb_pci.c,v 1.9 2002/09/15 02:02:44 deraadt Exp $	*/
/*	$NetBSD: vga_pci.c,v 1.4 1996/12/05 01:39:38 cgd Exp $	*/

/*
 * Copyright (c) 1995, 1996 Carnegie-Mellon University.
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
#include <sys/malloc.h>

#ifndef i386
#include <machine/autoconf.h>
#endif
#include <machine/pte.h>

#include <dev/cons.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplayvar.h>
#include <dev/wscons/wscons_raster.h>
#include <dev/rasops/rasops.h>
#include <dev/wsfont/wsfont.h>

#include <arch/macppc/pci/vgafbvar.h>
#include <arch/macppc/pci/vgafb_pcivar.h>

#define PCI_VENDORID(x) ((x) & 0xFFFF)
#define PCI_CHIPID(x)   (((x) >> 16) & 0xFFFF)

struct vgafb_pci_softc {
	struct device sc_dev;

	pcitag_t sc_pcitag;		/* PCI tag, in case we need it. */
	struct vgafb_config *sc_vc;	/* VGA configuration */
	int nscreens;
};

int vgafb_pci_probe(struct pci_attach_args *pa, int id, u_int32_t *ioaddr,
    u_int32_t *iosize, u_int32_t *memaddr, u_int32_t *memsize,
    u_int32_t *cacheable, u_int32_t *mmioaddr, u_int32_t *mmiosize);
#ifdef __BROKEN_INDIRECT_CONFIG
int	vgafb_pci_match(struct device *, void *, void *);
#else
int	vgafb_pci_match(struct device *, struct cfdata *, void *);
#endif
void	vgafb_pci_attach(struct device *, struct device *, void *);

paddr_t	vgafbpcimmap(void *, off_t, int);
int	vgafbpciioctl(void *, u_long, caddr_t, int, struct proc *);

struct cfattach vgafb_pci_ca = {
	sizeof(struct vgafb_pci_softc), (cfmatch_t)vgafb_pci_match, vgafb_pci_attach,
};

pcitag_t vgafb_pci_console_tag;
struct vgafb_config vgafb_pci_console_vc;

#if 0
#define DEBUG_VGAFB
#endif

int
vgafb_pci_probe(pa, id, ioaddr, iosize, memaddr, memsize, cacheable, mmioaddr, mmiosize)
	struct pci_attach_args *pa;
	int id;
	u_int32_t *ioaddr, *iosize;
	u_int32_t *memaddr, *memsize, *cacheable;
	u_int32_t *mmioaddr, *mmiosize;
{
	u_long addr;
	u_int32_t size, tcacheable;
	pci_chipset_tag_t pc = pa->pa_pc;
	int retval;
	int i;

	*iosize   = 0x0;
	*memsize  = 0x0;
	*mmiosize = 0x0;
	for (i = PCI_MAPREG_START; i <= PCI_MAPREG_PPB_END; i += 4) {
#ifdef DEBUG_VGAFB
		printf("vgafb confread %x %x\n",
		    i, pci_conf_read(pc, pa->pa_tag, i));
#endif
		/* need to check more than just two base addresses? */
		if (PCI_MAPREG_TYPE(pci_conf_read(pc, pa->pa_tag, i)) ==
		    PCI_MAPREG_TYPE_IO) {
			retval = pci_io_find(pc, pa->pa_tag, i,
			    &addr, &size);
#ifdef DEBUG_VGAFB
	printf("vgafb_pci_probe: io %x addr %x size %x\n", i, addr, size);
#endif

			if (retval) {
				return 0;
			}
			if (*iosize == 0) {
				*ioaddr = addr;
				*iosize = size;
			}

		} else {
			retval = pci_mem_find(pc, pa->pa_tag, i,
			    &addr, &size, &tcacheable);
#ifdef DEBUG_VGAFB
	printf("vgafb_pci_probe: mem %x addr %x size %x\n", i, addr, size);
#endif

			if (retval) {
				return 0;
			}
			if (size == 0) {
				/* ignore this entry */
			} else if (size <= (1024 * 1024)) {
#ifdef DEBUG_VGAFB
	printf("vgafb_pci_probe: mem %x addr %x size %x iosize %x\n",
	    i, addr, size, *iosize);
#endif
				if (*mmiosize == 0) {
					/* this is mmio, not memory */
					*mmioaddr = addr;
					if (size < 0x80000) {
						/* ATI driver maps 0x80000, grr */
						size = 0x80000;
					}
					*mmiosize = size;
					/* need skew in here for io memspace */
				}
			} else {
				if (*memsize == 0) {
					*memaddr = addr;
					*memsize = size;
					*cacheable = tcacheable;
				}
			}
		}
	}
#ifdef DEBUG_VGAFB
	printf("vgafb_pci_probe: id %x ioaddr %x, iosize %x, memaddr %x,\n"
	    " memsize %x, mmioaddr %x, mmiosize %x\n",
	    id, *ioaddr, *iosize, *memaddr, *memsize, *mmioaddr, *mmiosize);
#endif
	if (*iosize == 0) {
		if (id == 0) {
#ifdef powerpc
			/* this is only used if on openfirmware system and
			 * the device does not have a iobase config register,
			 * eg CirrusLogic 5434 VGA.  (they hardcode iobase to 0
			 * thus giving standard PC addresses for the registers)
			 */
			int s;
			u_int32_t sizedata;

			/*
			 * Open Firmware (yuck) shuts down devices before
			 * entering a program so we need to bring them back
			 * 'online' to respond to bus accesses... so far
			 * this is true on the power.4e.
			 */
			s = splhigh();
			sizedata = pci_conf_read(pc, pa->pa_tag,
			    PCI_COMMAND_STATUS_REG);
			sizedata |= (PCI_COMMAND_MASTER_ENABLE |
			    PCI_COMMAND_IO_ENABLE |
			    PCI_COMMAND_PARITY_ENABLE |
			    PCI_COMMAND_SERR_ENABLE);
			pci_conf_write(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG,
			    sizedata);
			splx(s);

#endif
			/* if this is the first card, allow it
			 * to be accessed in vga iospace
			 */
			*ioaddr = 0;
			*iosize = 0x10000; /* 64k, good as any */
		} else {
			/* iospace not available, assume 640x480, pray */
			*ioaddr = 0;
			*iosize=0;
		}
	}
#ifdef DEBUG_VGAFB
	printf("vgafb_pci_probe: id %x ioaddr %x, iosize %x, memaddr %x,\n"
	    " memsize %x, mmioaddr %x, mmiosize %x\n",
	    id, *ioaddr, *iosize, *memaddr, *memsize, *mmioaddr, *mmiosize);
#endif
	return 1;
}

int
vgafb_pci_match(parent, match, aux)
	struct device *parent;
#ifdef __BROKEN_INDIRECT_CONFIG
	void *match;
#else
	struct cfdata *match;
#endif
	void *aux;
{
	struct pci_attach_args *pa = aux;
	u_int32_t memaddr, memsize, cacheable;
	u_int32_t ioaddr, iosize;
	u_int32_t mmioaddr, mmiosize;
	int potential;
	int retval;
	static int id = 0;
	int myid;

	myid = id;

	potential = 0;

	/*
	 * If it's prehistoric/vga or display/vga, we might match.
	 * For the console device, this is jut a sanity check.
	 */
	if (PCI_CLASS(pa->pa_class) == PCI_CLASS_PREHISTORIC &&
	    PCI_SUBCLASS(pa->pa_class) == PCI_SUBCLASS_PREHISTORIC_VGA)
		potential = 1;
	if (PCI_CLASS(pa->pa_class) == PCI_CLASS_DISPLAY &&
	    PCI_SUBCLASS(pa->pa_class) == PCI_SUBCLASS_DISPLAY_VGA)
		potential = 1;
	if (PCI_CLASS(pa->pa_class) == PCI_CLASS_DISPLAY &&
	    PCI_SUBCLASS(pa->pa_class) == PCI_SUBCLASS_DISPLAY_MISC)
		potential = 1;

	if (!potential)
		return (0);

	/* If it's the console, we have a winner! */
	if (!bcmp(&pa->pa_tag, &vgafb_pci_console_tag, sizeof(pa->pa_tag))) {
		id++;
		return (1);
	}

#ifdef DEBUG_VGAFB
	{
		int i;
		pci_chipset_tag_t pc = pa->pa_pc;

		for (i = 0x10; i < 0x24; i+=4) {
			printf("vgafb confread %x %x\n",
			    i, pci_conf_read(pc, pa->pa_tag, i));
		}
	}
#endif

	memaddr=0xb8000;	/* default to isa addresses? */
	ioaddr = 0;		/* default to isa addresses? */

	retval = vgafb_pci_probe(pa, myid, &ioaddr, &iosize,
	    &memaddr, &memsize, &cacheable, &mmioaddr, &mmiosize);
	if (retval == 0) {
		return 0;
	}
#if 1
	printf("ioaddr %x, iosize %x, memaddr %x, memsize %x mmioaddr %x mmiosize %x\n",
	    ioaddr, iosize, memaddr, memsize, mmioaddr, mmiosize);
#endif

	if (!vgafb_common_probe(pa->pa_iot, pa->pa_memt, ioaddr, iosize,
	    memaddr, memsize, mmioaddr, mmiosize)) {
		printf("vgafb_pci_match: common_probe failed\n");
		return (0);
	}
	id++;

	return (1);
}

void
vgafb_pci_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct pci_attach_args *pa = aux;
	struct vgafb_pci_softc *sc = (struct vgafb_pci_softc *)self;
	struct vgafb_config *vc;
	u_int32_t memaddr, memsize, cacheable;
	u_int32_t ioaddr, iosize;
	u_int32_t mmioaddr, mmiosize;
	int console;
	static int id = 0;
	int myid;

	myid = id;

	vgafb_pci_probe(pa, myid, &ioaddr, &iosize,
	    &memaddr, &memsize, &cacheable, &mmioaddr, &mmiosize);


	console = (!bcmp(&pa->pa_tag, &vgafb_pci_console_tag, sizeof(pa->pa_tag)));
	if (console)
		vc = sc->sc_vc = &vgafb_pci_console_vc;
	else {
		vc = sc->sc_vc = (struct vgafb_config *)
		    malloc(sizeof(struct vgafb_config), M_DEVBUF, M_WAITOK);

		/* set up bus-independent VGA configuration */
		vgafb_common_setup(pa->pa_iot, pa->pa_memt, vc,
		    ioaddr, iosize, memaddr, memsize, mmioaddr, mmiosize);
	}
	vc->vc_mmap = vgafbpcimmap;
	vc->vc_ioctl = vgafbpciioctl;
	vc->membase = memaddr;
	vc->memsize = memsize;
	vc->mmiobase = mmioaddr;
	vc->mmiosize = mmiosize;

	sc->sc_pcitag = pa->pa_tag;

	if (iosize == 0) {
		printf(", no io");
	}
	if (mmiosize != 0) {
		printf(", mmio");
	}
	printf("\n");

	vgafb_wsdisplay_attach(self, vc, console);
	id++;
}

void
vgafb_pci_console(iot, ioaddr, iosize, memt, memaddr, memsize,
		pc, bus, device, function)
	bus_space_tag_t iot, memt;
	u_int32_t memaddr, memsize;
	u_int32_t ioaddr, iosize;
	pci_chipset_tag_t pc;
	int bus, device, function;
{
	struct vgafb_config *vc = &vgafb_pci_console_vc;
	u_int32_t mmioaddr;
	u_int32_t mmiosize;
	static struct pci_attach_args spa;
	struct pci_attach_args *pa = &spa;

	/* for later recognition */
	vgafb_pci_console_tag = pci_make_tag(pc, bus, device, function);

	pa->pa_iot = iot;
	pa->pa_memt = memt;
	pa->pa_tag = vgafb_pci_console_tag;
	/*
	pa->pa_pc = XXX;
	 */

/* XXX probe pci before pci bus config? */

	mmioaddr =0;
	mmiosize =0;
#if 0
	vgafb_pci_probe(pa, 0, &ioaddr, &iosize,
	    &memaddr, &memsize, &cacheable, mmioaddr, mmiosize);
#endif


	/* set up bus-independent VGA configuration */
	vgafb_common_setup(iot, memt, vc,
	    ioaddr, iosize, memaddr, memsize, mmioaddr, mmiosize);

	vgafb_cnattach(iot, memt, pc, bus, device, function);
}

int
vgafbpciioctl(v, cmd, data, flag, p)
	void *v;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	struct vgafb_pci_softc *sc = v;

	return (vgafb_ioctl(sc->sc_vc, cmd, data, flag, p));
}

paddr_t
vgafbpcimmap(v, offset, prot)
	void *v;
	off_t offset;
	int prot;
{
	struct vgafb_pci_softc *sc = v;

	return (vgafb_mmap(sc->sc_vc, offset, prot));
}

int
vgafb_alloc_screen(v, type, cookiep, curxp, curyp, attrp)
	void *v;
	const struct wsscreen_descr *type;
	void **cookiep;
	int *curxp, *curyp;
	long *attrp;
{
	struct vgafb_pci_softc *sc = v;
	long defattr;

	if (sc->nscreens > 0)
		return (ENOMEM);

	*cookiep = &sc->sc_vc->dc_rinfo; /* one and only for now */
	*curxp = 0;
	*curyp = 0;
	sc->sc_vc->dc_rinfo.ri_ops.alloc_attr(&sc->sc_vc->dc_rinfo,
	    0, 0, 0, &defattr);
	*attrp = defattr;
	sc->nscreens++;
	return (0);
}

void
vgafb_free_screen(v, cookie)
	void *v;
	void *cookie;
{
	struct vgafb_pci_softc *sc = v;

	if (sc->sc_vc == &vgafb_pci_console_vc)
		panic("vgafb_free_screen: console");

	sc->nscreens--;
}

int
vgafb_show_screen(v, cookie, waitok, cb, cbarg)
	void *v;
	void *cookie;
	int waitok;
	void (*cb)(void *, int, int);
	void *cbarg;
{

	return (0);
}
