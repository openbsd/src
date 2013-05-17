/*	$OpenBSD: angelfire.c,v 1.1 2013/05/17 22:51:59 miod Exp $	*/

/*
 * Copyright (c) 2013 Miodrag Vallat.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Logical bus for the AngelFire System Controller and on-board resources
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/proc.h>

#include <machine/autoconf.h>
#include <machine/board.h>
#include <machine/cpu.h>

#include <machine/mvme181.h>

struct angelfiresoftc {
	struct device	sc_dev;

	struct intrhand sc_abih;	/* abort switch */
	struct intrhand sc_parih;	/* parity error */
};

void	angelfireattach(struct device *, struct device *, void *);
int	angelfirematch(struct device *, void *, void *);

int	angelfireprint(void *, const char *);
int	angelfirescan(struct device *, void *, void *);
int	angelfireabort(void *);
int	angelfireparerr(void *);

const struct cfattach angelfire_ca = {
	sizeof(struct angelfiresoftc), angelfirematch, angelfireattach
};

struct cfdriver angelfire_cd = {
	NULL, "angelfire", DV_DULL
};

int
angelfirematch(struct device *parent, void *vcf, void *aux)
{
	switch (brdtyp) {
	case BRD_180:
	case BRD_181:
		return angelfire_cd.cd_ndevs == 0;
	default:
		return 0;
	}
}

void
angelfireattach(struct device *parent, struct device *self, void *aux)
{
	struct angelfiresoftc *sc = (struct angelfiresoftc *)self;

	printf("\n");

	sc->sc_abih.ih_fn = angelfireabort;
	sc->sc_abih.ih_arg = 0;
	sc->sc_abih.ih_wantframe = 1;
	sc->sc_abih.ih_ipl = IPL_ABORT;
	platform->intsrc_establish(INTSRC_ABORT, &sc->sc_abih, "abort");

	/*
	 * Don't bother registering the parity error interrupt handler.
	 * Parity error interrupts are asynchronous, and there is nothing
	 * we can do but acknowledge them... and hope these were detected
	 * during write cycles and the writes have been retried.
	 * In any case, they don't seem to be harmful.
	 */
#if 0
	if (brdtyp != BRD_180) {
		sc->sc_parih.ih_fn = angelfireparerr;
		sc->sc_parih.ih_arg = 0;
		sc->sc_parih.ih_wantframe = 1;
		sc->sc_parih.ih_ipl = IPL_HIGH;
		platform->intsrc_establish(INTSRC_PARERR, &sc->sc_parih,
		    "parity");
	}
#endif

	config_search(angelfirescan, self, aux);
}

int
angelfirescan(struct device *parent, void *child, void *args)
{
	struct cfdata *cf = child;
	struct confargs oca, *ca = args;

	bzero(&oca, sizeof oca);
	oca.ca_iot = ca->ca_iot;
	oca.ca_dmat = ca->ca_dmat;
	oca.ca_offset = cf->cf_loc[0];
	oca.ca_ipl = cf->cf_loc[1];
	if (oca.ca_offset != -1)
		oca.ca_paddr = ca->ca_paddr + oca.ca_offset;
	else
		oca.ca_paddr = -1;
	oca.ca_bustype = BUS_ANGELFIRE;
	oca.ca_name = cf->cf_driver->cd_name;

	if ((*cf->cf_attach->ca_match)(parent, cf, &oca) == 0)
		return (0);

	config_attach(parent, cf, &oca, angelfireprint);
	return (1);
}

int
angelfireprint(void *args, const char *bus)
{
	struct confargs *ca = args;

	if (ca->ca_offset != -1)
		printf(" offset 0x%x", ca->ca_offset);
	if (ca->ca_ipl > 0)
		printf(" ipl %d", ca->ca_ipl);
	return (UNCONF);
}

int
angelfireabort(void *eframe)
{
	*(volatile u_int32_t *)M181_CLRABRT = 0xffffffff;
	(void)*(volatile u_int32_t *)M181_CLRABRT;

	nmihand(eframe);

	return 1;
}

int
angelfireparerr(void *eframe)
{
	struct trapframe *frame = (struct trapframe *)eframe;
	vaddr_t pc;

	*(volatile u_int32_t *)M181_CPEI = 0xffffffff;
	(void)*(volatile u_int32_t *)M181_CPEI;

	pc = PC_REGS(&frame->tf_regs);
	if (frame->tf_epsr & PSR_MODE)
		printf("kernel parity error, PC = %p\n", pc);
	else
		printf("%s: parity error, PC = %p\n", curproc->p_comm, pc);

	return 1;
}
