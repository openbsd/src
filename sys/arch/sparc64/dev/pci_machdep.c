/*	$OpenBSD: pci_machdep.c,v 1.15 2004/12/02 02:41:02 brad Exp $	*/
/*	$NetBSD: pci_machdep.c,v 1.22 2001/07/20 00:07:13 eeh Exp $	*/

/*
 * Copyright (c) 1999, 2000 Matthew R. Green
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

/*
 * functions expected by the MI PCI code.
 */

#ifdef DEBUG
#define SPDB_CONF	0x01
#define SPDB_INTR	0x04
#define SPDB_INTMAP	0x08
#define SPDB_INTFIX	0x10
#define SPDB_PROBE	0x20
int sparc_pci_debug = 0x0;
#define DPRINTF(l, s)	do { if (sparc_pci_debug & l) printf s; } while (0)
#else
#define DPRINTF(l, s)
#endif

#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/device.h>
#include <sys/malloc.h>

#define _SPARC_BUS_DMA_PRIVATE
#include <machine/bus.h>
#include <machine/autoconf.h>
#include <machine/openfirm.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>

#include <dev/ofw/ofw_pci.h>

#include <sparc64/dev/iommureg.h>
#include <sparc64/dev/iommuvar.h>
#include <sparc64/dev/psychoreg.h>
#include <sparc64/dev/psychovar.h>

/* this is a base to be copied */
struct sparc_pci_chipset _sparc_pci_chipset = {
	NULL,
};

/*
 * functions provided to the MI code.
 */

void
pci_attach_hook(parent, self, pba)
	struct device *parent;
	struct device *self;
	struct pcibus_attach_args *pba;
{
	/* Don't do nothing */
}

int
pci_bus_maxdevs(pc, busno)
	pci_chipset_tag_t pc;
	int busno;
{

	return 32;
}

#ifdef __PCI_BUS_DEVORDER
int
pci_bus_devorder(pc, busno, devs)
	pci_chipset_tag_t pc;
	int busno;
	char *devs;
{
	struct ofw_pci_register reg;
	int node, len, device, i = 0;
	u_int32_t done = 0;
#ifdef DEBUG
	char name[80];
#endif

	node = pc->curnode;
#ifdef DEBUG
	if (sparc_pci_debug & SPDB_PROBE) {
		OF_getprop(node, "name", &name, sizeof(name));
		printf("pci_bus_devorder: curnode %x %s\n", node, name);
	}
#endif
	/*
	 * Initially, curnode is the root of the pci tree.  As we
	 * attach bridges, curnode should be set to that of the bridge.
	 */
	for (node = OF_child(node); node; node = OF_peer(node)) {
		len = OF_getproplen(node, "reg");
		if (len < sizeof(reg))
			continue;
		if (OF_getprop(node, "reg", (void *)&reg, sizeof(reg)) != len)
			panic("pci_probe_bus: OF_getprop len botch");

		device = OFW_PCI_PHYS_HI_DEVICE(reg.phys_hi);

		if (done & (1 << device))
			continue;

		devs[i++] = device;
		done |= 1 << device;
#ifdef DEBUG
	if (sparc_pci_debug & SPDB_PROBE) {
		OF_getprop(node, "name", &name, sizeof(name));
		printf("pci_bus_devorder: adding %x %s\n", node, name);
	}
#endif
		if (i == 32)
			break;
	}
	if (i < 32)
		devs[i] = -1;

	return i;
}
#endif

#ifdef __PCI_DEV_FUNCORDER
int
pci_dev_funcorder(pc, busno, device, funcs)
	pci_chipset_tag_t pc;
	int busno;
	int device;
	char *funcs;
{
	struct ofw_pci_register reg;
	int node, len, i = 0;
#ifdef DEBUG
	char name[80];
#endif

	node = pc->curnode;
#ifdef DEBUG
	if (sparc_pci_debug & SPDB_PROBE) {
		OF_getprop(node, "name", &name, sizeof(name));
		printf("pci_bus_funcorder: curnode %x %s\n", node, name);
	}
#endif
	/*
	 * Functions are siblings.  Presumably we're only called when the
	 * first instance of this device is detected, so we should be able to
	 * get to all the other functions with OF_peer().  But there seems
	 * some issues with this scheme, so we always go to the first node on
	 * this bus segment for a scan.  
	 */
	for (node = OF_child(OF_parent(node)); node; node = OF_peer(node)) {
		len = OF_getproplen(node, "reg");
		if (len < sizeof(reg))
			continue;
		if (OF_getprop(node, "reg", (void *)&reg, sizeof(reg)) != len)
			panic("pci_probe_bus: OF_getprop len botch");

		if (device != OFW_PCI_PHYS_HI_DEVICE(reg.phys_hi))
			continue;

		funcs[i++] = OFW_PCI_PHYS_HI_FUNCTION(reg.phys_hi);
#ifdef DEBUG
	if (sparc_pci_debug & SPDB_PROBE) {
		OF_getprop(node, "name", &name, sizeof(name));
		printf("pci_bus_funcorder: adding %x %s\n", node, name);
	}
#endif
		if (i == 8)
			break;
	}
	if (i < 8)
		funcs[i] = -1;

	return i;
}
#endif

pcitag_t
pci_make_tag(pc, b, d, f)
	pci_chipset_tag_t pc;
	int b;
	int d;
	int f;
{
	struct ofw_pci_register reg;
	pcitag_t tag;
	int busrange[2];
	int node, len;
#ifdef DEBUG
	char name[80];
	bzero(name, sizeof(name));
#endif

	/* 
	 * Hunt for the node that corresponds to this device 
	 *
	 * We could cache this info in an array in the parent
	 * device... except then we have problems with devices
	 * attached below pci-pci bridges, and we would need to
	 * add special code to the pci-pci bridge to cache this
	 * info.
	 */

	tag = PCITAG_CREATE(-1, b, d, f);
	node = pc->rootnode;
	/*
	 * First make sure we're on the right bus.  If our parent
	 * has a bus-range property and we're not in the range,
	 * then we're obviously on the wrong bus.  So go up one
	 * level.
	 */
#ifdef DEBUG
	if (sparc_pci_debug & SPDB_PROBE) {
		OF_getprop(node, "name", &name, sizeof(name));
		printf("curnode %x %s\n", node, name);
	}
#endif
#if 0
	while ((OF_getprop(OF_parent(node), "bus-range", (void *)&busrange,
		sizeof(busrange)) == sizeof(busrange)) &&
		(b < busrange[0] || b > busrange[1])) {
		/* Out of range, go up one */
		node = OF_parent(node);
#ifdef DEBUG
		if (sparc_pci_debug & SPDB_PROBE) {
			OF_getprop(node, "name", &name, sizeof(name));
			printf("going up to node %x %s\n", node, name);
		}
#endif
	}
#endif	
	/*
	 * Now traverse all peers until we find the node or we find
	 * the right bridge. 
	 *
	 * XXX We go up one and down one to make sure nobody's missed.
	 * but this should not be necessary.
	 */
	for (node = ((node)); node; node = OF_peer(node)) {

#ifdef DEBUG
		if (sparc_pci_debug & SPDB_PROBE) {
			OF_getprop(node, "name", &name, sizeof(name));
			printf("checking node %x %s\n", node, name);
		}
#endif

#if 1
		/*
		 * Check for PCI-PCI bridges.  If the device we want is
		 * in the bus-range for that bridge, work our way down.
		 */
		while ((OF_getprop(node, "bus-range", (void *)&busrange,
			sizeof(busrange)) == sizeof(busrange)) &&
			(b >= busrange[0] && b <= busrange[1])) {
			/* Go down 1 level */
			node = OF_child(node);
#ifdef DEBUG
			if (sparc_pci_debug & SPDB_PROBE) {
				OF_getprop(node, "name", &name, sizeof(name));
				printf("going down to node %x %s\n",
					node, name);
			}
#endif
		}
#endif
		/* 
		 * We only really need the first `reg' property. 
		 *
		 * For simplicity, we'll query the `reg' when we
		 * need it.  Otherwise we could malloc() it, but
		 * that gets more complicated.
		 */
		len = OF_getproplen(node, "reg");
		if (len < sizeof(reg))
			continue;
		if (OF_getprop(node, "reg", (void *)&reg, sizeof(reg)) != len)
			panic("pci_probe_bus: OF_getprop len botch");

		if (b != OFW_PCI_PHYS_HI_BUS(reg.phys_hi))
			continue;
		if (d != OFW_PCI_PHYS_HI_DEVICE(reg.phys_hi))
			continue;
		if (f != OFW_PCI_PHYS_HI_FUNCTION(reg.phys_hi))
			continue;

		/* Got a match */
		tag = PCITAG_CREATE(node, b, d, f);

		/*
		 * Record the node.  This has two effects:
		 *
		 * 1) We don't have to search as far.
		 * 2) pci_bus_devorder will scan the right bus.
		 */
		pc->curnode = node;

		/* Enable all the different spaces for this device */
		pci_conf_write(pc, tag, PCI_COMMAND_STATUS_REG,
			PCI_COMMAND_MEM_ENABLE|PCI_COMMAND_MASTER_ENABLE|
			PCI_COMMAND_IO_ENABLE);
		DPRINTF(SPDB_PROBE, ("found node %x %s\n", node, name));
		return (tag);
	}
	/* No device found -- return a dead tag */
	return (tag);
}

void
pci_decompose_tag(pc, tag, bp, dp, fp)
	pci_chipset_tag_t pc;
	pcitag_t tag;
	int *bp, *dp, *fp;
{

	if (bp != NULL)
		*bp = PCITAG_BUS(tag);
	if (dp != NULL)
		*dp = PCITAG_DEV(tag);
	if (fp != NULL)
		*fp = PCITAG_FUN(tag);
}

/* assume we are mapped little-endian/side-effect */
pcireg_t
pci_conf_read(pci_chipset_tag_t pc, pcitag_t tag, int reg)
{
        pcireg_t val = (pcireg_t)~0;

        DPRINTF(SPDB_CONF, ("pci_conf_read: tag %lx reg %x ",
                (long)PCITAG_OFFSET(tag), reg));
        if (PCITAG_NODE(tag) != -1) {
                val = bus_space_read_4(pc->bustag, pc->bushandle,
                        PCITAG_OFFSET(tag) + reg);
        }
#ifdef DEBUG
        else DPRINTF(SPDB_CONF, ("pci_conf_read: bogus pcitag %x\n",
            (int)PCITAG_OFFSET(tag)));
#endif
        DPRINTF(SPDB_CONF, (" returning %08x\n", (u_int)val));

        return (val);
}

void
pci_conf_write(pci_chipset_tag_t pc, pcitag_t tag, int reg, pcireg_t data)
{
        DPRINTF(SPDB_CONF, ("pci_conf_write: tag %lx; reg %x; data %x; ",
                (long)PCITAG_OFFSET(tag), reg, (int)data));

        /* If we don't know it, just punt. */
        if (PCITAG_NODE(tag) == -1) {
                DPRINTF(SPDB_CONF, ("pci_config_write: bad addr"));
                return;
        }

        bus_space_write_4(pc->bustag, pc->bushandle,
                PCITAG_OFFSET(tag) + reg, data);
}

/*
 * interrupt mapping foo.
 * XXX: how does this deal with multiple interrupts for a device?
 */
int
pci_intr_map(pa, ihp)
	struct pci_attach_args *pa;
	pci_intr_handle_t *ihp;
{
	pcitag_t tag = pa->pa_tag;
	int interrupts;
	int len, node = PCITAG_NODE(tag);
	char devtype[30];

	len = OF_getproplen(node, "interrupts");
	if (len < sizeof(interrupts)) {
		DPRINTF(SPDB_INTMAP,
			("pci_intr_map: interrupts len %d too small\n", len));
		return (ENODEV);
	}
	if (OF_getprop(node, "interrupts", (void *)&interrupts, 
		sizeof(interrupts)) != len) {
		DPRINTF(SPDB_INTMAP,
			("pci_intr_map: could not read interrupts\n"));
		return (ENODEV);
	}

	if (OF_mapintr(node, &interrupts, sizeof(interrupts), 
		sizeof(interrupts)) < 0) {
		printf("OF_mapintr failed\n");
	}
	/* Try to find an IPL for this type of device. */
	if (OF_getprop(node, "device_type", &devtype, sizeof(devtype)) > 0) {
		for (len = 0;  intrmap[len].in_class; len++)
			if (strcmp(intrmap[len].in_class, devtype) == 0) {
				interrupts |= INTLEVENCODE(intrmap[len].in_lev);
				break;
			}
	}

	/* XXXX -- we use the ino.  What if there is a valid IGN? */
	*ihp = interrupts;

	if (pa->pa_pc->intr_map)
		return ((*pa->pa_pc->intr_map)(pa, ihp));
	else
		return (0);
}

const char *
pci_intr_string(pc, ih)
	pci_chipset_tag_t pc;
	pci_intr_handle_t ih;
{
	static char str[16];

	DPRINTF(SPDB_INTR, ("pci_intr_string: ih %u", ih));
	snprintf(str, sizeof str, "ivec %x", ih);
	DPRINTF(SPDB_INTR, ("; returning %s\n", str));

	return (str);
}

const struct evcnt *
pci_intr_evcnt(pc, ih)
	pci_chipset_tag_t pc;
	pci_intr_handle_t ih;
{

	/* XXX for now, no evcnt parent reported */
	return NULL;
}

void *
pci_intr_establish(pc, ih, level, func, arg, what)
	pci_chipset_tag_t pc;
	pci_intr_handle_t ih;
	int level;
	int (*func)(void *);
	void *arg;
	char *what;
{
	void *cookie;
	struct psycho_pbm *pp = (struct psycho_pbm *)pc->cookie;

	DPRINTF(SPDB_INTR, ("pci_intr_establish: ih %lu; level %d",
	    (u_long)ih, level));
	cookie = bus_intr_establish(pp->pp_memt, ih, level, 0, func, arg, what);

	DPRINTF(SPDB_INTR, ("; returning handle %p\n", cookie));
	return (cookie);
}

void
pci_intr_disestablish(pc, cookie)
	pci_chipset_tag_t pc;
	void *cookie;
{

	DPRINTF(SPDB_INTR, ("pci_intr_disestablish: cookie %p\n", cookie));

	/* XXX */
	panic("can't disestablish PCI interrupts yet");
}
