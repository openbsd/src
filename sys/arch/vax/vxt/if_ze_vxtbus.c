/*	$OpenBSD: if_ze_vxtbus.c,v 1.3 2008/08/22 17:09:08 deraadt Exp $	*/
/*
 * Copyright (c) 1999 Ludd, University of Lule}, Sweden. All rights reserved.
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
 *      This product includes software developed at Ludd, University of 
 *      Lule}, Sweden and its contributors.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/nexus.h>

#include <vax/if/sgecreg.h>
#include <vax/if/sgecvar.h>

#include <vax/vxt/vxtbusvar.h>

int	ze_vxt_match(struct device *, void *, void *);
void	ze_vxt_attach(struct device *, struct device *, void *);

struct	cfattach ze_vxtbus_ca = {
	sizeof(struct ze_softc), ze_vxt_match, ze_vxt_attach
};

int
ze_vxt_match(struct device *parent, void *vcf, void	*aux)
{
	struct bp_conf *bp = aux;

	if (strcmp(bp->type, "sgec") == 0)
		return (1);
	return (0);
}

void
ze_vxt_attach(struct device *parent, struct device *self, void *aux)
{
	extern struct vax_bus_dma_tag vax_bus_dma_tag;
	struct ze_softc *sc = (void *)self;
	int *ea, i;

	/*
	 * Map in SGEC registers.
	 */
	sc->sc_ioh = vax_map_physmem(SGECADDR_VXT, 1);
	sc->sc_iot = 0; /* :-) */
	sc->sc_dmat = &vax_bus_dma_tag;

	sc->sc_intvec = VXT_INTRVEC;
	vxtbus_intr_establish(self->dv_xname, IPL_NET,
	    (int (*)(void *))sgec_intr, sc);

	/*
	 * Map in, read and release ethernet rom address.
	 */
	ea = (int *)vax_map_physmem(NISA_ROM_VXT, 1);
	for (i = 0; i < ETHER_ADDR_LEN; i++)
		sc->sc_ac.ac_enaddr[i] = ea[i] & 0xff;
	vax_unmap_physmem((vaddr_t)ea, 1);

	SET(sc->sc_flags, SGECF_VXTQUIRKS);
	sgec_attach(sc);
}
