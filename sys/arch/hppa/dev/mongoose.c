/*	$OpenBSD: mongoose.c,v 1.1 1999/04/20 20:21:51 mickey Exp $	*/

/*
 * Copyright (c) 1998,1999 Michael Shalayeff
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Michael Shalayeff.
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

#define MONGOOSE_DEBUG 9

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/reboot.h>

#include <machine/bus.h>
#include <machine/iomod.h>
#include <machine/autoconf.h>

#include <hppa/dev/cpudevs.h>

#include <dev/eisa/eisareg.h>
#include <dev/eisa/eisavar.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>

struct mongoose_softc {
	struct  device sc_dev;
	void *sc_ih;

	int sc_mod;
	bus_addr_t sc_hpa;		/* HPA */
	bus_space_tag_t sc_iot;		/* IO tag */
	bus_space_handle_t sc_ioha;	/* EISA Adapter IO handle */
	bus_space_handle_t sc_iohi;	/* Intr ACK register */

	struct hppa_eisa_chipset sc_ec;
	struct hppa_isa_chipset sc_ic;
};

union mongoose_attach_args {
	char *mongoose_name;
	struct eisabus_attach_args mongoose_eisa;
	struct isabus_attach_args mongoose_isa;
};

/* EISA Bus Adapter registers definitions */
#define	MONGOOSE_MONGOOSE	0x10000
#define	MONGOOSE_VERSION	0
#define	MONGOOSE_LOCK	1
#define	MONGOOSE_LIOWAIT	2
#define	MONGOOSE_SPEED	3
#define	MONGOOSE_INTACK	0x1f000
#define	MONGOOSE_INTACK_LEN	1
#define	MONGOOSE_IOMAP	0x100000

int	mgmatch __P((struct device *, void *, void *));
void	mgattach __P((struct device *, struct device *, void *));

struct cfattach mongoose_ca = {
	sizeof(struct mongoose_softc), mgmatch, mgattach
};

struct cfdriver mongoose_cd = {
	NULL, "mg", DV_DULL
};

int mgprint __P((void *aux, const char *pnp));
void	mongoose_eisa_attach_hook __P((struct device *, struct device *,
				  struct eisabus_attach_args *));
int	mongoose_intr_map __P((void *, u_int, eisa_intr_handle_t *));
const char *mongoose_intr_string __P((void *, int));
void	*mongoose_intr_establish __P((void *, int, int, int,
				 int (*) __P((void *)), void *, char *));
void	mongoose_intr_disestablish __P((void *, void *));

void	mongoose_isa_attach_hook __P((struct device *, struct device *,
				 struct isabus_attach_args *));
int	mongoose_intr_check __P((void *, int, int));
int	mongoose_intr __P((void *v));

int
mgmatch(parent, cfdata, aux)   
	struct device *parent;
	void *cfdata;
	void *aux;
{
	register struct confargs *ca = aux;
	/* struct cfdata *cf = cfdata; */
	bus_space_handle_t ioh;

	if (ca->ca_type.iodc_type != HPPA_TYPE_BHA ||
	    ca->ca_type.iodc_sv_model != HPPA_BHA_EISA)
		return 0;

	if (bus_space_map(ca->ca_iot, ca->ca_hpa + MONGOOSE_MONGOOSE, IOMOD_HPASIZE,
			  0, &ioh))
		return 0;

	/* XXX check EISA signature */

	bus_space_unmap(ca->ca_iot, ioh, IOMOD_HPASIZE);

	return 1;
}

void
mgattach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	register struct confargs *ca = aux;
	register struct mongoose_softc *sc = (struct mongoose_softc *)self;
	union mongoose_attach_args ea;
	u_int ver;

	sc->sc_mod = ca->ca_mod;
	sc->sc_iot = ca->ca_iot;
	sc->sc_hpa = ca->ca_hpa;
	if (bus_space_map(sc->sc_iot, sc->sc_hpa + MONGOOSE_MONGOOSE,
			  IOMOD_HPASIZE, 0, &sc->sc_ioha))
		panic("mgattach: unable to map adapter bus space");

	if (bus_space_map(sc->sc_iot, sc->sc_hpa + MONGOOSE_INTACK,
			  MONGOOSE_INTACK_LEN, 0, &sc->sc_iohi))
		panic("mgattach: unable to map intack bus space");

	/* XXX should we reset the chip here? */

	/* attach interrupt */
	sc->sc_ih = cpu_intr_establish(IPL_HIGH, ca->ca_irq,
				       mongoose_intr, sc, "eisa");

	/* XXX determine HP eisa board id (how?) */
	ver = bus_space_read_1(sc->sc_iot, sc->sc_ioha, MONGOOSE_VERSION);
	printf (": rev %d, %d MHz\n", ver & 0xff,
		(bus_space_read_1(sc->sc_iot, sc->sc_ioha, MONGOOSE_SPEED)? 33:25));

	/* disable isa wait states */
	bus_space_write_1(sc->sc_iot, sc->sc_ioha, MONGOOSE_LIOWAIT, 1);
	/* bus unlock */
	bus_space_write_1(sc->sc_iot, sc->sc_ioha, MONGOOSE_LOCK, 1);

	/* attach EISA */
	sc->sc_ec.ec_v = sc;
	sc->sc_ec.ec_attach_hook = mongoose_eisa_attach_hook;
	sc->sc_ec.ec_intr_establish = mongoose_intr_establish;
	sc->sc_ec.ec_intr_disestablish = mongoose_intr_disestablish;
	sc->sc_ec.ec_intr_string = mongoose_intr_string;
	sc->sc_ec.ec_intr_map = mongoose_intr_map;
	ea.mongoose_eisa.eba_busname = "eisa";
	ea.mongoose_eisa.eba_iot = HPPA_BUS_TAG_SET_BASE(sc->sc_iot,sc->sc_hpa);
	ea.mongoose_eisa.eba_memt = ca->ca_iot;
	ea.mongoose_eisa.eba_ec = &sc->sc_ec;
	config_found(self, &ea.mongoose_eisa, mgprint);

	/* attach ISA */
	sc->sc_ic.ic_v = sc;
	sc->sc_ic.ic_attach_hook = mongoose_isa_attach_hook;
	sc->sc_ic.ic_intr_establish = mongoose_intr_establish;
	sc->sc_ic.ic_intr_disestablish = mongoose_intr_disestablish;
	sc->sc_ic.ic_intr_check = mongoose_intr_check;
	ea.mongoose_isa.iba_busname = "isa";
	ea.mongoose_isa.iba_iot = HPPA_BUS_TAG_SET_BASE(sc->sc_iot, sc->sc_hpa);
	ea.mongoose_isa.iba_memt = ca->ca_iot;
	ea.mongoose_isa.iba_ic = &sc->sc_ic;
	config_found(self, &ea.mongoose_isa, mgprint);
}

int
mgprint(aux, pnp)
	void *aux;
	const char *pnp;
{
	union mongoose_attach_args *ea = aux;

	if (pnp)
		printf ("%s at %s", ea->mongoose_name, pnp);

	return (UNCONF);
}

void
mongoose_eisa_attach_hook(parent, self, mg)
	struct device *parent;
	struct device *self;
	struct eisabus_attach_args *mg;
{
#ifdef MONGOOSE_DEBUG
	register struct mongoose_softc *sc = (struct mongoose_softc *)self;
	struct pdc_memmap pdc_memmap;
	struct device_path dp;
	register int i;

	for (i = 0; i < 16; i++) {
		dp.dp_bc[0] = dp.dp_bc[1] = dp.dp_bc[2] = dp.dp_bc[3] = -1;
		dp.dp_bc[4] = sc->sc_mod;
		dp.dp_bc[5] = 0;
		dp.dp_mod = i;

		if (pdc_call((iodcio_t)pdc, 0, PDC_MEMMAP,
			     PDC_MEMMAP_HPA, &pdc_memmap, &dp) < 0)
			continue;

		printf ("eisa%d: hpa %x\n", i, pdc_memmap.hpa);
	}
#endif
}

int
mongoose_intr_map(v, irq, ehp)
	void *v;
	u_int irq;
	eisa_intr_handle_t *ehp;
{
	*ehp = irq;
	return 0;
}

const char *
mongoose_intr_string(v, irq)
	void *v;
	int irq;
{
	static char buf[16];

	sprintf (buf, "isa irq %d", irq);
	return buf;
}

void *
mongoose_intr_establish(v, irq, type, level, fn, arg, name)
	void *v;
	int irq;
	int type;
	int level;
	int (*fn) __P((void *));
	void *arg;
	char *name;
{
	void *cookie = "cookie";

	
	return cookie;
}

void
mongoose_intr_disestablish(v, cookie)
	void *v;
	void *cookie;
{

}

void
mongoose_isa_attach_hook(parent, self, iba)
	struct device *parent;
	struct device *self;
	struct isabus_attach_args *iba;
{

}

int
mongoose_intr_check(v, irq, type)
	void *v;
	int irq;
	int type;
{
	return 0;
}

int
mongoose_intr(v)
	void *v;
{
	return 0;
}

