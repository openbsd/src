/*	$OpenBSD: pluto.c,v 1.1 2005/04/01 10:40:47 mickey Exp $	*/

/*
 * Copyright (c) 2005 Michael Shalayeff
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* TODO IOA programming */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/reboot.h>

#include <machine/iomod.h>
#include <machine/autoconf.h>
#include <machine/bus.h>

struct pluto_regs {
	u_int32_t	version;	/* 0x000: version */
	u_int32_t	pad00;
	u_int32_t	ctrl;		/* 0x008: control register */
#define	PLUTO_CTRL_TOC	0x0001	/* enable toc */
#define	PLUTO_CTRL_COE	0x0002	/* enable coalescing */
#define	PLUTO_CTRL_DIE	0x0004	/* enable dillon */
#define	PLUTO_CTRL_RM	0x0010	/* real mode */
#define	PLUTO_CTRL_NCO	0x0020	/* non-coherent mode */
	u_int32_t	pad08;
	u_int32_t	resv0[(0x200-0x10)/4];
	u_int64_t	rope0;		/* 0x200: */
	u_int64_t	rope1;
	u_int64_t	rope2;
	u_int64_t	rope3;
	u_int64_t	rope4;
	u_int64_t	rope5;
	u_int64_t	rope6;
	u_int64_t	rope7;
	u_int32_t	resv1[(0x100-0x40)/4];
	u_int64_t	ibase;
	u_int64_t	imask;
	u_int64_t	pcom;
	u_int64_t	tconf;
	u_int64_t	pdir;
} __packed;

struct pluto_softc {
	struct device sc_dv;

	struct pluto_regs volatile *sc_regs;
};

int	plutomatch(struct device *, void *, void *);
void	plutoattach(struct device *, struct device *, void *);

struct cfattach plut_ca = {
	sizeof(struct pluto_softc), plutomatch, plutoattach
};

struct cfdriver plut_cd = {
	NULL, "plut", DV_DULL
};

int
plutomatch(parent, cfdata, aux)   
	struct device *parent;
	void *cfdata;
	void *aux;
{
	struct confargs *ca = aux;
	/* struct cfdata *cf = cfdata; */

	if (strcmp(ca->ca_name, "sba"))
		return 0;

	return 1;
}

void
plutoattach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct confargs *ca = aux, nca;
	struct pluto_softc *sc = (struct pluto_softc *)self;
	struct pluto_regs volatile *r;
	bus_space_handle_t ioh;
	u_int32_t ver;
	char buf[16];

	if (bus_space_map(ca->ca_iot, ca->ca_hpa, ca->ca_hpasz, 0, &ioh)) {
		printf(": can't map IO space\n");
		return;
	}
	sc->sc_regs = r = (void *)bus_space_vaddr(ca->ca_iot, ioh);

	/* r->ctrl = PLUTO_CTRL_RM|PLUTO_CTRL_TOC; */

	ver = letoh32(r->version);
	switch ((ca->ca_type.iodc_model << 4) |
	    (ca->ca_type.iodc_revision >> 4)) {
	case 0x582:
	case 0x780:
		snprintf(buf, sizeof(buf), "Astro rev %d.%d",
		    (ver & 7) + 1, (ver >> 3) & 3);
		break;

	case 0x803:
	case 0x781:
		snprintf(buf, sizeof(buf), "Ike rev %d", ver & 0xff);
		break;

	case 0x804:
	case 0x782:
		snprintf(buf, sizeof(buf), "Reo rev %d", ver & 0xff);
		break;

	case 0x880:
	case 0x784:
		snprintf(buf, sizeof(buf), "Pluto rev %d.%d",
		    (ver >> 4) & 0xf, ver & 0xf);
		break;

	default:
		snprintf(buf, sizeof(buf), "Fluffy rev 0x%x", ver);
		break;
	}

	printf(": %s\n", buf);

	nca = *ca;	/* clone from us */
	pdc_patscan(self, &nca, 0);
}
