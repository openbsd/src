/*	$OpenBSD: eba.c,v 1.1 1998/11/23 03:01:43 mickey Exp $	*/

/*
 * Copyright (c) 1998 Michael Shalayeff
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

struct eba_softc {
	struct  device sc_dv;
	u_int32_t  sc_in;

	bus_addr_t sc_hpa;		/* HPA */
	bus_space_tag_t sc_iot;		/* IO tag */
	bus_space_handle_t sc_ioha;	/* EISA Adapter IO handle */
	bus_space_handle_t sc_iohi;	/* Intr ACK register */

	struct hppa_eisa_chipset sc_ec;
	struct hppa_isa_chipset sc_ic;
};

union eba_attach_args {
	char *eba_name;
	struct eisabus_attach_args eba_eisa;
	struct isabus_attach_args eba_isa;
};

/* EISA Bus Adapter registers definitions */
#define	EBA_MONGOOSE	0x10000
#define	EBA_VERSION	0
#define	EBA_LOCK	1
#define	EBA_LIOWAIT	2
#define	EBA_SPEED	3
#define	EBA_INTACK	0x1f000
#define	EBA_INTACK_LEN	1
#define	EBA_IOMAP	0x100000

int	ebamatch __P((struct device *, void *, void *));
void	ebaattach __P((struct device *, struct device *, void *));

struct cfattach eba_ca = {
	sizeof(struct eba_softc), ebamatch, ebaattach
};

struct cfdriver eba_cd = {
	NULL, "eba", DV_DULL
};

int ebaprint __P((void *aux, const char *pnp));
void	eba_eisa_attach_hook __P((struct device *, struct device *,
				  struct eisabus_attach_args *));
int	eba_intr_map __P((void *, u_int, eisa_intr_handle_t *));
const char *eba_intr_string __P((void *, int));
void	*eba_intr_establish __P((void *, int, int, int,
				 int (*) __P((void *)), void *, char *));
void	eba_intr_disestablish __P((void *, void *));

void	eba_isa_attach_hook __P((struct device *, struct device *,
				 struct isabus_attach_args *));
int	eba_intr_check __P((void *, int, int));
int	eba_intr __P((void *v));

int
ebamatch(parent, cfdata, aux)   
	struct device *parent;
	void *cfdata;
	void *aux;
{
	struct confargs *ca = aux;
	/* struct cfdata *cf = cfdata; */
	register bus_space_tag_t iot;
	bus_space_handle_t ioh;

	if (ca->ca_type.iodc_type != HPPA_TYPE_BHA ||
	    ca->ca_type.iodc_sv_model != HPPA_BHA_EISA)
		return 0;

	iot = HPPA_BUS_TAG_SET_BYTE(ca->ca_iot);
	if (bus_space_map(iot, ca->ca_hpa + EBA_MONGOOSE, IOMOD_HPASIZE,
			  0, &ioh))
		return 0;

	/* TODO: check EISA signature */

	bus_space_unmap(iot, ioh, IOMOD_HPASIZE);

	return 1;
}

void
ebaattach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	register struct confargs *ca = aux;
	register struct eba_softc *sc = (struct eba_softc *)self;
	union eba_attach_args ea;
	u_int ver;

	sc->sc_hpa = ca->ca_hpa;
	sc->sc_iot = HPPA_BUS_TAG_SET_BYTE(ca->ca_iot);
	if (bus_space_map(sc->sc_iot, sc->sc_hpa + EBA_MONGOOSE,
			  IOMOD_HPASIZE, 0, &sc->sc_ioha))
		panic("ebaattach: unable to map adapter bus space");

	if (bus_space_map(sc->sc_iot, sc->sc_hpa + EBA_INTACK,
			  EBA_INTACK_LEN, 0, &sc->sc_iohi))
		panic("ebaattach: unable to map intack bus space");

	/* XXX should we reset the chip here? */

	/* attach interrupt */
	sc->sc_in = cpu_intr_establish(IPL_HIGH, eba_intr, sc, "eisa");
	/* TODO: setup ctrl to deliver on that int bit */

	ver = bus_space_read_1(sc->sc_iot, sc->sc_ioha, EBA_VERSION);
	printf (": rev %d, %d MHz, hpa 0x%x, irq %d\n", ver & 0xff,
		(bus_space_read_1(sc->sc_iot, sc->sc_ioha, EBA_SPEED)? 33: 25),
		ca->ca_hpa, sc->sc_in);

	/* disable isa wait states */
	bus_space_write_1(sc->sc_iot, sc->sc_ioha, EBA_LIOWAIT, 1);
	/* bus unlock */
	bus_space_write_1(sc->sc_iot, sc->sc_ioha, EBA_LOCK, 1);

	/* attach EISA */
	sc->sc_ec.ec_v = sc;
	sc->sc_ec.ec_attach_hook = eba_eisa_attach_hook;
	sc->sc_ec.ec_intr_establish = eba_intr_establish;
	sc->sc_ec.ec_intr_disestablish = eba_intr_disestablish;
	sc->sc_ec.ec_intr_string = eba_intr_string;
	sc->sc_ec.ec_intr_map = eba_intr_map;
	ea.eba_eisa.eba_busname = "eisa";
	ea.eba_eisa.eba_iot = HPPA_BUS_TAG_SET_BASE(sc->sc_iot, sc->sc_hpa);
	ea.eba_eisa.eba_memt = ca->ca_iot;
	ea.eba_eisa.eba_ec = &sc->sc_ec;
	config_found(self, &ea.eba_eisa, ebaprint);

	/* attach ISA */
	sc->sc_ic.ic_v = sc;
	sc->sc_ic.ic_attach_hook = eba_isa_attach_hook;
	sc->sc_ic.ic_intr_establish = eba_intr_establish;
	sc->sc_ic.ic_intr_disestablish = eba_intr_disestablish;
	sc->sc_ic.ic_intr_check = eba_intr_check;
	ea.eba_isa.iba_busname = "isa";
	ea.eba_isa.iba_iot = HPPA_BUS_TAG_SET_BASE(sc->sc_iot, sc->sc_hpa);
	ea.eba_isa.iba_memt = ca->ca_iot;
	ea.eba_isa.iba_ic = &sc->sc_ic;
	config_found(self, &ea.eba_isa, ebaprint);
}

int
ebaprint(aux, pnp)
	void *aux;
	const char *pnp;
{
	union eba_attach_args *ea = aux;

	if (pnp)
		printf ("%s at %s", ea->eba_name, pnp);

	return (UNCONF);
}

void
eba_eisa_attach_hook(parent, self, eba)
	struct device *parent;
	struct device *self;
	struct eisabus_attach_args *eba;
{

}

int
eba_intr_map(v, irq, ehp)
	void *v;
	u_int irq;
	eisa_intr_handle_t *ehp;
{
	*ehp = irq;
	return 0;
}

const char *
eba_intr_string(v, irq)
	void *v;
	int irq;
{
	static char buf[16];

	sprintf (buf, "isa irq %d", irq);
	return buf;
}

void *
eba_intr_establish(v, irq, type, level, fn, arg, name)
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
eba_intr_disestablish(v, cookie)
	void *v;
	void *cookie;
{

}

void
eba_isa_attach_hook(parent, self, iba)
	struct device *parent;
	struct device *self;
	struct isabus_attach_args *iba;
{

}

int
eba_intr_check(v, irq, type)
	void *v;
	int irq;
	int type;
{
	return 0;
}

int
eba_intr(v)
	void *v;
{
	return 0;
}

