/* $OpenBSD: pckbc.c,v 1.9 2004/11/02 21:21:00 miod Exp $ */
/* $NetBSD: pckbc.c,v 1.5 2000/06/09 04:58:35 soda Exp $ */

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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/timeout.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/errno.h>
#include <sys/queue.h>
#include <sys/lock.h>

#include <machine/bus.h>

#include <dev/ic/i8042reg.h>
#include <dev/ic/pckbcvar.h>

#include "pckbd.h"

#if (NPCKBD > 0)
#include <dev/pckbc/pckbdvar.h>
#endif

/* descriptor for one device command */
struct pckbc_devcmd {
	TAILQ_ENTRY(pckbc_devcmd) next;
	int flags;
#define KBC_CMDFLAG_SYNC 1 /* give descriptor back to caller */
#define KBC_CMDFLAG_SLOW 2
	u_char cmd[4];
	int cmdlen, cmdidx, retries;
	u_char response[4];
	int status, responselen, responseidx;
};

/* data per slave device */
struct pckbc_slotdata {
	int polling; /* don't read data port in interrupt handler */
	TAILQ_HEAD(, pckbc_devcmd) cmdqueue; /* active commands */
	TAILQ_HEAD(, pckbc_devcmd) freequeue; /* free commands */
#define NCMD 5
	struct pckbc_devcmd cmds[NCMD];
};

#define CMD_IN_QUEUE(q) (TAILQ_FIRST(&(q)->cmdqueue) != NULL)

void pckbc_init_slotdata(struct pckbc_slotdata *);
int pckbc_attach_slot(struct pckbc_softc *, pckbc_slot_t);
int pckbc_submatch(struct device *, void *, void *);
int pckbcprint(void *, const char *);

struct pckbc_internal pckbc_consdata;
int pckbc_console_attached;

static int pckbc_console;
static struct pckbc_slotdata pckbc_cons_slotdata;

static int pckbc_wait_output(bus_space_tag_t, bus_space_handle_t);

static int pckbc_get8042cmd(struct pckbc_internal *);
static int pckbc_put8042cmd(struct pckbc_internal *);
static int pckbc_send_devcmd(struct pckbc_internal *, pckbc_slot_t,
				  u_char);
static void pckbc_poll_cmd1(struct pckbc_internal *, pckbc_slot_t,
				 struct pckbc_devcmd *);

void pckbc_cleanqueue(struct pckbc_slotdata *);
void pckbc_cleanup(void *);
int pckbc_cmdresponse(struct pckbc_internal *, pckbc_slot_t, u_char);
void pckbc_start(struct pckbc_internal *, pckbc_slot_t);

const char *pckbc_slot_names[] = { "kbd", "aux" };

#define KBC_DEVCMD_ACK 0xfa
#define KBC_DEVCMD_RESEND 0xfe

#define	KBD_DELAY	DELAY(8)

static inline int
pckbc_wait_output(iot, ioh_c)
	bus_space_tag_t iot;
	bus_space_handle_t ioh_c;
{
	u_int i;

	for (i = 100000; i; i--)
		if (!(bus_space_read_1(iot, ioh_c, 0) & KBS_IBF)) {
			KBD_DELAY;
			return (1);
		}
	return (0);
}

int
pckbc_send_cmd(iot, ioh_c, val)
	bus_space_tag_t iot;
	bus_space_handle_t ioh_c;
	u_char val;
{
	if (!pckbc_wait_output(iot, ioh_c))
		return (0);
	bus_space_write_1(iot, ioh_c, 0, val);
	return (1);
}

int
pckbc_poll_data1(iot, ioh_d, ioh_c, slot, checkaux)
	bus_space_tag_t iot;
	bus_space_handle_t ioh_d, ioh_c;
	pckbc_slot_t slot;
	int checkaux;
{
	int i;
	u_char stat;

	/* if 1 port read takes 1us (?), this polls for 100ms */
	for (i = 100000; i; i--) {
		stat = bus_space_read_1(iot, ioh_c, 0);
		if (stat & KBS_DIB) {
			register u_char c;

			KBD_DELAY;
			c = bus_space_read_1(iot, ioh_d, 0);
			if (checkaux && (stat & 0x20)) { /* aux data */
				if (slot != PCKBC_AUX_SLOT) {
#ifdef PCKBCDEBUG
					printf("lost aux 0x%x\n", c);
#endif
					continue;
				}
			} else {
				if (slot == PCKBC_AUX_SLOT) {
#ifdef PCKBCDEBUG
					printf("lost kbd 0x%x\n", c);
#endif
					continue;
				}
			}
			return (c);
		}
	}
	return (-1);
}

/*
 * Get the current command byte.
 */
static int
pckbc_get8042cmd(t)
	struct pckbc_internal *t;
{
	bus_space_tag_t iot = t->t_iot;
	bus_space_handle_t ioh_d = t->t_ioh_d;
	bus_space_handle_t ioh_c = t->t_ioh_c;
	int data;

	if (!pckbc_send_cmd(iot, ioh_c, K_RDCMDBYTE))
		return (0);
	data = pckbc_poll_data1(iot, ioh_d, ioh_c, PCKBC_KBD_SLOT,
				t->t_haveaux);
	if (data == -1)
		return (0);
	t->t_cmdbyte = data;
	return (1);
}

/*
 * Pass command byte to keyboard controller (8042).
 */
static int
pckbc_put8042cmd(t)
	struct pckbc_internal *t;
{
	bus_space_tag_t iot = t->t_iot;
	bus_space_handle_t ioh_d = t->t_ioh_d;
	bus_space_handle_t ioh_c = t->t_ioh_c;

	if (!pckbc_send_cmd(iot, ioh_c, K_LDCMDBYTE))
		return (0);
	if (!pckbc_wait_output(iot, ioh_c))
		return (0);
	bus_space_write_1(iot, ioh_d, 0, t->t_cmdbyte);
	return (1);
}

static int
pckbc_send_devcmd(t, slot, val)
	struct pckbc_internal *t;
	pckbc_slot_t slot;
	u_char val;
{
	bus_space_tag_t iot = t->t_iot;
	bus_space_handle_t ioh_d = t->t_ioh_d;
	bus_space_handle_t ioh_c = t->t_ioh_c;

	if (slot == PCKBC_AUX_SLOT) {
		if (!pckbc_send_cmd(iot, ioh_c, KBC_AUXWRITE))
			return (0);
	}
	if (!pckbc_wait_output(iot, ioh_c))
		return (0);
	bus_space_write_1(iot, ioh_d, 0, val);
	return (1);
}

int
pckbc_is_console(iot, addr)
	bus_space_tag_t iot;
	bus_addr_t addr;
{
	if (pckbc_console && !pckbc_console_attached &&
	    pckbc_consdata.t_iot == iot &&
	    pckbc_consdata.t_addr == addr)
		return (1);
	return (0);
}

int
pckbc_submatch(parent, match, aux)
	struct device *parent;
	void *match;
	void *aux;
{
	struct cfdata *cf = match;
	struct pckbc_attach_args *pa = aux;

	if (cf->cf_loc[PCKBCCF_SLOT] != PCKBCCF_SLOT_DEFAULT &&
	    cf->cf_loc[PCKBCCF_SLOT] != pa->pa_slot)
		return (0);
	return ((*cf->cf_attach->ca_match)(parent, cf, aux));
}

int
pckbc_attach_slot(sc, slot)
	struct pckbc_softc *sc;
	pckbc_slot_t slot;
{
	struct pckbc_internal *t = sc->id;
	struct pckbc_attach_args pa;
	int found;

	pa.pa_tag = t;
	pa.pa_slot = slot;
	found = (config_found_sm((struct device *)sc, &pa,
				 pckbcprint, pckbc_submatch) != NULL);

	if (found && !t->t_slotdata[slot]) {
		t->t_slotdata[slot] = malloc(sizeof(struct pckbc_slotdata),
					     M_DEVBUF, M_NOWAIT);
		if (t->t_slotdata[slot] == NULL)
			return 0;
		pckbc_init_slotdata(t->t_slotdata[slot]);
	}
	return (found);
}

void
pckbc_attach(sc)
	struct pckbc_softc *sc;
{
	struct pckbc_internal *t;
	bus_space_tag_t iot;
	bus_space_handle_t ioh_d, ioh_c;
	int res;
	u_char cmdbits = 0;

	t = sc->id;
	iot = t->t_iot;
	ioh_d = t->t_ioh_d;
	ioh_c = t->t_ioh_c;

	if (pckbc_console == 0)
		timeout_set(&t->t_cleanup, pckbc_cleanup, t);

	/* flush */
	(void) pckbc_poll_data1(iot, ioh_d, ioh_c, PCKBC_KBD_SLOT, 0);

	/* set initial cmd byte */
	if (!pckbc_put8042cmd(t)) {
		printf("kbc: cmd word write error\n");
		return;
	}

/*
 * XXX Don't check the keyboard port. There are broken keyboard controllers
 * which don't pass the test but work normally otherwise.
 */
#if 0
	/*
	 * check kbd port ok
	 */
	if (!pckbc_send_cmd(iot, ioh_c, KBC_KBDTEST))
		return;
	res = pckbc_poll_data1(iot, ioh_d, ioh_c, PCKBC_KBD_SLOT, 0);

	/*
	 * Normally, we should get a "0" here.
	 * But there are keyboard controllers behaving differently.
	 */
	if (res == 0 || res == 0xfa || res == 0x01 || res == 0xab) {
#ifdef PCKBCDEBUG
		if (res != 0)
			printf("kbc: returned %x on kbd slot test\n", res);
#endif
		if (pckbc_attach_slot(sc, PCKBC_KBD_SLOT))
			cmdbits |= KC8_KENABLE;
	} else {
		printf("kbc: kbd port test: %x\n", res);
		return;
	}
#else
	if (pckbc_attach_slot(sc, PCKBC_KBD_SLOT))
		cmdbits |= KC8_KENABLE;
#endif /* 0 */

	/*
	 * Check aux port ok.
	 * Avoid KBC_AUXTEST because it hangs some older controllers
	 * (eg UMC880?).
	 */
	if (!pckbc_send_cmd(iot, ioh_c, KBC_AUXECHO)) {
		printf("kbc: aux echo error 1\n");
		goto nomouse;
	}
	if (!pckbc_wait_output(iot, ioh_c)) {
		printf("kbc: aux echo error 2\n");
		goto nomouse;
	}
	bus_space_write_1(iot, ioh_d, 0, 0x5a);	/* a random value */
	res = pckbc_poll_data1(iot, ioh_d, ioh_c, PCKBC_AUX_SLOT, 1);
	if (res != -1) {
		/*
		 * In most cases, the 0x5a gets echoed.
		 * Some old controllers (Gateway 2000 circa 1993)
		 * return 0xfe here.
		 * We are satisfied if there is anything in the
		 * aux output buffer.
		 */
#ifdef PCKBCDEBUG
		printf("kbc: aux echo: %x\n", res);
#endif
		t->t_haveaux = 1;
		if (pckbc_attach_slot(sc, PCKBC_AUX_SLOT))
			cmdbits |= KC8_MENABLE;
	}
#ifdef PCKBCDEBUG
	else
		printf("kbc: aux echo test failed\n");
#endif

nomouse:
	/* enable needed interrupts */
	t->t_cmdbyte |= cmdbits;
	if (!pckbc_put8042cmd(t))
		printf("kbc: cmd word write error\n");
}

int
pckbcprint(aux, pnp)
	void *aux;
	const char *pnp;
{
	struct pckbc_attach_args *pa = aux;

	if (!pnp)
		printf(" (%s slot)", pckbc_slot_names[pa->pa_slot]);
	return (QUIET);
}

void
pckbc_init_slotdata(q)
	struct pckbc_slotdata *q;
{
	int i;
	TAILQ_INIT(&q->cmdqueue);
	TAILQ_INIT(&q->freequeue);

	for (i = 0; i < NCMD; i++) {
		TAILQ_INSERT_TAIL(&q->freequeue, &(q->cmds[i]), next);
	}
	q->polling = 0;
}

void
pckbc_flush(self, slot)
	pckbc_tag_t self;
	pckbc_slot_t slot;
{
	struct pckbc_internal *t = self;

	(void) pckbc_poll_data1(t->t_iot, t->t_ioh_d, t->t_ioh_c,
				slot, t->t_haveaux);
}

int
pckbc_poll_data(self, slot)
	pckbc_tag_t self;
	pckbc_slot_t slot;
{
	struct pckbc_internal *t = self;
	struct pckbc_slotdata *q = t->t_slotdata[slot];
	int c;

	c = pckbc_poll_data1(t->t_iot, t->t_ioh_d, t->t_ioh_c,
			     slot, t->t_haveaux);
	if (c != -1 && q && CMD_IN_QUEUE(q)) {
		/* we jumped into a running command - try to
		 deliver the response */
		if (pckbc_cmdresponse(t, slot, c))
			return (-1);
	}
	return (c);
}

/*
 * switch scancode translation on / off
 * return nonzero on success
 */
int
pckbc_xt_translation(self, slot, on)
	pckbc_tag_t self;
	pckbc_slot_t slot;
	int on;
{
	struct pckbc_internal *t = self;
	int ison;

	if (slot != PCKBC_KBD_SLOT) {
		/* translation only for kbd slot */
		if (on)
			return (0);
		else
			return (1);
	}

	ison = t->t_cmdbyte & KC8_TRANS;
	if ((on && ison) || (!on && !ison))
		return (1);

	t->t_cmdbyte ^= KC8_TRANS;
	if (!pckbc_put8042cmd(t))
		return (0);

	/* read back to be sure */
	if (!pckbc_get8042cmd(t))
		return (0);

	ison = t->t_cmdbyte & KC8_TRANS;
	if ((on && ison) || (!on && !ison))
		return (1);
	return (0);
}

static struct pckbc_portcmd {
	u_char cmd_en, cmd_dis;
} pckbc_portcmd[2] = {
	{
		KBC_KBDENABLE, KBC_KBDDISABLE,
	}, {
		KBC_AUXENABLE, KBC_AUXDISABLE,
	}
};

void
pckbc_slot_enable(self, slot, on)
	pckbc_tag_t self;
	pckbc_slot_t slot;
	int on;
{
	struct pckbc_internal *t = (struct pckbc_internal *)self;
	struct pckbc_portcmd *cmd;

	cmd = &pckbc_portcmd[slot];

	if (!pckbc_send_cmd(t->t_iot, t->t_ioh_c,
			    on ? cmd->cmd_en : cmd->cmd_dis))
		printf("pckbc_slot_enable(%d) failed\n", on);
}

void
pckbc_set_poll(self, slot, on)
	pckbc_tag_t self;
	pckbc_slot_t slot;
	int on;
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
			pckbcintr(t->t_sc);
			splx(s);
		}
	}
}

/*
 * Pass command to device, poll for ACK and data.
 * to be called at spltty()
 */
static void
pckbc_poll_cmd1(t, slot, cmd)
	struct pckbc_internal *t;
	pckbc_slot_t slot;
	struct pckbc_devcmd *cmd;
{
	bus_space_tag_t iot = t->t_iot;
	bus_space_handle_t ioh_d = t->t_ioh_d;
	bus_space_handle_t ioh_c = t->t_ioh_c;
	int i, c = 0;

	while (cmd->cmdidx < cmd->cmdlen) {
		if (!pckbc_send_devcmd(t, slot, cmd->cmd[cmd->cmdidx])) {
			printf("pckbc_cmd: send error\n");
			cmd->status = EIO;
			return;
		}
		for (i = 10; i; i--) { /* 1s ??? */
			c = pckbc_poll_data1(iot, ioh_d, ioh_c, slot,
					     t->t_haveaux);
			if (c != -1)
				break;
		}

		if (c == KBC_DEVCMD_ACK) {
			cmd->cmdidx++;
			continue;
		}
		if (c == KBC_DEVCMD_RESEND) {
#ifdef PCKBCDEBUG
			printf("pckbc_cmd: RESEND\n");
#endif
			if (cmd->retries++ < 5)
				continue;
			else {
#ifdef PCKBCDEBUG
				printf("pckbc: cmd failed\n");
#endif
				cmd->status = EIO;
				return;
			}
		}
		if (c == -1) {
#ifdef PCKBCDEBUG
			printf("pckbc_cmd: timeout\n");
#endif
			cmd->status = EIO;
			return;
		}
#ifdef PCKBCDEBUG
		printf("pckbc_cmd: lost 0x%x\n", c);
#endif
	}

	while (cmd->responseidx < cmd->responselen) {
		if (cmd->flags & KBC_CMDFLAG_SLOW)
			i = 100; /* 10s ??? */
		else
			i = 10; /* 1s ??? */
		while (i--) {
			c = pckbc_poll_data1(iot, ioh_d, ioh_c, slot,
					     t->t_haveaux);
			if (c != -1)
				break;
		}
		if (c == -1) {
#ifdef PCKBCDEBUG
			printf("pckbc_cmd: no data\n");
#endif
			cmd->status = ETIMEDOUT;
			return;
		} else
			cmd->response[cmd->responseidx++] = c;
	}
}

/* for use in autoconfiguration */
int
pckbc_poll_cmd(self, slot, cmd, len, responselen, respbuf, slow)
	pckbc_tag_t self;
	pckbc_slot_t slot;
	u_char *cmd;
	int len, responselen;
	u_char *respbuf;
	int slow;
{
	struct pckbc_internal *t = self;
	struct pckbc_devcmd nc;

	if ((len > 4) || (responselen > 4))
		return (EINVAL);

	bzero(&nc, sizeof(nc));
	bcopy(cmd, nc.cmd, len);
	nc.cmdlen = len;
	nc.responselen = responselen;
	nc.flags = (slow ? KBC_CMDFLAG_SLOW : 0);

	pckbc_poll_cmd1(t, slot, &nc);

	if (nc.status == 0 && respbuf)
		bcopy(nc.response, respbuf, responselen);

	return (nc.status);
}

/*
 * Clean up a command queue, throw away everything.
 */
void
pckbc_cleanqueue(q)
	struct pckbc_slotdata *q;
{
	struct pckbc_devcmd *cmd;
#ifdef PCKBCDEBUG
	int i;
#endif

	while ((cmd = TAILQ_FIRST(&q->cmdqueue))) {
		TAILQ_REMOVE(&q->cmdqueue, cmd, next);
#ifdef PCKBCDEBUG
		printf("pckbc_cleanqueue: removing");
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
pckbc_cleanup(self)
	void *self;
{
	struct pckbc_internal *t = self;
	int s;

	printf("pckbc: command timeout\n");

	s = spltty();

	if (t->t_slotdata[PCKBC_KBD_SLOT])
		pckbc_cleanqueue(t->t_slotdata[PCKBC_KBD_SLOT]);
	if (t->t_slotdata[PCKBC_AUX_SLOT])
		pckbc_cleanqueue(t->t_slotdata[PCKBC_AUX_SLOT]);

	while (bus_space_read_1(t->t_iot, t->t_ioh_c, 0) & KBS_DIB) {
		KBD_DELAY;
		(void) bus_space_read_1(t->t_iot, t->t_ioh_d, 0);
	}

	/* reset KBC? */

	splx(s);
}

/*
 * Pass command to device during normal operation.
 * to be called at spltty()
 */
void
pckbc_start(t, slot)
	struct pckbc_internal *t;
	pckbc_slot_t slot;
{
	struct pckbc_slotdata *q = t->t_slotdata[slot];
	struct pckbc_devcmd *cmd = TAILQ_FIRST(&q->cmdqueue);

	if (q->polling) {
		do {
			pckbc_poll_cmd1(t, slot, cmd);
			if (cmd->status)
				printf("pckbc_start: command error\n");

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

	if (!pckbc_send_devcmd(t, slot, cmd->cmd[cmd->cmdidx])) {
		printf("pckbc_start: send error\n");
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
pckbc_cmdresponse(t, slot, data)
	struct pckbc_internal *t;
	pckbc_slot_t slot;
	u_char data;
{
	struct pckbc_slotdata *q = t->t_slotdata[slot];
	struct pckbc_devcmd *cmd = TAILQ_FIRST(&q->cmdqueue);
#ifdef DIAGNOSTIC
	if (!cmd)
		panic("pckbc_cmdresponse: no active command");
#endif
	if (cmd->cmdidx < cmd->cmdlen) {
		if (data != KBC_DEVCMD_ACK && data != KBC_DEVCMD_RESEND)
			return (0);

		if (data == KBC_DEVCMD_RESEND) {
			if (cmd->retries++ < 5) {
				/* try again last command */
				goto restart;
			} else {
				printf("pckbc: cmd failed\n");
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
	pckbc_start(t, slot);
	return (1);
}

/*
 * Put command into the device's command queue, return zero or errno.
 */
int
pckbc_enqueue_cmd(self, slot, cmd, len, responselen, sync, respbuf)
	pckbc_tag_t self;
	pckbc_slot_t slot;
	u_char *cmd;
	int len, responselen, sync;
	u_char *respbuf;
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
		pckbc_cleanqueue(q);
	}

	isactive = CMD_IN_QUEUE(q);
	TAILQ_INSERT_TAIL(&q->cmdqueue, nc, next);
	if (!isactive)
		pckbc_start(t, slot);

	if (q->polling)
		res = (sync ? nc->status : 0);
	else if (sync) {
		if ((res = tsleep(nc, 0, "kbccmd", 1*hz))) {
			TAILQ_REMOVE(&q->cmdqueue, nc, next);
			pckbc_cleanup(t);
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

void
pckbc_set_inputhandler(self, slot, func, arg, name)
	pckbc_tag_t self;
	pckbc_slot_t slot;
	pckbc_inputfcn func;
	void *arg;
	char *name;
{
	struct pckbc_internal *t = (struct pckbc_internal *)self;
	struct pckbc_softc *sc = t->t_sc;

	if (slot >= PCKBC_NSLOTS)
		panic("pckbc_set_inputhandler: bad slot %d", slot);

	(*sc->intr_establish)(sc, slot);

	sc->inputhandler[slot] = func;
	sc->inputarg[slot] = arg;
	sc->subname[slot] = name;
}

int
pckbcintr(vsc)
	void *vsc;
{
	struct pckbc_softc *sc = (struct pckbc_softc *)vsc;
	struct pckbc_internal *t = sc->id;
	u_char stat;
	pckbc_slot_t slot;
	struct pckbc_slotdata *q;
	int served = 0, data;

	for(;;) {
		stat = bus_space_read_1(t->t_iot, t->t_ioh_c, 0);
		if (!(stat & KBS_DIB))
			break;

		served = 1;

		slot = (t->t_haveaux && (stat & 0x20)) ?
		    PCKBC_AUX_SLOT : PCKBC_KBD_SLOT;
		q = t->t_slotdata[slot];

		if (!q) {
			/* XXX do something for live insertion? */
			printf("pckbcintr: no dev for slot %d\n", slot);
			KBD_DELAY;
			(void) bus_space_read_1(t->t_iot, t->t_ioh_d, 0);
			continue;
		}

		if (q->polling)
			break; /* pckbc_poll_data() will get it */

		KBD_DELAY;
		data = bus_space_read_1(t->t_iot, t->t_ioh_d, 0);

		if (CMD_IN_QUEUE(q) && pckbc_cmdresponse(t, slot, data))
			continue;

		if (sc->inputhandler[slot])
			(*sc->inputhandler[slot])(sc->inputarg[slot], data);
#ifdef PCKBCDEBUG
		else
			printf("pckbcintr: slot %d lost %d\n", slot, data);
#endif
	}

	return (served);
}

int
pckbc_cnattach(iot, addr, cmd_offset, slot)
	bus_space_tag_t iot;
	bus_addr_t addr;
	bus_size_t cmd_offset;
	pckbc_slot_t slot;
{
	bus_space_handle_t ioh_d, ioh_c;
	int res = 0;

	if (bus_space_map(iot, addr + KBDATAP, 1, 0, &ioh_d))
                return (ENXIO);
	if (bus_space_map(iot, addr + cmd_offset, 1, 0, &ioh_c)) {
		bus_space_unmap(iot, ioh_d, 1);
                return (ENXIO);
	}

	pckbc_consdata.t_iot = iot;
	pckbc_consdata.t_ioh_d = ioh_d;
	pckbc_consdata.t_ioh_c = ioh_c;
	pckbc_consdata.t_addr = addr;
	timeout_set(&pckbc_consdata.t_cleanup, pckbc_cleanup, &pckbc_consdata);

	/* flush */
	(void) pckbc_poll_data1(iot, ioh_d, ioh_c, PCKBC_KBD_SLOT, 0);

	/* selftest? */

	/* init cmd byte, enable ports */
	pckbc_consdata.t_cmdbyte = KC8_CPU;
	if (!pckbc_put8042cmd(&pckbc_consdata)) {
		printf("kbc: cmd word write error\n");
		res = EIO;
	}

	if (!res) {
#if (NPCKBD > 0)
		res = pckbd_cnattach(&pckbc_consdata, slot);
#else
		res = ENXIO;
#endif /* NPCKBD > 0 */
	}

	if (res) {
		bus_space_unmap(iot, pckbc_consdata.t_ioh_d, 1);
		bus_space_unmap(iot, pckbc_consdata.t_ioh_c, 1);
	} else {
		pckbc_consdata.t_slotdata[slot] = &pckbc_cons_slotdata;
		pckbc_init_slotdata(&pckbc_cons_slotdata);
		pckbc_console = 1;
	}

	return (res);
}

struct cfdriver pckbc_cd = {
	NULL, "pckbc", DV_DULL
};
