/*	$OpenBSD: nofnvar.h,v 1.1 2002/01/07 23:16:38 jason Exp $	*/

/*
 * Copyright (c) 2002 Jason L. Wright (jason@thought.net)
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
 *	This product includes software developed by Jason L. Wright
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */


struct nofn_softc {
	struct device sc_dv;		/* us, as a device */
	void *sc_ih;			/* interrupt vectoring */
	bus_space_handle_t sc_sh0;	/* group 0 handle */
	bus_space_tag_t sc_st0;		/* group 0 tag */
	bus_space_handle_t sc_sh1;	/* group 1 handle */
	bus_space_tag_t sc_st1;		/* group 1 tag */
	bus_space_handle_t sc_sh2;	/* gpram handle */
	bus_space_tag_t sc_st2;		/* gpram tag */
	pci_chipset_tag_t sc_pci_pc;	/* pci config space */
	pcitag_t sc_pci_tag;		/* pci config space tag */
	struct timeout sc_rngto;	/* rng timeout */
	int sc_rnghz;			/* rng ticks */
};

#define	G0_READ_4(sc,reg) \
    bus_space_read_4((sc)->sc_st0, (sc)->sc_sh0, (reg))
#define	G0_WRITE_4(sc,reg, val) \
    bus_space_write_4((sc)->sc_st0, (sc)->sc_sh0, (reg), (val))
#define	G1_READ_4(sc,reg) \
    bus_space_read_4((sc)->sc_st1, (sc)->sc_sh1, (reg))
#define	G1_WRITE_4(sc,reg, val) \
    bus_space_write_4((sc)->sc_st1, (sc)->sc_sh1, (reg), (val))
