/*	$OpenBSD: kbd.c,v 1.4 1996/05/29 10:15:33 niklas Exp $	*/
/*	$NetBSD: kbd.c,v 1.22 1996/05/16 20:18:01 is Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1990 The Regents of the University of California.
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
 *	kbd.c
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/proc.h>
#include <sys/file.h>
#include <sys/kernel.h>
#include <sys/syslog.h>
#include <sys/signalvar.h>
#include <dev/cons.h>
#include <machine/cpu.h>
#include <amiga/amiga/device.h>
#include <amiga/amiga/custom.h>
#ifdef DRACO
#include <amiga/amiga/drcustom.h>
#endif
#include <amiga/amiga/cia.h>
#include <amiga/dev/itevar.h>
#include <amiga/dev/kbdreg.h>
#include <amiga/dev/kbdmap.h>
#include <amiga/dev/event_var.h>
#include <amiga/dev/vuid_event.h>
#include "kbd.h"

#include <sys/conf.h>
#include <machine/conf.h>

struct kbd_softc {
	int k_event_mode;	/* if true, collect events, else pass to ite */
	struct evvar k_events;	/* event queue state */
#ifdef DRACO
	u_char k_rlprfx;	/* MF-II rel. prefix has been seen */
#endif
};
struct kbd_softc kbd_softc;

int kbdmatch __P((struct device *, void *, void *));
void kbdattach __P((struct device *, struct device *, void *));
void kbdintr __P((int));
void kbdstuffchar __P((u_char));

struct cfattach kbd_ca = {
	sizeof(struct device), kbdmatch, kbdattach
};

struct cfdriver kbd_cd = {
	NULL, "kbd", DV_DULL, NULL, 0
};

/*ARGSUSED*/
int
kbdmatch(pdp, match, auxp)
	struct device *pdp;
	void *match, *auxp;
{

	if (matchname((char *)auxp, "kbd"))
		return(1);
	return(0);
}

/*ARGSUSED*/
void
kbdattach(pdp, dp, auxp)
	struct device *pdp, *dp;
	void *auxp;
{
#ifdef DRACO
	/* 
	 * XXX Must be kept in sync with kbdenable() switch.
	 * XXX This should be probed, but this way we dont need to initialize
	 * the keyboards.
	 */
	switch (is_draco()) {
		case 0: 
		case 1: 
		case 2: 
			printf(": CIA A type Amiga\n");
			break;
		case 3:
		case 4:
		default:
			printf(": QuickLogic type MF-II\n");
			break;
	}
#else
	printf(": CIA A type Amiga\n");
#endif

}

/* definitions for amiga keyboard encoding. */
#define KEY_CODE(c)  ((c) & 0x7f)
#define KEY_UP(c)    ((c) & 0x80)

void
kbdenable()
{
	int s;

#ifdef DRACO
	u_char c;
#endif
	/*
	 * collides with external ints from SCSI, watch out for this when
	 * enabling/disabling interrupts there !!
	 */
	s = spltty();
#ifdef DRACO
	switch (is_draco()) {
		case 0:
			custom.intena = INTF_SETCLR | INTF_PORTS;

			ciaa.icr = CIA_ICR_IR_SC | CIA_ICR_SP;
						/* SP interrupt enable */
			ciaa.cra &= ~(1<<6);	/* serial line == input */
			printf("ok.\n");
			break;
		case 1:
		case 2:
			/* XXX: tobedone: conditionally enable that one */
			/* XXX: for now, just enable DraCo ports and CIA */
			*draco_intena |= DRIRQ_INT2;
			ciaa.icr = CIA_ICR_IR_SC | CIA_ICR_SP;
						/* SP interrupt enable */
			ciaa.cra &= ~(1<<6);	/* serial line == input */
			break;

		case 3:
			ciaa.icr = CIA_ICR_SP;  /* CIA SP interrupt disable */
			ciaa.cra &= ~(1<<6);	/* serial line == input */
			/* FALLTHROUGH */
		case 4:
		default:
			/* XXX: for now: always enable own keyboard */

			while (draco_ioct->io_status & DRSTAT_KBDRECV) {
				c = draco_ioct->io_kbddata;
				draco_ioct->io_kbdrst = 0;
				printf(".");
				DELAY(2000);
			}

			draco_ioct->io_control &= ~DRCNTRL_KBDINTENA;
			break;
	}
#else
	custom.intena = INTF_SETCLR | INTF_PORTS;
	ciaa.icr = CIA_ICR_IR_SC | CIA_ICR_SP;  /* SP interrupt enable */
	ciaa.cra &= ~(1<<6);		/* serial line == input */
#endif
	kbd_softc.k_event_mode = 0;
	kbd_softc.k_events.ev_io = 0;
	splx(s);
}


int
kbdopen(dev, flags, mode, p)
	dev_t dev;
	int flags, mode;
	struct proc *p;
{

	if (kbd_softc.k_events.ev_io)
		return EBUSY;

	kbd_softc.k_events.ev_io = p;
	ev_init(&kbd_softc.k_events);
	return (0);
}

int
kbdclose(dev, flags, mode, p)
	dev_t dev;
	int flags, mode;
	struct proc *p;
{

	/* Turn off event mode, dump the queue */
	kbd_softc.k_event_mode = 0;
	ev_fini(&kbd_softc.k_events);
	kbd_softc.k_events.ev_io = NULL;
	return (0);
}

int
kbdread(dev, uio, flags)
	dev_t dev;
	struct uio *uio;
	int flags;
{
	return ev_read (&kbd_softc.k_events, uio, flags);
}

int
kbdioctl(dev, cmd, data, flag, p)
	dev_t dev;
	u_long cmd;
	register caddr_t data;
	int flag;
	struct proc *p;
{
	register struct kbd_softc *k = &kbd_softc;

	switch (cmd) {
		case KIOCTRANS:
			if (*(int *)data == TR_UNTRANS_EVENT)
				return 0;
			break;

		case KIOCGTRANS:
			/* Get translation mode */
			*(int *)data = TR_UNTRANS_EVENT;
			return 0;

		case KIOCSDIRECT:
			k->k_event_mode = *(int *)data;
			return 0;

		case FIONBIO:	/* we will remove this someday (soon???) */
			return 0;

		case FIOASYNC:
			k->k_events.ev_async = *(int *)data != 0;
			return 0;

		case TIOCSPGRP:
			if (*(int *)data != k->k_events.ev_io->p_pgid)
				return EPERM;
			return 0;

		default:
			return ENOTTY;
	}

	/* We identified the ioctl, but we do not handle it. */
	return EOPNOTSUPP;	/* misuse, but what the heck */
}

int
kbdselect(dev, rw, p)
	dev_t dev;
	int rw;
	struct proc *p;
{
	return ev_select (&kbd_softc.k_events, rw, p);
}


void
kbdintr(mask)
	int mask;
{
	u_char c;
#ifdef KBDRESET
	static int reset_warn;
#endif
 
	/* 
	 * now only invoked from generic CIA interrupt handler if there *is*
	 * a keyboard interrupt pending
	 */
    
	c = ~ciaa.sdr;	/* keyboard data is inverted */
	/* ack */
	ciaa.cra |= (1 << 6);	/* serial line output */
#ifdef KBDRESET
	if (reset_warn && c == 0xf0) {
#ifdef DEBUG
		printf ("kbdintr: !!!! Reset Warning !!!!\n");
#endif
		bootsync();
		reset_warn = 0;
		DELAY(30000000);
	}
#endif
	/* wait 200 microseconds (for bloody Cherry keyboards..) */
	DELAY(2000);			/* fudge delay a bit for some keyboards */
	ciaa.cra &= ~(1 << 6);

	/* process the character */
	c = (c >> 1) | (c << 7);	/* rotate right once */

#ifdef KBDRESET
	if (c == 0x78) {
#ifdef DEBUG
		printf ("kbdintr: Reset Warning started\n");
#endif
		++reset_warn;
		return;
	}
#endif
	kbdstuffchar(c);
}
  
#ifdef DRACO
/* maps MF-II keycodes to Amiga keycodes */

u_char drkbdtab[] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x50,
	0x45, 0xff, 0xff, 0xff, 0xff, 0x42, 0x00, 0x51,

	0xff, 0x64, 0x60, 0x30, 0x63, 0x10, 0x01, 0x52,
	0xff, 0x66, 0x31, 0x21, 0x20, 0x11, 0x02, 0x53,

	0xff, 0x33, 0x32, 0x22, 0x12, 0x04, 0x03, 0x54,
	0xff, 0x40, 0x34, 0x23, 0x14, 0x13, 0x05, 0x55,

	0xff, 0x36, 0x35, 0x25, 0x24, 0x15, 0x06, 0x56,
	0xff, 0x67, 0x37, 0x26, 0x16, 0x07, 0x08, 0x57,
	/* --- */
	0xff, 0x38, 0x27, 0x17, 0x18, 0x0a, 0x09, 0x58,
	0xff, 0x39, 0x3a, 0x28, 0x29, 0x19, 0x0b, 0x59,

	0xff, 0xff, 0x2a, 0x2b, 0x1a, 0x0c, 0x4b, 0xff,
	0x65, 0x61, 0x44, 0x1b, 0xff, 0xff, 0x6f, 0xff,

	0x4d, 0x4f, 0xff, 0x4c, 0x0d, 0xff, 0x41, 0x46,
	0xff, 0x1d, 0x4e, 0x2d, 0x3d, 0x4a, 0x5f, 0x62,

	0x0f, 0x3c, 0x1e, 0x2e, 0x2f, 0x3e, 0x5a, 0x5b,
	0xff, 0x43, 0x1f, 0xff, 0x5e, 0x3f, 0x5c, 0xff,
	/* --- */
	0xff, 0xff, 0xff, 0xff, 0x5d
};
#endif


int
kbdgetcn ()
{
	int s;
	u_char ints, mask, c, in;

#ifdef DRACO
	/*
	 * XXX todo: if CIA DraCo, get from cia if cia kbd 
	 * installed.
	 */
	if (is_draco()) {
		c = 0;
		s = spltty ();
		while ((draco_ioct->io_status & DRSTAT_KBDRECV) == 0);
		in = draco_ioct->io_kbddata;
		draco_ioct->io_kbdrst = 0;
		if (in == 0xF0) { /* release prefix */
			c = 0x80;
			while ((draco_ioct->io_status & DRSTAT_KBDRECV) == 0);
			in = draco_ioct->io_kbddata;
			draco_ioct->io_kbdrst = 0;
		}
		splx(s);
#ifdef DRACORAWKEYDEBUG
		printf("<%02x>", in);
#endif
		return (in>=sizeof(drkbdtab) ? 0xff : drkbdtab[in]|c);
	}
#endif
	s = spltty();
	for (ints = 0; ! ((mask = ciaa.icr) & CIA_ICR_SP); 
	    ints |= mask) ;

	in = ciaa.sdr;
	c = ~in;
  
	/* ack */
	ciaa.cra |= (1 << 6);	/* serial line output */
	ciaa.sdr = 0xff;	/* ack */
	/* wait 200 microseconds */
	DELAY(2000);	/* XXXX only works as long as DELAY doesn't 
			 * use a timer and waits.. */
	ciaa.cra &= ~(1 << 6);
	ciaa.sdr = in;

	splx (s);
	c = (c >> 1) | (c << 7);

	/* take care that no CIA-interrupts are lost */
	if (ints)
		dispatch_cia_ints (0, ints);

	return c;
}

void
kbdstuffchar(c)
	u_char c;
{
	struct firm_event *fe;
	struct kbd_softc *k = &kbd_softc;
	int put;

	/* 
	 * If not in event mode, deliver straight to ite to process 
	 * key stroke 
	 */

	if (! k->k_event_mode) {
		ite_filter (c, ITEFILT_TTY);
		return;
	}

	/* 
	 * Keyboard is generating events. Turn this keystroke into an
	 * event and put it in the queue. If the queue is full, the
	 * keystroke is lost (sorry!).
	 */
	
	put = k->k_events.ev_put;
	fe = &k->k_events.ev_q[put];
	put = (put + 1) % EV_QSIZE;
	if (put == k->k_events.ev_get) {
		log(LOG_WARNING, "keyboard event queue overflow\n");
			/* ??? */
		return;
	}
	fe->id = KEY_CODE(c);
	fe->value = KEY_UP(c) ? VKEY_UP : VKEY_DOWN;
	fe->time = time;
	k->k_events.ev_put = put;
	EV_WAKEUP(&k->k_events);
}


#ifdef DRACO
void
drkbdintr()
{
	u_char in;
	struct kbd_softc *k = &kbd_softc;

	in = draco_ioct->io_kbddata;
	draco_ioct->io_kbdrst = 0;

	if (in == 0xF0) 
		k->k_rlprfx = 0x80;
	else {
		kbdstuffchar(in>=sizeof(drkbdtab) ? 0xff :
		    drkbdtab[in] | k->k_rlprfx);
		k->k_rlprfx = 0;
	}
}

#endif
