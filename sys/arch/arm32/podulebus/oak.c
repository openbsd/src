/* $NetBSD: oak.c,v 1.3 1996/03/17 01:24:51 thorpej Exp $ */

/*
 * Copyright (c) 1995 Melvin Tang-Richardson 1996.
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
 *	This product includes software developed by RiscBSD.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY RISCBSD ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL RISCBSD OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * oak.c
 *
 * Oak SCSI Driver.
 */

#undef USE_OWN_PIO_ROUTINES

/* Some system includes *****************************************************/

#include <sys/types.h>
#include <sys/cdefs.h>
#include <sys/time.h>
#include <sys/systm.h>
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <sys/buf.h>
/*#include <machine/bootconfig.h>*/

/* SCSI bus includes */

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

/* Hardware related include chip/card/bus ***********************************/

#include <machine/io.h>
#include <machine/irqhandler.h>
#include <machine/katelib.h>
#include <arm32/podulebus/podulebus.h>
#include <arm32/podulebus/ncr5380reg.h>
#include <arm32/podulebus/ncr5380var.h>

/****************************************************************************/
/* Some useful definitions **************************************************/
/****************************************************************************/

#define MY_MANUFACTURER	(0x21)
#define MY_PODULE	(0x58)
#define MAX_DMA_LEN	(0xe000)

/****************************************************************************/
/* Prototype internal data structures ***************************************/
/****************************************************************************/

struct oak_softc {
	struct ncr5380_softc ncr_sc;
	int sc_podule;
	int sc_base;
};

/****************************************************************************/
/* Function and data prototypes *********************************************/
/****************************************************************************/

int  oakprobe 	__P(( struct device *, void *, void * ));
void oakattach 	__P(( struct device *, struct device *, void * ));
int  oakprint   __P(( void *, char * ));
void oakminphys __P(( struct buf * ));

#ifdef USE_OWN_PIO_ROUTINES
int oak_pio_in  __P(( struct ncr5380_softc *, int, int, unsigned char * ));
int oak_pio_out __P(( struct ncr5380_softc *, int, int, unsigned char * ));
#endif

struct scsi_adapter oak_adapter = {
	ncr5380_scsi_cmd,
	oakminphys,
	NULL,
	NULL,
};

struct scsi_device oak_device = {
	NULL,
	NULL,
	NULL,
	NULL,
};

int
oakprobe(parent, match, aux)
	struct device *parent;
	void *match;
	void *aux;
{
	struct oak_softc *sc = (void *) match;
	struct podule_attach_args *pa = (void *) aux;
	int podule;

	podule = findpodule(MY_MANUFACTURER, MY_PODULE, pa->pa_podule_number);

	if (podule == -1)
		return 0;

	sc->sc_podule = podule;
	sc->sc_base = podules[podule].mod_base;

	return 1;
}

void
oakattach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct oak_softc *sc = (void *) self;
	struct ncr5380_softc *ncr_sc = (struct ncr5380_softc *)sc;
	struct podule_attach_args *pa = (void *)aux;

	printf(" 16-bit");

	ncr_sc->sc_link.adapter_softc = sc;
	ncr_sc->sc_link.adapter_target = 7;
	ncr_sc->sc_link.adapter = &oak_adapter;
	ncr_sc->sc_link.device = &oak_device;

	ncr_sc->sci_r0 = (volatile u_char *)sc->sc_base + 0x00;
	ncr_sc->sci_r1 = (volatile u_char *)sc->sc_base + 0x04;
	ncr_sc->sci_r2 = (volatile u_char *)sc->sc_base + 0x08;
	ncr_sc->sci_r3 = (volatile u_char *)sc->sc_base + 0x0c;
	ncr_sc->sci_r4 = (volatile u_char *)sc->sc_base + 0x10;
	ncr_sc->sci_r5 = (volatile u_char *)sc->sc_base + 0x14;
	ncr_sc->sci_r6 = (volatile u_char *)sc->sc_base + 0x18;
	ncr_sc->sci_r7 = (volatile u_char *)sc->sc_base + 0x1c;

#ifdef USE_OWN_PIO_ROUTINES
 	printf ( ", my pio" );
	ncr_sc->sc_pio_out = oak_pio_out;
	ncr_sc->sc_pio_in  = oak_pio_in;
#else
	printf ( ", normal pio" );
	ncr_sc->sc_pio_out = ncr5380_pio_out;
	ncr_sc->sc_pio_in  = ncr5380_pio_in;
#endif

	ncr_sc->sc_dma_alloc = NULL;
	ncr_sc->sc_dma_free  = NULL;
	ncr_sc->sc_dma_poll  = NULL;
	ncr_sc->sc_dma_setup  = NULL;
	ncr_sc->sc_dma_start  = NULL;
	ncr_sc->sc_dma_eop  = NULL;
	ncr_sc->sc_dma_stop  = NULL;

	printf(", polling");
	ncr_sc->sc_intr_on   = NULL;
	ncr_sc->sc_intr_off  = NULL;

	ncr_sc->sc_flags  = NCR5380_FORCE_POLLING;

	ncr5380_init(ncr_sc);
	ncr5380_reset_scsibus(ncr_sc);

	printf(" UNDER DEVELOPMENT\n");

	config_found(self, &(ncr_sc->sc_link), oakprint);
}

int
oakprint(aux, name)
	void *aux;
	char *name;
{
	if (name != NULL)
		printf("%s: scsibus ", name);
	return UNCONF;
}

void
oakminphys(bp)
	struct buf *bp;
{
	if (bp->b_bcount > MAX_DMA_LEN) {
		printf("oak: DEBUG reducing dma length\n");
		bp->b_bcount = MAX_DMA_LEN;
	}
	return (minphys(bp));
}

struct cfattach oak_ca = {
	sizeof(struct oak_softc), oakprobe, oakattach
};

struct cfdriver oak_cd = {
	NULL, "oak", DV_DISK, NULL, 0,
};

#ifdef USE_OWN_PIO_ROUTINES

/****************************************************************************/
/* Copyright (c) 1996 Melvin Tang-Richardson				    */
/* Copyright (c) 1995 David Jones, Gordon W. Rose			    */
/* Copyright (c) 1994 Jarle Greipsland					    */
/****************************************************************************/

static ncr5380_wait_req_timo = 1000 * 50;	/* X2 = 100 mS */
static ncr5380_wait_nrq_timo = 1000 * 25;	/* X2 =  50 mS */

/* Return zero on success. */
static __inline__ int ncr5380_wait_req(sc)
	struct ncr5380_softc *sc;
{
	register int timo = ncr5380_wait_req_timo;
	for (;;) {
		if (*sc->sci_bus_csr & SCI_BUS_REQ) {
			timo = 0;	/* return 0 */
			break;
		}
		if (--timo < 0)
			break;	/* return -1 */
		delay(2);
	}
	return (timo);
}

/* Return zero on success. */
static __inline__ int ncr5380_wait_not_req(sc)
	struct ncr5380_softc *sc;
{
	register int timo = ncr5380_wait_nrq_timo;
	for (;;) {
		if ((*sc->sci_bus_csr & SCI_BUS_REQ) == 0) {
			timo = 0;	/* return 0 */
			break;
		}
		if (--timo < 0)
			break;	/* return -1 */
		delay(2);
	}
	return (timo);
}

int
oak_pio_out(sc, phase, count, data)
	struct ncr5380_softc *sc;
	int phase, count;
	unsigned char		*data;
{
	register u_char 	icmd;
	register int		resid;
	register int		error;

	printf("oak: pio_out %d %d\n", phase, count);

	icmd = *(sc->sci_icmd) & SCI_ICMD_RMASK;

	icmd |= SCI_ICMD_DATA;
	*sc->sci_icmd = icmd;

	resid = count;
	while (resid > 0) {
		if (!SCI_BUSY(sc)) {
			NCR_TRACE("pio_out: lost BSY, resid=%d\n", resid);
			break;
		}
		if (ncr5380_wait_req(sc)) {
			NCR_TRACE("pio_out: no REQ, resid=%d\n", resid);
			break;
		}
		if (SCI_BUS_PHASE(*sc->sci_bus_csr) != phase)
			break;

		/* Put the data on the bus. */
		*sc->sci_odata = *data++;

		/* Tell the target it's there. */
		icmd |= SCI_ICMD_ACK;
		*sc->sci_icmd = icmd;

		/* Wait for target to get it. */
		error = ncr5380_wait_not_req(sc);

		/* OK, it's got it (or we gave up waiting). */
		icmd &= ~SCI_ICMD_ACK;
		*sc->sci_icmd = icmd;

		if (error) {
			NCR_TRACE("pio_out: stuck REQ, resid=%d\n", resid);
			break;
		}

		--resid;
	}

	/* Stop driving the data bus. */
	icmd &= ~SCI_ICMD_DATA;
	*sc->sci_icmd = icmd;

	return (count - resid);
}

int
oak_pio_in(sc, phase, count, data)
	struct ncr5380_softc *sc;
	int phase, count;
	unsigned char	*data;
{
	register u_char 	icmd;
	register int		resid;
	register int		error;

	printf("oak: pio_in %d %d\n", phase, count);

	icmd = *(sc->sci_icmd) & SCI_ICMD_RMASK;

	resid = count;
	while (resid > 0) {
		if (!SCI_BUSY(sc)) {
			NCR_TRACE("pio_in: lost BSY, resid=%d\n", resid);
			break;
		}
		if (ncr5380_wait_req(sc)) {
			NCR_TRACE("pio_in: no REQ, resid=%d\n", resid);
			break;
		}
		/* A phase change is not valid until AFTER REQ rises! */
		if (SCI_BUS_PHASE(*sc->sci_bus_csr) != phase)
			break;

		/* Read the data bus. */
		*data++ = *sc->sci_data;

		/* Tell target we got it. */
		icmd |= SCI_ICMD_ACK;
		*sc->sci_icmd = icmd;

		/* Wait for target to drop REQ... */
		error = ncr5380_wait_not_req(sc);

		/* OK, we can drop ACK. */
		icmd &= ~SCI_ICMD_ACK;
		*sc->sci_icmd = icmd;

		if (error) {
			NCR_TRACE("pio_in: stuck REQ, resid=%d\n", resid);
			break;
		}

		--resid;
	}

	return (count - resid);
}

#endif /* USE_OWN_PIO_ROUTINES */

