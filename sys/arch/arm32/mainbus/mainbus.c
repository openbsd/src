/* $NetBSD: mainbus.c,v 1.3 1996/03/20 18:38:00 mark Exp $ */

/*
 * Copyright (c) 1994,1995 Mark Brinicombe.
 * Copyright (c) 1994 Brini.
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
 *	This product includes software developed by Brini.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BRINI ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL BRINI OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * RiscBSD kernel project
 *
 * mainbus.c
 *
 * mainbus configuration
 *
 * Created      : 15/12/94
 * Last updated : 03/07/95
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/malloc.h>
#include <sys/device.h>

#include <arm32/mainbus/mainbus.h>
#include <machine/io.h>

int mainbusmatch __P((struct device *, void *, void *));
void mainbusattach __P((struct device *, struct device *, void *));

struct cfattach mainbus_ca = {
	sizeof(struct device), mainbusmatch, mainbusattach
};

struct cfdriver mainbus_cd = {
	NULL, "mainbus", DV_DULL, 1
};

int
mainbusmatch(parent, match, aux)
	struct device *parent;
	void *match;
	void *aux;
{
	return (1);
}

int
mainbusprint(aux, mainbus)
	void *aux;
	const char *mainbus;
{
	struct mainbus_attach_args *mb = aux;

	if (mb->mb_iobase)
		printf(" base 0x%x", mb->mb_iobase);
	if (mb->mb_iosize > 1)
		printf("-0x%x", mb->mb_iobase + mb->mb_iosize - 1);
	if (mb->mb_irq != -1)
		printf(" irq %d", mb->mb_irq);
	if (mb->mb_drq != -1)
		printf(" drq 0x%08x", mb->mb_drq);

/* XXXX print flags */
	return (QUIET);
}


void
mainbusscan(parent, match)
	struct device *parent;
	void *match;
{
	struct device *dev = match;
	struct cfdata *cf = dev->dv_cfdata;
	struct mainbus_attach_args mb;

	if (cf->cf_fstate == FSTATE_STAR)
		panic("eekkk, I'm stuffed");

	if (cf->cf_loc[0] == -1) {
		mb.mb_iobase = 0;
		mb.mb_iosize = 0;
		mb.mb_drq = -1;
		mb.mb_irq = -1;
	} else {    
		mb.mb_iobase = cf->cf_loc[0] + IO_CONF_BASE;
		mb.mb_iosize = 0;
		mb.mb_drq = cf->cf_loc[1];
		mb.mb_irq = cf->cf_loc[2];
	}
	if ((*cf->cf_attach->ca_match)(parent, dev, &mb) > 0)
		config_attach(parent, dev, &mb, mainbusprint);
	else
		free(dev, M_DEVBUF);
}

void
mainbusattach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	printf("\n");

	config_scan(mainbusscan, self);
}

/* End of mainbus.c */
