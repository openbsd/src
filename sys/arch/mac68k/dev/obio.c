/*	$OpenBSD: obio.c,v 1.7 2008/06/26 05:42:12 ray Exp $	*/
/*	$NetBSD: obio.c,v 1.7 1997/02/13 19:01:07 scottr Exp $	*/

/*
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to the NetBSD Foundation
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/bus.h>

#include <mac68k/dev/obiovar.h>

static int	obio_match(struct device *, void *, void *);
static void	obio_attach(struct device *, struct device *, void *);
static int	obio_print(void *, const char *);
static int	obio_search(struct device *, void *, void *);

struct cfattach obio_ca = {
	sizeof(struct device), obio_match, obio_attach
};

struct cfdriver obio_cd = {
	NULL, "obio", DV_DULL
};

static int
obio_match(parent, vcf, aux)
	struct device *parent;
	void *vcf;
	void *aux;
{
	static int obio_matched = 0;

	/* Allow only one instance. */
	if (obio_matched)
		return (0);

	obio_matched = 1;
	return (1);
}

static void
obio_attach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	printf("\n");

	/* Search for and attach children. */
	(void)config_search(obio_search, self, aux);
}

int
obio_print(args, name)
	void *args;
	const char *name;
{
	struct obio_attach_args *oa = (struct obio_attach_args *)args;

	if (oa->oa_addr != (-1))
		printf(" addr %x", oa->oa_addr);

	return (UNCONF);
}

int
obio_search(parent, vcf, aux)
	struct device *parent;
	void *vcf;
	void *aux;
{
	struct obio_attach_args oa;
	struct cfdata	*cf = (struct cfdata *) vcf;

	oa.oa_addr = cf->cf_loc[0];
	oa.oa_tag = MAC68K_BUS_SPACE_MEM;

	if ((*cf->cf_attach->ca_match)(parent, cf, &oa) > 0)
		config_attach(parent, cf, &oa, obio_print);

	return (0);
}
