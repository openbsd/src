/*	$OpenBSD: siop_gsc.c,v 1.1 1998/11/04 17:01:35 mickey Exp $	*/

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

#include <machine/bus.h>
#include <machine/intr.h>
#include <machine/iomod.h>
#include <machine/autoconf.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

#include <dev/ic/ncr53c7xxreg.h>
#include <dev/ic/ncr53c7xxvar.h>

#include <hppa/dev/cpudevs.h>

int	ncr53c7xx_gsc_probe __P((struct device *, void *, void *));
void	ncr53c7xx_gsc_attach __P((struct device *, struct device *, void *));

struct cfattach ncr_ca = {
	sizeof(struct ncr53c7xx_softc),
	ncr53c7xx_gsc_probe, ncr53c7xx_gsc_attach
};

struct cfdriver ncr_cd = {
	NULL, "ncr", DV_DULL, NULL, 0
};

struct scsi_adapter ncr53c7xx_gsc_scsiswitch = {
	ncr53c7xx_scsicmd,
	ncr53c7xx_minphys,
	0,			/* no lun support */
	0,			/* no lun support */
};

struct scsi_device ncr53c7xx_gsc_scsidev = {
	NULL,		/* use default error handler */
	NULL,		/* do not have a start functio */
	NULL,		/* have no async handler */
	NULL,		/* Use default done routine */
};


int
ncr53c7xx_gsc_probe(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	register struct confargs *ca = aux;
	register bus_space_tag_t iot;
	bus_space_handle_t ioh;
	int rv = 1;

	if (ca->ca_type.iodc_type != HPPA_TYPE_FIO ||
	    (ca->ca_type.iodc_sv_model != HPPA_FIO_GSCSI &&
	     ca->ca_type.iodc_sv_model != HPPA_FIO_SCSI))
		if (ca->ca_type.iodc_type != HPPA_TYPE_ADMA ||
		    ca->ca_type.iodc_sv_model != HPPA_ADMA_FWSCSI)
			return 0;

	iot = HPPA_BUS_TAG_SET_BYTE(ca->ca_iot);
	if (bus_space_map(ca->ca_iot, ca->ca_hpa, IOMOD_HPASIZE, 0, &ioh))
		return 0;
	ioh |= IOMOD_DEVOFFSET;



	ioh &= ~IOMOD_DEVOFFSET;
	bus_space_unmap(ca->ca_iot, ioh, IOMOD_HPASIZE);
	return rv;
}

void
ncr53c7xx_gsc_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	register struct ncr53c7xx_softc *sc = (void *)self;
	register struct confargs *ca = aux;

	sc->sc_iot = HPPA_BUS_TAG_SET_BYTE(ca->ca_iot);
	if (bus_space_map(sc->sc_iot, ca->ca_hpa, IOMOD_HPASIZE,
			  0, &sc->sc_ioh))
		panic("ncr53c7xx_gsc_attach: couldn't map I/O ports");
	sc->sc_ioh |= IOMOD_DEVOFFSET;

	sc->sc_clock_freq = ca->ca_pdc_iodc_read->filler2[14] / 1000000;
	if (!sc->sc_clock_freq)
		sc->sc_clock_freq = 50;

	if (ca->ca_type.iodc_sv_model == HPPA_FIO_GSCSI)
		sc->sc_type = 10;
	else if (ca->ca_type.iodc_sv_model == HPPA_ADMA_FWSCSI)
		sc->sc_type = 20;
	else
		sc->sc_type = 0;

	sc->sc_ctest7 = 0;
	sc->sc_dcntl = 0;
	sc->sc_flags = NCR53C7XX_NODMA;

	sc->sc_link.adapter_softc = sc;
	sc->sc_link.adapter_target = 7;
	sc->sc_link.adapter = &ncr53c7xx_gsc_scsiswitch;
	sc->sc_link.device = &ncr53c7xx_gsc_scsidev;
	sc->sc_link.openings = 2;

	ncr53c7xx_initialize(sc);

	config_found(self, &sc->sc_link, scsiprint);
}
