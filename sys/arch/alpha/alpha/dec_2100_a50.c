/*	$OpenBSD: dec_2100_a50.c,v 1.8 1999/01/11 05:10:58 millert Exp $	*/
/*	$NetBSD: dec_2100_a50.c,v 1.18 1996/11/25 03:59:19 cgd Exp $	*/

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
#include <sys/device.h>
#include <sys/termios.h>
#include <dev/cons.h>

#include <machine/rpb.h>
#include <machine/autoconf.h>
#include <machine/cpuconf.h>

#include <dev/isa/isavar.h>
#include <dev/ic/comreg.h>
#include <dev/ic/comvar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <alpha/pci/apecsreg.h>
#include <alpha/pci/apecsvar.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

cpu_decl(dec_2100_a50);

const char *
dec_2100_a50_model_name()
{
	static char s[80];

	switch (hwrpb->rpb_variation & SV_ST_MASK) {
	case SV_ST_AVANTI:
	case SV_ST_AVANTI_XXX:		/* XXX apparently the same? */
		return "AlphaStation 400 4/233 (\"Avanti\")";

	case SV_ST_MUSTANG2_4_166:
		return "AlphaStation 200 4/166 (\"Mustang II\")";

	case SV_ST_MUSTANG2_4_233:
		return "AlphaStation 200 4/233 (\"Mustang II\")";

	case 0x2000:
		return "AlphaStation 250 4/266";

	case SV_ST_MUSTANG2_4_100:
		return "AlphaStation 200 4/100 (\"Mustang II\")";

	case 0xa800:
		return "AlphaStation 255/233";

	default:
		sprintf(s, "DEC 2100/A50 (\"Avanti\") family, variation %lx",
		    hwrpb->rpb_variation & SV_ST_MASK);
		return s;
	}
}

void
dec_2100_a50_cons_init()
{
	struct ctb *ctb;
	struct apecs_config *acp;
	extern struct apecs_config apecs_configuration;

	acp = &apecs_configuration;
	apecs_init(acp, 0);

	ctb = (struct ctb *)(((caddr_t)hwrpb) + hwrpb->rpb_ctb_off);

	switch (ctb->ctb_term_type) {
	case 2: 
		/* serial console ... */
		/* XXX */
		{
			static struct consdev comcons = { NULL, NULL,
			    comcngetc, comcnputc, comcnpollc, NODEV, 1 };

			/* Delay to allow PROM putchars to complete */
			DELAY(10000);

			comconsaddr = 0x3f8;
			comconsinit = 0;
			comconsiot = acp->ac_iot;
			if (bus_space_map(comconsiot, comconsaddr, COM_NPORTS,
			    0, &comconsioh))
				panic("can't map serial console I/O ports");
			comconscflag = (TTYDEF_CFLAG & ~(CSIZE | PARENB)) | CS8;
			cominit(comconsiot, comconsioh, comdefaultrate);

			cn_tab = &comcons;
			comcons.cn_dev = makedev(26, 0);	/* XXX */
			break;
		}

	case 3:
		/* display console ... */
		/* XXX */
		if (ctb->ctb_turboslot == 0)
			isa_display_console(acp->ac_iot, acp->ac_memt);
		else
			pci_display_console(acp->ac_iot, acp->ac_memt,
			    &acp->ac_pc, (ctb->ctb_turboslot >> 8) & 0xff,
			    ctb->ctb_turboslot & 0xff, 0);
		break;

	default:
		printf("ctb->ctb_term_type = 0x%lx\n", ctb->ctb_term_type);
		printf("ctb->ctb_turboslot = 0x%lx\n", ctb->ctb_turboslot);

		panic("consinit: unknown console type %d",
		    ctb->ctb_term_type);
	}
}

const char *
dec_2100_a50_iobus_name()
{

	return ("apecs");
}

void
dec_2100_a50_device_register(dev, aux)
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
		netboot = (strcmp(b->protocol, "BOOTP") == 0);
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

			if (b->bus != pba->pba_bus)
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

			if (b->slot != pa->pa_device)
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

			if (b->slot != pa->pa_device)
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
