/*	$NetBSD: dec_axppci_33.c,v 1.1 1995/11/23 02:33:55 cgd Exp $	*/

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
#include <sys/device.h>
#include <dev/cons.h>

#include <machine/rpb.h>

#include <dev/isa/isavar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <alpha/pci/lcareg.h>
#include <alpha/pci/lcavar.h>

#include <alpha/alpha/dec_axppci_33.h>

char *
dec_axppci_33_modelname()
{

	switch (hwrpb->rpb_variation & SV_ST_MASK) {
	case 0:						/* XXX */
		return "Alpha PC AXPpci33 (\"NoName\")";
		
	default:
		printf("unknown system variation %lx\n",
		    hwrpb->rpb_variation & SV_ST_MASK);
		return NULL;
	}
}

void
dec_axppci_33_consinit(constype)
	char *constype;
{
	struct ctb *ctb;
	struct lca_config *lcp;
	extern struct lca_config lca_configuration;

	lcp = &lca_configuration;
	lca_init(lcp);

	ctb = (struct ctb *)(((caddr_t)hwrpb) + hwrpb->rpb_ctb_off);

	printf("constype = %s\n", constype);
	printf("ctb->ctb_term_type = 0x%lx\n", ctb->ctb_term_type);
	printf("ctb->ctb_turboslot = 0x%lx\n", ctb->ctb_turboslot);

	switch (ctb->ctb_term_type) {
	case 2: 
		/* serial console ... */
		/* XXX */
		{
			extern int comdefaultrate, comconsole;
			extern int comconsaddr, comconsinit;
			extern int comcngetc __P((dev_t));
			extern void comcnputc __P((dev_t, int));
			extern void comcnpollc __P((dev_t, int));
			extern __const struct isa_pio_fns *comconsipf;
			extern __const void *comconsipfa;
			static struct consdev comcons = { NULL, NULL,
			    comcngetc, comcnputc, comcnpollc, NODEV, 1 };


			cominit(lcp->lc_piofns, lcp->lc_pioarg, 0,
			    comdefaultrate);
			comconsole = 0;				/* XXX */
			comconsaddr = 0x3f8;			/* XXX */
			comconsinit = 0;
			comconsipf = lcp->lc_piofns;
			comconsipfa = lcp->lc_pioarg;

			cn_tab = &comcons;
			comcons.cn_dev = makedev(26, 0);	/* XXX */
			break;
		}

	case 3:
		/* display console ... */
		/* XXX */
		pci_display_console(lcp->lc_conffns, lcp->lc_confarg,
		    lcp->lc_memfns, lcp->lc_memarg, lcp->lc_piofns,
		    lcp->lc_pioarg, 0, ctb->ctb_turboslot & 0xffff, 0);
		break;

	default:
		panic("consinit: unknown console type %d\n",
		    ctb->ctb_term_type);
	}
}

dev_t
dec_axppci_33_bootdev(booted_dev)
	char *booted_dev;
{

	panic("gack.");
}
