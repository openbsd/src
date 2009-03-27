/*	$OpenBSD: iopi2cvar.h,v 1.3 2009/03/27 16:02:41 oga Exp $	*/
/*	$NetBSD: iopi2cvar.h,v 1.2 2005/12/11 12:16:51 christos Exp $	*/

/*
 * Copyright (c) 2003 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
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
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _ARM_XSCALE_IOPIICVAR_H_ 
#define _ARM_XSCALE_IOPIICVAR_H_ 

struct iopiic_softc {
	struct device sc_dev;
	bus_space_tag_t sc_st;
	bus_space_handle_t sc_sh;

	void *sc_ih;

	struct i2c_controller sc_i2c;
	struct rwlock sc_buslock;
	uint32_t sc_icr;
};

void iopiic_attach(struct iopiic_softc *sc);

#endif /* _ARM_XSCALE_IOPIICVAR_H_  */
