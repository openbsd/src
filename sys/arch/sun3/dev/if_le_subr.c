/*	$NetBSD: if_le_subr.c,v 1.12 1995/09/26 04:02:05 gwr Exp $	*/

/*
 * Copyright (c) 1994 Gordon W. Ross
 * Copyright (c) 1993 Adam Glass
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
 *	This product includes software developed by Adam Glass.
 * 4. The name of the Author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Adam Glass ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Adam Glass BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Machine-dependent glue for the LANCE Ethernet (le) driver.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <net/if.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#endif

#include <machine/autoconf.h>
#include <machine/cpu.h>
#include <machine/dvma.h>
#include <machine/isr.h>
#include <machine/obio.h>
#include <machine/idprom.h>

#include "if_lereg.h"
#include "if_le.h"
#include "if_le_subr.h"

int
le_md_match(parent, vcf, args)
	struct device *parent;
	void *vcf, *args;
{
	struct cfdata *cf = vcf;
	struct confargs *ca = args;
	int x;

	if (ca->ca_paddr == -1)
		ca->ca_paddr = OBIO_AMD_ETHER;
	if (ca->ca_intpri == -1)
		ca->ca_intpri = 3;

	/* The peek returns -1 on bus error. */
	x = bus_peek(ca->ca_bustype, ca->ca_paddr, 1);
	return (x != -1);
}

void
le_md_attach(parent, self, args)
	struct device *parent;
	struct device *self;
	void *args;
{
	struct le_softc *sc = (void *) self;
	struct confargs *ca = args;
	caddr_t p;

	/* register access */
	sc->sc_regs = (struct le_regs *)
		obio_alloc(ca->ca_paddr, OBIO_AMD_ETHER_SIZE);
	if (sc->sc_regs == NULL)
		panic(": not enough obio space\n");

	/* allocate "shared" memory */
	sc->sc_mem = dvma_malloc(MEMSIZE);
	if (sc->sc_mem == NULL)
		panic(": not enough dvma space");

	/* Install interrupt handler. */
	isr_add_autovect(leintr, (void *)sc, ca->ca_intpri);
	idprom_etheraddr(sc->sc_enaddr); /* ethernet addr */
}
