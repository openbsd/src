/*	$NetBSD: kbd_xlate.h,v 1.1.1.1 1996/01/24 01:15:35 gwr Exp $	*/

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)kbd.c	8.2 (Berkeley) 10/30/93
 */

/*
 * This defines the interface provided by kbd_xlate()
 */

#include <dev/sun/kbd_tables.h>

#define	KEY_CODE(c)	((c) & KBD_KEYMASK)	/* keyboard code index */
#define	KEY_UP(c)	((c) & KBD_UP)		/* true => key went up */

#define KBMOD_CTRL_MASK 	((1 << KBMOD_CTRL_L)  | (1 << KBMOD_CTRL_R))
#define KBMOD_SHIFT_MASK	((1 << KBMOD_SHIFT_L) | (1 << KBMOD_SHIFT_R))
#define KBMOD_META_MASK 	((1 << KBMOD_META_L)  | (1 << KBMOD_META_R))
#define KBMOD_ALT_MASK  	((1 << KBMOD_ALT_L)   | (1 << KBMOD_ALT_R))


/*
 * Keycode translator state.
 * We need to remember the state of the keyboard's shift and
 * control keys, and we need a per-type translation table.
 */
struct kbd_state {
	struct keyboard kbd_k;	/* table pointers */
	int	kbd_modbits;		/* modifier keys */
	int kbd_expect; 		/* expect ID or layout byte */
#define	KBD_EXPECT_IDCODE	1
#define	KBD_EXPECT_LAYOUT	2

	u_char	kbd_id;		/* a place to store the ID */
	u_char	kbd_layout;	/* which keyboard layout */
	u_char	kbd_click;	/* true => keyclick enabled */
	u_char	kbd_leds;	/* LED state */

};

extern void kbd_xlate_init __P((struct kbd_state *ks));
extern int kbd_code_to_keysym __P((struct kbd_state *ks, int c));

