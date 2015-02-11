/*	$OpenBSD: iockbc.c,v 1.11 2015/02/11 07:05:39 dlg Exp $	*/
/*
 * Copyright (c) 2013, Miodrag Vallat
 * Copyright (c) 2006, 2007, 2009 Joel Sing <jsing@openbsd.org>
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
 * Derived from sys/dev/ic/pckbc.c under the following terms:
 * $NetBSD: pckbc.c,v 1.5 2000/06/09 04:58:35 soda Exp $ */

/*
 * Copyright (c) 1998
 *	Matthias Drochner.  All rights reserved.
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
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Driver for IOC3 and IOC4 PS/2 Controllers (iockbc)
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/timeout.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/signalvar.h>
#include <sys/errno.h>
#include <sys/queue.h>

#include <machine/autoconf.h>
#include <machine/bus.h>

#include <mips64/archtype.h>

#include <sgi/pci/iocreg.h>
#include <sgi/pci/iocvar.h>
#include <sgi/pci/iofreg.h>
#include <sgi/pci/iofvar.h>

#include <dev/ic/i8042reg.h>
#include <dev/ic/pckbcvar.h>
#include <dev/pckbc/pckbdreg.h>
#define KBC_DEVCMD_ACK		KBR_ACK
#define KBC_DEVCMD_RESEND	KBR_RESEND
#include <dev/pckbc/pckbdvar.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>

#include <sgi/dev/iockbcreg.h>
#include <sgi/dev/iockbcvar.h>

#include "iockbc.h"

const char *iockbc_slot_names[] = { "kbd", "mouse" };

/* #define IOCKBC_DEBUG */
#ifdef IOCKBC_DEBUG
#define DPRINTF(x...)           do { printf(x); } while(0)
#else
#define DPRINTF(x...)
#endif

struct iockbc_reginfo {
	bus_addr_t rx;
	bus_addr_t tx;
	bus_addr_t cs;
	uint32_t busy;
};

struct iockbc_softc {
	struct pckbc_softc		 sc_pckbc;
	bus_space_tag_t			 iot;
	bus_space_handle_t		 ioh;
	const struct iockbc_reginfo	*reginfo;
	int				 console;
};

int	iockbc_match(struct device *, void *, void *);
void	iockbc_ioc_attach(struct device *, struct device *, void *);
void	iockbc_iof_attach(struct device *, struct device *, void *);

#if NIOCKBC_IOC > 0
struct cfattach iockbc_ioc_ca = {
	sizeof(struct iockbc_softc), iockbc_match, iockbc_ioc_attach
};
#endif
#if NIOCKBC_IOF > 0
struct cfattach iockbc_iof_ca = {
	sizeof(struct iockbc_softc), iockbc_match, iockbc_iof_attach
};
#endif

struct cfdriver iockbc_cd = {
	NULL, "iockbc", DV_DULL
};

/* Descriptor for one device command. */
struct pckbc_devcmd {
	TAILQ_ENTRY(pckbc_devcmd) next;
	int flags;
#define KBC_CMDFLAG_SYNC 1 /* Give descriptor back to caller. */
#define KBC_CMDFLAG_SLOW 2
	u_char cmd[4];
	int cmdlen, cmdidx, retries;
	u_char response[4];
	int status, responselen, responseidx;
};

/* Data per slave device. */
struct pckbc_slotdata {
	int polling; /* Don't read data port in interrupt handler. */
	TAILQ_HEAD(, pckbc_devcmd) cmdqueue; /* Active commands. */
	TAILQ_HEAD(, pckbc_devcmd) freequeue; /* Free commands. */
#define NCMD 5
	struct pckbc_devcmd cmds[NCMD];
	int rx_queue[3];
	int rx_index;
	const struct iockbc_reginfo *reginfo;
};

#define CMD_IN_QUEUE(q) (TAILQ_FIRST(&(q)->cmdqueue) != NULL)

enum iockbc_slottype { EMPTY, KBD, MOUSE };

static struct pckbc_internal iockbc_consdata;
static struct pckbc_slotdata iockbc_cons_slotdata;

int	iockbc_is_ioc_console(struct ioc_attach_args *);
int	iockbc_is_iof_console(struct iof_attach_args *);
void	iockbc_attach_common(struct iockbc_softc *, bus_addr_t, int,
	    const struct iockbc_reginfo *, const struct iockbc_reginfo *);
void	iockbc_start(struct pckbc_internal *, pckbc_slot_t);
int	iockbc_attach_slot(struct iockbc_softc *, pckbc_slot_t);
void	iockbc_init_slotdata(struct pckbc_slotdata *,
	    const struct iockbc_reginfo *);
int	iockbc_submatch(struct device *, void *, void *);
int	iockbcprint(void *, const char *);
int	iockbcintr(void *);
int	iockbcintr_internal(struct pckbc_internal *, struct pckbc_softc *);
void	iockbc_cleanqueue(struct pckbc_slotdata *);
void	iockbc_cleanup(void *);
void	iockbc_poll(void *);
int	iockbc_cmdresponse(struct pckbc_internal *, pckbc_slot_t, u_char);
int	iockbc_poll_read(struct pckbc_internal *, pckbc_slot_t);
int	iockbc_poll_write(struct pckbc_internal *, pckbc_slot_t, int);
void	iockbc_process_input(struct pckbc_softc *, struct pckbc_internal *,
	    int, uint);
enum iockbc_slottype
	iockbc_probe_slot(struct pckbc_internal *, pckbc_slot_t);

int
iockbc_match(struct device *parent, void *cf, void *aux)
{
	/*
	 * We expect ioc and iof NOT to attach us if there are no PS/2 ports.
	 */
	return 1;
}

#if NIOCKBC_IOC > 0
/*
 * Register assignments
 */

const struct iockbc_reginfo iockbc_ioc[PCKBC_NSLOTS] = {
	[PCKBC_KBD_SLOT] = {
		.rx = IOC3_KBC_KBD_RX,
		.tx = IOC3_KBC_KBD_TX,
		.cs = IOC3_KBC_CTRL_STATUS,
		.busy = IOC3_KBC_STATUS_KBD_WRITE_PENDING
	},
	[PCKBC_AUX_SLOT] = {
		.rx = IOC3_KBC_AUX_RX,
		.tx = IOC3_KBC_AUX_TX,
		.cs = IOC3_KBC_CTRL_STATUS,
		.busy = IOC3_KBC_STATUS_AUX_WRITE_PENDING
	}
};

const struct iockbc_reginfo iockbc_ioc_inverted[PCKBC_NSLOTS] = {
	[PCKBC_KBD_SLOT] = {
		.rx = IOC3_KBC_AUX_RX,
		.tx = IOC3_KBC_AUX_TX,
		.cs = IOC3_KBC_CTRL_STATUS,
		.busy = IOC3_KBC_STATUS_AUX_WRITE_PENDING
	},
	[PCKBC_AUX_SLOT] = {
		.rx = IOC3_KBC_KBD_RX,
		.tx = IOC3_KBC_KBD_TX,
		.cs = IOC3_KBC_CTRL_STATUS,
		.busy = IOC3_KBC_STATUS_KBD_WRITE_PENDING
	}
};

void
iockbc_ioc_attach(struct device *parent, struct device *self, void *aux)
{
	struct iockbc_softc *isc = (void*)self;
	struct ioc_attach_args *iaa = aux;

	/* Setup bus space mapping. */
	isc->iot = iaa->iaa_memt;
	isc->ioh = iaa->iaa_memh;

	/* Establish interrupt handler. */
	if (ioc_intr_establish(parent, iaa->iaa_dev, IPL_TTY, iockbcintr,
	    (void *)isc, self->dv_xname))
		printf("\n");
	else
		printf(": unable to establish interrupt\n");

	iockbc_attach_common(isc, iaa->iaa_base, iockbc_is_ioc_console(iaa),
	    iockbc_ioc, iockbc_ioc_inverted);
}
#endif

#if NIOCKBC_IOF > 0
/*
 * Register assignments
 */

const struct iockbc_reginfo iockbc_iof[PCKBC_NSLOTS] = {
	[PCKBC_KBD_SLOT] = {
		.rx = IOC4_KBC_KBD_RX,
		.tx = IOC4_KBC_KBD_TX,
		.cs = IOC4_KBC_CTRL_STATUS,
		.busy = IOC3_KBC_STATUS_KBD_WRITE_PENDING
	},
	[PCKBC_AUX_SLOT] = {
		.rx = IOC4_KBC_AUX_RX,
		.tx = IOC4_KBC_AUX_TX,
		.cs = IOC4_KBC_CTRL_STATUS,
		.busy = IOC3_KBC_STATUS_AUX_WRITE_PENDING
	}
};

const struct iockbc_reginfo iockbc_iof_inverted[PCKBC_NSLOTS] = {
	[PCKBC_KBD_SLOT] = {
		.rx = IOC4_KBC_AUX_RX,
		.tx = IOC4_KBC_AUX_TX,
		.cs = IOC4_KBC_CTRL_STATUS,
		.busy = IOC3_KBC_STATUS_AUX_WRITE_PENDING
	},
	[PCKBC_AUX_SLOT] = {
		.rx = IOC4_KBC_KBD_RX,
		.tx = IOC4_KBC_KBD_TX,
		.cs = IOC4_KBC_CTRL_STATUS,
		.busy = IOC3_KBC_STATUS_KBD_WRITE_PENDING
	}
};

void
iockbc_iof_attach(struct device *parent, struct device *self, void *aux)
{
	struct iockbc_softc *isc = (void*)self;
	struct iof_attach_args *iaa = aux;

	/* Setup bus space mapping. */
	isc->iot = iaa->iaa_memt;
	isc->ioh = iaa->iaa_memh;

	/* Establish interrupt handler. */
	if (iof_intr_establish(parent, iaa->iaa_dev, IPL_TTY, iockbcintr,
	    (void *)isc, self->dv_xname))
		printf("\n");
	else
		printf(": unable to establish interrupt\n");

	iockbc_attach_common(isc, iaa->iaa_base, iockbc_is_iof_console(iaa),
	    iockbc_iof, iockbc_iof_inverted);
}
#endif

void
iockbc_init_slotdata(struct pckbc_slotdata *q,
    const struct iockbc_reginfo *reginfo)
{
	int i;
	TAILQ_INIT(&q->cmdqueue);
	TAILQ_INIT(&q->freequeue);

	for (i = 0; i < NCMD; i++)
		TAILQ_INSERT_TAIL(&q->freequeue, &(q->cmds[i]), next);

	q->polling = 0;
	q->rx_index = -1;

	q->reginfo = reginfo;
}

int
iockbcprint(void *aux, const char *pnp)
{
	struct pckbc_attach_args *pa = aux;

	if (!pnp)
		printf(" (%s slot)", iockbc_slot_names[pa->pa_slot]);

	return (QUIET);
}

int
iockbc_submatch(struct device *parent, void *match, void *aux)
{
	struct cfdata *cf = match;
	struct pckbc_attach_args *pa = aux;

	if (cf->cf_loc[PCKBCCF_SLOT] != PCKBCCF_SLOT_DEFAULT &&
	    cf->cf_loc[PCKBCCF_SLOT] != pa->pa_slot)
		return (0);

	return ((*cf->cf_attach->ca_match)(parent, cf, pa));
}

/*
 * Figure out what kind of device is connected to the given slot, if any.
 */
enum iockbc_slottype
iockbc_probe_slot(struct pckbc_internal *t, pckbc_slot_t slot)
{
	int rc, i, tries;

	/* reset device */
	pckbc_flush(t, slot);
	for (tries = 0; tries < 5; tries++) {
		rc = iockbc_poll_write(t, slot, KBC_RESET);
		if (rc < 0) {
			DPRINTF(("%s: slot %d write failed\n", __func__, slot));
			return EMPTY;
		}
		for (i = 10; i != 0; i--) {
			rc = iockbc_poll_read(t, slot);
			if (rc >= 0)
				break;
		}
		if (rc < 0) {
			DPRINTF(("%s: slot %d no answer to reset\n",
			    __func__, slot));
			return EMPTY;
		}
		if (rc == KBC_DEVCMD_ACK)
			break;
		if (rc == KBC_DEVCMD_RESEND)
			continue;
		DPRINTF(("%s: slot %d bogus reset ack %02x\n",
		    __func__, slot, rc));
		return EMPTY;
	}

	/* get answer byte */
	for (i = 10; i != 0; i--) {
		rc = iockbc_poll_read(t, slot);
		if (rc >= 0)
			break;
	}
	if (rc < 0) {
		DPRINTF(("%s: slot %d no answer to reset after ack\n",
		    __func__, slot));
		return EMPTY;
	}
	if (rc != KBR_RSTDONE) {
		DPRINTF(("%s: slot %d bogus reset answer %02x\n",
		    __func__, slot, rc));
		return EMPTY;
	}
	/* mice send an extra byte */
	(void)iockbc_poll_read(t, slot);

	/* ask for device id */
	for (tries = 0; tries < 5; tries++) {
		rc = iockbc_poll_write(t, slot, KBC_READID);
		if (rc == -1) {
			DPRINTF(("%s: slot %d write failed\n", __func__, slot));
			return EMPTY;
		}
		for (i = 10; i != 0; i--) {
			rc = iockbc_poll_read(t, slot);
			if (rc >= 0)
				break;
		}
		if (rc < 0) {
			DPRINTF(("%s: slot %d no answer to command\n",
			    __func__, slot));
			return EMPTY;
		}
		if (rc == KBC_DEVCMD_ACK)
			break;
		if (rc == KBC_DEVCMD_RESEND)
			continue;
		DPRINTF(("%s: slot %d bogus command ack %02x\n",
		    __func__, slot, rc));
		return EMPTY;
	}

	/* get first answer byte */
	for (i = 10; i != 0; i--) {
		rc = iockbc_poll_read(t, slot);
		if (rc >= 0)
			break;
	}
	if (rc < 0) {
		DPRINTF(("%s: slot %d no answer to command after ack\n",
		    __func__, slot));
		return EMPTY;
	}

	switch (rc) {
	case KCID_KBD1:		/* keyboard */
		/* get second answer byte */
		rc = iockbc_poll_read(t, slot);
		if (rc < 0) {
			DPRINTF(("%s: slot %d truncated keyboard answer\n",
			    __func__, slot));
			return EMPTY;
		}
		if (rc != KCID_KBD2) {
			DPRINTF(("%s: slot %d unexpected keyboard answer"
			    " 0x%02x 0x%02x\n", __func__, slot, KCID_KBD1, rc));
			/* return EMPTY; */
		}
		return KBD;
	case KCID_MOUSE:	/* mouse */
		return MOUSE;
	default:
		DPRINTF(("%s: slot %d unknown device answer 0x%02x\n",
		    __func__, slot, rc));
		return EMPTY;
	}
}

int
iockbc_attach_slot(struct iockbc_softc *isc, pckbc_slot_t slot)
{
	struct pckbc_softc *sc = &isc->sc_pckbc;
	struct pckbc_internal *t = sc->id;
	struct pckbc_attach_args pa;
	int found;

	iockbc_init_slotdata(t->t_slotdata[slot], &isc->reginfo[slot]);

	pa.pa_tag = t;
	pa.pa_slot = slot;

	found = (config_found_sm((struct device *)sc, &pa,
	    iockbcprint, iockbc_submatch) != NULL);

	return (found);
}

void
iockbc_attach_common(struct iockbc_softc *isc, bus_addr_t addr, int console,
    const struct iockbc_reginfo *reginfo,
    const struct iockbc_reginfo *reginfo_inverted)
{
	struct pckbc_softc *sc = &isc->sc_pckbc;
	struct pckbc_internal *t;
	bus_addr_t cs;
	uint32_t csr;
	pckbc_slot_t slot;

	if (console) {
		iockbc_consdata.t_sc = sc;
		sc->id = t = &iockbc_consdata;
		isc->console = 1;
		if (&reginfo[PCKBC_KBD_SLOT] == iockbc_cons_slotdata.reginfo)
			isc->reginfo = reginfo;
		else
			isc->reginfo = reginfo_inverted;
	} else {
		/*
		 * Setup up controller: do not force pull clock and data lines
		 * low, clamp clocks after one byte received.
		 */
		cs = reginfo[PCKBC_KBD_SLOT].cs;
		csr = bus_space_read_4(isc->iot, isc->ioh, cs);
		csr &= ~(IOC3_KBC_CTRL_KBD_PULL_DATA_LOW |
		    IOC3_KBC_CTRL_KBD_PULL_CLOCK_LOW |
		    IOC3_KBC_CTRL_AUX_PULL_DATA_LOW |
		    IOC3_KBC_CTRL_AUX_PULL_CLOCK_LOW |
		    IOC3_KBC_CTRL_KBD_CLAMP_3 | IOC3_KBC_CTRL_AUX_CLAMP_3);
		csr |= IOC3_KBC_CTRL_KBD_CLAMP_1 | IOC3_KBC_CTRL_AUX_CLAMP_1;
		bus_space_write_4(isc->iot, isc->ioh, cs, csr);

		/* Setup pckbc_internal structure. */
		t = malloc(sizeof(struct pckbc_internal), M_DEVBUF,
		    M_WAITOK | M_ZERO);
		t->t_iot = isc->iot;
		t->t_ioh_d = isc->ioh;
		t->t_ioh_c = isc->ioh;
		t->t_addr = addr;
		t->t_sc = sc;
		sc->id = t;

		timeout_set(&t->t_cleanup, iockbc_cleanup, t);
		timeout_set(&t->t_poll, iockbc_poll, t);

		isc->reginfo = reginfo;
	}

	for (slot = 0; slot < PCKBC_NSLOTS; slot++) {
		if (t->t_slotdata[slot] == NULL) {
			t->t_slotdata[slot] =
			    malloc(sizeof(struct pckbc_slotdata),
			      M_DEVBUF, M_WAITOK);
		}
	}

	if (!console) {
		enum iockbc_slottype slottype;
		int mouse_on_main = 0;

		/*
		 * Probe for a keyboard. If none is found at the regular
		 * keyboard port, but one is found at the mouse port, then
		 * it is likely that this particular system has both ports
		 * inverted (or incorrect labels on the chassis), unless
		 * this is a human error. In any case, try to get the
		 * keyboard to attach to the `keyboard' port and the
		 * pointing device to the `mouse' port.
		 */

		for (slot = 0; slot < PCKBC_NSLOTS; slot++) {
			iockbc_init_slotdata(t->t_slotdata[slot],
			    &isc->reginfo[slot]);
			slottype = iockbc_probe_slot(t, slot);
			if (slottype == KBD)
				break;
			if (slottype == MOUSE)
				mouse_on_main = slot == PCKBC_KBD_SLOT;
		}
		if (slot == PCKBC_NSLOTS) {
			/*
			 * We could not identify a keyboard. Let's assume
			 * none is connected; if a mouse has been found on
			 * the keyboard port and none on the aux port, the
			 * ports are likely to be inverted.
			 */
			if (mouse_on_main)
				slot = PCKBC_AUX_SLOT;
			else
				slot = PCKBC_KBD_SLOT;
		}
		if (slot == PCKBC_AUX_SLOT) {
			/*
			 * Either human error or inverted wiring; use
			 * the inverted port settings.
			 * iockbc_attach_slot() below will call
			 * iockbc_init_slotdata() again.
			 */
			isc->reginfo = reginfo_inverted;
		}
	}

	/*
	 * Attach "slots". 
	 */
	iockbc_attach_slot(isc, PCKBC_KBD_SLOT);
	iockbc_attach_slot(isc, PCKBC_AUX_SLOT);
}

void
iockbc_poll(void *self)
{
	struct pckbc_internal *t = self;
        int s;

	s = spltty();
	(void)iockbcintr_internal(t, t->t_sc);
	timeout_add_sec(&t->t_poll, 1);
	splx(s);
}

int
iockbcintr(void *vsc)
{
	struct iockbc_softc *isc = (struct iockbc_softc *)vsc;
	struct pckbc_softc *sc = &isc->sc_pckbc;
	struct pckbc_internal *t = sc->id;

	return iockbcintr_internal(t, sc);
}

int
iockbcintr_internal(struct pckbc_internal *t, struct pckbc_softc *sc)
{
	pckbc_slot_t slot;
	struct pckbc_slotdata *q;
	int served = 0;
	uint32_t data;
	uint32_t val;

	/* Reschedule timeout further into the idle times. */
	if (timeout_pending(&t->t_poll))
		timeout_add_sec(&t->t_poll, 1);

	/*
	 * Need to check both "slots" since interrupt could be from
	 * either controller.
	 */ 
	for (slot = 0; slot < PCKBC_NSLOTS; slot++) {
		q = t->t_slotdata[slot];

		for (;;) {
			if (!q) {
				DPRINTF("iockbcintr: no slot%d data!\n", slot);
				break;
			}

			if (q->polling) {
				served = 1;
				break; /* pckbc_poll_data() will get it */
			}

			val = bus_space_read_4(t->t_iot, t->t_ioh_d,
			    q->reginfo->rx);
			if ((val & IOC3_KBC_DATA_VALID) == 0)
        	        	break;

			served = 1;

			/* Process received data. */
			if (val & IOC3_KBC_DATA_0_VALID) {
				data = (val & IOC3_KBC_DATA_0_MASK) >>
				    IOC3_KBC_DATA_0_SHIFT;
				iockbc_process_input(sc, t, slot, data);
			}

			if (val & IOC3_KBC_DATA_1_VALID) {
				data = (val & IOC3_KBC_DATA_1_MASK) >>
				    IOC3_KBC_DATA_1_SHIFT;
				iockbc_process_input(sc, t, slot, data);
			}

			if (val & IOC3_KBC_DATA_2_VALID) {
				data = (val & IOC3_KBC_DATA_2_MASK) >>
				    IOC3_KBC_DATA_2_SHIFT;
				iockbc_process_input(sc, t, slot, data);
			}
		}
	}
	
	return (served);
}

void
iockbc_process_input(struct pckbc_softc *sc, struct pckbc_internal *t,
    int slot, uint data)
{
	struct pckbc_slotdata *q;

	q = t->t_slotdata[slot];
	if (CMD_IN_QUEUE(q) && iockbc_cmdresponse(t, slot, data))
		return;

	if (sc->inputhandler[slot])
		(*sc->inputhandler[slot])(sc->inputarg[slot], data);
	else
		DPRINTF("iockbcintr: slot %d lost %d\n", slot, data);
}

int
iockbc_poll_write(struct pckbc_internal *t, pckbc_slot_t slot, int val)
{
	struct pckbc_slotdata *q = t->t_slotdata[slot];
	bus_space_tag_t iot = t->t_iot;
	bus_space_handle_t ioh = t->t_ioh_d;
	u_int64_t stat;
	int timeout = 10000;

	/* Attempt to write a value to the controller. */
	while (timeout--) {
		stat = bus_space_read_4(iot, ioh, q->reginfo->cs);
		if ((stat & q->reginfo->busy) == 0) {
			bus_space_write_4(iot, ioh, q->reginfo->tx, val & 0xff);
			return 0;
		}
		delay(50);
	}

	DPRINTF("iockbc_poll_write: timeout, sts %08x\n", stat);
	return -1;
}

int
iockbc_poll_read(struct pckbc_internal *t, pckbc_slot_t slot)
{
	struct pckbc_slotdata *q = t->t_slotdata[slot];
	int timeout = 10000;
	u_int32_t val;

	/* See if we already have bytes queued. */
	if (q->rx_index >= 0)
		return q->rx_queue[q->rx_index--];

        /* Poll input from controller. */
        while (timeout--) {
		val = bus_space_read_4(t->t_iot, t->t_ioh_d, q->reginfo->rx);
		if (val & IOC3_KBC_DATA_VALID)
                	break;
                delay(50);
        }
	if ((val & IOC3_KBC_DATA_VALID) == 0) {
		DPRINTF("iockbc_poll_read: timeout, wx %08x\n", val);
		return -1;
	}

	/* Process received data. */
	if (val & IOC3_KBC_DATA_2_VALID)
		q->rx_queue[++q->rx_index] =
		    (val & IOC3_KBC_DATA_2_MASK) >> IOC3_KBC_DATA_2_SHIFT;

	if (val & IOC3_KBC_DATA_1_VALID)
		q->rx_queue[++q->rx_index] =
		    (val & IOC3_KBC_DATA_1_MASK) >> IOC3_KBC_DATA_1_SHIFT;

	if (val & IOC3_KBC_DATA_0_VALID)
		q->rx_queue[++q->rx_index] =
		    (val & IOC3_KBC_DATA_0_MASK) >> IOC3_KBC_DATA_0_SHIFT;

	if (q->rx_index >= 0)
		return q->rx_queue[q->rx_index--];
	else 
		return -1;
}

/*
 * Pass command to device, poll for ACK and data.
 * to be called at spltty()
 */
static void
iockbc_poll_cmd(struct pckbc_internal *t, pckbc_slot_t slot, 
    struct pckbc_devcmd *cmd)
{
	int i, c = 0;

	while (cmd->cmdidx < cmd->cmdlen) {
		if (iockbc_poll_write(t, slot, cmd->cmd[cmd->cmdidx]) == -1) {
			DPRINTF("iockbc_poll_cmd: send error\n");
			cmd->status = EIO;
			return;
		}
		for (i = 10; i; i--) { /* 1s ??? */
			c = iockbc_poll_read(t, slot);
			if (c != -1)
				break;
		}
		if (c == KBC_DEVCMD_ACK) {
			cmd->cmdidx++;
			continue;
		}
		if (c == KBC_DEVCMD_RESEND) {
			DPRINTF("iockbc_cmd: RESEND\n");
			if (cmd->retries++ < 5)
				continue;
			else {
				DPRINTF("iockbc: cmd failed\n");
				cmd->status = EIO;
				return;
			}
		}
		if (c == -1) {
			DPRINTF("iockbc_cmd: timeout\n");
			cmd->status = EIO;
			return;
		}
		DPRINTF("iockbc_cmd: lost 0x%x\n", c);
	}

	while (cmd->responseidx < cmd->responselen) {
		if (cmd->flags & KBC_CMDFLAG_SLOW)
			i = 100; /* 10s ??? */
		else
			i = 10; /* 1s ??? */
		while (i--) {
			c = iockbc_poll_read(t, slot);
			if (c != -1)
				break;
		}
		if (c == -1) {
			DPRINTF("iockbc_poll_cmd: no data\n");
			cmd->status = ETIMEDOUT;
			return;
		} else
			cmd->response[cmd->responseidx++] = c;
	}
}

/*
 * Clean up a command queue, throw away everything.
 */
void
iockbc_cleanqueue(struct pckbc_slotdata *q)
{
	struct pckbc_devcmd *cmd;
#ifdef IOCKBC_DEBUG
	int i;
#endif

	while ((cmd = TAILQ_FIRST(&q->cmdqueue))) {
		TAILQ_REMOVE(&q->cmdqueue, cmd, next);
#ifdef IOCKBC_DEBUG
		printf("iockbc_cleanqueue: removing");
		for (i = 0; i < cmd->cmdlen; i++)
			printf(" %02x", cmd->cmd[i]);
		printf("\n");
#endif
		TAILQ_INSERT_TAIL(&q->freequeue, cmd, next);
	}
}

/*
 * Timeout error handler: clean queues and data port.
 * XXX could be less invasive.
 */
void
iockbc_cleanup(void *self)
{
	struct pckbc_internal *t = self;
	int s;

	printf("iockbc: command timeout\n");

	s = spltty();

	if (t->t_slotdata[PCKBC_KBD_SLOT])
		iockbc_cleanqueue(t->t_slotdata[PCKBC_KBD_SLOT]);
	if (t->t_slotdata[PCKBC_AUX_SLOT])
		iockbc_cleanqueue(t->t_slotdata[PCKBC_AUX_SLOT]);

	while (iockbc_poll_read(t, PCKBC_KBD_SLOT) 
	       != -1) ;
	while (iockbc_poll_read(t, PCKBC_AUX_SLOT) 
	       != -1) ;

	/* Reset KBC? */

	splx(s);
}

/*
 * Pass command to device during normal operation.
 * to be called at spltty()
 */
void
iockbc_start(struct pckbc_internal *t, pckbc_slot_t slot)
{
	struct pckbc_slotdata *q = t->t_slotdata[slot];
	struct pckbc_devcmd *cmd = TAILQ_FIRST(&q->cmdqueue);

	if (q->polling) {
		do {
			iockbc_poll_cmd(t, slot, cmd);
			if (cmd->status)
				printf("iockbc_start: command error\n");

			TAILQ_REMOVE(&q->cmdqueue, cmd, next);
			if (cmd->flags & KBC_CMDFLAG_SYNC)
				wakeup(cmd);
			else {
				timeout_del(&t->t_cleanup);
				TAILQ_INSERT_TAIL(&q->freequeue, cmd, next);
			}
			cmd = TAILQ_FIRST(&q->cmdqueue);
		} while (cmd);
		return;
	}

	if (iockbc_poll_write(t, slot, cmd->cmd[cmd->cmdidx])) {
		printf("iockbc_start: send error\n");
		/* XXX what now? */
		return;
	}
}

/*
 * Handle command responses coming in asynchronously,
 * return nonzero if valid response.
 * to be called at spltty()
 */
int
iockbc_cmdresponse(struct pckbc_internal *t, pckbc_slot_t slot, u_char data)
{
	struct pckbc_slotdata *q = t->t_slotdata[slot];
	struct pckbc_devcmd *cmd = TAILQ_FIRST(&q->cmdqueue);
#ifdef DIAGNOSTIC
	if (!cmd)
		panic("iockbc_cmdresponse: no active command");
#endif
	if (cmd->cmdidx < cmd->cmdlen) {
		if (data != KBC_DEVCMD_ACK && data != KBC_DEVCMD_RESEND)
			return (0);

		if (data == KBC_DEVCMD_RESEND) {
			if (cmd->retries++ < 5) {
				/* try again last command */
				goto restart;
			} else {
				DPRINTF("iockbc: cmd failed\n");
				cmd->status = EIO;
				/* dequeue */
			}
		} else {
			if (++cmd->cmdidx < cmd->cmdlen)
				goto restart;
			if (cmd->responselen)
				return (1);
			/* else dequeue */
		}
	} else if (cmd->responseidx < cmd->responselen) {
		cmd->response[cmd->responseidx++] = data;
		if (cmd->responseidx < cmd->responselen)
			return (1);
		/* else dequeue */
	} else
		return (0);

	/* dequeue: */
	TAILQ_REMOVE(&q->cmdqueue, cmd, next);
	if (cmd->flags & KBC_CMDFLAG_SYNC)
		wakeup(cmd);
	else {
		timeout_del(&t->t_cleanup);
		TAILQ_INSERT_TAIL(&q->freequeue, cmd, next);
	}
	if (!CMD_IN_QUEUE(q))
		return (1);
restart:
	iockbc_start(t, slot);
	return (1);
}

/*
 * Interfaces to act like pckbc(4).
 */

int
pckbc_xt_translation(pckbc_tag_t self)
{
	/* Translation isn't supported... */
	return (-1);
}

/* For use in autoconfiguration. */
int
pckbc_poll_cmd(pckbc_tag_t self, pckbc_slot_t slot, u_char *cmd, int len, 
    int responselen, u_char *respbuf, int slow)
{
	struct pckbc_devcmd nc;
	int s;

	if ((len > 4) || (responselen > 4))
		return (EINVAL);

	bzero(&nc, sizeof(nc));
	bcopy(cmd, nc.cmd, len);
	nc.cmdlen = len;
	nc.responselen = responselen;
	nc.flags = (slow ? KBC_CMDFLAG_SLOW : 0);

	s = spltty();
	iockbc_poll_cmd(self, slot, &nc);
	splx(s);

	if (nc.status == 0 && respbuf)
		bcopy(nc.response, respbuf, responselen);

	return (nc.status);
}

void
pckbc_flush(pckbc_tag_t self, pckbc_slot_t slot)
{
	/* Read any data and discard. */
	struct pckbc_internal *t = self;
	(void) iockbc_poll_read(t, slot);
}

/*
 * Put command into the device's command queue, return zero or errno.
 */
int
pckbc_enqueue_cmd(pckbc_tag_t self, pckbc_slot_t slot, u_char *cmd, int len, 
    int responselen, int sync, u_char *respbuf)
{
	struct pckbc_internal *t = self;
	struct pckbc_slotdata *q = t->t_slotdata[slot];
	struct pckbc_devcmd *nc;
	int s, isactive, res = 0;

	if ((len > 4) || (responselen > 4))
		return (EINVAL);
	s = spltty();
	nc = TAILQ_FIRST(&q->freequeue);
	if (nc)
		TAILQ_REMOVE(&q->freequeue, nc, next);
	splx(s);
	if (!nc)
		return (ENOMEM);

	bzero(nc, sizeof(*nc));
	bcopy(cmd, nc->cmd, len);
	nc->cmdlen = len;
	nc->responselen = responselen;
	nc->flags = (sync ? KBC_CMDFLAG_SYNC : 0);

	s = spltty();

	if (q->polling && sync) {
		/*
		 * XXX We should poll until the queue is empty.
		 * But we don't come here normally, so make
		 * it simple and throw away everything.
		 */
		iockbc_cleanqueue(q);
	}

	isactive = CMD_IN_QUEUE(q);
	TAILQ_INSERT_TAIL(&q->cmdqueue, nc, next);
	if (!isactive)
		iockbc_start(t, slot);

	if (q->polling)
		res = (sync ? nc->status : 0);
	else if (sync) {
		if ((res = tsleep(nc, 0, "kbccmd", 1*hz))) {
			TAILQ_REMOVE(&q->cmdqueue, nc, next);
			iockbc_cleanup(t);
		} else
			res = nc->status;
	} else
		timeout_add_sec(&t->t_cleanup, 1);

	if (sync) {
		if (respbuf)
			bcopy(nc->response, respbuf, responselen);
		TAILQ_INSERT_TAIL(&q->freequeue, nc, next);
	}

	splx(s);

	return (res);
}

int
pckbc_poll_data(pckbc_tag_t self, pckbc_slot_t slot)
{
	struct pckbc_internal *t = self;
	struct pckbc_slotdata *q = t->t_slotdata[slot];
	int c;

	c = iockbc_poll_read(t, slot);
	if (c != -1 && q && CMD_IN_QUEUE(q)) {
		/* We jumped into a running command - try to deliver the 
		   response. */
		if (iockbc_cmdresponse(t, slot, c))
			return (-1);
	}
	return (c);
}

void
pckbc_set_inputhandler(pckbc_tag_t self, pckbc_slot_t slot, pckbc_inputfcn func,
    void *arg, char *name)
{
	struct pckbc_internal *t = (struct pckbc_internal *)self;
	struct pckbc_softc *sc = t->t_sc;
	struct iockbc_softc *isc = (struct iockbc_softc *)sc;

	if (slot >= PCKBC_NSLOTS)
		panic("iockbc_set_inputhandler: bad slot %d", slot);

	sc->inputhandler[slot] = func;
	sc->inputarg[slot] = arg;
	sc->subname[slot] = name;

	if ((isc == NULL || isc->console) && slot == PCKBC_KBD_SLOT)
		timeout_add_sec(&t->t_poll, 1);
}

void
pckbc_slot_enable(pckbc_tag_t self, pckbc_slot_t slot, int on)
{
	struct pckbc_internal *t = (struct pckbc_internal *)self;

	if (slot == PCKBC_KBD_SLOT) {
		if (on)
			timeout_add_sec(&t->t_poll, 1);
		else
			timeout_del(&t->t_poll);
	}
}

void
pckbc_set_poll(pckbc_tag_t self, pckbc_slot_t slot, int on)
{
	struct pckbc_internal *t = (struct pckbc_internal *)self;

	t->t_slotdata[slot]->polling = on;

	if (!on) {
		int s;

		/*
		 * If disabling polling on a device that's been configured,
		 * make sure there are no bytes left in the FIFO, holding up
		 * the interrupt line.  Otherwise we won't get any further
		 * interrupts.
		 */
		if (t->t_sc) {
			s = spltty();
			iockbcintr(t->t_sc);
			splx(s);
		}
	}
}

/*
 * Console support.
 */

static int iockbc_console;

int
iockbc_cnattach()
{
	bus_space_tag_t iot = &sys_config.console_io;
	bus_space_handle_t ioh = (bus_space_handle_t)iot->bus_base;
	struct pckbc_internal *t = &iockbc_consdata;
	const struct iockbc_reginfo *reginfo = NULL, *reginfo_inverted;
	enum iockbc_slottype slottype;
	pckbc_slot_t slot;
	uint32_t csr;
	int is_ioc;
	int rc;

	is_ioc = console_input.specific ==
	    PCI_ID_CODE(PCI_VENDOR_SGI, PCI_PRODUCT_SGI_IOC3);
	if (is_ioc) {
#if NIOCKBC_IOC > 0
		reginfo = iockbc_ioc;
		reginfo_inverted = iockbc_ioc_inverted;
#endif
	} else {
#if NIOCKBC_IOF > 0
		reginfo = iockbc_iof;
		reginfo_inverted = iockbc_iof_inverted;
#endif
	}
	if (reginfo == NULL)
		return ENXIO;

	/*
	 * Setup up controller: do not force pull clock and data lines
	 * low, clamp clocks after one byte received.
	 */
	csr = bus_space_read_4(iot, ioh, reginfo->cs);
	csr &= ~(IOC3_KBC_CTRL_KBD_PULL_DATA_LOW |
	    IOC3_KBC_CTRL_KBD_PULL_CLOCK_LOW |
	    IOC3_KBC_CTRL_AUX_PULL_DATA_LOW |
	    IOC3_KBC_CTRL_AUX_PULL_CLOCK_LOW |
	    IOC3_KBC_CTRL_KBD_CLAMP_3 | IOC3_KBC_CTRL_AUX_CLAMP_3);
	csr |= IOC3_KBC_CTRL_KBD_CLAMP_1 | IOC3_KBC_CTRL_AUX_CLAMP_1;
	bus_space_write_4(iot, ioh, reginfo->cs, csr);

	/* Setup pckbc_internal structure. */
	t->t_iot = iot;
	t->t_ioh_d = (bus_space_handle_t)iot->bus_base;
	t->t_addr = 0;	/* unused */

	timeout_set(&t->t_cleanup, iockbc_cleanup, t);
	timeout_set(&t->t_poll, iockbc_poll, t);

	/*
	 * Probe for a keyboard. There must be one connected, for the PROM
	 * would not have advertized glass console if none had been
	 * detected.
	 */

	for (slot = 0; slot < PCKBC_NSLOTS; slot++) {
		iockbc_init_slotdata(&iockbc_cons_slotdata, &reginfo[slot]);
		t->t_slotdata[slot] = &iockbc_cons_slotdata;
		slottype = iockbc_probe_slot(t, slot);
		t->t_slotdata[slot] = NULL;
		if (slottype == KBD)
			break;
	}
	if (slot == PCKBC_NSLOTS) {
		/*
		 * We could not identify a keyboard, but the PROM did;
		 * let's assume it's a fluke and assume it exists and
		 * is connected to the first connector.
		 */
		slot = PCKBC_KBD_SLOT;
		/*
		 * For some reason keyboard and mouse ports are inverted on
		 * Fuel. They also are inverted on some IO9 boards, but
		 * we can't tell both IO9 flavour apart, yet.
		 */
		if (is_ioc && sys_config.system_type == SGI_IP35)
			slot = PCKBC_AUX_SLOT;
	}

	if (slot == PCKBC_AUX_SLOT) {
		/*
		 * Either human error when plugging the keyboard, or the
		 * physical connectors on the chassis are inverted.
		 * Compensate by switching in software (pckbd relies upon
		 * being at PCKBC_KBD_SLOT).
		 */
		reginfo = reginfo_inverted;
	}

	iockbc_init_slotdata(&iockbc_cons_slotdata, &reginfo[PCKBC_KBD_SLOT]);
	t->t_slotdata[PCKBC_KBD_SLOT] = &iockbc_cons_slotdata;

	rc = pckbd_cnattach(t);
	if (rc == 0)
		iockbc_console = 1;

	return rc;
}

#if NIOCKBC_IOC > 0
int
iockbc_is_ioc_console(struct ioc_attach_args *iaa)
{
	if (iockbc_console == 0)
		return 0;

	return location_match(&iaa->iaa_location, &console_input);
}
#endif

#if NIOCKBC_IOF > 0
int
iockbc_is_iof_console(struct iof_attach_args *iaa)
{
	if (iockbc_console == 0)
		return 0;

	return location_match(&iaa->iaa_location, &console_input);
}
#endif
