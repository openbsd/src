/* $OpenBSD: cs4231var.h,v 1.2 1996/03/08 16:42:53 niklas Exp $ */
/* $NetBSD: cs4231var.h,v 1.2 1996/02/05 02:21:51 jtc Exp $ */

/*-
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Ken Hornstein and John Kohl.
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
 *	  Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its 
 *    contributors may be used to endorse or promote products derived 
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Software gunk for CS4231, as used in Gravis UltraSound MAX.
 */

struct cs4231_softc {
    int in_port;			/* which MUX input port? */
#define CS4231_MUX_MIXED_IN	0
#define CS4231_MUX_MIC_IN	1
#define CS4231_MUX_AUX1_IN	2
#define CS4231_MUX_LINE_IN	3
};

int	cs4231_set_linein_gain __P((struct ad1848_softc *, struct ad1848_volume *));
int	cs4231_get_linein_gain __P((struct ad1848_softc *, struct ad1848_volume *));
int	cs4231_set_mono_gain __P((struct ad1848_softc *, struct ad1848_volume *));
int	cs4231_get_mono_gain __P((struct ad1848_softc *, struct ad1848_volume *));
void	cs4231_mute_mono __P((struct ad1848_softc *, int /* onoff */));
void	cs4231_mute_line __P((struct ad1848_softc *, int /* onoff */));
void	cs4231_mute_monitor __P((struct ad1848_softc *, int /* onoff */));
