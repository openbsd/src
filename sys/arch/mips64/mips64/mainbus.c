/*	$OpenBSD: mainbus.c,v 1.4 2005/01/31 21:35:50 grange Exp $ */

/*
 * Copyright (c) 2001-2003 Opsycon AB  (www.opsycon.se / www.opsycon.com)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/reboot.h>

#include <mips64/archtype.h>
#include <machine/autoconf.h>

struct mainbus_softc {
	struct	device sc_dv;
	struct	abus sc_bus;
};

/* Definition of the mainbus driver. */
static int	mbmatch(struct device *, void *, void *);
static void	mbattach(struct device *, struct device *, void *);
static int	mbprint(void *, const char *);

struct cfattach mainbus_ca = {
	sizeof(struct mainbus_softc), mbmatch, mbattach
};
struct cfdriver mainbus_cd = {
	NULL, "mainbus", DV_DULL, NULL, 0
};

void	*mb_intr_establish(void *, u_long, int, int, int (*)(void *),
	    void *, char *);
void	mb_intr_disestablish(void *, void *);
caddr_t	mb_cvtaddr(struct confargs *);
int	mb_matchname(struct confargs *, char *);

static int
mbmatch(parent, cfdata, aux)
	struct device *parent;
	void *cfdata;
	void *aux;
{
	struct cfdata *cf = cfdata;

	if (cf->cf_unit > 0)
		return(0);
	return(1);
}

static void
mbattach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct mainbus_softc *sc = (struct mainbus_softc *)self;
	struct confargs nca;

	printf("\n");

	sc->sc_bus.ab_dv = (struct device *)sc;
	sc->sc_bus.ab_type = BUS_MAIN;
	sc->sc_bus.ab_intr_establish = mb_intr_establish;
	sc->sc_bus.ab_intr_disestablish = mb_intr_disestablish;
	sc->sc_bus.ab_cvtaddr = mb_cvtaddr;
	sc->sc_bus.ab_matchname = mb_matchname;

	/*
	 * Try to find and attach all of the CPUs in the machine.
	 * ( Right now only one CPU so code is simple )
	 */

	nca.ca_name = "cpu";
	nca.ca_bus = &sc->sc_bus;
	config_found(self, &nca, mbprint);

	/*
	 *  Attach the clocks.
	 */
	nca.ca_name = "clock";
	nca.ca_bus = &sc->sc_bus;
	config_found(self, &nca, mbprint);

	if (sys_config.system_type == SGI_INDY) {
		nca.ca_name = "indy";
		nca.ca_bus = &sc->sc_bus;
		config_found(self, &nca, mbprint);
	}
	else if (sys_config.system_type == SGI_O2) {
		nca.ca_name = "macebus";
		nca.ca_bus = &sc->sc_bus;
		config_found(self, &nca, mbprint);

		nca.ca_name = "macepcibr";
		nca.ca_bus = &sc->sc_bus;
		nca.ca_num = 0;
		config_found(self, &nca, mbprint);
	}
	else if (sys_config.system_type == ALGOR_P4032 ||
            sys_config.system_type == ALGOR_P5064 ||
	    sys_config.system_type == MOMENTUM_CP7000 ||
	    sys_config.system_type == MOMENTUM_CP7000G ||
	    sys_config.system_type == MOMENTUM_JAGUAR ||
	    sys_config.system_type == GALILEO_EV64240) {

		nca.ca_name = "localbus";
		nca.ca_bus = &sc->sc_bus;
		config_found(self, &nca, mbprint);

		nca.ca_name = "pcibr";
		nca.ca_bus = &sc->sc_bus;
		nca.ca_num = 0;
		config_found(self, &nca, mbprint);

		nca.ca_name = "pcibr";
		nca.ca_bus = &sc->sc_bus;
		nca.ca_num = 1;
		config_found(self, &nca, mbprint);
	}
}

static int
mbprint(aux, pnp)
	void *aux;
	const char *pnp;
{
	if (pnp)
		return (QUIET);
	return (UNCONF);
}

void *
mb_intr_establish(icp, irq, type, level, ih_fun, ih_arg, ih_what)
        void *icp;
        u_long irq;     /* XXX pci_intr_handle_t compatible XXX */
        int type;
        int level;
        int (*ih_fun)(void *);
        void *ih_arg;
        char *ih_what;
{
	panic("can never mb_intr_establish");
}

void
mb_intr_disestablish(void *p1, void *p2)
{
	panic("can never mb_intr_disestablish");
}

caddr_t
mb_cvtaddr(ca)
	struct confargs *ca;
{
	return (NULL);
}

int
mb_matchname(ca, name)
	struct confargs *ca;
	char *name;
{
	return (strcmp(name, ca->ca_name) == 0);
}
