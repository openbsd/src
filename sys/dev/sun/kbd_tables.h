/*	$NetBSD: kbd_tables.h,v 1.1.1.1 1996/01/24 01:15:35 gwr Exp $	*/

/*
 * Copyright (c) 1996 Gordon W. Ross
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 * 4. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Gordon Ross
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Keyboard translation tables.  These tables contain
 * "Key symbols" (or "keysyms", to use X terminology).
 * The key symbol space is divided into "classes" where
 * the high 8 bits determine the symbol class, and the
 * low 8 bits are interpreted in a way specific to each
 * class.  The simplest class is ASCII, which is defined
 * as class zero in order to simplify table definition.
 *
 * For convenience in the driver, all keysyms that
 * deserve autorepeat are below 0x8000.  The driver
 * uses the following macro to determine if a keysym
 * should NOT get autorepeat.
 */
#define KEYSYM_NOREPEAT(sym) ((sym) & 0x8000)

#define KEYSYM_CLASS(sym) ((sym) & 0xFF00)
#define KEYSYM_DATA(sym)  ((sym) & 0x00FF)

/*
 * The ACSII class.  Low 8 bits are the character.
 */
#define KEYSYM_ASCII 0

/*
 * The "special" class.
 * We don't expect to receive any of these often,
 * except for KEYSYM_NOP.  All can be ignored,
 * because HW_ERR, LAYOUT, RESET, are all handled
 * at a lower layer, before the translation code.
 */
#define KEYSYM_SPECIAL	0x8300
#define KEYSYM_NOP  	0x8300
#define KEYSYM_OOPS 	0x8301
#define KEYSYM_HOLE 	0x8302
#define KEYSYM_HW_ERR	0x8307	/* kbd sent 0x7e */
#define KEYSYM_LAYOUT	0x830e	/* kbd sent 0xfe */
#define KEYSYM_RESET	0x830f	/* kbd sent 0xff */


/*
 * Floating accents class:  (not implemented)
 * umlaut, circumflex, tilde, cedilla, acute, grave,...
 */
#define	KEYSYM_ACCENT 0x0400

/*
 * The string entry class.
 * The low 4 bits select one of the entries from
 * the string table.  (see kbd_stringtab[])
 * By default, the string table has ANSI movement
 * sequences for the arrow keys.
 */
#define	KEYSYM_STRING 0x0500

/*
 * Function keys (compatible with SunOS 4.1)
 * L:left, R:right, F:func(top), N:numeric
 */
#define	KEYSYM_FUNC   0x0600
#define	KEYSYM_FUNC_L(x) (KEYSYM_FUNC | ((x) - 1))
#define	KEYSYM_FUNC_R(x) (KEYSYM_FUNC | ((x) - 1 + 0x10))
#define	KEYSYM_FUNC_F(x) (KEYSYM_FUNC | ((x) - 1 + 0x20))
#define	KEYSYM_FUNC_N(x) (KEYSYM_FUNC | ((x) - 1 + 0x30))

/*
 * Modifier symbols, to set/clear/toggle a modifier
 * The low 5 bits define the position of a modifier
 * bit to be cleared, set, or inverted.  The meanings
 * of the modifier bit positions are defined below.
 */
#define KEYSYM_CLRMOD 0x9000
#define KEYSYM_SETMOD 0x9100
#define KEYSYM_INVMOD 0x9200
#define KEYSYM_ALL_UP 0x9300

/*
 * Modifier indices.
 * (logical OR with {CLR,SET,TOG}MOD above)
 */
#define	KBMOD_CTRL_L	0
#define	KBMOD_CTRL_R	1
#define	KBMOD_SHIFT_L	2
#define	KBMOD_SHIFT_R	3
#define	KBMOD_META_L	4
#define	KBMOD_META_R	5
#define KBMOD_ALT_L 	6
#define KBMOD_ALT_R 	7
/* Note 0-15 are cleared by ALL_UP */
#define KBMOD_CAPSLOCK  16
#define KBMOD_NUMLOCK	17

#define	KBMOD_ALTGRAPH	KBMOD_ALT_R


#ifdef	_KERNEL

#define KEYMAP_SIZE 128

struct keymap {
	unsigned short	keymap[KEYMAP_SIZE];
};

struct keyboard {
	struct keymap	*k_release; 	/* Key released */
	struct keymap	*k_control; 	/* Ctrl is down */
	struct keymap	*k_normal;  	/* No shifts */
	struct keymap	*k_shifted; 	/* Shift is down */
	/* capslock? numlock? */
};

extern char kbd_stringtab[16][10];
extern struct keyboard * keyboards[];
extern int kbd_max_type;
#define	KBD_MIN_TYPE 2

#endif	/* _KERNEL */
