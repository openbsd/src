/*	$OpenBSD: oplvar.h,v 1.1 1999/01/02 00:02:41 niklas Exp $	*/
/*	$NetBSD: oplvar.h,v 1.3 1998/11/25 22:17:06 augustss Exp $	*/

/*
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (augustss@netbsd.org).
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
 *        This product includes software developed by the NetBSD
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

#include <dev/midivar.h>
#include <dev/midisynvar.h>

struct opl_voice {
	int voiceno;
	int iooffs;
	u_int8_t op[4];
	struct opl_operators *patch;
	u_int8_t rB0;
};

struct opl_softc {
	struct midi_softc mididev;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	int	offs;
	int	model;
#define OPL_2 2
#define OPL_3 3
	struct	midisyn syn;

	struct opl_voice voices[OPL3_NVOICE];
	int volume;

	int	(*spkrctl)__P((void *, int));
	void    *spkrarg;
};

struct opl_attach_arg {
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	int offs;
	int done;
};

struct opl_operators {
	u_int8_t opl3;
	u_int8_t ops[22];
#define OO_CHARS	0
#define OO_KSL_LEV	2
#define OO_ATT_DEC	4
#define OO_SUS_REL	6
#define OO_WAV_SEL	8
#define OO_FB_CONN	10
#define OO_4OP_OFFS	11
};

#define OPL_NINSTR 256
extern struct opl_operators opl2_instrs[];
extern struct opl_operators opl3_instrs[];

int	opl_find __P((struct opl_softc *));
void	opl_attach __P((struct opl_softc *));
