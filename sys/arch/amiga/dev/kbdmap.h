/*	$NetBSD: kbdmap.h,v 1.9 1995/05/07 15:37:13 chopps Exp $	*/

/*
 * Copyright (c) 1993 Markus Wild
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
 *      This product includes software developed by Markus Wild.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
#define NUL	0
#define SOH	1
#define STX	2
#define ETX	3
#define EOT	4
#define ENQ	5
#define ACK	6
#define	BEL	7
#define BS	8
#define HT	9
#define LF	10
#define VT	11
#define FF	12
#define CR	13
#define SO	14
#define SI	15
#define DLE	16
#define DC1	17
#define DC2	18
#define DC3	19
#define DC4	20
#define NAK	21
#define SYN	22
#define ETB	23
#define CAN	24
#define EM	25
#define SUB	26
#define ESC	27
#define FS	28
#define GS	29
#define RS	30
#define US	31
#define	DEL	127
#define IND	132
#define NEL	133
#define SSA	134
#define ESA	135
#define HTS	136
#define HTJ	137
#define VTS	138
#define PLD	139
#define PLU	140
#define RI	141
#define SS2	142
#define SS3	143
#define DCS	144
#define PU1	145
#define PU2	146
#define STS	147
#define CCH	148
#define MW	149
#define SPA	150
#define EPA	151
#define CSI	155
#define ST	156
#define OSC	157
#define PM	158
#define APC	159


/* a normal Amiga keyboard has 0x60 `normal' keys, plus modifier keys that
   don't appear in the keymap */
#define KBD_NUM_KEYS	0x60

/* size of string table */
#define KBD_STRTAB_SIZE	255

/* for dead keys, index into acctable (plus 1!) */
#define	KBD_ACC_GRAVE	1
#define KBD_ACC_ACUTE	2
#define KBD_ACC_CIRC	3
#define KBD_ACC_TILDE	4
#define KBD_ACC_DIER	5
#define KBD_NUM_ACC	6


struct key {
	unsigned char	mode;	/* see possible values below */
	unsigned char	code;
};

#define KBD_MODE_STRING	(1<<0)	/* code is index into strings[] */
#define KBD_MODE_DEAD	(1<<1)	/* acc-index in upper nibble, code = plain acc */
#define KBD_MODE_CAPS	(1<<2)	/* key is capsable. Only used in non-shifted maps */
#define KBD_MODE_KPAD	(1<<3)	/* key is on keypad */
#define KBD_MODE_GRAVE	(KBD_ACC_GRAVE << 4)
#define KBD_MODE_ACUTE	(KBD_ACC_ACUTE << 4)
#define KBD_MODE_CIRC	(KBD_ACC_CIRC  << 4)
#define KBD_MODE_TILDE	(KBD_ACC_TILDE << 4)
#define KBD_MODE_DIER	(KBD_ACC_DIER  << 4)
#define KBD_MODE_ACCENT(m) (((m) >> 4) - 1)	/* get accent from mode */
#define KBD_MODE_ACCMASK  (0xf0)

struct kbdmap {
	struct key 	keys[KBD_NUM_KEYS],
		   	shift_keys[KBD_NUM_KEYS],
		   	alt_keys[KBD_NUM_KEYS],
			alt_shift_keys[KBD_NUM_KEYS];
	unsigned char	strings[KBD_STRTAB_SIZE];
};


#ifdef _KERNEL
/* XXX: ITE interface */
extern struct kbdmap kbdmap, ascii_kbdmap;
extern unsigned char acctable[KBD_NUM_ACC][64];
#endif
