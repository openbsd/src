/*	$OpenBSD: aed.c,v 1.6 2002/09/15 09:01:58 deraadt Exp $	*/
/*	$NetBSD: aed.c,v 1.5 2000/03/23 06:40:33 thorpej Exp $	*/

/*
 * Copyright (C) 1994	Bradley A. Grantham
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
#include <sys/poll.h>
#include <sys/select.h>
#include <sys/proc.h>
#include <sys/signalvar.h>
#include <sys/systm.h>

#include <machine/autoconf.h>
#include <machine/conf.h>
#include <machine/cpu.h>

#include <macppc/dev/keyboard.h>
#include <macppc/dev/adbvar.h>
#include <macppc/dev/aedvar.h>
#include <macppc/dev/akbdvar.h>

#define spladb splhigh

/*
 * Function declarations.
 */
int	aedmatch(struct device *, void *, void *);
void	aedattach(struct device *, struct device *, void *);
void	aed_emulate_mouse(adb_event_t *event);
void	aed_kbdrpt(void *kstate);
void	aed_dokeyupdown(adb_event_t *event);
void	aed_handoff(adb_event_t *event);
void	aed_enqevent(adb_event_t *event);

/*
 * Local variables.
 */
struct aed_softc *aed_sc = NULL;
int aed_options = 0; /* | AED_MSEMUL; */

/* Driver definition */
struct cfdriver aed_cd = {
	NULL, "aed", DV_DULL
};
/* Driver definition */
struct cfattach aed_ca = {
	sizeof(struct aed_softc), aedmatch, aedattach
};

int
aedmatch(parent, cf, aux)
	struct device *parent;
	void *cf;
	void *aux;
{
	struct adb_attach_args *aa_args = (struct adb_attach_args *)aux;
	static int aed_matched = 0;

	/* Allow only one instance. */
        if ((aa_args->origaddr == 0) && (!aed_matched)) {
		aed_matched = 1;
                return (1);
        } else
                return (0);
}

void
aedattach(parent, self, aux)
	struct device *parent, *self;
	void   *aux;
{
	struct adb_attach_args *aa_args = (struct adb_attach_args *)aux;
	struct aed_softc *sc = (struct aed_softc *)self;

	aed_sc = sc;

	timeout_set(&sc->sc_repeat_ch, aed_kbdrpt, aed_sc);

	sc->origaddr = aa_args->origaddr;
	sc->adbaddr = aa_args->adbaddr;
	sc->handler_id = aa_args->handler_id;

	sc->sc_evq_tail = 0;
	sc->sc_evq_len = 0;

	sc->sc_rptdelay = 20;
	sc->sc_rptinterval = 6;
	sc->sc_repeating = -1;          /* not repeating */

	/* Pull in the options flags. */ 
	sc->sc_options = (sc->sc_dev.dv_cfdata->cf_flags | aed_options);

	sc->sc_ioproc = NULL;
	
	sc->sc_buttons = 0;

	sc->sc_open = 0;

	printf("ADB Event device\n");
}

/*
 * Given a keyboard ADB event, record the keycode and call the key 
 * repeat handler, optionally passing the event through the mouse
 * button emulation handler first.  Pass mouse events directly to
 * the handoff function.
 */
void
aed_input(event)
        adb_event_t *event;
{
        adb_event_t new_event = *event;

	switch (event->def_addr) {
	case ADBADDR_KBD:
		if (aed_sc->sc_options & AED_MSEMUL)
			aed_emulate_mouse(&new_event);
		else
			aed_dokeyupdown(&new_event);
		break;
	case ADBADDR_MS:
		new_event.u.m.buttons |= aed_sc->sc_buttons;
		aed_handoff(&new_event);
		break;
	default:                /* God only knows. */
#ifdef DIAGNOSTIC
		panic("aed: received event from unsupported device!");
#endif
		break;
	}

}

/*
 * Handles mouse button emulation via the keyboard.  If the emulation
 * modifier key is down, left and right arrows will generate 2nd and
 * 3rd mouse button events while the 1, 2, and 3 keys will generate
 * the corresponding mouse button event.
 */
void 
aed_emulate_mouse(event)
	adb_event_t *event;
{
	static int emulmodkey_down = 0;
	adb_event_t new_event;

	if (event->u.k.key == ADBK_KEYDOWN(ADBK_OPTION)) {
		emulmodkey_down = 1;
	} else if (event->u.k.key == ADBK_KEYUP(ADBK_OPTION)) {
		/* key up */
		emulmodkey_down = 0;
		if (aed_sc->sc_buttons & 0xfe) {
			aed_sc->sc_buttons &= 1;
			new_event.def_addr = ADBADDR_MS;
			new_event.u.m.buttons = aed_sc->sc_buttons;
			new_event.u.m.dx = new_event.u.m.dy = 0;
			microtime(&new_event.timestamp);
			aed_handoff(&new_event);
		}
	} else if (emulmodkey_down) {
		switch(event->u.k.key) {
#ifdef ALTXBUTTONS
		case ADBK_KEYDOWN(ADBK_1):
			aed_sc->sc_buttons |= 1;	/* left down */
			new_event.def_addr = ADBADDR_MS;
			new_event.u.m.buttons = aed_sc->sc_buttons;
			new_event.u.m.dx = new_event.u.m.dy = 0;
			microtime(&new_event.timestamp);
			aed_handoff(&new_event);
			break;
		case ADBK_KEYUP(ADBK_1):
			aed_sc->sc_buttons &= ~1;	/* left up */
			new_event.def_addr = ADBADDR_MS;
			new_event.u.m.buttons = aed_sc->sc_buttons;
			new_event.u.m.dx = new_event.u.m.dy = 0;
			microtime(&new_event.timestamp);
			aed_handoff(&new_event);
			break;
#endif
		case ADBK_KEYDOWN(ADBK_LEFT):
#ifdef ALTXBUTTONS
		case ADBK_KEYDOWN(ADBK_2):
#endif
			aed_sc->sc_buttons |= 2;	/* middle down */
			new_event.def_addr = ADBADDR_MS;
			new_event.u.m.buttons = aed_sc->sc_buttons;
			new_event.u.m.dx = new_event.u.m.dy = 0;
			microtime(&new_event.timestamp);
			aed_handoff(&new_event);
			break;
		case ADBK_KEYUP(ADBK_LEFT):
#ifdef ALTXBUTTONS
		case ADBK_KEYUP(ADBK_2):
#endif
			aed_sc->sc_buttons &= ~2;	/* middle up */
			new_event.def_addr = ADBADDR_MS;
			new_event.u.m.buttons = aed_sc->sc_buttons;
			new_event.u.m.dx = new_event.u.m.dy = 0;
			microtime(&new_event.timestamp);
			aed_handoff(&new_event);
			break;
		case ADBK_KEYDOWN(ADBK_RIGHT):
#ifdef ALTXBUTTONS
		case ADBK_KEYDOWN(ADBK_3):
#endif
			aed_sc->sc_buttons |= 4;	/* right down */
			new_event.def_addr = ADBADDR_MS;
			new_event.u.m.buttons = aed_sc->sc_buttons;
			new_event.u.m.dx = new_event.u.m.dy = 0;
			microtime(&new_event.timestamp);
			aed_handoff(&new_event);
			break;
		case ADBK_KEYUP(ADBK_RIGHT):
#ifdef ALTXBUTTONS
		case ADBK_KEYUP(ADBK_3):
#endif
			aed_sc->sc_buttons &= ~4;	/* right up */
			new_event.def_addr = ADBADDR_MS;
			new_event.u.m.buttons = aed_sc->sc_buttons;
			new_event.u.m.dx = new_event.u.m.dy = 0;
			microtime(&new_event.timestamp);
			aed_handoff(&new_event);
			break;
		case ADBK_KEYUP(ADBK_SHIFT):
		case ADBK_KEYDOWN(ADBK_SHIFT):
		case ADBK_KEYUP(ADBK_CONTROL):
		case ADBK_KEYDOWN(ADBK_CONTROL):
		case ADBK_KEYUP(ADBK_FLOWER):
		case ADBK_KEYDOWN(ADBK_FLOWER):
			/* ctrl, shift, cmd */
			aed_dokeyupdown(event);
			break;
		default:
			if (event->u.k.key & 0x80)
				/* ignore keyup */
				break;

			/* key down */
			new_event = *event;

			/* send option-down */
			new_event.u.k.key = ADBK_KEYDOWN(ADBK_OPTION);
			new_event.bytes[0] = new_event.u.k.key;
			microtime(&new_event.timestamp);
			aed_dokeyupdown(&new_event);

			/* send key-down */
			new_event.u.k.key = event->bytes[0];
			new_event.bytes[0] = new_event.u.k.key;
			microtime(&new_event.timestamp);
			aed_dokeyupdown(&new_event);

			/* send key-up */
			new_event.u.k.key =
				ADBK_KEYUP(ADBK_KEYVAL(event->bytes[0]));
			microtime(&new_event.timestamp);
			new_event.bytes[0] = new_event.u.k.key;
			aed_dokeyupdown(&new_event);

			/* send option-up */
			new_event.u.k.key = ADBK_KEYUP(ADBK_OPTION);
			new_event.bytes[0] = new_event.u.k.key;
			microtime(&new_event.timestamp);
			aed_dokeyupdown(&new_event);
			break;
		}
	} else {
		aed_dokeyupdown(event);
	}
}

/*
 * Keyboard autorepeat timeout function.  Sends key up/down events
 * for the repeating key and schedules the next call at sc_rptinterval
 * ticks in the future.
 */
void 
aed_kbdrpt(kstate)
	void *kstate;
{
	struct aed_softc *aed_sc = (struct aed_softc *)kstate;

	aed_sc->sc_rptevent.bytes[0] |= 0x80;
	microtime(&aed_sc->sc_rptevent.timestamp);
	aed_handoff(&aed_sc->sc_rptevent);	/* do key up */

	aed_sc->sc_rptevent.bytes[0] &= 0x7f;
	microtime(&aed_sc->sc_rptevent.timestamp);
	aed_handoff(&aed_sc->sc_rptevent);	/* do key down */

	if (aed_sc->sc_repeating == aed_sc->sc_rptevent.u.k.key) {
		timeout_add(&aed_sc->sc_repeat_ch, aed_sc->sc_rptinterval);
	}
}


/*
 * Cancels the currently repeating key event if there is one, schedules
 * a new repeating key event if needed, and hands the event off to the
 * appropriate subsystem.
 */
void 
aed_dokeyupdown(event)
	adb_event_t *event;
{
	int     kbd_key;

	kbd_key = ADBK_KEYVAL(event->u.k.key);
	if (ADBK_PRESS(event->u.k.key) && keyboard[kbd_key][0] != 0) {
		/* ignore shift & control */
		if (aed_sc->sc_repeating != -1) {
			timeout_del(&aed_sc->sc_repeat_ch);
		}
		aed_sc->sc_rptevent = *event;
		aed_sc->sc_repeating = kbd_key;
		timeout_add(&aed_sc->sc_repeat_ch, aed_sc->sc_rptdelay);
	} else {
		if (aed_sc->sc_repeating != -1) {
			aed_sc->sc_repeating = -1;
			timeout_del(&aed_sc->sc_repeat_ch);
		}
		aed_sc->sc_rptevent = *event;
	}
	aed_handoff(event);
}

/*
 * Place the event in the event queue if a requesting device is open
 * and we are not polling.
 */
void
aed_handoff(event)
	adb_event_t *event;
{
	if (aed_sc->sc_open && !adb_polling)
		aed_enqevent(event);
}

/*
 * Place the event in the event queue and wakeup any waiting processes.
 */
void 
aed_enqevent(event)
	adb_event_t *event;
{
	int     s;

	s = spladb();

#ifdef DIAGNOSTIC
	if (aed_sc->sc_evq_tail < 0 || aed_sc->sc_evq_tail >= AED_MAX_EVENTS)
		panic("adb: event queue tail is out of bounds");

	if (aed_sc->sc_evq_len < 0 || aed_sc->sc_evq_len > AED_MAX_EVENTS)
		panic("adb: event queue len is out of bounds");
#endif

	if (aed_sc->sc_evq_len == AED_MAX_EVENTS) {
		splx(s);
		return;		/* Oh, well... */
	}
	aed_sc->sc_evq[(aed_sc->sc_evq_len + aed_sc->sc_evq_tail) %
	    AED_MAX_EVENTS] = *event;
	aed_sc->sc_evq_len++;

	selwakeup(&aed_sc->sc_selinfo);
	if (aed_sc->sc_ioproc)
		psignal(aed_sc->sc_ioproc, SIGIO);

	splx(s);
}

int 
aedopen(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	int unit;
	int error = 0;
	int s;

	unit = minor(dev);

	if (unit != 0)
		return (ENXIO);

	s = spladb();
	if (aed_sc->sc_open) {
		splx(s);
		return (EBUSY);
	}
	aed_sc->sc_evq_tail = 0;
	aed_sc->sc_evq_len = 0;
	aed_sc->sc_open = 1;
	aed_sc->sc_ioproc = p;
	splx(s);

	return (error);
}


int 
aedclose(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	int s = spladb();

	aed_sc->sc_open = 0;
	aed_sc->sc_ioproc = NULL;
	splx(s);

	return (0);
}


int 
aedread(dev, uio, flag)
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

	s = spladb();
	if (aed_sc->sc_evq_len == 0) {
		splx(s);
		return (0);
	}
	willfit = howmany(uio->uio_resid, sizeof(adb_event_t));
	total = (aed_sc->sc_evq_len < willfit) ? aed_sc->sc_evq_len : willfit;

	firstmove = (aed_sc->sc_evq_tail + total > AED_MAX_EVENTS)
	    ? (AED_MAX_EVENTS - aed_sc->sc_evq_tail) : total;

	error = uiomove((caddr_t) & aed_sc->sc_evq[aed_sc->sc_evq_tail],
	    firstmove * sizeof(adb_event_t), uio);
	if (error) {
		splx(s);
		return (error);
	}
	moremove = total - firstmove;

	if (moremove > 0) {
		error = uiomove((caddr_t) & aed_sc->sc_evq[0],
		    moremove * sizeof(adb_event_t), uio);
		if (error) {
			splx(s);
			return (error);
		}
	}
	aed_sc->sc_evq_tail = (aed_sc->sc_evq_tail + total) % AED_MAX_EVENTS;
	aed_sc->sc_evq_len -= total;
	splx(s);
	return (0);
}


int 
aedwrite(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	return 0;
}


int 
aedioctl(dev, cmd, data, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	switch (cmd) {
	case ADBIOCDEVSINFO:
	{
		adb_devinfo_t *di;
		ADBDataBlock adbdata;
		int totaldevs;
		int adbaddr;
		int i;

		di = (void *)data;

		/* Initialize to no devices */
		for (i = 0; i < 16; i++)
			di->dev[i].addr = -1;

		totaldevs = CountADBs();
		for (i = 1; i <= totaldevs; i++) {
			adbaddr = GetIndADB(&adbdata, i);
			di->dev[adbaddr].addr = adbaddr;
			di->dev[adbaddr].default_addr = (int)(adbdata.origADBAddr);
			di->dev[adbaddr].handler_id = (int)(adbdata.devType);
		}

		/* Must call ADB Manager to get devices now */
	}
		break;

	case ADBIOCGETREPEAT:
	{
		adb_rptinfo_t *ri;

		ri = (void *)data;
		ri->delay_ticks = aed_sc->sc_rptdelay;
		ri->interval_ticks = aed_sc->sc_rptinterval;
	}
		break;

	case ADBIOCSETREPEAT:
	{
		adb_rptinfo_t *ri;

		ri = (void *) data;
		aed_sc->sc_rptdelay = ri->delay_ticks;
		aed_sc->sc_rptinterval = ri->interval_ticks;
	}
		break;

	case ADBIOCRESET:
		/* Do nothing for now */
		break;

	case ADBIOCLISTENCMD:
	{
		adb_listencmd_t *lc;

		lc = (void *)data;
	}
		/* FALLTHROUGH */

	default:
		return (EINVAL);
	}

	return (0);
}
