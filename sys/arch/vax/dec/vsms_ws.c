/*	$OpenBSD: vsms_ws.c,v 1.2 2007/04/10 22:37:17 miod Exp $	*/
/*	$NetBSD: dzms.c,v 1.1 2000/12/02 17:03:55 ragge Exp $	*/

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 *	@(#)ms.c	8.1 (Berkeley) 6/11/93
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/syslog.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/tty.h>

#include <machine/bus.h>

#include <vax/qbus/dzreg.h>
#include <vax/qbus/dzvar.h>

#include <vax/dec/dzkbdvar.h>
#include <vax/dec/vsmsvar.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsmousevar.h>

struct	cfdriver lkms_cd = {
	NULL, "lkms", DV_DULL
};

int
lkms_ioctl(void *v, u_long cmd, caddr_t data, int flag, struct proc *p)
{
#if 0
	struct lkms_softc *sc = v;
#endif

	switch (cmd) {
	case WSMOUSEIO_GTYPE:
		*(int *)data = WSMOUSE_TYPE_VSXXX;
		return 0;
	}

	return -1;
}

int
lkms_input(void *vsc, int data)
{
	struct lkms_softc *sc = vsc;

	if (!sc->sc_enabled) {
		if (sc->sc_selftest > 0) {
			sc->sc_selftest--;
			if (sc->sc_selftest == 0)
				wakeup(&sc->sc_enabled);
		}
		return (1);
	}

#define WSMS_BUTTON1    0x01
#define WSMS_BUTTON2    0x02
#define WSMS_BUTTON3    0x04

	if ((data & MOUSE_START_FRAME) != 0)
		sc->inputstate = 1;
	else
		sc->inputstate++;

	if (sc->inputstate == 1) {
		sc->buttons = 0;
		if ((data & LEFT_BUTTON) != 0)
			sc->buttons |= WSMS_BUTTON1;
		if ((data & MIDDLE_BUTTON) != 0)
			sc->buttons |= WSMS_BUTTON2;
		if ((data & RIGHT_BUTTON) != 0)
			sc->buttons |= WSMS_BUTTON3;
	    
		sc->dx = data & MOUSE_X_SIGN;
		sc->dy = data & MOUSE_Y_SIGN;
	} else if (sc->inputstate == 2) {
		if (sc->dx == 0)
			sc->dx = -data;
		else
			sc->dx = data;
	} else if (sc->inputstate == 3) {
		sc->inputstate = 0;
		if (sc->dy == 0)
			sc->dy = -data;
		else
			sc->dy = data;
		wsmouse_input(sc->sc_wsmousedev, sc->buttons,
		    sc->dx, sc->dy, 0, 0, WSMOUSE_INPUT_DELTA);
	}

	return (1);
}
