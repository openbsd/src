/*	$OpenBSD: footbridgevar.h,v 1.1 2004/02/01 05:09:49 drahn Exp $	*/
/*	$NetBSD: footbridgevar.h,v 1.2 2002/02/10 12:26:00 chris Exp $	*/

/*
 * Copyright (c) 1997 Mark Brinicombe.
 * Copyright (c) 1997 Causality Limited
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
 *	This product includes software developed by Mark Brinicombe
 *	for the NetBSD Project.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <machine/bus.h>
#include <machine/rtc.h>
#include <dev/pci/pcivar.h>
#include <arm/footbridge/todclockvar.h>

/*
 * DC21285 softc structure.
 *
 * Contains the device node, bus space tag, handle and address
 */

struct footbridge_softc {
	struct device 		sc_dev;	/* device node */
	bus_space_tag_t		sc_iot;	/* bus tag */
	bus_space_handle_t	sc_ioh;	/* bus handle */

	/* Clock related variables - used in footbridge_clock.c */
	unsigned int		sc_clock_ticks_per_256us;
	unsigned int		sc_clock_count;
	void	 		*sc_clockintr;
	unsigned int		sc_statclock_count;
	void	 		*sc_statclockintr;

	/* Miscellaneous interrupts */
	void *			sc_serr_ih;
	void *			sc_sdram_par_ih;
	void *			sc_data_par_ih;
	void *			sc_master_abt_ih;
	void *			sc_target_abt_ih;
	void *			sc_parity_ih;
};

/*
 * Attach args for child devices
 */

union footbridge_attach_args {
	const char *fba_name;			/* first element*/
	struct {
		bus_space_tag_t fba_iot;	/* Bus tag */
		bus_space_handle_t fba_ioh;	/* Bus handle */
	} fba_fba;
	struct pcibus_attach_args fba_pba;	/* pci attach args */
	struct todclock_attach_args fba_tca;
	struct fcom_attach_args {
		char *fca_name;
		bus_space_tag_t fca_iot;
		bus_space_handle_t fca_ioh;
		int fca_rx_irq;
		int fca_tx_irq;
	} fba_fca;
/*	struct clock_attach_args {
		char *ca_name;
		bus_space_tag_t ca_iot;
		bus_space_handle_t ca_ioh;
	} fba_ca;*/
};

/* End of footbridgevar.h */
