/*	$OpenBSD: vsms_ws.c,v 1.4 2008/08/26 19:46:23 miod Exp $	*/
/*	$NetBSD: dzms.c,v 1.1 2000/12/02 17:03:55 ragge Exp $	*/

/*
 * Copyright (c) 2008 Miodrag Vallat.
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

int	lkms_handle_error(struct lkms_softc *, int);
void	lkms_input_disabled(struct lkms_softc *, int);
void	lkms_input_mouse(struct lkms_softc *, int);
void	lkms_input_tablet(struct lkms_softc *, int);

#define WSMS_BUTTON(x)	(1 << ((x) - 1))

struct	cfdriver lkms_cd = {
	NULL, "lkms", DV_DULL
};

/*
 * Report the device type and status on attachment, and return nonzero
 * if it is not in working state.
 */
int
lkms_handle_error(struct lkms_softc *sc, int mask)
{
	int error = sc->sc_error;

#ifdef DEBUG
	printf("%s: ", sc->dzms_dev.dv_xname);
#endif

	if (ISSET(sc->sc_flags, MS_TABLET)) {
#ifdef DEBUG
		printf("tablet, ");
#endif
		/*
		 * If we are a tablet, the stylet vs puck information
		 * is returned as a non-fatal status code.  Handle
		 * it there so that this does not get in the way.
		 */
		switch (error) {
		case ERROR_TABLET_NO_POINTER:
			/* i can has cheezpuck? */
#ifdef DEBUG
			printf("neither stylus nor puck connected\n");
#else
			printf("%s: neither stylus nor puck connected\n",
			    sc->dzms_dev.dv_xname);
#endif
			error = ERROR_OK;
			break;
		case ERROR_TABLET_STYLUS:
#ifdef DEBUG
			printf("stylus\n");
#endif
			SET(sc->sc_flags, MS_STYLUS);
			error = ERROR_OK;
			break;
		default:
#ifdef DEBUG
			printf("puck\n");
#endif
			break;
		}
	} else {
#ifdef DEBUG
		printf("mouse\n");
#endif
	}

	switch (error) {
	case ERROR_MEMORY_CKSUM_ERROR:
		printf("%s: memory checksum error\n",
		    sc->dzms_dev.dv_xname);
		break;
	case ERROR_BUTTON_ERROR:
	    {
		int btn;

		/*
		 * Print the list of defective parts
		 */
		if (ISSET(sc->sc_flags, MS_TABLET)) {
			if ((mask & FRAME_T_PR) != 0)
				printf("%s: proximity sensor defective\n",
				    sc->dzms_dev.dv_xname);
		} else
			mask <<= 1;

		for (btn = 1; btn < 4; btn++)
			if ((mask & (1 << btn)) != 0)
				printf("%s: button %d held down or defective\n",
				    sc->dzms_dev.dv_xname, btn);
	    }
		break;
	case ERROR_TABLET_LINK:
		/* how vague this error is... */
		printf("%s: analog or digital error\n",
		    sc->dzms_dev.dv_xname);
		break;
	default:
		printf("%s: %sselftest error %02x\n", sc->dzms_dev.dv_xname,
		    error >= ERROR_FATAL ? "fatal " : "", error);
		break;
	case ERROR_OK:
		break;
	}

	if (error >= ERROR_FATAL)
		return ENXIO;

	return 0;
}

int
lkms_ioctl(void *v, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	struct lkms_softc *sc = v;
	struct wsmouse_calibcoords *wsmc = (struct wsmouse_calibcoords *)data;

	switch (cmd) {
	case WSMOUSEIO_GTYPE:
		*(int *)data = WSMOUSE_TYPE_VSXXX;
		return 0;

	case WSMOUSEIO_GCALIBCOORDS:
		if (ISSET(sc->sc_flags, MS_TABLET)) {
			/*
			 * The tablet has a usable size of 11 inch on each
			 * axis, with a 200dpi resolution.
			 */
			wsmc->minx = 0;
			wsmc->maxx = 200 * 11;
			wsmc->miny = 0;
			wsmc->maxy = 200 * 11;
			wsmc->swapxy = 0;
			wsmc->resx = wsmc->maxx;	/* anything better? */
			wsmc->resy = wsmc->maxy;	/* anything better? */
			wsmc->samplelen = 0;
			return 0;
		} else
			break;
	}

	return -1;
}

int
lkms_input(void *vsc, int data)
{
	struct lkms_softc *sc = vsc;

	if ((data & FRAME_MASK) != 0) {
		sc->sc_frametype = data & FRAME_TYPE_MASK;
		sc->sc_framepos = 0;
	} else
		sc->sc_framepos++;

	if (ISSET(sc->sc_flags, MS_ENABLED) &&
	    !ISSET(sc->sc_flags, MS_SELFTEST)) {
		switch (sc->sc_frametype) {
		case FRAME_MOUSE:
			lkms_input_mouse(sc, data);
			break;
		case FRAME_TABLET:
			lkms_input_tablet(sc, data);
			break;
		}
	} else
		lkms_input_disabled(sc, data);

	return 1;
}

/*
 * Input processing while the device is disabled.  We only are
 * interested in processing self test frames, so as to identify
 * the device and report its state.
 */
void
lkms_input_disabled(struct lkms_softc *sc, int data)
{
	if (!ISSET(sc->sc_flags, MS_SELFTEST))
		return;

	if (sc->sc_frametype == FRAME_SELFTEST) {
		switch (sc->sc_framepos) {
		case 0:
			break;
		case 1:
			data &= FRAME_ST_DEVICE_MASK;
			if (data == FRAME_ST_DEVICE_TABLET)
				SET(sc->sc_flags, MS_TABLET);
			else if (data != FRAME_ST_DEVICE_MOUSE) {
				printf("%s: unrecognized device type %02x\n",
				    sc->dzms_dev.dv_xname, data);
				goto fail;
			}
			break;
		case 2:
			sc->sc_error = data;
			break;
		case 3:
			if (lkms_handle_error(sc, data) != 0)
				goto fail;

			CLR(sc->sc_flags, MS_SELFTEST);
			goto success;
			break;
		}

		return;
	} /* else goto fail; */

fail:
	/*
	 * Our self test frame has been truncated, or we have received
	 * incorrect data (both could be a cable problem), or the
	 * selftest reported an error.  The device is unusable.
	 */
	CLR(sc->sc_flags, MS_TABLET | MS_STYLUS);

success:
	sc->sc_frametype = 0;
	wakeup(&sc->sc_flags);
}

/*
 * Input processing while the device is enabled, for mouse frames.
 */
void
lkms_input_mouse(struct lkms_softc *sc, int data)
{
	switch (sc->sc_framepos) {
	case 0:
		sc->buttons = 0;
		/* button order is inverted from wscons */
		if ((data & FRAME_MS_B3) != 0)
			sc->buttons |= WSMS_BUTTON(1);
		if ((data & FRAME_MS_B2) != 0)
			sc->buttons |= WSMS_BUTTON(2);
		if ((data & FRAME_MS_B1) != 0)
			sc->buttons |= WSMS_BUTTON(3);

		sc->dx = data & FRAME_MS_X_SIGN;
		sc->dy = data & FRAME_MS_Y_SIGN;
		break;
	case 1:
		if (sc->dx == 0)
			sc->dx = -data;
		else
			sc->dx = data;
		break;
	case 2:
		if (sc->dy == 0)
			sc->dy = -data;
		else
			sc->dy = data;
		wsmouse_input(sc->sc_wsmousedev, sc->buttons,
		    sc->dx, sc->dy, 0, 0, WSMOUSE_INPUT_DELTA);

		sc->sc_frametype = 0;
		break;
	}
}

/*
 * Input processing while the device is enabled, for tablet frames.
 */
void
lkms_input_tablet(struct lkms_softc *sc, int data)
{
	switch (sc->sc_framepos) {
	case 0:
		/*
		 * Button information will depend on the type of positional
		 * device:
		 * - puck buttons get reported as is, as a 4 button mouse.
		 *   Button order is opposite from mouse.
		 * - stylus barrel gets reported as left button, while tip
		 *   gets reported as right button.
		 *   Proximity sensor gets reported as a fictitious fifth
		 *   button.
		 */
		sc->buttons = 0;
		if ((data & FRAME_T_B1) != 0)
			sc->buttons |= WSMS_BUTTON(1);
		if ((data & FRAME_T_B2) != 0) {
			if (ISSET(sc->sc_flags, MS_STYLUS))
				sc->buttons |= WSMS_BUTTON(3);
			else
				sc->buttons |= WSMS_BUTTON(2);
		}
		if ((data & FRAME_T_B3) != 0)
			sc->buttons |= WSMS_BUTTON(3);
		if ((data & FRAME_T_B4) != 0)
			sc->buttons |= WSMS_BUTTON(4);
		if ((data & FRAME_T_PR) == 0)
			sc->buttons |= WSMS_BUTTON(5);
		break;
	case 1:
		sc->dx = data & 0x3f;
		break;
	case 2:
		sc->dx |= (data & 0x3f) << 6;
		break;
	case 3:
		sc->dy = data & 0x3f;
		break;
	case 4:
		sc->dy |= (data & 0x3f) << 6;
		wsmouse_input(sc->sc_wsmousedev, sc->buttons,
		    sc->dx, sc->dy, 0, 0,
		    WSMOUSE_INPUT_ABSOLUTE_X | WSMOUSE_INPUT_ABSOLUTE_Y);

		sc->sc_frametype = 0;
		break;
	}
}
