/*	$NetBSD$	*/
/*	$OpenBSD: if_sn_obio.c,v 1.2 1997/03/14 14:11:35 briggs Exp $	*/

/*
 * Copyright (C) 1997 Allen Briggs
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
 *      This product includes software developed by Allen Briggs
 * 4. The name of the author may not be used to endorse or promote products
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
#include <sys/device.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/systm.h>

#include <net/if.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_ether.h>
#endif

#include <machine/bus.h>
#include <machine/macinfo.h>
#include <machine/viareg.h>

#include "obiovar.h"
#include "if_snreg.h"
#include "if_snvar.h"

#define SONIC_REG_BASE	0x50F0A000
#define SONIC_PROM_BASE	0x50F08000

static int	sn_obio_match __P((struct device *, void *, void *));
static void	sn_obio_attach __P((struct device *, struct device *, void *));
static void	sn_obio_getaddr __P((struct sn_softc *));
void	snsetup __P((struct sn_softc *));

struct cfattach sn_obio_ca = {
	sizeof(struct sn_softc), sn_obio_match, sn_obio_attach
};

static int
sn_obio_match(parent, vcf, aux)
	struct device *parent;
	void *vcf;
	void *aux;
{
	if (mac68k_machine.sonic)
		return 1;

	return 0;
}

/*
 * Install interface into kernel networking data structures
 */
static void
sn_obio_attach(parent, self, aux)
	struct device *parent, *self;
	void   *aux;
{
	struct obio_attach_args *oa = (struct obio_attach_args *) aux;
        struct sn_softc *sc = (void *)self;

        sc->s_dcr = DCR_LBR | DCR_SYNC | DCR_WAIT0 |
                    DCR_DMABLOCK | DCR_RFT16 | DCR_TFT16;

        switch (current_mac_model->class) {
        case MACH_CLASSQ:
        case MACH_CLASSQ2:
                sc->s_dcr |= DCR_DW32;
		sc->bitmode = 1;
                break;

        case MACH_CLASSPB:
                sc->s_dcr |= DCR_DW16;
		sc->bitmode = 0;
                return;

	default:
		printf("unsupported machine type\n");
		return;
        }

	sc->sc_regt = oa->oa_tag;
	if (bus_space_map(sc->sc_regt, SONIC_REG_BASE, SN_REGSIZE,
				0, &sc->sc_regh)) {
		panic("failed to map space for SONIC regs.\n");
	}

	sc->slotno = 9;

	sn_obio_getaddr(sc);

	snsetup(sc);
}

static u_char bbr4[] = {0,8,4,12,2,10,6,14,1,9,5,13,3,11,7,15};
#define bbr(v)	((bbr4[(v)&0xf] << 4) | bbr4[((v)>>4) & 0xf])

static void
sn_obio_getaddr(sc)
	struct sn_softc	*sc;
{
	bus_space_handle_t	bsh;
	int			i, do_bbr;
	u_char			b;

	if (bus_space_map(sc->sc_regt, SONIC_PROM_BASE, NBPG, 0, &bsh)) {
		panic("failed to map space to read SONIC address.\n");
	}

	/*
	 * Apparently Apple goofed here.  The ethernet MAC address is
	 * stored in bit-byte-reversed format.  It is rumored that this
	 * is only true for some systems.
	 */
	do_bbr = 0;
	b = bus_space_read_1(sc->sc_regt, bsh, 0);
	if (b == 0x10)
		do_bbr = 1;
	sc->sc_arpcom.ac_enaddr[0] = (do_bbr) ? bbr(b) : b;

	for (i = 1 ; i < ETHER_ADDR_LEN ; i++) {
		b = bus_space_read_1(sc->sc_regt, bsh, i);
		sc->sc_arpcom.ac_enaddr[i] = (do_bbr) ? bbr(b) : b;
	}

	bus_space_unmap(sc->sc_regt, bsh, NBPG);
}
