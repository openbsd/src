/* $OpenBSD: dec_axppci_33.c,v 1.14 2001/12/13 19:13:22 nate Exp $ */
/* $NetBSD: dec_axppci_33.c,v 1.44 2000/05/22 20:13:32 thorpej Exp $ */

/*
 * Copyright (c) 1995, 1996, 1997 Carnegie-Mellon University.
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
 * Additional Copyright (c) 1997 by Matthew Jacob for NASA/Ames Research Center
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/termios.h>
#include <dev/cons.h>
#include <sys/conf.h>

#include <machine/rpb.h>
#include <machine/autoconf.h>
#include <machine/cpuconf.h>

#include <dev/ic/comreg.h>
#include <dev/ic/comvar.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>
#include <dev/ic/i8042reg.h>
#include <dev/ic/pckbcvar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <alpha/pci/lcareg.h>
#include <alpha/pci/lcavar.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

#include "pckbd.h"

#ifndef CONSPEED
#define CONSPEED TTYDEF_SPEED
#endif
static int comcnrate = CONSPEED;

void dec_axppci_33_init __P((void));
static void dec_axppci_33_cons_init __P((void));
static void dec_axppci_33_device_register __P((struct device *, void *));

const struct alpha_variation_table dec_axppci_33_variations[] = {
	{ 0, "Alpha PC AXPpci33 (\"NoName\")" },
	{ 0, NULL },
};

static struct lca_config *lca_preinit __P((void));

static struct lca_config *
lca_preinit()
{
	extern struct lca_config lca_configuration;

	lca_init(&lca_configuration, 0);
	return &lca_configuration;
}

#define	NSIO_PORT  0x26e	/* Hardware enabled option: 0x398 */
#define	NSIO_BASE  0
#define	NSIO_INDEX NSIO_BASE
#define	NSIO_DATA  1
#define	NSIO_SIZE  2
#define	NSIO_CFG0  0
#define	NSIO_CFG1  1
#define	NSIO_CFG2  2
#define	NSIO_IDE_ENABLE 0x40

void
dec_axppci_33_init()
{
	int cfg0val;
	u_int64_t variation;
	bus_space_tag_t iot;
	struct lca_config *lcp;
	bus_space_handle_t nsio;
#define	A33_NSIOBARRIER(type) bus_space_barrier(iot, nsio,\
				NSIO_BASE, NSIO_SIZE, (type))

	platform.family = "DEC AXPpci";

	if ((platform.model = alpha_dsr_sysname()) == NULL) {
		variation = hwrpb->rpb_variation & SV_ST_MASK;
		if ((platform.model = alpha_variation_name(variation,
		    dec_axppci_33_variations)) == NULL)
			platform.model = alpha_unknown_sysname();
	}

	platform.iobus = "lca";
	platform.cons_init = dec_axppci_33_cons_init;
	platform.device_register = dec_axppci_33_device_register;

	lcp = lca_preinit();
	iot = lcp->lc_iot;
	if (bus_space_map(iot, NSIO_PORT, NSIO_SIZE, 0, &nsio))
		return;

	bus_space_write_1(iot, nsio, NSIO_INDEX, NSIO_CFG0);
	A33_NSIOBARRIER(BUS_BARRIER_READ | BUS_BARRIER_WRITE);
	cfg0val = bus_space_read_1(iot, nsio, NSIO_DATA);

	cfg0val |= NSIO_IDE_ENABLE;

	bus_space_write_1(iot, nsio, NSIO_INDEX, NSIO_CFG0);
	A33_NSIOBARRIER(BUS_BARRIER_WRITE);
	bus_space_write_1(iot, nsio, NSIO_DATA, cfg0val);
	A33_NSIOBARRIER(BUS_BARRIER_WRITE);
	bus_space_write_1(iot, nsio, NSIO_DATA, cfg0val);

	/* Leave nsio mapped to catch any accidental port space collisions  */
}

static void
dec_axppci_33_cons_init()
{
	struct ctb *ctb;
	struct lca_config *lcp;

	lcp = lca_preinit();

	ctb = (struct ctb *)(((caddr_t)hwrpb) + hwrpb->rpb_ctb_off);

	switch (ctb->ctb_term_type) {
	case CTB_PRINTERPORT: 
		/* serial console ... */
		/* XXX */
		{
			/*
			 * Delay to allow PROM putchars to complete.
			 * FIFO depth * character time,
			 * character time = (1000000 / (defaultrate / 10))
			 */
			DELAY(160000000 / comcnrate);

			if(comcnattach(lcp->lc_iot, 0x3f8, comcnrate,
			    COM_FREQ,
			    (TTYDEF_CFLAG & ~(CSIZE | PARENB)) | CS8))
				panic("can't init serial console");

			break;
		}

	case CTB_GRAPHICS:
#if NPCKBD > 0
		/* display console ... */
		/* XXX */
		(void) pckbc_cnattach(lcp->lc_iot, IO_KBD, KBCMDP,
		    PCKBC_KBD_SLOT);

		if (CTB_TURBOSLOT_TYPE(ctb->ctb_turboslot) ==
		    CTB_TURBOSLOT_TYPE_ISA)
			isa_display_console(lcp->lc_iot, lcp->lc_memt);
		else
			pci_display_console(lcp->lc_iot, lcp->lc_memt,
			    &lcp->lc_pc, CTB_TURBOSLOT_BUS(ctb->ctb_turboslot),
			    CTB_TURBOSLOT_SLOT(ctb->ctb_turboslot), 0);
#else
		panic("not configured to use display && keyboard console");
#endif
		break;

	default:
		printf("ctb->ctb_term_type = 0x%lx\n", ctb->ctb_term_type);
		printf("ctb->ctb_turboslot = 0x%lx\n", ctb->ctb_turboslot);

		panic("consinit: unknown console type %ld\n",
		    ctb->ctb_term_type);
	}
}

static void
dec_axppci_33_device_register(dev, aux)
	struct device *dev;
	void *aux;
{
	static int found, initted, scsiboot, netboot;
	static struct device *pcidev, *scsidev;
	struct bootdev_data *b = bootdev_data;
	struct device *parent = dev->dv_parent;
	struct cfdata *cf = dev->dv_cfdata;
	struct cfdriver *cd = cf->cf_driver;

	if (found)
		return;

	if (!initted) {
		scsiboot = (strcmp(b->protocol, "SCSI") == 0);
		netboot = (strcmp(b->protocol, "BOOTP") == 0) ||
		    (strcmp(b->protocol, "MOP") == 0);
#if 0
		printf("scsiboot = %d, netboot = %d\n", scsiboot, netboot);
#endif
		initted =1;
	}

	if (pcidev == NULL) {
		if (strcmp(cd->cd_name, "pci"))
			return;
		else {
			struct pcibus_attach_args *pba = aux;

			if ((b->slot / 1000) != pba->pba_bus)
				return;
	
			pcidev = dev;
#if 0
			printf("\npcidev = %s\n", pcidev->dv_xname);
#endif
			return;
		}
	}

	if (scsiboot && (scsidev == NULL)) {
		if (parent != pcidev)
			return;
		else {
			struct pci_attach_args *pa = aux;

			if ((b->slot % 1000) != pa->pa_device)
				return;

			/* XXX function? */
	
			scsidev = dev;
#if 0
			printf("\nscsidev = %s\n", scsidev->dv_xname);
#endif
			return;
		}
	}

	if (scsiboot &&
	    (!strcmp(cd->cd_name, "sd") ||
	     !strcmp(cd->cd_name, "st") ||
	     !strcmp(cd->cd_name, "cd"))) {
		struct scsibus_attach_args *sa = aux;

		if (parent->dv_parent != scsidev)
			return;

		if (b->unit / 100 != sa->sa_sc_link->target)
			return;

		/* XXX LUN! */

		switch (b->boot_dev_type) {
		case 0:
			if (strcmp(cd->cd_name, "sd") &&
			    strcmp(cd->cd_name, "cd"))
				return;
			break;
		case 1:
			if (strcmp(cd->cd_name, "st"))
				return;
			break;
		default:
			return;
		}

		/* we've found it! */
		booted_device = dev;
#if 0
		printf("\nbooted_device = %s\n", booted_device->dv_xname);
#endif
		found = 1;
	}

	if (netboot) {
		if (parent != pcidev)
			return;
		else {
			struct pci_attach_args *pa = aux;

			if ((b->slot % 1000) != pa->pa_device)
				return;

			/* XXX function? */
	
			booted_device = dev;
#if 0
			printf("\nbooted_device = %s\n", booted_device->dv_xname);
#endif
			found = 1;
			return;
		}
	}
}
