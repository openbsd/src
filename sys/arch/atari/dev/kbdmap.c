/*	$NetBSD: kbdmap.c,v 1.2 1995/07/24 05:56:14 leo Exp $	*/

/*
 * Copyright (c) 1995 Leo Weppelman.
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
 *      This product includes software developed by Leo Weppelman.
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

#include <atari/dev/kbdmap.h>

/* define a default keymap. This can be changed by keyboard ioctl's 
   (later at least..) */

/* mode shortcuts: */
#define	S KBD_MODE_STRING
#define C KBD_MODE_CAPS
#define K KBD_MODE_KPAD

struct kbdmap ascii_kbdmap = {
	/* normal map */
	{
/* 0x00 */	0, 0,		0, ESC,		0, '1',		0, '2',
/* 0x04 */	0, '3',		0, '4',		0, '5',		0, '6',
/* 0x08 */	0, '7',		0, '8',		0, '9',		0, '0',
/* 0x0c */	0, '-',		0, '=',		0, '\b',	0, '\t',
/* 0x10	*/	C, 'q',		C, 'w',		C, 'e',		C, 'r',
/* 0x14 */	C, 't',		C, 'y',		C, 'u',		C, 'i',
/* 0x18 */	C, 'o',		C, 'p',		0, '[',		0, ']',
/* 0x1c */	0, '\r',	0, 0,		C, 'a',		C, 's',
/* 0x20 */	C, 'd',		C, 'f',		C, 'g',		C, 'h',
/* 0x24 */	C, 'j',		C, 'k',		C, 'l',		0, ';',
#ifdef US_KBD
/* 0x28 */	0, '\'',	0, '`',		0, 0,		0, '\\',
#else
/* 0x28 */	0, '\'',	0, '`',		0, 0,		0, '#',
#endif
/* 0x2c */	C, 'z',		C, 'x',		C, 'c',		C, 'v',
/* 0x30 */	C, 'b',		C, 'n',		C, 'm',		0, ',',
/* 0x34 */	0, '.',		0, '/',		0, 0,		0, 0,
/* 0x38 */	0, 0,		0, ' ',		0, 0,		S, 0x10,
/* 0x3c */	S, 0x15,	S, 0x1A,	S, 0x1F,	S, 0x24,
/* 0x40 */	S, 0x29,	S, 0x2E,	S, 0x33,	S, 0x38,
/* 0x44 */	S, 0x3D,	0, 0,		0, 0,		0, 0,
/* 0x48 */	S, 0x00,	0, 0,		0, '-',		S, 0x0C,
/* 0x4c */	0, 0,		S, 0x08,	0, '+',		0, 0,
/* 0x50 */	S, 0x04,	0, 0,		0, 0,		0, DEL,
/* 0x54 */	0, 0,		0, 0,		0, 0,		0, 0,
/* 0x58 */	0, 0,		0, 0,		0, 0,		0, 0,
/* 0x5c */	0, 0,		0, 0,		0, 0,		0, 0,
/* 0x60 */
#ifdef US_KBD
/* 0x60 */	0, 0,		0, 0,		0, 0,		0, '(',
#else
/* 0x60 */	0, '\\',	0, 0,		0, 0,		0, '(',
#endif
/* 0x64 */	0, ')',		0, '/',		0, '*',		K, '7',
/* 0x68 */	K, '8',		K, '9',		K, '4',		K, '5',
/* 0x6c */	K, '6',		K, '1',		K, '2',		K, '3',
/* 0x70 */	K, '0',		K, '.',		K, '\r',	0, 0,
/* 0x74 */	0, 0,		0, 0,		0, 0,		0, 0,
/* 0x78 */	0, 0,		0, 0,		0, 0,		0, 0,
/* 0x7c */	0, 0,		0, 0,		0, 0,		0, 0
},
	/* shifted map */
	{
#ifdef US_KBD
/* 0x00 */	0, 0,		0, ESC,		0, '!',		0, '@',
#else
/* 0x00 */	0, 0,		0, ESC,		0, '!',		0, '"',
#endif
/* 0x04 */	0, '#',		0, '$',		0, '%',		0, '^',
/* 0x08 */	0, '&',		0, '*',		0, '(',		0, ')',
/* 0x0c */	0, '_',		0, '+',		0, '\b',	0, '\t',
/* 0x10	*/	C, 'Q',		C, 'W',		C, 'E',		C, 'R',
/* 0x14 */	C, 'T',		C, 'Y',		C, 'U',		C, 'I',
/* 0x18 */	C, 'O',		C, 'P',		0, '{',		0, '}',
/* 0x1c */	0, '\r',	0, 0,		C, 'A',		C, 'S',
/* 0x20 */	C, 'D',		C, 'F',		C, 'G',		C, 'H',
/* 0x24 */	C, 'J',		C, 'K',		C, 'L',		0, ':',
#ifdef US_KBD
/* 0x28 */	0, '"',		0, '~',		0, 0,		0, '|',
#else
/* 0x28 */	0, '@',		0, '_',		0, 0,		0, '~',
#endif
/* 0x2c */	C, 'Z',		C, 'X',		C, 'C',		C, 'V',
/* 0x30 */	C, 'B',		C, 'N',		C, 'M',		0, '<',
/* 0x34 */	0, '>',		0, '?',		0, 0,		0, 0,
/* 0x38 */	0, 0,		0, ' ',		0, 0,		S, 0x5d,
/* 0x3c */	S, 0x63,	S, 0x69,	S, 0x6F,	S, 0x75,
/* 0x40 */	S, 0x7b,	S, 0x81,	S, 0x87,	S, 0x8d,
/* 0x44 */	S, 0x93,	0, 0,		0, 0,		0, 0,
/* 0x48 */	S, 0x47,	0, 0,		0, '-',		S, 0x57,
/* 0x4c */	0, 0,		S, 0x51,	0, '+',		0, 0,
/* 0x50 */	S, 0x4c,	0, 0,		0, 0,		0, DEL,
/* 0x54 */	0, 0,		0, 0,		0, 0,		0, 0,
/* 0x58 */	0, 0,		0, 0,		0, 0,		0, 0,
/* 0x5c */	0, 0,		0, 0,		0, 0,		0, 0,
/* 0x60 */
#ifdef US_KBD
/* 0x60 */	0, 0,		0, 0,		0, 0,		0, '(',
#else
/* 0x60 */	0, '|',		0, 0,		0, 0,		0, '(',
#endif
/* 0x64 */	0, ')',		0, '/',		0, '*',		K, '7',
/* 0x68 */	K, '8',		K, '9',		K, '4',		K, '5',
/* 0x6c */	K, '6',		K, '1',		K, '2',		K, '3',
/* 0x70 */	K, '0',		K, '.',		K, '\r',	0, 0,
/* 0x74 */	0, 0,		0, 0,		0, 0,		0, 0,
/* 0x78 */	0, 0,		0, 0,		0, 0,		0, 0,
/* 0x7c */	0, 0,		0, 0,		0, 0,		0, 0
	},
		
	/* alt map FIXME: No altmap yet.. */
	{
		0, 0
	},

	/* shift alt map FIXME: No shift altmap yet... */
	{
		0, 0
	},

	{	   
	  /* string table. If there's a better way to get the offsets into the
	     above table, please tell me..
	     
	     NOTE: save yourself and others a lot of grief by *not* using
	           CSI == 0x9b, using the two-character sequence gives
	           much less trouble, especially in GNU-Emacs.. */
	  
	  3, ESC, '[', 'A',		/* 0x00: CRSR UP */
	  3, ESC, '[', 'B',		/* 0x04: CRSR DOWN */
	  3, ESC, '[', 'C',		/* 0x08: CRSR RIGHT */
	  3, ESC, '[', 'D',		/* 0x0C: CRSR LEFT */
	  4, ESC, '[', '0', '~',	/* 0x10: F1 */
	  4, ESC, '[', '1', '~',	/* 0x15: F2 */
	  4, ESC, '[', '2', '~',	/* 0x1A: F3 */
	  4, ESC, '[', '3', '~',	/* 0x1F: F4 */
	  4, ESC, '[', '4', '~',	/* 0x24: F5 */
	  4, ESC, '[', '5', '~',	/* 0x29: F6 */
	  4, ESC, '[', '6', '~',	/* 0x2E: F7 */
	  4, ESC, '[', '7', '~',	/* 0x33: F8 */
	  4, ESC, '[', '8', '~',	/* 0x38: F9 */
	  4, ESC, '[', '9', '~',	/* 0x3D: F10 */
	  4, ESC, '[', '?', '~',	/* 0x42: HELP */

	  4, ESC, '[', 'T', '~',	/* 0x47: shift CRSR UP */
	  4, ESC, '[', 'S', '~',	/* 0x4C: shift CRSR DOWN */
	  5, ESC, '[', ' ', '@', '~',	/* 0x51: shift CRSR RIGHT */
	  5, ESC, '[', ' ', 'A', '~',	/* 0x57: shift CRSR LEFT */
	  5, ESC, '[', '1', '0', '~',	/* 0x5D: shift F1 */
	  5, ESC, '[', '1', '1', '~',	/* 0x63: shift F2 */
	  5, ESC, '[', '1', '2', '~',	/* 0x69: shift F3 */
	  5, ESC, '[', '1', '3', '~',	/* 0x6F: shift F4 */
	  5, ESC, '[', '1', '4', '~',	/* 0x75: shift F5 */
	  5, ESC, '[', '1', '5', '~',	/* 0x7B: shift F6 */
	  5, ESC, '[', '1', '6', '~',	/* 0x81: shift F7 */
	  5, ESC, '[', '1', '7', '~',	/* 0x87: shift F8 */
	  5, ESC, '[', '1', '8', '~',	/* 0x8D: shift F9 */
	  5, ESC, '[', '1', '9', '~',	/* 0x93: shift F10 */
	  3, ESC, '[', 'Z',		/* 0x99: shift TAB */
	  2, ESC, '[',			/* 0x9d: alt ESC == CSI */
	},
};

unsigned char acctable[KBD_NUM_ACC][64] = {
  {	"@ÀBCDÈFGHÌJKLMNÒPQRSTÙVWXYZ[\\]^_"
	"`àbcdèfghìjklmnòpqrstùvwxyz{|}~\177"},	/* KBD_ACC_GRAVE */
	
  {	"@ÁBCDÉFGHÍJKLMNÓPQRSTÚVWXYZ[\\]^_"
	"`ábcdéfghíjklmnópqrstúvwxyz{|}~\177"},	/* KBD_ACC_ACUTE */
	
  {	"@ÂBCDÊFGHÎJKLMNÔPQRSTÛVWXYZ[\\]^_"
	"`âbcdêfghîjklmnôpqrstûvwxyz{|}~\177"},	/* KBD_ACC_CIRC */

  {	"@ÃBCDEFGHIJKLMÑÕPQRSTUVWXYZ[\\]^_"
	"`ãbcdefghijklmñÕpqrstuvwxyz{|}~\177"},	/* KBD_ACC_TILDE */
	
  {	"@ÄBCDËFGHÏJKLMNÖPQRSTÜVWXYZ[\\]^_"
	"`äbcdëfghïjklmnöpqrstüvwxyz{|}~\177"},	/* KBD_ACC_DIER */
};

	
