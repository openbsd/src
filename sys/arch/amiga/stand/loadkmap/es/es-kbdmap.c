/* $OpenBSD: es-kbdmap.c,v 1.1 1999/04/18 23:26:30 espie Exp $ */
/*	$NetBSD: es-kbdmap.c,v 1.1 1998/03/13 19:28:11 is Exp $	*/

#include "../../../dev/kbdmap.h"

/* define a default keymap. This can be changed by keyboard ioctl's
   (later at least..) */

/* mode shortcuts: */
#define S KBD_MODE_STRING
#define DG (KBD_MODE_DEAD | KBD_MODE_GRAVE)
#define DA (KBD_MODE_DEAD | KBD_MODE_ACUTE)
#define DC (KBD_MODE_DEAD | KBD_MODE_CIRC)
#define DT (KBD_MODE_DEAD | KBD_MODE_TILDE)
#define DD (KBD_MODE_DEAD | KBD_MODE_DIER)
#define C KBD_MODE_CAPS
#define K KBD_MODE_KPAD

struct kbdmap kbdmap = {
	/* normal map */
	{
	   0, '`',      /* 0x00 */
	   0, '1',
	   0, '2',
	   0, '3',
	   0, '4',
	   0, '5',
	   0, '6',
	   0, '7',
	   0, '8',      /* 0x08 */
	   0, '9',
	   0, '0',
	   0, '-',
	   0, '=',
	   0, '\\',
	   0, 0,
	   K, '0',
	   C, 'q',      /* 0x10 */
	   C, 'w',
	   C, 'e',
	   C, 'r',
	   C, 't',
	   C, 'y',
	   C, 'u',
	   C, 'i',
	   C, 'o',      /* 0x18 */
	   C, 'p',
	   DA, '\'',
	   DG, '`',
	   0, 0,
	   K, '1',
	   K, '2',
	   K, '3',
	   C, 'a',      /* 0x20 */
	   C, 's',
	   C, 'd',
	   C, 'f',
	   C, 'g',
	   C, 'h',
	   C, 'j',
	   C, 'k',
	   C, 'l',      /* 0x28 */
	   C, 'ñ',
	   0, ';',
	   C, 'ç',
	   0, 0,
	   K, '4',
	   K, '5',
	   K, '6',
	   0, '<',      /* 0x30 */
	   C, 'z',
	   C, 'x',
	   C, 'c',
	   C, 'v',
	   C, 'b',
	   C, 'n',
	   C, 'm',
	   0, ',',      /* 0x38 */
	   0, '.',
	   0, '\'',
	   0, 0,
	   K, '.',
	   K, '7',
	   K, '8',
	   K, '9',
	   0, ' ',      /* 0x40 */
	   0, '\b',	/* really BS, DEL & BS not!! swapped */
	   0, '\t',
	   K, '\r',     /* enter */
	   0, '\r',     /* return */
	   0, ESC,
	   0, DEL,     /* really DEL, BS & DEL not!! swapped */
	   0, 0,
	   0, 0,	/* 0x48 */
	   0, 0,
	   K, '-',
	   0, 0,
	   S, 0x00,	/* now it gets hairy.. CRSR UP */
	   S, 0x04,	/* CRSR DOWN */
	   S, 0x08,	/* CRSR RIGHT */
	   S, 0x0C,	/* CRSR LEFT */
	   S, 0x10,	/* 0x50 F1 */
	   S, 0x15,	/* F2 */
	   S, 0x1A,	/* F3 */
	   S, 0x1F,	/* F4 */
	   S, 0x24,	/* F5 */
	   S, 0x29,	/* F6 */
	   S, 0x2E,	/* F7 */
	   S, 0x33,	/* F8 */
	   S, 0x38,	/* 0x58 F9 */
	   S, 0x3D,	/* F10 */
	   K, '[',
	   K, ']',
	   K, '/',
	   K, '*',
	   K, '+',
	   S, 0x42,	/* HELP */
	},

	/* shifted map */
	{
	   0, '~',      /* 0x00 */
	   0, '¡',
	   0, '¿',
	   0, '#',
	   0, '$',
	   0, '%',
	   0, '/',
	   0, '&',
	   0, '*',      /* 0x08 */
	   0, '(',
	   0, ')',
	   0, '_',
	   0, '+',
	   0, '|',
	   0, 0,
	   K, '0',
	   C, 'Q',      /* 0x10 */
	   C, 'W',
	   C, 'E',
	   C, 'R',
	   C, 'T',
	   C, 'Y',
	   C, 'U',
	   C, 'I',
	   C, 'O',      /* 0x18 */
	   C, 'P',
	   DD, '¨',
	   DC, '^',
	   0, 0,
	   K, '1',
	   K, '2',
	   K, '3',
	   C, 'A',      /* 0x20 */
	   C, 'S',
	   C, 'D',
	   C, 'F',
	   C, 'G',
	   C, 'H',
	   C, 'J',
	   C, 'K',
	   C, 'L',      /* 0x28 */
	   C, 'Ñ',
	   C, ':',
	   0, 'Ç',
	   0, 0,
	   K, '4',
	   K, '5',
	   K, '6',
	   0, '>',      /* 0x30 */
	   C, 'Z',
	   C, 'X',
	   C, 'C',
	   C, 'V',
	   C, 'B',
	   C, 'N',
	   C, 'M',
	   0, '?',      /* 0x38 */
	   0, '!',
	   0, '\"',
	   0, 0,
	   K, '.',
	   K, '7',
	   K, '8',
	   K, '9',
	   0, ' ',      /* 0x40 */
	   0, DEL,	/* really BS, DEL & BS swapped */
	   S, 0x99,	/* shift TAB */
	   K, '\r',     /* enter */
	   0, '\r',     /* return */
	   0, ESC,
	   0, '\b',     /* really DEL, BS & DEL swapped */
	   0, 0,
	   0, 0,	/* 0x48 */
	   0, 0,
	   K, '-',
	   0, 0,
	   S, 0x47,	/* shift CRSR UP */
	   S, 0x4C,	/* shift CRSR DOWN */
	   S, 0x51,	/* shift CRSR RIGHT */
	   S, 0x57,	/* shift CRSR LEFT */
	   S, 0x5D,	/* 0x50 shift F1 */
	   S, 0x63,	/* shift F2 */
	   S, 0x69,	/* shift F3 */
	   S, 0x6F,	/* shift F4 */
	   S, 0x75,	/* shift F5 */
	   S, 0x7B,	/* shift F6 */
	   S, 0x81,	/* shift F7 */
	   S, 0x87,	/* shift F8 */
	   S, 0x8D,	/* 0x58 shift F9 */
	   S, 0x93,	/* shift F10 */
	   K, '{',
	   K, '}',
	   K, '/',
	   K, '*',
	   K, '+',
	   S, 0x42,	/* HELP (no special shift code) */
	},


	/* alt map */
	{
	   0, '`',      /* 0x00 */
	   0, '¹',
	   0, '²',
	   0, '³',
	   0, '¢',
	   0, '¼',
	   0, '½',
	   0, '¾',
	   0, '·',      /* 0x08 */
	   0, '«',
	   0, '»',
	   0, '-',
	   0, '=',
	   0, '\\',
	   0, 0,
	   K, '0',
	   C, 'å',      /* 0x10 */
	   0, '°',
	   0, '©',
	   0, '®',
	   C, 'þ',
	   0, '¤',
	   0, 'µ',
	   0, '¡',
	   C, 'ø',      /* 0x18 */
	   0, '¶',
	   0, '[',
	   0, ']',
	   0, 0,
	   K, '1',
	   K, '2',
	   K, '3',
	   C, 'æ',      /* 0x20 */
	   0, 'ß',
	   C, 'ð',
	   DA, '\'',
	   DG, '`',
	   DC, '^',
	   DT, '~',
	   DD, '¨',
	   0, '£',      /* 0x28 */
	   0, ';',
	   0, '\'',
	   0, 'ª',
	   0, 0,
	   K, '4',
	   K, '5',
	   K, '6',
	   0, '«',	/* 0x30 */
	   0, '±',
	   0, '×',
	   C, 'ç',
	   0, 'ª',
	   0, 'º',
	   0, '­',
	   0, '¸',
	   0, ',',      /* 0x38 */
	   0, '.',
	   0, '/',
	   0, 0,
	   K, '.',
	   K, '7',
	   K, '8',
	   K, '9',
	   0, ' ',      /* 0x40 */
	   0, '\b',	/* really BS, DEL & BS not!! swapped */
	   0, '\t',
	   K, '\r',     /* enter */
	   0, '\r',     /* return */
	   S, 0x9d,	/* CSI */
	   0, DEL,     /* really DEL, BS & DEL not!! swapped */
	   0, 0,
	   0, 0,	/* 0x48 */
	   0, 0,
	   K, '-',
	   0, 0,
	   S, 0x00,	/* now it gets hairy.. CRSR UP */
	   S, 0x04,	/* CRSR DOWN */
	   S, 0x08,	/* CRSR RIGHT */
	   S, 0x0C,	/* CRSR LEFT */
	   S, 0x10,	/* 0x50 F1 */
	   S, 0x15,	/* F2 */
	   S, 0x1A,	/* F3 */
	   S, 0x1F,	/* F4 */
	   S, 0x24,	/* F5 */
	   S, 0x29,	/* F6 */
	   S, 0x2E,	/* F7 */
	   S, 0x33,	/* F8 */
	   S, 0x38,	/* 0x58 F9 */
	   S, 0x3D,	/* F10 */
	   K, '[',
	   K, ']',
	   K, '/',
	   K, '*',
	   K, '+',
	   S, 0x42,	/* HELP */
	},

	/* shift alt map */
	{
	   0, '~',      /* 0x00 */
	   0, '!',
	   0, '@',
	   0, '#',
	   0, '°',
	   0, '%',
	   0, '^',
	   0, '&',
	   0, '*',      /* 0x08 */
	   0, '(',
	   0, ')',
	   0, '_',
	   0, '+',
	   0, '|',
	   0, 0,
	   K, '0',
	   C, 'Å',      /* 0x10 */
	   0, '°',
	   0, '©',
	   0, '®',
	   C, 'Þ',
	   0, '¥',
	   0, 'µ',
	   0, '¦',
	   C, 'Ø',      /* 0x18 */
	   0, '¶',
	   0, '{',
	   0, '}',
	   0, 0,
	   K, '1',
	   K, '2',
	   K, '3',
	   C, 'Æ',      /* 0x20 */
	   0, '§',
	   C, 'Ð',
	   DA, '\'',
	   DG, '`',
	   DC, '^',
	   DT, '~',
	   DD, '¨',
	   0, '£',      /* 0x28 */
	   0, ':',
	   0, '"',
	   0, 'º',
	   0, 0,
	   K, '4',
	   K, '5',
	   K, '6',
	   0, '»',	/* 0x30 */
	   0, '¬',
	   0, '÷',
	   C, 'Ç',
	   0, 'ª',
	   0, 'º',
	   0, '¯',
	   0, '¿',
	   0, '<',      /* 0x38 */
	   0, '>',
	   0, '?',
	   0, 0,
	   K, '.',
	   K, '7',
	   K, '8',
	   K, '9',
	   0, ' ',      /* 0x40 */
	   0, DEL,	/* really BS, DEL & BS swapped */
	   0, '\t',
	   K, '\r',     /* enter */
	   0, '\r',     /* return */
	   S, 0x9d,	/* CSI */
	   0, '\b',     /* really DEL, BS & DEL swapped */
	   0, 0,
	   0, 0,	/* 0x48 */
	   0, 0,
	   K, '-',
	   0, 0,
	   S, 0x00,	/* now it gets hairy.. CRSR UP */
	   S, 0x04,	/* CRSR DOWN */
	   S, 0x08,	/* CRSR RIGHT */
	   S, 0x0C,	/* CRSR LEFT */
	   S, 0x10,	/* 0x50 F1 */
	   S, 0x15,	/* F2 */
	   S, 0x1A,	/* F3 */
	   S, 0x1F,	/* F4 */
	   S, 0x24,	/* F5 */
	   S, 0x29,	/* F6 */
	   S, 0x2E,	/* F7 */
	   S, 0x33,	/* 0x58 F8 */
	   S, 0x38,	/* F9 */
	   S, 0x3D,	/* F10 */
	   K, '{',
	   K, '}',
	   K, '/',
	   K, '*',
	   K, '+',
	   S, 0x42,	/* HELP */
	},

	{
	  /* string table. If there's a better way to get the offsets into the
	     above table, please tell me..

	     NOTE: save yourself and others a lot of grief by *not* using
		   CSI == 0x9b, using the two-character sequence gives
		   much less trouble, especially in GNU-Emacs.. */

	  3, ESC, '[', 'A',             /* 0x00: CRSR UP */
	  3, ESC, '[', 'B',             /* 0x04: CRSR DOWN */
	  3, ESC, '[', 'C',             /* 0x08: CRSR RIGHT */
	  3, ESC, '[', 'D',             /* 0x0C: CRSR LEFT */
	  4, ESC, '[', '0', '~',        /* 0x10: F1 */
	  4, ESC, '[', '1', '~',        /* 0x15: F2 */
	  4, ESC, '[', '2', '~',        /* 0x1A: F3 */
	  4, ESC, '[', '3', '~',        /* 0x1F: F4 */
	  4, ESC, '[', '4', '~',        /* 0x24: F5 */
	  4, ESC, '[', '5', '~',        /* 0x29: F6 */
	  4, ESC, '[', '6', '~',        /* 0x2E: F7 */
	  4, ESC, '[', '7', '~',        /* 0x33: F8 */
	  4, ESC, '[', '8', '~',        /* 0x38: F9 */
	  4, ESC, '[', '9', '~',        /* 0x3D: F10 */
	  4, ESC, '[', '?', '~',        /* 0x42: HELP */

	  4, ESC, '[', 'T', '~',        /* 0x47: shift CRSR UP */
	  4, ESC, '[', 'S', '~',        /* 0x4C: shift CRSR DOWN */
	  5, ESC, '[', ' ', '@', '~',   /* 0x51: shift CRSR RIGHT */
	  5, ESC, '[', ' ', 'A', '~',   /* 0x57: shift CRSR LEFT */
	  5, ESC, '[', '1', '0', '~',   /* 0x5D: shift F1 */
	  5, ESC, '[', '1', '1', '~',   /* 0x63: shift F2 */
	  5, ESC, '[', '1', '2', '~',   /* 0x69: shift F3 */
	  5, ESC, '[', '1', '3', '~',   /* 0x6F: shift F4 */
	  5, ESC, '[', '1', '4', '~',   /* 0x75: shift F5 */
	  5, ESC, '[', '1', '5', '~',   /* 0x7B: shift F6 */
	  5, ESC, '[', '1', '6', '~',   /* 0x81: shift F7 */
	  5, ESC, '[', '1', '7', '~',   /* 0x87: shift F8 */
	  5, ESC, '[', '1', '8', '~',   /* 0x8D: shift F9 */
	  5, ESC, '[', '1', '9', '~',   /* 0x93: shift F10 */
	  3, ESC, '[', 'Z',             /* 0x99: shift TAB */
	  2, ESC, '[',                  /* 0x9D: alt ESC == CSI */
	},
};

unsigned char acctable[KBD_NUM_ACC][64] = {
  {	"@ÀBCDÈFGHÌJKLMNÒPQRSTÙVWXYZ[\\]^_"
	"`àbcdèfghìjklmnòpqrstùvwxyz{|}~\177"}, /* KBD_ACC_GRAVE */

  {	"@ÁBCDÉFGHÍJKLMNÓPQRSTÚVWXÝZ[\\]^_"
	"`ábcdéfghíjklmnópqrstúvwxýz{|}~\177"}, /* KBD_ACC_ACUTE */

  {	"@ÂBCDÊFGHÎJKLMNÔPQRSTÛVWXYZ[\\]^_"
	"`âbcdêfghîjklmnôpqrstûvwxyz{|}~\177"}, /* KBD_ACC_CIRC */

  {	"@ÃBCDEFGHIJKLMÑÕPQRSTUVWXYZ[\\]^_"
	"`ãbcdefghijklmñÕpqrstuvwxyz{|}~\177"}, /* KBD_ACC_TILDE */

  {	"@ÄBCDËFGHÏJKLMNÖPQRSTÜVWXYZ[\\]^_"
	"`äbcdëfghïjklmnöpqrstüvwxÿz{|}~\177"}, /* KBD_ACC_DIER */
};


main()
{
  write (1, &kbdmap, sizeof (kbdmap));
}
