/*	$OpenBSD: sysbeep_isa.c,v 1.2 2004/05/19 03:17:07 drahn Exp $	*/
/*	$NetBSD: sysbeep_isa.c,v 1.4 2002/10/02 15:45:10 thorpej Exp $	*/

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Mark Brinicombe of Causality Limited.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <dev/isa/isavar.h>

#include <dev/isa/pcppivar.h>

/* Prototypes */
int sysbeep_isa_match (struct device *parent, void *cf, void *aux);
void sysbeep_isa_attach (struct device *parent, struct device *self, void *aux);
void sysbeep_isa (int pitch, int period);

/* device attach structure */
struct cfattach sysbeep_isa_ca = {
	sizeof (struct device), sysbeep_isa_match, sysbeep_isa_attach
};

struct cfdriver sysbeep_cd = {
	NULL, "sysbeep_isa", DV_DULL
};

static int ppi_attached;
static pcppi_tag_t ppicookie;

int
sysbeep_isa_match(parent, match, aux)
	struct device *parent;
	void *match;
	void *aux;
{
	return (!ppi_attached);
}

void
sysbeep_isa_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	printf("\n");

	ppicookie = ((struct pcppi_attach_args *)aux)->pa_cookie;
	ppi_attached = 1;
}

void
sysbeep(pitch, period)
	int pitch, period;
{
	if (ppi_attached)
		pcppi_bell(ppicookie, pitch, period, 0);
}
