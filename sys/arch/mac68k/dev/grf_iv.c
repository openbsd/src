/*	$OpenBSD: grf_iv.c,v 1.21 2004/12/03 06:40:13 miod Exp $	*/
/*	$NetBSD: grf_iv.c,v 1.17 1997/02/20 00:23:27 scottr Exp $	*/

/*
 * Copyright (c) 1995 Allen Briggs.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Allen Briggs.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Graphics display driver for the Macintosh internal video for machines
 * that don't map it into a fake nubus card.
 */

#include <sys/param.h>

#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <sys/malloc.h>
#include <sys/mman.h>
#include <sys/proc.h>
#include <sys/systm.h>

#include <machine/autoconf.h>
#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/grfioctl.h>
#include <machine/viareg.h>

#include "nubus.h"
#include "obiovar.h"
#include "grfvar.h"

extern u_int32_t	mac68k_vidlog;
extern u_int32_t	mac68k_vidphys;
extern long		videorowbytes;
extern long		videobitdepth;
extern unsigned long	videosize;

static int	grfiv_mode(struct grf_softc *gp, int cmd, void *arg);
static caddr_t	grfiv_phys(struct grf_softc *gp, vm_offset_t addr);
static int	grfiv_match(struct device *, void *, void *);
static void	grfiv_attach(struct device *, struct device *, void *);

struct cfdriver intvid_cd = {
	NULL, "intvid", DV_DULL
};

struct cfattach intvid_ca = {
	sizeof(struct grfbus_softc), grfiv_match, grfiv_attach
};

#define QUADRA_DAFB_BASE	0xF9800000

static int
grfiv_match(parent, vcf, aux)
	struct device *parent;
	void *vcf;
	void *aux;
{
	struct obio_attach_args *oa = (struct obio_attach_args *) aux;
	bus_space_handle_t	bsh;
	int			found, sense;

	found = 1;

        switch (current_mac_model->class) {
	case MACH_CLASSQ:
	case MACH_CLASSQ2:

		/*
		 * Assume DAFB for all of these, unless we can't
		 * access the memory.
		 */

		if (bus_space_map(oa->oa_tag, QUADRA_DAFB_BASE, 0x1000,
					0, &bsh)) {
			panic("failed to map space for DAFB regs.");
		}

		if (mac68k_bus_space_probe(oa->oa_tag, bsh, 0x1C, 4) == 0) {
			bus_space_unmap(oa->oa_tag, bsh, 0x1000);
			found = 0;
			goto nodafb;
		}

		sense = (bus_space_read_4(oa->oa_tag, bsh, 0x1C) & 7);

#if 0	/* when using a non-Apple monitor with an adapter, sense is zero */
		if (sense == 0)
			found = 0;
#endif

		/* Set "Turbo SCSI" configuration to default */
		bus_space_write_4(oa->oa_tag, bsh, 0x24, 0x1d1); /* ch0 */
		bus_space_write_4(oa->oa_tag, bsh, 0x28, 0x1d1); /* ch1 */

		/* Disable interrupts */
		bus_space_write_4(oa->oa_tag, bsh, 0x104, 0);

		/* Clear any interrupts */
		bus_space_write_4(oa->oa_tag, bsh, 0x10C, 0);
		bus_space_write_4(oa->oa_tag, bsh, 0x110, 0);
		bus_space_write_4(oa->oa_tag, bsh, 0x114, 0);

		bus_space_unmap(oa->oa_tag, bsh, 0x1000);

		break;

	default:
nodafb:
		if (mac68k_vidlog == 0) {
			found = 0;
		}

		break;
	}

	return found;
}

#define R4(sc, o) (bus_space_read_4((sc)->sc_tag, (sc)->sc_regh, o) & 0xfff)

static void
grfiv_attach(parent, self, aux)
	struct device *parent, *self;
	void   *aux;
{
	struct obio_attach_args *oa = (struct obio_attach_args *) aux;
	struct grfbus_softc	*sc;
	struct grfmode		*gm;

	sc = (struct grfbus_softc *) self;

	sc->card_id = 0;

        switch (current_mac_model->class) {
        case MACH_CLASSQ:
        case MACH_CLASSQ2:
		sc->sc_tag = oa->oa_tag;
		if (bus_space_map(sc->sc_tag, QUADRA_DAFB_BASE, 0x1000,
					0, &sc->sc_regh)) {
			panic("failed to map space for DAFB regs.");
		}
		printf(": DAFB: Monitor sense %x.\n", R4(sc,0x1C)&7);
		break;
	default:
		printf(": Internal Video\n");
		break;
	}

	gm = &(sc->curr_mode);
	gm->mode_id = 0;
	gm->psize = videobitdepth;
	gm->ptype = 0;
	gm->width = videosize & 0xffff;
	gm->height = (videosize >> 16) & 0xffff;
	gm->rowbytes = videorowbytes;
	gm->hres = 80;		/* XXX Hack */
	gm->vres = 80;		/* XXX Hack */
	gm->fbsize = gm->rowbytes * gm->height;
	gm->fbbase = (caddr_t) m68k_trunc_page(mac68k_vidlog);
	gm->fboff = mac68k_vidlog & PGOFSET;

	/* Perform common video attachment. */
	grf_establish(sc, NULL, grfiv_mode, grfiv_phys);
}

static int
grfiv_mode(sc, cmd, arg)
	struct grf_softc *sc;
	int cmd;
	void *arg;
{
	switch (cmd) {
	case GM_GRFON:
	case GM_GRFOFF:
		return 0;
	case GM_CURRMODE:
		break;
	case GM_NEWMODE:
		break;
	case GM_LISTMODES:
		break;
	}
	return EINVAL;
}

static caddr_t
grfiv_phys(gp, addr)
	struct grf_softc *gp;
	vm_offset_t addr;
{
	/*
	 * If we're using IIsi or similar, this will be 0.
	 * If we're using IIvx or similar, this will be correct.
	 */
	return (caddr_t) mac68k_vidphys;
}
