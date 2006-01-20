/*	$OpenBSD: akbd_machdep.c,v 1.2 2006/01/20 00:10:09 miod Exp $	*/
/*	$NetBSD: akbd.c,v 1.17 2005/01/15 16:00:59 chs Exp $	*/

/*
 * Copyright (C) 1998	Colin Wood
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
 *	This product includes software developed by Colin Wood.
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
#include <sys/timeout.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/fcntl.h>
#include <sys/poll.h>
#include <sys/selinfo.h>
#include <sys/proc.h>
#include <sys/signalvar.h>
#include <sys/systm.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wskbdvar.h>

#include <machine/cpu.h>
#include <machine/viareg.h>

#include <dev/adb/adb.h>
#include <dev/adb/akbdvar.h>
#include <dev/adb/keyboard.h>

void	akbd_cnbell(void *, u_int, u_int, u_int);
void	akbd_cngetc(void *, u_int *, int *);
void	akbd_cnpollc(void *, int);

struct wskbd_consops akbd_consops = {
	akbd_cngetc,
	akbd_cnpollc,
	akbd_cnbell
};

int
akbd_is_console(void)
{
	return ((mac68k_machine.serial_console & 0x03) == 0);
}

int
akbd_cnattach(void)
{
	wskbd_cnattach(&akbd_consops, NULL, &akbd_keymapdata);
	return 0;
}

void
akbd_cngetc(void *v, u_int *type, int *data)
{
	int intbits, key, press, val;
	int s;
	extern int adb_intr(void *);
	extern void pm_intr(void *);

	s = splhigh();

	adb_polledkey = -1;
	adb_polling = 1;

	while (adb_polledkey == -1) {
		intbits = via_reg(VIA1, vIFR);

		if (intbits & V1IF_ADBRDY) {
			adb_intr(NULL);
			via_reg(VIA1, vIFR) = V1IF_ADBRDY;
		}
		if (intbits & V1IF_ADBCLK) {
			pm_intr(NULL);
			via_reg(VIA1, vIFR) = 0x10;
		}
	}

	adb_polling = 0;
	splx(s);

	key = adb_polledkey;
	press = ADBK_PRESS(key);
	val = ADBK_KEYVAL(key);

	*data = val;
	*type = press ? WSCONS_EVENT_KEY_DOWN : WSCONS_EVENT_KEY_UP;
}

void
akbd_cnpollc(void *v, int on)
{
}

void
akbd_cnbell(void *v, u_int pitch, u_int period, u_int volume)
{
	mac68k_ring_bell(pitch, period * hz / 1000, volume);
}
