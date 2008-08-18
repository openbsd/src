/*	$OpenBSD: dzinput.c,v 1.1 2008/08/18 23:04:28 miod Exp $	*/
/*	$NetBSD: dz_ibus.c,v 1.15 1999/08/27 17:50:42 ragge Exp $ */
/*
 * Copyright (c) 1998 Ludd, University of Lule}, Sweden.
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
 *     This product includes software developed at Ludd, University of 
 *     Lule}, Sweden and its contributors.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
 * Common input routines used by dzkbd and dzms devices.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/tty.h>
#include <sys/conf.h>
#include <sys/device.h>

#include <machine/bus.h>

#include <vax/qbus/dzreg.h>
#include <vax/qbus/dzvar.h>

#include <vax/dec/dzkbdvar.h>

#include <dev/cons.h>

cons_decl(dz);

int
dz_print(void *aux, const char *name)
{
	struct dzkm_attach_args *dz_args = aux;

	if (name != NULL)
		printf(dz_args->daa_line == 0 ? "lkkbd at %s" : "lkms at %s",
		    name);
	else
		printf(" line %d", dz_args->daa_line);

	return (UNCONF);
}

void
dzputc(struct dz_linestate *ls, int ch)
{
	int line;
	u_short tcr;
	int s;

	/*
	 * If the dz has already been attached, the MI
	 * driver will do the transmitting...
	 */
	if (ls && ls->dz_sc) {
		s = spltty();
		line = ls->dz_line;
		putc(ch, &ls->dz_tty->t_outq);
		tcr = DZ_READ_WORD(ls->dz_sc, dr_tcr);
		if (!(tcr & (1 << line)))
			DZ_WRITE_WORD(ls->dz_sc, dr_tcr, tcr | (1 << line));
		dzxint(ls->dz_sc);
		splx(s);
		return;
	}

	/*
	 * Otherwise, use dzcnputc to do the transmitting.
	 * This situation only happens for a console keyboard, which is
	 * why the minor is hardcoded to the line number.  Also, dzcnputc()
	 * does not care about the major number, so we skip a not-so-cheap
	 * getminor() call.
	 */
	dzcnputc(makedev(0 /*getmajor(dzopen)*/, 0), ch);
}
