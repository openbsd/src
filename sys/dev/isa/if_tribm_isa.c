/*	$OpenBSD: if_tribm_isa.c,v 1.2 2001/11/05 17:25:58 art Exp $	*/
/*	$NetBSD: if_tribm_isa.c,v 1.2 1999/03/22 23:01:37 bad Exp $	*/

/*
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Onno van der Linden.
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
 *        This product includes software developed by The NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its 
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <sys/device.h>
#include <sys/timeout.h>

#include <net/if.h>
#include <net/if_media.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <machine/bus.h>

#include <dev/isa/isavar.h>

#include <dev/ic/tropicreg.h>
#include <dev/ic/tropicvar.h>

int	tribm_isa_probe __P((struct device *, void *, void *));
int	tr_isa_map_io __P((struct isa_attach_args *, bus_space_handle_t *,
	    bus_space_handle_t *));
void	tr_isa_unmap_io __P((struct isa_attach_args *, bus_space_handle_t,
	    bus_space_handle_t));

int
tribm_isa_probe(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct isa_attach_args *ia = aux;
	static int irq_f[4] = { 9, 3, 6, 7 };
	static int irq_e[4] = { 9, 3, 10, 11 };
	bus_space_tag_t piot = ia->ia_iot;
	bus_space_tag_t memt = ia->ia_memt;
	bus_space_handle_t pioh, mmioh;
	int i, irq;
	u_int8_t s;

#ifdef notyet
/* XXX Try both 0xa20 and 0xa24 and store that info like 3com */
	if (ia->ia_iobase == IOBASEUNK)
		ia->ia_iobase = 0xa20;
#else
	if (ia->ia_iobase == IOBASEUNK)
		return 0;
#endif

	ia->ia_iosize = 4;
	ia->ia_aux = NULL;

	if (tr_isa_map_io(ia, &pioh, &mmioh))
		return 0;

/*
 * F = Token-Ring Network PC Adapter
 *     Token-Ring Network PC Adapter II
 *     Token-Ring Network Adapter/A
 * E = Token-Ring Network 16/4 Adapter/A (long card)
 *     Token-Ring Network 16/4 Adapter
 * D = Token-Ring Network 16/4 Adapter/A (short card)
 *     16/4 ISA-16 Adapter
 * C = Auto 16/4 Token-Ring ISA Adapter
 *     Auto 16/4 Token-Ring MC Adapter
 */
/*
 * XXX Both 0xD and 0xC types should be able to use 16-bit read and writes
 */
	switch (bus_space_read_1(memt, mmioh, TR_TYP_OFFSET)) {
	case 0xF:
	case 0xE:
	case 0xD:
		if (ia->ia_maddr == MADDRUNK)
#ifdef notyet
			ia->ia_maddr = TR_SRAM_DEFAULT;
#else
			return 0;
#endif
		break;
	case 0xC:
		i = bus_space_read_1(memt, mmioh, TR_ACA_OFFSET) << 12;
		if (ia->ia_maddr == MADDRUNK)
			ia->ia_maddr = i;
		else if (ia->ia_maddr != i) {
			printf(
"tribm_isa_probe: sram mismatch; kernel configured %x != board configured %x\n",
				ia->ia_maddr, i);
			tr_isa_unmap_io(ia, pioh, mmioh);
			return 0;
		}
		break;
	default:
		printf("tribm_isa_probe: unknown type code %x\n",
		    bus_space_read_1(memt, mmioh, TR_TYP_OFFSET));
		tr_isa_unmap_io(ia, pioh, mmioh);
		return 0;
	}

	s = bus_space_read_1(piot, pioh, TR_SWITCH);

	switch (bus_space_read_1(memt, mmioh, TR_IRQ_OFFSET)) {
	case 0xF:
		irq = irq_f[s & 3];
		break;
	case 0xE:
		irq = irq_e[s & 3];
		break;
	default:
		printf("tribm_isa_probe: Unknown IRQ code %x\n",
		    bus_space_read_1(memt, mmioh, TR_IRQ_OFFSET));
		tr_isa_unmap_io(ia, pioh, mmioh);
		return 0;
	}

	if (ia->ia_irq == IRQUNK)
		ia->ia_irq = irq;
	else if (ia->ia_irq != irq) {
		printf(
"tribm_isa_probe: irq mismatch; kernel configured %d != board configured %d\n",
			ia->ia_irq, irq);
		tr_isa_unmap_io(ia, pioh, mmioh);
		return 0;
	}
/*
 * XXX 0x0c == MSIZEMASK (MSIZEBITS)
 */
	ia->ia_msize = 8192 <<
	    ((bus_space_read_1(memt, mmioh, TR_ACA_OFFSET + 1) & 0x0c) >> 2);
	tr_isa_unmap_io(ia, pioh, mmioh);
	/* Check alignment of membase. */
	if ((ia->ia_maddr & (ia->ia_msize-1)) != 0) {
		printf("tribm_isa_probe: SRAM unaligned 0x%04x/%d\n",
		    ia->ia_maddr, ia->ia_msize);
		return 0;
	}
 	return 1;
}
