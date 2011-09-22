/*	$OpenBSD: mainbus.c,v 1.6 2011/09/22 17:45:59 miod Exp $	*/
/* $NetBSD: mainbus.c,v 1.3 2001/06/13 17:52:43 nathanw Exp $ */

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
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>

#include <arm/mainbus/mainbus.h>

/* Prototypes for functions provided */

int  mainbusmatch(struct device *, void *, void *);
void mainbusattach(struct device *, struct device *, void *);
int  mainbusprint(void *aux, const char *mainbus);
int mainbussearch(struct device *,  void *, void *);

/* attach and device structures for the device */

struct cfattach mainbus_ca = {
	sizeof(struct device), mainbusmatch, mainbusattach, NULL,
	config_activate_children
};

struct cfdriver mainbus_cd = {
	NULL, "mainbus", DV_DULL
};

/*
 * int mainbusmatch(struct device *parent, struct cfdata *cf, void *aux)
 */

int
mainbusmatch(struct device *parent, void *cf, void *aux)
{
	return (1);
}

/*
 * void mainbusattach(struct device *parent, struct device *self, void *aux)
 *
 * probe and attach all children
 */

void
mainbusattach(struct device *parent, struct device *self, void *aux)
{
	printf("\n");

	config_search(mainbussearch, self, aux);
}

int
mainbussearch(struct device *parent, void *vcf, void *aux)
{
	struct mainbus_attach_args ma;
	struct cfdata *cf = vcf;

	ma.ma_name = cf->cf_driver->cd_name;

	/* allow for devices to be disabled in UKC */
	if ((*cf->cf_attach->ca_match)(parent, cf, &ma) == 0)
		return 0;

	config_attach(parent, cf, &ma, mainbusprint);
	return 1;
}

/*
 * int mainbusprint(void *aux, const char *mainbus)
 *
 * print routine used during config of children
 */

int
mainbusprint(void *aux, const char *mainbus)
{
	struct mainbus_attach_args *ma = aux;

	if (mainbus != NULL)
		printf("%s at %s", ma->ma_name, mainbus);

	return (UNCONF);
}
