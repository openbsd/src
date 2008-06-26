/* $OpenBSD: sio.c,v 1.4 2008/06/26 05:42:11 ray Exp $ */
/* $NetBSD: sio.c,v 1.1 2000/01/05 08:48:55 nisimura Exp $ */

/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Tohru Nishimura.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

/* from NetBSD/luna68k dev/sio.c */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/cpu.h>
#include <machine/autoconf.h>

#include <luna88k/luna88k/isr.h>
#include <luna88k/dev/siovar.h>

int  sio_match(struct device *, void *, void *);
void sio_attach(struct device *, struct device *, void *);
int  sio_print(void *, const char *);

const struct cfattach sio_ca = {
	sizeof(struct sio_softc), sio_match, sio_attach
};

struct cfdriver sio_cd = {
	NULL, "sio", DV_DULL, 0
};

void nullintr(int);
int xsiointr(void *);

int
sio_match(parent, cf, aux)
	struct device *parent;
	void *cf, *aux;
{
	struct mainbus_attach_args *ma = aux;

	if (strcmp(ma->ma_name, sio_cd.cd_name))
		return 0;
	if (badaddr((vaddr_t)ma->ma_addr, 4))
		return 0;
	return 1;
}

void
sio_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct sio_softc *sc = (void *)self;
	struct mainbus_attach_args *ma = aux;
	struct sio_attach_args sio_args;
	int channel;
	extern int sysconsole; /* console: 0 for ttya, 1 for desktop */

	printf(": 7201a\n");

	sc->scp_ctl = (caddr_t)ma->ma_addr;
	sc->scp_intr[0] = sc->scp_intr[1] = nullintr;
	for (channel = 0; channel < 2; channel++) {
		sio_args.channel = channel;
		sio_args.hwflags = (channel == sysconsole);
		config_found(self, (void *)&sio_args, sio_print);
	}

	isrlink_autovec(xsiointr, sc, ma->ma_ilvl, ISRPRI_TTYNOBUF,
	    self->dv_xname);
}

int
sio_print(aux, name)
	void *aux;
	const char *name;
{
	struct sio_attach_args *args = aux;

	if (name != NULL)
		printf("%s: ", name);

	if (args->channel != -1)
		printf(" channel %d", args->channel);

	return UNCONF;
}

int
xsiointr(arg)
	void *arg;
{
	struct sio_softc *sc = arg;

	(*sc->scp_intr[0])(0); 	/* 0: ttya system serial port */
	(*sc->scp_intr[1])(1);	/* 1: keyboard and mouse */
	return 1;
}

void nullintr(v)
	int v;
{
}
