/* $NetBSD: cs4231var.h,v 1.1 1995/07/07 02:11:57 brezak Exp $ */
/*
 *  Copyright (c) 1995 John T. Kohl
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR `AS IS'' AND ANY EXPRESS OR
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
 *
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
