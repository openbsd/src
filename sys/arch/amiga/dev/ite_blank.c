/* $OpenBSD: ite_blank.c,v 1.2 2001/08/20 19:35:18 miod Exp $ */
/*-
 * Copyright (c) 1999 Marc Espie.
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
 *	This product includes software developed by Marc Espie for the OpenBSD
 * Project.
 *
 * THIS SOFTWARE IS PROVIDED BY THE OPENBSD PROJECT AND CONTRIBUTORS 
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR 
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OPENBSD
 * PROJECT OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/systm.h>
#include <sys/ioctl.h>
#include <amiga/dev/grfioctl.h>
#include <amiga/dev/grfvar.h>
#include <amiga/dev/itevar.h>
#include <amiga/dev/ite_blank.h>

#define SUBR_BLANK(ip, mode)	((ip)->grf->g_mode((ip)->grf, GM_GRFIOCTL, &(mode), GRFIOCBLANK, (ip)->grf->g_grfdev ))

void ite_blank __P((void *));
void ite_unblank __P((struct ite_softc *));

static int blanked_screen = 0;
static int blank_enabled = 0;
static long last_schedule = 0L;
int blank_time = 600;

void 
ite_blank(arg)
	void *arg;
{
	struct ite_softc *kbd = arg;
	int data = GRFIOCBLANK_DARK;

	SUBR_BLANK(kbd, data);
	blanked_screen = 1;
}

void 
ite_unblank(kbd)
	struct ite_softc *kbd;
{
	if (blanked_screen) {
		int data = GRFIOCBLANK_LIVE;

		SUBR_BLANK(kbd, data);
		blanked_screen = 0;
	}
}
	
void
ite_restart_blanker(kbd)
	struct ite_softc *kbd;
{
	int x = spltty();

	/* steal timing trick from pcvt */
	if (last_schedule != time.tv_sec) {
		if (!timeout_initialized(&kbd->blank_timeout))
			timeout_set(&kbd->blank_timeout, ite_blank, kbd);
		if (blank_enabled && !blanked_screen)
			timeout_del(&kbd->blank_timeout);
		if (blank_enabled && blank_time) 
			timeout_add(&kbd->blank_timeout, blank_time * hz);
		last_schedule = time.tv_sec;
	}
	ite_unblank(kbd);

	splx(x);
}

void 
ite_reset_blanker(kbd)
	struct ite_softc *kbd;
{
	last_schedule = 0L;
	ite_restart_blanker(kbd);
}

void 
ite_disable_blanker(kbd)
	struct ite_softc *kbd;
{
	int x = spltty();

	timeout_del(&kbd->blank_timeout);
	blank_enabled = 0;
	ite_unblank(kbd);

	splx(x);
}
		
void
ite_enable_blanker(kbd)
	struct ite_softc *kbd;
{
	blank_enabled = 1;
	ite_reset_blanker(kbd);
}

