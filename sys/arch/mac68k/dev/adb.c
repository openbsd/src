/*	$OpenBSD: adb.c,v 1.14 2005/11/21 18:16:37 millert Exp $	*/
/*	$NetBSD: adb.c,v 1.13 1996/12/16 16:17:02 scottr Exp $	*/

/*-
 * Copyright (C) 1994	Bradley A. Grantham
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
e*    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Bradley A. Grantham.
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
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/device.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/selinfo.h>
#include <sys/poll.h>
#include <sys/proc.h>
#include <sys/signalvar.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/timeout.h>

#include <machine/autoconf.h>
#include <machine/keyboard.h>

#include <arch/mac68k/mac68k/macrom.h>
#include <arch/mac68k/dev/adbvar.h>
#include <arch/mac68k/dev/itevar.h>

/*
 * Function declarations.
 */
static int	adbmatch(struct device *, void *, void *);
static void	adbattach(struct device *, struct device *, void *);

/*
 * Global variables.
 */
int     adb_polling = 0;	/* Are we polling?  (Debugger mode) */

/*
 * Local variables.
 */

/* External keyboard translation matrix */
extern unsigned char keyboard[128][3];

/* Event queue definitions */
#if !defined(ADB_MAX_EVENTS)
#define ADB_MAX_EVENTS 200	/* Maximum events to be kept in queue */
				/* maybe should be higher for slower macs? */
#endif				/* !defined(ADB_MAX_EVENTS) */
static adb_event_t adb_evq[ADB_MAX_EVENTS];	/* ADB event queue */
static int adb_evq_tail = 0;	/* event queue tail */
static int adb_evq_len = 0;	/* event queue length */

/* ADB device state information */
static int adb_isopen = 0;	/* Are we queuing events for adb_read? */
static struct selinfo adb_selinfo;	/* select() info */
static struct proc *adb_ioproc = NULL;	/* process to wakeup */

/* Key repeat parameters */
static int adb_rptdelay = 20;	/* ticks before auto-repeat */
static int adb_rptinterval = 6;	/* ticks between auto-repeat */
static int adb_repeating = -1;	/* key that is auto-repeating */
static adb_event_t adb_rptevent;/* event to auto-repeat */

/* Driver definition.  -- This should probably be a bus...  */
struct cfattach adb_ca = {
	sizeof(struct device), adbmatch, adbattach
};

struct cfdriver adb_cd = {
	NULL, "adb", DV_DULL
};

struct timeout repeat_timeout;

static int
adbmatch(parent, vcf, aux)
	struct device *parent;
	void *vcf;
	void *aux;
{
	return 1;
}

static void
adbattach(parent, dev, aux)
	struct device *parent, *dev;
	void   *aux;
{
	printf(" (ADB event device)\n");
}

void 
adb_enqevent(event)
    adb_event_t *event;
{
	int     s;

	if (adb_evq_tail < 0 || adb_evq_tail >= ADB_MAX_EVENTS)
		panic("adb: event queue tail is out of bounds");

	if (adb_evq_len < 0 || adb_evq_len > ADB_MAX_EVENTS)
		panic("adb: event queue len is out of bounds");

	s = splhigh();

	if (adb_evq_len == ADB_MAX_EVENTS) {
		splx(s);
		return;		/* Oh, well... */
	}
	adb_evq[(adb_evq_len + adb_evq_tail) % ADB_MAX_EVENTS] =
	    *event;
	adb_evq_len++;

	selwakeup(&adb_selinfo);
	if (adb_ioproc)
		psignal(adb_ioproc, SIGIO);

	splx(s);
}

void 
adb_handoff(event)
    adb_event_t *event;
{
	if (adb_isopen && !adb_polling) {
		adb_enqevent(event);
	} else {
		if (event->def_addr == 2)
			ite_intr(event);
	}
}


void 
adb_autorepeat(keyp)
    void *keyp;
{
	int     key = (int) keyp;

	adb_rptevent.bytes[0] |= 0x80;
	microtime(&adb_rptevent.timestamp);
	adb_handoff(&adb_rptevent);	/* do key up */

	adb_rptevent.bytes[0] &= 0x7f;
	microtime(&adb_rptevent.timestamp);
	adb_handoff(&adb_rptevent);	/* do key down */

	if (adb_repeating == key) {
		timeout_set(&repeat_timeout, adb_autorepeat, keyp);
		timeout_add(&repeat_timeout, adb_rptinterval);
	}
}


void 
adb_dokeyupdown(event)
    adb_event_t *event;
{
	int     adb_key;

	if (event->def_addr == 2) {
		adb_key = event->u.k.key & 0x7f;
		if (!(event->u.k.key & 0x80) &&
		    keyboard[event->u.k.key & 0x7f][0] != 0) {
			/* ignore shift & control */
			if (adb_repeating != -1) {
				timeout_del(&repeat_timeout);
			}
			adb_rptevent = *event;
			adb_repeating = adb_key;
			timeout_set(&repeat_timeout, adb_autorepeat,
			    (caddr_t)adb_key);
			timeout_add(&repeat_timeout, adb_rptdelay);
		} else {
			if (adb_repeating != -1) {
				adb_repeating = -1;
				timeout_del(&repeat_timeout);
			}
			adb_rptevent = *event;
		}
	}
	adb_handoff(event);
}

static int adb_ms_buttons = 0;

void 
adb_keymaybemouse(event)
    adb_event_t *event;
{
	static int optionkey_down = 0;
	adb_event_t new_event;

	if (event->u.k.key == ADBK_KEYDOWN(ADBK_OPTION)) {
		optionkey_down = 1;
	} else if (event->u.k.key == ADBK_KEYUP(ADBK_OPTION)) {
		/* key up */
		optionkey_down = 0;
		if (adb_ms_buttons & 0xfe) {
			adb_ms_buttons &= 1;
			new_event.def_addr = ADBADDR_MS;
			new_event.u.m.buttons = adb_ms_buttons;
			new_event.u.m.dx = new_event.u.m.dy = 0;
			microtime(&new_event.timestamp);
			adb_dokeyupdown(&new_event);
		}
	} else if (optionkey_down) {
		if (event->u.k.key == ADBK_KEYDOWN(ADBK_LEFT)) {
			adb_ms_buttons |= 2;	/* middle down */
			new_event.def_addr = ADBADDR_MS;
			new_event.u.m.buttons = adb_ms_buttons;
			new_event.u.m.dx = new_event.u.m.dy = 0;
			microtime(&new_event.timestamp);
			adb_dokeyupdown(&new_event);
		} else if (event->u.k.key == ADBK_KEYUP(ADBK_LEFT)) {
			adb_ms_buttons &= ~2;	/* middle up */
			new_event.def_addr = ADBADDR_MS;
			new_event.u.m.buttons = adb_ms_buttons;
			new_event.u.m.dx = new_event.u.m.dy = 0;
			microtime(&new_event.timestamp);
			adb_dokeyupdown(&new_event);
		} else if (event->u.k.key == ADBK_KEYDOWN(ADBK_RIGHT)) {
			adb_ms_buttons |= 4;	/* right down */
			new_event.def_addr = ADBADDR_MS;
			new_event.u.m.buttons = adb_ms_buttons;
			new_event.u.m.dx = new_event.u.m.dy = 0;
			microtime(&new_event.timestamp);
			adb_dokeyupdown(&new_event);
		} else if (event->u.k.key == ADBK_KEYUP(ADBK_RIGHT)) {
			adb_ms_buttons &= ~4;	/* right up */
			new_event.def_addr = ADBADDR_MS;
			new_event.u.m.buttons = adb_ms_buttons;
			new_event.u.m.dx = new_event.u.m.dy = 0;
			microtime(&new_event.timestamp);
			adb_dokeyupdown(&new_event);
		} else if (ADBK_MODIFIER(event->u.k.key)) {
		/* ctrl, shift, cmd */
			adb_dokeyupdown(event);
		} else if (!(event->u.k.key & 0x80)) {
		/* key down */
			new_event = *event;

			/* send option-down */
			new_event.u.k.key = ADBK_KEYDOWN(ADBK_OPTION);
			new_event.bytes[0] = new_event.u.k.key;
			microtime(&new_event.timestamp);
			adb_dokeyupdown(&new_event);

			/* send key-down */
			new_event.u.k.key = event->bytes[0];
			new_event.bytes[0] = new_event.u.k.key;
			microtime(&new_event.timestamp);
			adb_dokeyupdown(&new_event);

			/* send key-up */
			new_event.u.k.key =
				ADBK_KEYUP(ADBK_KEYVAL(event->bytes[0]));
			microtime(&new_event.timestamp);
			new_event.bytes[0] = new_event.u.k.key;
			adb_dokeyupdown(&new_event);

			/* send option-up */
			new_event.u.k.key = ADBK_KEYUP(ADBK_OPTION);
			new_event.bytes[0] = new_event.u.k.key;
			microtime(&new_event.timestamp);
			adb_dokeyupdown(&new_event);
		} else {
			/* option-keyup -- do nothing. */
		}
	} else {
		adb_dokeyupdown(event);
	}
}


void 
adb_processevent(event)
    adb_event_t *event;
{
	adb_event_t new_event;
	int i, button_bit, max_byte, mask, buttons;

	new_event = *event;
	buttons = 0;

	switch (event->def_addr) {
	case ADBADDR_KBD:
		new_event.u.k.key = event->bytes[0];
		new_event.bytes[1] = 0xff;
		adb_keymaybemouse(&new_event);
		if (event->bytes[1] != 0xff) {
			new_event.u.k.key = event->bytes[1];
			new_event.bytes[0] = event->bytes[1];
			new_event.bytes[1] = 0xff;
			adb_keymaybemouse(&new_event);
		}
		break;
	case ADBADDR_MS:
		/*
		 * This should handle both plain ol' Apple mice and mice
		 * that claim to support the Extended Apple Mouse Protocol.
		 */
		max_byte = event->byte_count;
		button_bit = 1;
		switch (event->hand_id) {
		case ADBMS_USPEED:
			/* MicroSpeed mouse */
			if (max_byte == 4)
				buttons = (~event->bytes[2]) & 0xff;
			else
				buttons = (event->bytes[0] & 0x80) ? 0 : 1;
			break;
		default:
			/* Classic Mouse Protocol (up to 2 buttons) */
			for (i = 0; i < 2; i++, button_bit <<= 1)
				/* 0 when button down */
				if (!(event->bytes[i] & 0x80))
					buttons |= button_bit;
				else
					buttons &= ~button_bit;
			/* Extended Protocol (up to 6 more buttons) */
			for (mask = 0x80; i < max_byte;
			     i += (mask == 0x80), button_bit <<= 1) {
				/* 0 when button down */
				if (!(event->bytes[i] & mask))
					buttons |= button_bit;
				else
					buttons &= ~button_bit;
				mask = ((mask >> 4) & 0xf)
					| ((mask & 0xf) << 4);
			}
			break;
		}
		new_event.u.m.buttons = adb_ms_buttons | buttons;
		new_event.u.m.dx = ((signed int) (event->bytes[1] & 0x3f)) -
					((event->bytes[1] & 0x40) ? 64 : 0);
		new_event.u.m.dy = ((signed int) (event->bytes[0] & 0x3f)) -
					((event->bytes[0] & 0x40) ? 64 : 0);
		adb_dokeyupdown(&new_event);
		break;
	default:		/* God only knows. */
		adb_dokeyupdown(event);
	}
}


int 
adbopen(dev, flag, mode, p)
    dev_t dev;
    int flag, mode;
    struct proc *p;
{
	register int unit;
	int error = 0;
	int s;

	unit = minor(dev);
	if (unit != 0)
		return (ENXIO);

	s = splhigh();
	if (adb_isopen) {
		splx(s);
		return (EBUSY);
	}
	splx(s);
	adb_evq_tail = 0;
	adb_evq_len = 0;
	adb_isopen = 1;
	adb_ioproc = p;

	return (error);
}


int 
adbclose(dev, flag, mode, p)
    dev_t dev;
    int flag, mode;
    struct proc *p;
{
	adb_isopen = 0;
	adb_ioproc = NULL;
	return (0);
}


int 
adbread(dev, uio, flag)
    dev_t dev;
    struct uio *uio;
    int flag;
{
	int s, error;
	int willfit;
	int total;
	int firstmove;
	int moremove;

	if (uio->uio_resid < sizeof(adb_event_t))
		return (EMSGSIZE);	/* close enough. */

	s = splhigh();
	if (adb_evq_len == 0) {
		splx(s);
		return (0);
	}
	willfit = howmany(uio->uio_resid, sizeof(adb_event_t));
	total = (adb_evq_len < willfit) ? adb_evq_len : willfit;

	firstmove = (adb_evq_tail + total > ADB_MAX_EVENTS)
	    ? (ADB_MAX_EVENTS - adb_evq_tail) : total;

	error = uiomove((caddr_t) & adb_evq[adb_evq_tail],
	    firstmove * sizeof(adb_event_t), uio);
	if (error) {
		splx(s);
		return (error);
	}
	moremove = total - firstmove;

	if (moremove > 0) {
		error = uiomove((caddr_t) & adb_evq[0],
		    moremove * sizeof(adb_event_t), uio);
		if (error) {
			splx(s);
			return (error);
		}
	}
	adb_evq_tail = (adb_evq_tail + total) % ADB_MAX_EVENTS;
	adb_evq_len -= total;
	splx(s);
	return (0);
}


int 
adbwrite(dev, uio, flag)
    dev_t dev;
    struct uio *uio;
    int flag;
{
	return 0;
}


int 
adbioctl(dev, cmd, data, flag, p)
    dev_t dev;
    int cmd;
    caddr_t data;
    int flag;
    struct proc *p;
{
	switch (cmd) {
	case ADBIOC_DEVSINFO: {
		adb_devinfo_t *di;
		ADBDataBlock adbdata;
		int totaldevs;
		int adbaddr;
		int i;

		di = (void *) data;

		/* Initialize to no devices */
		for (i = 0; i < 16; i++)
			di->dev[i].addr = -1;

		totaldevs = CountADBs();
		for (i = 1; i <= totaldevs; i++) {
			adbaddr = GetIndADB(&adbdata, i);
			di->dev[adbaddr].addr = adbaddr;
			di->dev[adbaddr].default_addr = adbdata.origADBAddr;
			di->dev[adbaddr].handler_id = adbdata.devType;
			}

		/* Must call ADB Manager to get devices now */
		break;
	}

	case ADBIOC_GETREPEAT:{
		adb_rptinfo_t *ri;

		ri = (void *) data;
		ri->delay_ticks = adb_rptdelay;
		ri->interval_ticks = adb_rptinterval;
		break;
	}

	case ADBIOC_SETREPEAT:{
		adb_rptinfo_t *ri;

		ri = (void *) data;
		adb_rptdelay = ri->delay_ticks;
		adb_rptinterval = ri->interval_ticks;
		break;
	}

	case ADBIOC_RESET:
		adb_init();
		break;

	case ADBIOC_LISTENCMD:{
		adb_listencmd_t *lc;

		lc = (void *) data;
	}

	default:
		return (EINVAL);
	}
	return (0);
}


int 
adbpoll(dev, events, p)
    dev_t dev;
    int events;
    struct proc *p;
{
	int revents = 0;

	if (events & (POLLIN | POLLRDNORM)) {
		/* succeed if there is something to read */
		if (adb_evq_len > 0)
			revents |= events & (POLLIN | POLLRDNORM);
		else
			selrecord(p, &adb_selinfo);
	}
	if (events & (POLLOUT | POLLWRNORM)) {
		/* always fails => never blocks */
		revents |= events & (POLLOUT | POLLWRNORM);
	}

	return (revents);
}
