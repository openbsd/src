/*	$NetBSD: kbd.c,v 1.15 1995/05/07 15:37:11 chopps Exp $	*/

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
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/kernel.h>
#include <sys/syslog.h>
#include <dev/cons.h>
#include <machine/cpu.h>
#include <amiga/amiga/device.h>
#include <amiga/amiga/custom.h>
#include <amiga/amiga/cia.h>
#include <amiga/dev/itevar.h>
#include <amiga/dev/kbdreg.h>
#include <amiga/dev/event_var.h>
#include <amiga/dev/vuid_event.h>
#include "kbd.h"

struct kbd_softc {
	int k_event_mode;	/* if true, collect events, else pass to ite */
	struct evvar k_events;	/* event queue state */
};
struct kbd_softc kbd_softc;

void kbdattach __P((struct device *, struct device *, void *));
int kbdmatch __P((struct device *, struct cfdata *, void *));

struct cfdriver kbdcd = {
	NULL, "kbd", (cfmatch_t)kbdmatch, kbdattach, DV_DULL,
	sizeof(struct device), NULL, 0 };

/*ARGSUSED*/
int
kbdmatch(pdp, cfp, auxp)
	struct device *pdp;
	struct cfdata *cfp;
	void *auxp;
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
	printf("\n");
}

/* definitions for amiga keyboard encoding. */
#define KEY_CODE(c)  ((c) & 0x7f)
#define KEY_UP(c)    ((c) & 0x80)

void
kbdenable ()
{
	int s;

	/*
	 * collides with external ints from SCSI, watch out for this when
	 * enabling/disabling interrupts there !!
	 */
	s = spltty();
	custom.intena = INTF_SETCLR | INTF_PORTS;
	ciaa.icr = CIA_ICR_IR_SC | CIA_ICR_SP;  /* SP interrupt enable */
	ciaa.cra &= ~(1<<6);		/* serial line == input */
	kbd_softc.k_event_mode = 0;
	kbd_softc.k_events.ev_io = 0;
	splx(s);
}


int
kbdopen (dev_t dev, int flags, int mode, struct proc *p)
{
  int s, error;

  if (kbd_softc.k_events.ev_io)
    return EBUSY;

  kbd_softc.k_events.ev_io = p;
  ev_init(&kbd_softc.k_events);
  return (0);
}

int
kbdclose (dev_t dev, int flags, int mode, struct proc *p)
{
  /* Turn off event mode, dump the queue */
  kbd_softc.k_event_mode = 0;
  ev_fini(&kbd_softc.k_events);
  kbd_softc.k_events.ev_io = NULL;
  return (0);
}

int
kbdread (dev_t dev, struct uio *uio, int flags)
{
  return ev_read (&kbd_softc.k_events, uio, flags);
}

int
kbdioctl(dev_t dev, u_long cmd, register caddr_t data, int flag, struct proc *p)
{
  register struct kbd_softc *k = &kbd_softc;

  switch (cmd) 
    {
    case KIOCTRANS:
      if (*(int *)data == TR_UNTRANS_EVENT)
	return 0;
      break;

    case KIOCGTRANS:
      /*
       * Get translation mode
       */
      *(int *)data = TR_UNTRANS_EVENT;
      return 0;

    case KIOCSDIRECT:
      k->k_event_mode = *(int *)data;
      return 0;

    case FIONBIO:		/* we will remove this someday (soon???) */
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

  /*
   * We identified the ioctl, but we do not handle it.
   */
  return EOPNOTSUPP;		/* misuse, but what the heck */
}

int
kbdselect (dev_t dev, int rw, struct proc *p)
{
  return ev_select (&kbd_softc.k_events, rw, p);
}


int
kbdintr (mask)
     int mask;
{
  u_char c;
  struct kbd_softc *k = &kbd_softc;
  struct firm_event *fe;
  int put;
#ifdef KBDRESET
  static int reset_warn;
#endif
 
  /* now only invoked from generic CIA interrupt handler if there *is*
     a keyboard interrupt pending */
    
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
  /* if not in event mode, deliver straight to ite to process key stroke */
  if (! k->k_event_mode)
    {
      ite_filter (c, ITEFILT_TTY);
      return;
    }

  /* Keyboard is generating events.  Turn this keystroke into an
     event and put it in the queue.  If the queue is full, the
     keystroke is lost (sorry!). */
  
  put = k->k_events.ev_put;
  fe = &k->k_events.ev_q[put];
  put = (put + 1) % EV_QSIZE;
  if (put == k->k_events.ev_get) 
    {
      log(LOG_WARNING, "keyboard event queue overflow\n"); /* ??? */
      return;
    }
  fe->id = KEY_CODE(c);
  fe->value = KEY_UP(c) ? VKEY_UP : VKEY_DOWN;
  fe->time = time;
  k->k_events.ev_put = put;
  EV_WAKEUP(&k->k_events);
}


int
kbdgetcn ()
{
  int s = spltty ();
  u_char ints, mask, c, in;

  for (ints = 0; ! ((mask = ciaa.icr) & CIA_ICR_SP); ints |= mask) ;

  in = ciaa.sdr;
  c = ~in;
  
  /* ack */
  ciaa.cra |= (1 << 6);	/* serial line output */
  ciaa.sdr = 0xff;		/* ack */
  /* wait 200 microseconds */
  DELAY(2000);    /* XXXX only works as long as DELAY doesn't use a timer and waits.. */
  ciaa.cra &= ~(1 << 6);
  ciaa.sdr = in;

  splx (s);
  c = (c >> 1) | (c << 7);

  /* take care that no CIA-interrupts are lost */
  if (ints)
    dispatch_cia_ints (0, ints);

  return c;
}
