/*	$NetBSD: vga_isa.c,v 1.4 1996/12/05 01:39:32 cgd Exp $	*/

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

#include <machine/autoconf.h>
#include <machine/pte.h>

#include <dev/isa/isavar.h>

#include <alpha/common/vgavar.h>
#include <alpha/isa/vga_isavar.h>

struct vga_isa_softc {
	struct device sc_dev; 
 
	struct vga_config *sc_vc;	/* VGA configuration */ 
};

#ifdef __BROKEN_INDIRECT_CONFIG
int	vga_isa_match __P((struct device *, void *, void *));
#else
int	vga_isa_match __P((struct device *, struct cfdata *, void *));
#endif
void	vga_isa_attach __P((struct device *, struct device *, void *));

struct cfattach vga_isa_ca = {
	sizeof(struct vga_isa_softc), vga_isa_match, vga_isa_attach,
};

int vga_isa_console_tag;			/* really just a boolean. */
struct vga_config vga_isa_console_vc;

int
vga_isa_match(parent, match, aux)
	struct device *parent;
#ifdef __BROKEN_INDIRECT_CONFIG
	void *match;
#else
	struct cfdata *match;
#endif
	void *aux;
{
	struct isa_attach_args *ia = aux;
	int rv;

	/* If values are hardwired to something that they can't be, punt. */
	if (ia->ia_iobase != IOBASEUNK || /* ia->ia_iosize != 0 || XXX isa.c */
	    (ia->ia_maddr != MADDRUNK && ia->ia_maddr != 0xb8000) ||
	    (ia->ia_msize != 0 && ia->ia_msize != 0x8000) ||
	    ia->ia_irq != IRQUNK || ia->ia_drq != DRQUNK)
		return (0);

	if (vga_isa_console_tag)
		return (1);

	rv = vga_common_probe(ia->ia_iot, ia->ia_memt);

	if (rv) {
		ia->ia_iobase = 0x3b0;
		ia->ia_iosize = 0x30;
		ia->ia_maddr = 0xb8000;
		ia->ia_msize = 0x8000;
	}
	return (rv);
}

void
vga_isa_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct isa_attach_args *ia = aux;
	struct vga_isa_softc *sc = (struct vga_isa_softc *)self;
	struct vga_config *vc;
	int console;

	console = vga_isa_console_tag;
	if (console)
		vc = sc->sc_vc = &vga_isa_console_vc;
	else {
		vc = sc->sc_vc = (struct vga_config *)
		    malloc(sizeof(struct vga_config), M_DEVBUF, M_WAITOK);

		/* set up bus-independent VGA configuration */
		vga_common_setup(ia->ia_iot, ia->ia_memt, vc);
	}

	printf("\n");

	vga_wscons_attach(self, vc, console);
}

int
vga_isa_console_match(iot, memt)
	bus_space_tag_t iot, memt;
{

	return (vga_common_probe(iot, memt));
}

void
vga_isa_console_attach(iot, memt)
	bus_space_tag_t iot, memt;
{
	struct vga_config *vc = &vga_isa_console_vc;

	/* for later recognition */
	vga_isa_console_tag = 1;

	/* set up bus-independent VGA configuration */
	vga_common_setup(iot, memt, vc);

	vga_wscons_console(vc);
}
