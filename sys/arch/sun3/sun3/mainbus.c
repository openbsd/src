/*	$NetBSD: mainbus.c,v 1.1 1996/03/26 15:03:58 gwr Exp $	*/

/*
 * Copyright (c) 1996 Gordon W. Ross
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 * 4. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Gordon Ross
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

static int 	main_match __P((struct device *, void *, void *));
static void	main_attach __P((struct device *, struct device *, void *));

struct cfattach mainbus_ca = {
	sizeof(struct device), main_match, main_attach
};

struct cfdriver mainbus_cd = {
	NULL, "mainbus", DV_DULL
};

/*
 * Probe for the mainbus; always succeeds.
 */
static int
main_match(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{

	return 1;
}

/*
 * Do "direct" configuration for the bus types on mainbus.
 * This controls the order of autoconfig for important things
 * used early.  For example, idprom is used by Ether drivers.
 */
static int bus_order[] = {
	BUS_OBIO,	/* eeprom, clock */
	BUS_OBMEM,
	BUS_VME16,
	BUS_VME32
};
#define BUS_ORDER_SZ (sizeof(bus_order)/sizeof(bus_order[0]))

static void
main_attach(parent, self, args)
	struct device *parent;
	struct device *self;
	void *args;
{
	struct confargs ca;
	struct cfdata *new_match;
	int i;

	printf("\n");

	for (i = 0; i < BUS_ORDER_SZ; i++) {
		ca.ca_bustype = bus_order[i];
		(void) config_found(self, &ca, NULL);
	}
}
