/*	$OpenBSD: intio.c,v 1.7 2008/06/26 05:42:10 ray Exp $	*/
/*	$NetBSD: intio.c,v 1.2 1997/01/30 09:18:54 thorpej Exp $	*/

/*-
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Autoconfiguration support for hp300 internal i/o space.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/bus.h>

#include <hp300/dev/intiovar.h>

int	intiomatch(struct device *, void *, void *);
void	intioattach(struct device *, struct device *, void *);
int	intioprint(void *, const char *);
int	intiosearch(struct device *, void *, void *);

struct cfattach intio_ca = {
	sizeof(struct device), intiomatch, intioattach
};

struct cfdriver intio_cd = {
	NULL, "intio", DV_DULL
};

int
intiomatch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	static int intio_matched = 0;

	/* Allow only one instance. */
	if (intio_matched)
		return (0);

	intio_matched = 1;
	return (1);
}

void
intioattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{

	printf("\n");

	/* Search for and attach children. */
	config_search(intiosearch, self, NULL);
}

int
intioprint(aux, pnp)
	void *aux;
	const char *pnp;
{
	struct intio_attach_args *ia = aux;

	if (ia->ia_addr != 0)
		printf(" addr %p", ia->ia_addr);
	return (UNCONF);
}

int
intiosearch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct cfdata *cf = match;
	struct intio_attach_args ia;
	extern struct hp300_bus_space_tag hp300_mem_tag;

	bzero(&ia, sizeof(ia));
	ia.ia_tag = &hp300_mem_tag;

	if ((*cf->cf_attach->ca_match)(parent, cf, &ia) > 0)
		config_attach(parent, cf, &ia, intioprint);

	return (0);
}
