/* $OpenBSD: radio.c,v 1.2 2001/12/05 10:27:06 mickey Exp $ */
/* $RuOBSD: radio.c,v 1.7 2001/12/04 06:03:05 tm Exp $ */

/*
 * Copyright (c) 2001 Maxim Tsyplakov <tm@oganer.net>
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* This is /dev/radio driver for OpenBSD */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/device.h>
#include <sys/radioio.h>

#include <dev/radio_if.h>
#include <dev/radiovar.h>

int	radioprobe(struct device *, void *, void *);
void	radioattach(struct device *, struct device *, void *);
int	radioopen(dev_t, int, int, struct proc *);
int	radioclose(dev_t, int, int, struct proc *);
int	radioioctl(dev_t, u_long, caddr_t, int, struct proc *);
int	radioprint(void *, const char *);

struct cfattach radio_ca = {
	sizeof(struct radio_softc), radioprobe, radioattach,
	NULL, NULL
};

struct cfdriver radio_cd = {
	NULL, "radio", DV_DULL
};

int
radioprobe(struct device *parent, void *match, void *aux)
{
	printf("\n");		/* stub!?, fixme */
	return 1;
}

void
radioattach(struct device *parent, struct device *self, void *aux)
{
	struct radio_softc *sc = (void *) self;
	struct radio_attach_args *sa = aux;
	struct radio_hw_if *hwp = sa->hwif;
	void  *hdlp = sa->hdl;

	printf("\n");
	sc->hw_if = hwp;
	sc->hw_hdl = hdlp;
	sc->sc_dev = parent;
}

int
radioopen(dev_t dev, int flags, int fmt, struct proc *p)
{
	int	unit;
	struct radio_softc *sc;

	unit = RADIOUNIT(dev);
	if (unit >= radio_cd.cd_ndevs ||
	    (sc = radio_cd.cd_devs[unit]) == NULL ||
	     sc->hw_if == NULL)
		return (ENXIO); 

	if (sc->hw_if->open != NULL)
		return (sc->hw_if->open(sc->hw_hdl, flags, fmt, p));
	else
		return (0);
}

int
radioclose(dev_t dev, int flags, int fmt, struct proc *p) 
{
	struct radio_softc *sc;

	sc = radio_cd.cd_devs[RADIOUNIT(dev)];

	if (sc->hw_if->close != NULL)
		return (sc->hw_if->close(sc->hw_hdl, flags, fmt, p));
	else
		return (0);
}

int
radioioctl(dev_t dev, u_long cmd, caddr_t data, int flags, struct proc *p)
{
	struct radio_softc *sc;
	int unit, error;

	unit = RADIOUNIT(dev);
	if (unit >= radio_cd.cd_ndevs ||
	    (sc = radio_cd.cd_devs[unit]) == NULL || sc->hw_if == NULL)
		return (ENXIO);

	error = EOPNOTSUPP;
	switch (cmd) {
	case RIOCGINFO:
		if (sc->hw_if->get_info)
			error = (sc->hw_if->get_info)(sc->hw_hdl,
					(struct radio_info *)data);
			break;
	case RIOCSINFO:
		if (sc->hw_if->set_info)
			error = (sc->hw_if->set_info)(sc->hw_hdl,
				(struct radio_info *)data);
		break;
	case RIOCSSRCH:
		if (sc->hw_if->search)
			error = (sc->hw_if->search)(sc->hw_hdl,
					*(int *)data);
		break;
	default:
		error = EINVAL;
	}

	return (error);
}

/*
 * Called from hardware driver. This is where the MI radio driver gets
 * probed/attached to the hardware driver
 */

struct device  *
radio_attach_mi(struct radio_hw_if *rhwp, void *hdlp, struct device *dev)
{
	struct radio_attach_args arg;

	arg.hwif = rhwp;
	arg.hdl = hdlp;
	return (config_found(dev, &arg, radioprint));
}

int
radioprint(void *aux, const char *pnp)
{
	return UNCONF;
}
