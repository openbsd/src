/*	$NetBSD: vme.c,v 1.1 1994/12/12 18:59:26 gwr Exp $	*/

/*
 * Copyright (c) 1994 Gordon W. Ross
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
 *	This product includes software developed by Gordon Ross
 * 4. The name of the Author may not be used to endorse or promote products
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
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/autoconf.h>
/* #include <machine/vme.h> */

static int  vme_match __P((struct device *, void *, void *));
static void vme16attach __P((struct device *, struct device *, void *));
static void vme32attach __P((struct device *, struct device *, void *));
static void vme16scan __P((struct device *, void *));
static void vme32scan __P((struct device *, void *));

struct cfdriver vmescd = {
	NULL, "vmes", vme_match, vme16attach, DV_DULL,
	sizeof(struct device), 0 };

struct cfdriver vmelcd = {
	NULL, "vmel", vme_match, vme32attach, DV_DULL,
	sizeof(struct device), 0 };

int vme_match(parent, vcf, aux)
	struct device *parent;
	void *vcf, *aux;
{
	/* Does this machine have a VME bus? */
	extern int cpu_has_vme;

	return (cpu_has_vme);
}

static void
vme16attach(parent, self, args)
	struct device *parent;
	struct device *self;
	void *args;
{
	printf("\n");
	config_scan(vme16scan, self);
}

static void
vme16scan(parent, child)
	struct device *parent;
	void *child;
{
	bus_scan(parent, child, BUS_VME16);
}

static void
vme32attach(parent, self, args)
	struct device *parent;
	struct device *self;
	void *args;
{
	printf("\n");
	config_scan(vme32scan, self);
}

static void
vme32scan(parent, child)
	struct device *parent;
	void *child;
{
	bus_scan(parent, child, BUS_VME32);
}
