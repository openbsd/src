/*	$OpenBSD: creatorvar.h,v 1.5 2002/07/29 05:53:12 jason Exp $	*/

/*
 * Copyright (c) 2002 Jason L. Wright (jason@thought.net),
 *  Federico G. Schwindt (fgsch@openbsd.org)
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

#define FFB_CREATOR		0
#define FFB_AFB			1

struct creator_softc {
	struct device sc_dv;
	bus_space_tag_t sc_bt;
	bus_space_handle_t sc_pixel_h;
	bus_space_handle_t sc_fbc_h;
	bus_addr_t sc_addrs[FFB_NREGS];
	bus_size_t sc_sizes[FFB_NREGS];
	int sc_height, sc_width, sc_linebytes, sc_depth;
	int sc_nscreens, sc_nreg;
	int sc_console;
	int sc_node;
	int sc_type;
	u_int sc_mode;
	struct rasops_info sc_rasops;
	int32_t sc_fifo_cache, sc_fg_cache;
};

#define	FBC_WRITE(sc,r,v) \
    bus_space_write_4((sc)->sc_bt, (sc)->sc_fbc_h, (r), (v))
#define	FBC_READ(sc,r) \
    bus_space_read_4((sc)->sc_bt, (sc)->sc_fbc_h, (r))

void	creator_attach(struct creator_softc *);
