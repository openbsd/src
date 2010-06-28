/*	$OpenBSD: vsdma.c,v 1.14 2010/06/28 18:31:01 krw Exp $ */
/*
 * Copyright (c) 1999 Steve Murphree, Jr.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)vsdma.c
 */

/*
 * MVME328 scsi adaptor driver
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

#include <machine/autoconf.h>

#include <mvme68k/dev/vsreg.h>
#include <mvme68k/dev/vsvar.h>
#include <mvme68k/dev/vme.h>

int	vsmatch(struct device *, void *, void *);
void	vsattach(struct device *, struct device *, void *);
int	vsprint(void *auxp, char *);
void  vs_initialize(struct vs_softc *);
int	vs_intr(struct vs_softc *);
int	vs_nintr(void *);
int	vs_eintr(void *);

struct scsi_adapter vs_scsiswitch = {
	vs_scsicmd,
	scsi_minphys,
	0,			/* no lun support */
	0,			/* no lun support */
};

struct cfattach vs_ca = {
        sizeof(struct vs_softc), vsmatch, vsattach,
};    
 
struct cfdriver vs_cd = {
        NULL, "vs", DV_DULL
}; 

int
vsmatch(pdp, vcf, args)
	struct device *pdp;
	void *vcf, *args;
{
	struct confargs *ca = args;
	return(!badvaddr((vaddr_t)ca->ca_vaddr, 1));
}

void
vsattach(parent, self, auxp)
	struct device *parent, *self;
	void *auxp;
{
	struct vs_softc *sc = (struct vs_softc *)self;
	struct confargs *ca = auxp;
	struct scsibus_attach_args saa;
	struct vsreg * rp;
	int tmp;

	sc->sc_vsreg = rp = (void *)ca->ca_vaddr;

	sc->sc_ipl = ca->ca_ipl;
	sc->sc_nvec = ca->ca_vec + 0;
	sc->sc_evec = ca->ca_vec + 1;
	sc->sc_link.adapter_softc = sc;
	sc->sc_link.adapter_target = 7;
	sc->sc_link.adapter = &vs_scsiswitch;
	sc->sc_link.luns = 1;
	sc->sc_link.openings = 1;

	sc->sc_ih_n.ih_fn = vs_nintr;
	sc->sc_ih_n.ih_arg = sc;
	sc->sc_ih_n.ih_ipl = ca->ca_ipl;
   
	sc->sc_ih_e.ih_fn = vs_eintr;
	sc->sc_ih_e.ih_arg = sc;
	sc->sc_ih_e.ih_ipl = ca->ca_ipl;
   
	vs_initialize(sc);

	snprintf(sc->sc_intrname_e, sizeof sc->sc_intrname_e,
	    "%s_err", self->dv_xname);

	vmeintr_establish(sc->sc_nvec, &sc->sc_ih_n, self->dv_xname);
	vmeintr_establish(sc->sc_evec, &sc->sc_ih_e, sc->sc_intrname_e);

	/*
	 * attach all scsi units on us, watching for boot device
	 * (see device_register).
	 */
	bzero(&saa, sizeof(saa));
	saa.saa_sc_link = &sc->sc_link;
	tmp = bootpart;
	if (ca->ca_paddr != bootaddr) 
		bootpart = -1;          /* invalid flag to device_register */
	config_found(self, &saa, scsiprint);
	bootpart = tmp;             /* restore old value */
}

/*
 * print diag if pnp is NULL else just extra
 */
int
vsprint(auxp, pnp)
	void *auxp;
	char *pnp;
{
	if (pnp == NULL)
		return (UNCONF);
	return (QUIET);
}

/* normal interrupt function */
int
vs_nintr(arg)
	void *arg;
{
	struct vs_softc *sc = (struct vs_softc *)arg;

#ifdef SDEBUG
	printf("Normal Interrupt!!!\n");
#endif 
	vs_intr(sc);
	return (1);
}

/* error interrupt function */
int
vs_eintr(arg)
	void *arg;
{
	struct vs_softc *sc = (struct vs_softc *)arg;

#ifdef SDEBUG
	printf("Error Interrupt!!!\n");
#endif
	vs_intr(sc);
	return (1);
}


