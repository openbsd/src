/*	$OpenBSD: mainbus.c,v 1.8 2008/06/26 05:42:12 ray Exp $	*/
/*	$NetBSD: mainbus.c,v 1.7 1996/12/17 06:47:41 scottr Exp $	*/

/*
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

#include <sys/param.h>
#include <sys/device.h>
#include <sys/systm.h>

static int	mainbus_match(struct device *, void *, void *);
static void	mainbus_attach(struct device *, struct device *, void *);
static int	mainbus_search(struct device *, void *, void *);

struct cfattach mainbus_ca = {
	sizeof(struct device), mainbus_match, mainbus_attach
};

struct cfdriver mainbus_cd = {
	NULL, "mainbus", DV_DULL
};

static int
mainbus_match(parent, vcf, aux)
	struct device *parent;
	void *vcf;
	void *aux;
{
	static int mainbus_matched = 0;

	/* Allow only one instance. */
	if (mainbus_matched)
		return (0);

	mainbus_matched = 1;
	return 1;
}

static void
mainbus_attach(parent, self, aux)
	struct device	*parent, *self;
	void		*aux;
{
	printf("\n");

	/* Search for and attach children. */
	config_search(mainbus_search, self, NULL);
}

static int
mainbus_search(parent, vcf, aux)
	struct device *parent;
	void *vcf;
	void *aux;
{
	struct cfdata *cf = (struct cfdata *) vcf;

	if ((*cf->cf_attach->ca_match)(parent, cf, NULL) > 0)
		config_attach(parent, cf, NULL, NULL);
	return 0;
}
