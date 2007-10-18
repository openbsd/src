/*
 * Copyright (c) 2006-2007, Joel Sing
 * All rights reserved
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
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF MIND,
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Derived from sys/dev/ic/pckbc.c under the following terms:
 * $OpenBSD: mkbc.c,v 1.1 2007/10/18 18:59:29 jsing Exp $ 
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
 * Driver for Moosehead PS/2 Controllers (mkbc)
 *
 * There are actually two separate controllers attached to the macebus.
 * However in the interest of reusing code, we want to act like a pckbc
 * so that we can directly attach pckbd and pms. As a result, we make 
 * each controller look like a "slot" and combine them into a single device.
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/timeout.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/signalvar.h>

#include <machine/autoconf.h>

#include <mips64/archtype.h>

#include <sgi/localbus/macebus.h>
#include <sgi/localbus/crimebus.h>

#include <dev/ic/pckbcvar.h>

#include <sys/errno.h>
#include <sys/queue.h>
#include <sys/lock.h>

#include <machine/bus.h>

#include "mkbcreg.h"

const char *mkbc_slot_names[] = { "kbd", "mouse" };

#define KBC_DEVCMD_ACK 0xfa
#define KBC_DEVCMD_RESEND 0xfe

#define KBD_DELAY       DELAY(8)

struct mkbc_softc {
	struct pckbc_softc sc_pckbc;
	int sc_irq;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
};

int mkbc_match(struct device *, void *, void *);
void mkbc_attach(struct device *, struct device *, void *);

struct cfattach mkbc_ca = {
	sizeof(struct mkbc_softc), mkbc_match, mkbc_attach
};

struct cfdriver mkbc_cd = {
	NULL, "mkbc", DV_DULL
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
	bus_space_handle_t ioh;
};

#define CMD_IN_QUEUE(q) (TAILQ_FIRST(&(q)->cmdqueue) != NULL)

void mkbc_start(struct pckbc_internal *, pckbc_slot_t);
int mkbc_attach_slot(struct mkbc_softc *, pckbc_slot_t);
void mkbc_init_slotdata(struct pckbc_slotdata *);
int mkbc_submatch(struct device *, void *, void *);
int mkbcprint(void *, const char *);
int mkbcintr(void *);
void mkbc_cleanqueue(struct pckbc_slotdata *);
void mkbc_cleanup(void *self);
int mkbc_cmdresponse(struct pckbc_internal *t, pckbc_slot_t slot, u_char data);
int mkbc_poll_read(bus_space_tag_t, bus_space_handle_t);
int mkbc_poll_write(bus_space_tag_t, bus_space_handle_t, int);

int
mkbc_match(struct device *parent, void *cf, void *aux)
{
	struct confargs *ca = aux;

	/* MACE PS/2 controller only on SGI O2 */
	if (ca->ca_sys == SGI_O2)
		return 1;

	return 0;
}

void
mkbc_init_slotdata(struct pckbc_slotdata *q)
{
	int i;
	TAILQ_INIT(&q->cmdqueue);
	TAILQ_INIT(&q->freequeue);

	for (i = 0; i < NCMD; i++) {
		TAILQ_INSERT_TAIL(&q->freequeue, &(q->cmds[i]), next);
	}
	q->polling = 0;
}

int
mkbcprint(void *aux, const char *pnp)
{
	struct pckbc_attach_args *pa = aux;

	if (!pnp)
		printf(" (%s slot)", mkbc_slot_names[pa->pa_slot]);
	return (QUIET);
}

int
mkbc_submatch(struct device *parent, void *match, void *aux)
{
	struct cfdata *cf = match;
	struct pckbc_attach_args *pa = aux;

	if (cf->cf_loc[PCKBCCF_SLOT] != PCKBCCF_SLOT_DEFAULT &&
	    cf->cf_loc[PCKBCCF_SLOT] != pa->pa_slot)
		return (0);
	return ((*cf->cf_attach->ca_match)(parent, cf, aux));
}

int
mkbc_attach_slot(struct mkbc_softc *msc, pckbc_slot_t slot)
{
	struct pckbc_softc *sc = &msc->sc_pckbc;
	struct pckbc_internal *t = sc->id;
	struct pckbc_attach_args pa;
	bus_space_handle_t ioh;
	int found;

	if (!t->t_slotdata[slot]) {

		t->t_slotdata[slot] = malloc(sizeof(struct pckbc_slotdata),
					     M_DEVBUF, M_NOWAIT);

		if (t->t_slotdata[slot] == NULL) {
			printf("Failed to allocate slot data!\n");
			return 0;
		}
		mkbc_init_slotdata(t->t_slotdata[slot]);

		/* Map subregion of bus space for this "slot". */
		if (bus_space_subregion(msc->iot, msc->ioh, 
					MKBC_PORTSIZE * slot, 
					MKBC_PORTSIZE, &ioh)) {
			printf("Unable to map slot subregion!\n");
			return 0;
		}
		t->t_slotdata[slot]->ioh = ioh;

		/* Initialise controller. */
		bus_space_write_8(msc->iot, ioh, MKBC_CONTROL,
			MKBC_CONTROL_TX_CLOCK_DISABLE | MKBC_CONTROL_RESET);
		delay(100); /* 100us */

		/* Enable controller. */
		bus_space_write_8(t->t_iot, ioh, MKBC_CONTROL,
			MKBC_CONTROL_RX_CLOCK_ENABLE | MKBC_CONTROL_TX_ENABLE);

	}

	pa.pa_tag = t;
	pa.pa_slot = slot;
	found = (config_found_sm((struct device *)msc, &pa,
				 mkbcprint, mkbc_submatch) != NULL);

	return (found);
}

void
mkbc_attach(struct device *parent, struct device *self, void *aux)
{
	struct mkbc_softc *msc = (void*)self;
	struct confargs *ca = aux;
	struct pckbc_softc *sc = &msc->sc_pckbc;
	struct pckbc_internal *t;
	void *rv = NULL;

	/* Setup bus space mapping. */
	msc->iot = ca->ca_iot;
	if (bus_space_map(msc->iot, ca->ca_baseaddr, MKBC_PORTSIZE * 2, 0, 
	    &msc->ioh)) {
		printf(": unable to map bus space!\n");
		return;
	}

	/* Setup pckbc_internal structure (from pckbc_isa.c). */
	t = malloc(sizeof(struct pckbc_internal), M_DEVBUF, M_WAITOK);
	bzero(t, sizeof(struct pckbc_internal));
	t->t_iot = msc->iot;
	t->t_ioh_d = NULL;
	t->t_ioh_c = NULL;
	t->t_addr = ca->ca_baseaddr;
	t->t_cmdbyte = 0;
	t->t_sc = (struct pckbc_softc *)msc;
	sc->id = t;

	/* Establish interrupt handler. */
	msc->sc_irq = ca->ca_intr;
	rv = macebus_intr_establish(NULL, msc->sc_irq, IST_EDGE,
		IPL_TTY, mkbcintr, msc, sc->sc_dv.dv_xname);
	if (rv == NULL) {
		printf(": unable to establish interrupt\n");
	} else {
		printf(": using irq %d\n", msc->sc_irq);
	}

	/*
	 * Attach "slots" - technically these are separate controllers
	 * in the same bus space, however we want to act like pckbc so
	 * that we can attach pckbd and pms. 
	 */
	mkbc_attach_slot(msc, PCKBC_KBD_SLOT);
	mkbc_attach_slot(msc, PCKBC_AUX_SLOT);

	return;

}

int
mkbcintr(void *vsc)
{
	struct mkbc_softc *msc = (struct mkbc_softc *)vsc;
	struct pckbc_softc *sc = &msc->sc_pckbc;
	struct pckbc_internal *t = sc->id;
	pckbc_slot_t slot;
	struct pckbc_slotdata *q;
	int served = 0;
	u_int64_t stat;
	u_int64_t data;

	/*
	 * Need to check both "slots" since interrupt could be from
	 * either controller.
	 */ 
	slot = PCKBC_KBD_SLOT;
	q = t->t_slotdata[slot];
	for(;;) {

		if (!q) {
			printf("mkbcintr: no kbd slot data!\n");
			break;
		}

		stat = bus_space_read_8(t->t_iot, q->ioh, MKBC_STATUS);
		if (!(stat & MKBC_STATUS_RX_FULL))
			break;

		served = 1;

		if (q->polling)
			break; /* pckbc_poll_data() will get it */

		KBD_DELAY;
		data = bus_space_read_8(t->t_iot, q->ioh, MKBC_RX_PORT);
		if (CMD_IN_QUEUE(q) && mkbc_cmdresponse(t, slot, data))
			continue;

		if (sc->inputhandler[slot])
			(*sc->inputhandler[slot])(sc->inputarg[slot], data);
#ifdef MKBCDEBUG
		else
			printf("mkbcintr: slot %d lost %d\n", slot, data);
#endif
	}
	
	/* Mouse controller/slot. */
	slot = PCKBC_AUX_SLOT;
	q = t->t_slotdata[slot];
	for(;;) {

		if (!q) {
			printf("mkbcintr: no mouse slot data!\n");
			break;
		}

		stat = bus_space_read_8(t->t_iot, q->ioh, MKBC_STATUS);
		if (!(stat & MKBC_STATUS_RX_FULL))
			break;

		served = 1;

		if (q->polling)
			break; /* pckbc_poll_data() will get it. */

		KBD_DELAY;
		data = bus_space_read_8(t->t_iot, q->ioh, MKBC_RX_PORT);
		if (CMD_IN_QUEUE(q) && mkbc_cmdresponse(t, slot, data))
			continue;

		if (sc->inputhandler[slot])
			(*sc->inputhandler[slot])(sc->inputarg[slot], data);
#ifdef MKBCDEBUG
		else
			printf("mkbcintr: slot %d lost %d\n", slot, data);
#endif
	}

	return (served);

}

int
mkbc_poll_write(bus_space_tag_t iot, bus_space_handle_t ioh, int val)
{

	int timeout = 10000;
	u_int64_t stat;

	/* Attempt to write a value to the controller. */
	while (timeout--) {
		stat = bus_space_read_8(iot, ioh, MKBC_STATUS);
		if (stat & MKBC_STATUS_TX_EMPTY) {
		    bus_space_write_8(iot, ioh, MKBC_TX_PORT, val & 0xff);
		    return 0;
		}
		delay(50);
	}
	return -1;

}

int
mkbc_poll_read(bus_space_tag_t iot, bus_space_handle_t ioh)
{

	int timeout = 10000;
	u_int64_t stat, val;

	/* Poll input from controller. */
	while (timeout--) {
		stat = bus_space_read_8(iot, ioh, MKBC_STATUS);
		if (stat & MKBC_STATUS_RX_FULL) {
		    val = bus_space_read_8(iot, ioh, MKBC_RX_PORT);
		    return val & 0xff;
		}
		delay(50);
	}
	return -1;

}

/*
 * Pass command to device, poll for ACK and data.
 * to be called at spltty()
 */
static void
mkbc_poll_cmd(struct pckbc_internal *t, pckbc_slot_t slot, 
	      struct pckbc_devcmd *cmd)
{
	bus_space_tag_t iot = t->t_iot;
	bus_space_handle_t ioh = t->t_slotdata[slot]->ioh;

	int i, c = 0;

	while (cmd->cmdidx < cmd->cmdlen) {
		if (mkbc_poll_write(iot, ioh, cmd->cmd[cmd->cmdidx]) == -1) {
			printf("mkbc_poll_cmd: send error\n");
			cmd->status = EIO;
			return;
		}
		for (i = 10; i; i--) { /* 1s ??? */
			c = mkbc_poll_read(iot, ioh);
			if (c != -1)
				break;
		}

		if (c == KBC_DEVCMD_ACK) {
			cmd->cmdidx++;
			continue;
		}
		if (c == KBC_DEVCMD_RESEND) {
#ifdef MKBCDEBUG
			printf("mkbc_cmd: RESEND\n");
#endif
			if (cmd->retries++ < 5)
				continue;
			else {
#ifdef MKBCDEBUG
				printf("mkbc: cmd failed\n");
#endif
				cmd->status = EIO;
				return;
			}
		}
		if (c == -1) {
#ifdef MKBCDEBUG
			printf("mkbc_cmd: timeout\n");
#endif
			cmd->status = EIO;
			return;
		}
#ifdef MKBCDEBUG
		printf("mkbc_cmd: lost 0x%x\n", c);
#endif
	}

	while (cmd->responseidx < cmd->responselen) {
		if (cmd->flags & KBC_CMDFLAG_SLOW)
			i = 100; /* 10s ??? */
		else
			i = 10; /* 1s ??? */
		while (i--) {
			c = mkbc_poll_read(iot, ioh);
			if (c != -1)
				break;
		}
		if (c == -1) {
#ifdef MKBCDEBUG
			printf("mkbc_poll_cmd: no data\n");
#endif
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
mkbc_cleanqueue(struct pckbc_slotdata *q)
{
	struct pckbc_devcmd *cmd;
#ifdef MKBCDEBUG
	int i;
#endif

	while ((cmd = TAILQ_FIRST(&q->cmdqueue))) {
		TAILQ_REMOVE(&q->cmdqueue, cmd, next);
#ifdef MKBCDEBUG
		printf("mkbc_cleanqueue: removing");
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
mkbc_cleanup(void *self)
{
	struct pckbc_internal *t = self;
	int s;

	printf("mkbc: command timeout\n");

	s = spltty();

	if (t->t_slotdata[PCKBC_KBD_SLOT])
		mkbc_cleanqueue(t->t_slotdata[PCKBC_KBD_SLOT]);
	if (t->t_slotdata[PCKBC_AUX_SLOT])
		mkbc_cleanqueue(t->t_slotdata[PCKBC_AUX_SLOT]);

	while (mkbc_poll_read(t->t_iot, t->t_slotdata[PCKBC_KBD_SLOT]->ioh) 
	       != -1) ;
	while (mkbc_poll_read(t->t_iot, t->t_slotdata[PCKBC_AUX_SLOT]->ioh) 
	       != -1) ;

	/* Reset KBC? */

	splx(s);
}

/*
 * Pass command to device during normal operation.
 * to be called at spltty()
 */
void
mkbc_start(struct pckbc_internal *t, pckbc_slot_t slot)
{
	struct pckbc_slotdata *q = t->t_slotdata[slot];
	struct pckbc_devcmd *cmd = TAILQ_FIRST(&q->cmdqueue);

	if (q->polling) {
		do {
			mkbc_poll_cmd(t, slot, cmd);
			if (cmd->status)
				printf("mkbc_start: command error\n");

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

	if (mkbc_poll_write(t->t_iot, t->t_slotdata[slot]->ioh, 
	    cmd->cmd[cmd->cmdidx])) {
		printf("mkbc_start: send error\n");
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
mkbc_cmdresponse(struct pckbc_internal *t, pckbc_slot_t slot, u_char data)
{
	struct pckbc_slotdata *q = t->t_slotdata[slot];
	struct pckbc_devcmd *cmd = TAILQ_FIRST(&q->cmdqueue);
#ifdef DIAGNOSTIC
	if (!cmd)
		panic("mkbc_cmdresponse: no active command");
#endif
	if (cmd->cmdidx < cmd->cmdlen) {
		if (data != KBC_DEVCMD_ACK && data != KBC_DEVCMD_RESEND)
			return (0);

		if (data == KBC_DEVCMD_RESEND) {
			if (cmd->retries++ < 5) {
				/* try again last command */
				goto restart;
			} else {
#ifdef MKBCDEBUG
				printf("mkbc: cmd failed\n");
#endif
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
	mkbc_start(t, slot);
	return (1);
}

/*
 * Interfaces to act like a pckbc(4).
 */

int
pckbc_xt_translation(pckbc_tag_t self, pckbc_slot_t slot, int on)
{
	/* Translation isn't supported... */
	return 0;
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
	mkbc_poll_cmd(self, slot, &nc);
	splx(s);

	if (nc.status == 0 && respbuf)
		bcopy(nc.response, respbuf, responselen);

	return (nc.status);
}

void
pckbc_flush(pckbc_tag_t self, pckbc_slot_t slot)
{
	/* Read any data and discard */
	struct pckbc_internal *t = self;
	(void) mkbc_poll_read(t->t_iot, t->t_slotdata[slot]->ioh);
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
	if (nc) {
		TAILQ_REMOVE(&q->freequeue, nc, next);
	}
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
		mkbc_cleanqueue(q);
	}

	isactive = CMD_IN_QUEUE(q);
	TAILQ_INSERT_TAIL(&q->cmdqueue, nc, next);
	if (!isactive)
		mkbc_start(t, slot);

	if (q->polling)
		res = (sync ? nc->status : 0);
	else if (sync) {
		if ((res = tsleep(nc, 0, "kbccmd", 1*hz))) {
			TAILQ_REMOVE(&q->cmdqueue, nc, next);
			mkbc_cleanup(t);
		} else
			res = nc->status;
	} else
		timeout_add(&t->t_cleanup, hz);

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

	c = mkbc_poll_read(t->t_iot, q->ioh);
	if (c != -1 && q && CMD_IN_QUEUE(q)) {
		/* we jumped into a running command - try to
		 deliver the response */
		if (mkbc_cmdresponse(t, slot, c))
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

	if (slot >= PCKBC_NSLOTS)
		panic("mkbc_set_inputhandler: bad slot %d", slot);

	sc->inputhandler[slot] = func;
	sc->inputarg[slot] = arg;
	sc->subname[slot] = name;
}

void
pckbc_slot_enable(pckbc_tag_t self, pckbc_slot_t slot, int on)
{
	struct pckbc_internal *t = (struct pckbc_internal *)self;

	/*
	 * Should we also enable/disable the controller?? 
	 * If we did then none of the poll_ functions would work...
	 */

	if (on) {
	
		/* Enable controller interrupts. */
		bus_space_write_8(t->t_iot, t->t_slotdata[slot]->ioh, 
			MKBC_CONTROL,
			MKBC_CONTROL_RX_CLOCK_ENABLE | MKBC_CONTROL_TX_ENABLE
			| MKBC_CONTROL_RX_INT_ENABLE);

	} else {

		/* Disable controller interrupts. */
		bus_space_write_8(t->t_iot, t->t_slotdata[slot]->ioh, 
			MKBC_CONTROL,
			MKBC_CONTROL_RX_CLOCK_ENABLE | MKBC_CONTROL_TX_ENABLE);

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
			mkbcintr(t->t_sc);
			splx(s);
		}
	}
}
