/*	$OpenBSD: asc_tc.c,v 1.3 1998/05/18 00:25:09 millert Exp $	*/
/*	$NetBSD: asc_tc.c,v 1.8 1997/10/31 06:29:59 jonathan Exp $	*/

/*
 * Copyright 1996 The Board of Trustees of The Leland Stanford
 * Junior University. All Rights Reserved.
 *
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies.  Stanford University
 * makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without
 * express or implied warranty.
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/device.h>
#include <dev/tc/tcvar.h>
#include <machine/autoconf.h>
#include <dev/tc/ioasicvar.h>

#include <pmax/dev/device.h>	/* XXX */
#include <pmax/dev/scsi.h>	/* XXX */

#include <pmax/dev/ascreg.h>	/* XXX */
#include <dev/tc/ascvar.h>

/*XXX*/


/*
 * Autoconfiguration data for config.
 */
int asc_tc_match __P((struct device *, void *, void *));
void asc_tc_attach __P((struct device *, struct device *, void *));

struct cfattach asc_tc_ca = {
	sizeof(struct asc_softc), asc_tc_match, asc_tc_attach
};

/*
 * DMA callbacks
 */

static int
tc_dma_start __P((struct asc_softc *asc, struct scsi_state *state,
		  caddr_t cp, int flag, int len, int off));

static void
tc_dma_end __P((struct asc_softc *asc, struct scsi_state *state,
		int flag));


int
asc_tc_match(parent, match, aux)
	struct device *parent;
	void *match;
	void *aux;
{
	struct tc_attach_args *t = aux;
	void *ascaddr;

	if (strncmp(t->ta_modname, "PMAZ-AA ", TC_ROM_LLEN))
		return (0);

	ascaddr = (void*)t->ta_addr;

	if (tc_badaddr(ascaddr + ASC_OFFSET_53C94))
		return (0);

	return (1);
}



void
asc_tc_attach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	register struct tc_attach_args *t = aux;
	register asc_softc_t asc = (asc_softc_t) self;
	u_char *buff;
	int i, speed;

	void *ascaddr;
	int unit;

	/* Use uncached address for chip registers.  */
	ascaddr = (void*)MIPS_PHYS_TO_KSEG1(t->ta_addr);
	unit = asc->sc_dev.dv_unit;
	
	/*
	 * Initialize hw descriptor, cache some pointers
	 */
	asc->regs = (asc_regmap_t *)(ascaddr + ASC_OFFSET_53C94);

	/*
	 * Set up machine dependencies.
	 * (1) how to do dma
	 * (2) timing based on turbochannel frequency
	 */

	/*
	 * Fall through for turbochannel option.
	 */
	asc->dmar = (volatile int *)(ascaddr + ASC_OFFSET_DMAR);
	buff = (u_char *)(ascaddr + ASC_OFFSET_RAM);

	/*
	 * Statically partition the DMA buffer between targets.
	 * This way we will eventually be able to attach/detach
	 * drives on-fly.  And 18k/target is plenty for normal use.
	 */

	/*
	 * Give each target its own DMA buffer region.
	 * We may want to try ping ponging buffers later.
	 */
	for (i = 0; i < ASC_NCMD; i++)
		asc->st[i].dmaBufAddr = buff + PER_TGT_DMA_SIZE * i;

	asc->dma_start = tc_dma_start;
	asc->dma_end = tc_dma_end;

	/*
	 * Now for timing. The 3max has a 25Mhz tb whereas the 3min and
	 * maxine are 12.5Mhz.
	 */
	printf(" (bus speed: %s MHz) ", t->ta_busspeed? "25"  : "12.5");

	switch (t->ta_busspeed) {
	case TC_SPEED_25_MHZ:
		speed = ASC_SPEED_25_MHZ;
		break;

	default:
		printf(" (unknown TC speed, assuming 12.5MHz) ");
		/* FALLTHROUGH*/
	case TC_SPEED_12_5_MHZ:
		speed = ASC_SPEED_12_5_MHZ;
		break;
	};

	ascattach(asc, speed);

	/* tie pseudo-slot to device */
	tc_intr_establish(parent, t->ta_cookie, TC_IPL_BIO,
			  asc_intr, asc);
}


/*
 * DMA handling routines. For a turbochannel device, just set the dmar.
 * For the I/O ASIC, handle the actual DMA interface.
 */
static int
tc_dma_start(asc, state, cp, flag, len, off)
	asc_softc_t asc;
	State *state;
	caddr_t cp;
	int flag;
	int len;
	int off;
{

	if (len > PER_TGT_DMA_SIZE)
		len = PER_TGT_DMA_SIZE;
	if (flag == ASCDMA_WRITE)
		bcopy(cp, state->dmaBufAddr + off, len);
	if (flag == ASCDMA_WRITE)
		*asc->dmar = ASC_DMAR_WRITE | ASC_DMA_ADDR(state->dmaBufAddr + off);
	else
		*asc->dmar = ASC_DMA_ADDR(state->dmaBufAddr + off);
	return (len);
}

static void
tc_dma_end(asc, state, flag)
	asc_softc_t asc;
	State *state;
	int flag;
{
	if (flag == ASCDMA_READ)
		bcopy(state->dmaBufAddr, state->buf, state->dmalen);
}
