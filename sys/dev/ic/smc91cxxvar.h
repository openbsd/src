/*	$OpenBSD: smc91cxxvar.h,v 1.3 2000/02/02 19:09:58 fgsch Exp $	*/
/*	$NetBSD: smc91cxxvar.h,v 1.4 1997/10/15 05:56:13 explorer Exp $	*/

/*-
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
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

#if NRND > 0
#include <sys/rnd.h>
#endif

struct smc91cxx_softc {
	struct	device sc_dev;		/* generic device glue */
#ifdef __NetBSD__
	struct	ethercom sc_ethercom;	/* ethernet common glue */
#endif
	struct	arpcom sc_arpcom;	/* ethernet common glue */

	bus_space_tag_t sc_bst;		/* bus space */
	bus_space_handle_t sc_bsh;

	struct	ifmedia sc_media;	/* our media info */

	/* Power management hooks and state. */
	int	(*sc_enable) __P((struct smc91cxx_softc *));
	void	(*sc_disable) __P((struct smc91cxx_softc *));
	int	sc_enabled;

#if NRND > 0
	rndsource_element_t rnd_source;
#endif
};

#define	SMC_SELECT_BANK(sc, x)						\
	bus_space_write_2((sc)->sc_bst, (sc)->sc_bsh,			\
	    BANK_SELECT_REG_W, (x))

void	smc91cxx_attach __P((struct smc91cxx_softc *, u_int8_t *));
int	smc91cxx_intr __P((void *));
void	smc91cxx_init __P((struct smc91cxx_softc *));
void	smc91cxx_stop __P((struct smc91cxx_softc *));
