/*	$OpenBSD: keymap.h,v 1.1.1.1 1996/09/07 21:40:28 downsj Exp $	*/
/* vi:set ts=4 sw=4:
 *
 * VIM - Vi IMproved		by Bram Moolenaar
 *
 * Do ":help uganda"  in Vim to read copying and usage conditions.
 * Do ":help credits" in Vim to see a list of people who contributed.
 */

#define K_CCIRCM		0x1e	/* control circumflex */

/*
 * For MSDOS some keys produce codes larger than 0xff. They are split into two
 * chars, the first one is K_NUL (same value used in term.h).
 */
#define K_NUL			(0xce)		/* for MSDOS: special key follows */

/*
 * Keycode definitions for special keys.
 *
 * Any special key code sequences are replaced by these codes.
 */

/*
 * K_SPECIAL is the first byte of a special key code and is always followed by
 * two bytes.
 * The second byte can have any value. ASCII is used for normal termcap
 * entries, 0x80 and higher for special keys, see below.
 * The third byte is guaranteed to be between 0x02 and 0x7f.
 */

#define K_SPECIAL			(0x80)

/*
 * characters 0x0000 - 0x00ff are "normal"
 * characters 0x0100 - 0x01ff are used for abbreviations
 * characters 0x0200 - 0xffff are special key codes
 */
#define IS_SPECIAL(c)		((c) >= 0x200)
#define IS_ABBR(c)			((c) >= 0x100 && (c) < 0x200)
#define ABBR_OFF			0x100

/*
 * NUL cannot be in the input string, therefore it is replaced by
 *		K_SPECIAL	KS_ZERO		K_FILLER
 */
#define KS_ZERO				255

/*
 * K_SPECIAL cannot be in the input string, therefore it is replaced by
 *		K_SPECIAL	KS_SPECIAL	K_FILLER
 */
#define KS_SPECIAL			254

/*
 * KS_EXTRA is used for keys that have no termcap name
 *		K_SPECIAL	KS_EXTRA	KE_xxx
 */
#define KS_EXTRA			253

/*
 * KS_MODIFIER is used when a modifier is given for a (special) key
 *		K_SPECIAL	KS_MODIFIER	bitmask
 */
#define KS_MODIFIER			252

/*
 * These are used for the GUI
 * 		K_SPECIAL	KS_xxx		K_FILLER
 */
#define KS_MOUSE			251
#define KS_MENU				250
#define KS_SCROLLBAR		249
#define KS_HORIZ_SCROLLBAR	248

/*
 * Filler used after KS_SPECIAL and others
 */
#define K_FILLER			('X')

/*
 * translation of three byte code "K_SPECIAL a b" into int "K_xxx" and back
 */
#define TERMCAP2KEY(a, b)	((a) + ((b) << 8))
#define KEY2TERMCAP0(x)		((x) & 0xff)
#define KEY2TERMCAP1(x)		(((x) >> 8) & 0xff)

/*
 * get second or third byte when translating special key code into three bytes
 */
#define K_SECOND(c)		((c) == K_SPECIAL ? KS_SPECIAL : (c) == NUL ? KS_ZERO : KEY2TERMCAP0(c))

#define K_THIRD(c)		(((c) == K_SPECIAL || (c) == NUL) ? K_FILLER : KEY2TERMCAP1(c))

/*
 * get single int code from second byte after K_SPECIAL
 */
#define TO_SPECIAL(a, b)	((a) == KS_SPECIAL ? K_SPECIAL : (a) == KS_ZERO ? K_ZERO : TERMCAP2KEY(a, b))

/*
 * Codes for keys that do not have a termcap name.
 *
 * K_SPECIAL KS_EXTRA KE_xxx
 */
#define KE_NAME			3			/* name of this terminal entry */

#define KE_S_UP			4
#define KE_S_DOWN		5
	
#define KE_S_F1			6			/* shifted function keys */
#define KE_S_F2			7
#define KE_S_F3			8
#define KE_S_F4			9
#define KE_S_F5			10
#define KE_S_F6			11
#define KE_S_F7			12
#define KE_S_F8			13
#define KE_S_F9			14
#define KE_S_F10		15

#define KE_S_F11		16
#define KE_S_F12		17
#define KE_S_F13		18
#define KE_S_F14		19
#define KE_S_F15		20
#define KE_S_F16		21
#define KE_S_F17		22
#define KE_S_F18		23
#define KE_S_F19		24
#define KE_S_F20		25

#define KE_S_F21		26
#define KE_S_F22		27
#define KE_S_F23		28
#define KE_S_F24		29
#define KE_S_F25		30
#define KE_S_F26		31
#define KE_S_F27		32
#define KE_S_F28		33
#define KE_S_F29		34
#define KE_S_F30		35
	
#define KE_S_F31		36
#define KE_S_F32		37
#define KE_S_F33		38
#define KE_S_F34		39
#define KE_S_F35		40

#define KE_MOUSE		41			/* mouse event start */

	/*
	 * Symbols for pseudo keys which are translated from the real key symbols
	 * above.
	 */
#define KE_LEFTMOUSE	42			/* Left mouse button click */
#define KE_LEFTDRAG		43			/* Drag with left mouse button down */
#define KE_LEFTRELEASE	44			/* Left mouse button release */
#define KE_MIDDLEMOUSE	45			/* Middle mouse button click */
#define KE_MIDDLEDRAG	46			/* Drag with middle mouse button down */
#define KE_MIDDLERELEASE 47			/* Middle mouse button release */
#define KE_RIGHTMOUSE	48			/* Right mouse button click */
#define KE_RIGHTDRAG	49			/* Drag with right mouse button down */
#define KE_RIGHTRELEASE 50			/* Right mouse button release */

#define KE_IGNORE		51			/* Ignored mouse drag/release */

#define KE_TAB			52			/* unshifted TAB key */
#define KE_S_TAB		53			/* shifted TAB key */

/*
 * the three byte codes are replaced with the following int when using vgetc()
 */
#define K_ZERO			TERMCAP2KEY(KS_ZERO, K_FILLER)

#define K_UP			TERMCAP2KEY('k', 'u')
#define K_DOWN			TERMCAP2KEY('k', 'd')
#define K_LEFT			TERMCAP2KEY('k', 'l')
#define K_RIGHT			TERMCAP2KEY('k', 'r')
#define K_S_UP			TERMCAP2KEY(KS_EXTRA, KE_S_UP)
#define K_S_DOWN		TERMCAP2KEY(KS_EXTRA, KE_S_DOWN)
#define K_S_LEFT		TERMCAP2KEY('#', '4')
#define K_S_RIGHT		TERMCAP2KEY('%', 'i')
#define K_TAB			TERMCAP2KEY(KS_EXTRA, KE_TAB)
#define K_S_TAB			TERMCAP2KEY(KS_EXTRA, KE_S_TAB)

#define K_F1			TERMCAP2KEY('k', '1')	/* function keys */
#define K_F2			TERMCAP2KEY('k', '2')
#define K_F3			TERMCAP2KEY('k', '3')
#define K_F4			TERMCAP2KEY('k', '4')
#define K_F5			TERMCAP2KEY('k', '5')
#define K_F6			TERMCAP2KEY('k', '6')
#define K_F7			TERMCAP2KEY('k', '7')
#define K_F8			TERMCAP2KEY('k', '8')
#define K_F9			TERMCAP2KEY('k', '9')
#define K_F10			TERMCAP2KEY('k', ';')

#define K_F11			TERMCAP2KEY('F', '1')
#define K_F12			TERMCAP2KEY('F', '2')
#define K_F13			TERMCAP2KEY('F', '3')
#define K_F14			TERMCAP2KEY('F', '4')
#define K_F15			TERMCAP2KEY('F', '5')
#define K_F16			TERMCAP2KEY('F', '6')
#define K_F17			TERMCAP2KEY('F', '7')
#define K_F18			TERMCAP2KEY('F', '8')
#define K_F19			TERMCAP2KEY('F', '9')
#define K_F20			TERMCAP2KEY('F', 'A')

#define K_F21			TERMCAP2KEY('F', 'B')
#define K_F22			TERMCAP2KEY('F', 'C')
#define K_F23			TERMCAP2KEY('F', 'D')
#define K_F24			TERMCAP2KEY('F', 'E')
#define K_F25			TERMCAP2KEY('F', 'F')
#define K_F26			TERMCAP2KEY('F', 'G')
#define K_F27			TERMCAP2KEY('F', 'H')
#define K_F28			TERMCAP2KEY('F', 'I')
#define K_F29			TERMCAP2KEY('F', 'J')
#define K_F30			TERMCAP2KEY('F', 'K')

#define K_F31			TERMCAP2KEY('F', 'L')
#define K_F32			TERMCAP2KEY('F', 'M')
#define K_F33			TERMCAP2KEY('F', 'N')
#define K_F34			TERMCAP2KEY('F', 'O')
#define K_F35			TERMCAP2KEY('F', 'P')

#define K_S_F1			TERMCAP2KEY(KS_EXTRA, KE_S_F1)	/* shifted func. keys */
#define K_S_F2			TERMCAP2KEY(KS_EXTRA, KE_S_F2)
#define K_S_F3			TERMCAP2KEY(KS_EXTRA, KE_S_F3)
#define K_S_F4			TERMCAP2KEY(KS_EXTRA, KE_S_F4)
#define K_S_F5			TERMCAP2KEY(KS_EXTRA, KE_S_F5)
#define K_S_F6			TERMCAP2KEY(KS_EXTRA, KE_S_F6)
#define K_S_F7			TERMCAP2KEY(KS_EXTRA, KE_S_F7)
#define K_S_F8			TERMCAP2KEY(KS_EXTRA, KE_S_F8)
#define K_S_F9			TERMCAP2KEY(KS_EXTRA, KE_S_F9)
#define K_S_F10			TERMCAP2KEY(KS_EXTRA, KE_S_F10)

#define K_S_F11			TERMCAP2KEY(KS_EXTRA, KE_S_F11)
#define K_S_F12			TERMCAP2KEY(KS_EXTRA, KE_S_F12)
#define K_S_F13			TERMCAP2KEY(KS_EXTRA, KE_S_F13)
#define K_S_F14			TERMCAP2KEY(KS_EXTRA, KE_S_F14)
#define K_S_F15			TERMCAP2KEY(KS_EXTRA, KE_S_F15)
#define K_S_F16			TERMCAP2KEY(KS_EXTRA, KE_S_F16)
#define K_S_F17			TERMCAP2KEY(KS_EXTRA, KE_S_F17)
#define K_S_F18			TERMCAP2KEY(KS_EXTRA, KE_S_F18)
#define K_S_F19			TERMCAP2KEY(KS_EXTRA, KE_S_F19)
#define K_S_F20			TERMCAP2KEY(KS_EXTRA, KE_S_F20)

#define K_S_F21			TERMCAP2KEY(KS_EXTRA, KE_S_F21)
#define K_S_F22			TERMCAP2KEY(KS_EXTRA, KE_S_F22)
#define K_S_F23			TERMCAP2KEY(KS_EXTRA, KE_S_F23)
#define K_S_F24			TERMCAP2KEY(KS_EXTRA, KE_S_F24)
#define K_S_F25			TERMCAP2KEY(KS_EXTRA, KE_S_F25)
#define K_S_F26			TERMCAP2KEY(KS_EXTRA, KE_S_F26)
#define K_S_F27			TERMCAP2KEY(KS_EXTRA, KE_S_F27)
#define K_S_F28			TERMCAP2KEY(KS_EXTRA, KE_S_F28)
#define K_S_F29			TERMCAP2KEY(KS_EXTRA, KE_S_F29)
#define K_S_F30			TERMCAP2KEY(KS_EXTRA, KE_S_F30)

#define K_S_F31			TERMCAP2KEY(KS_EXTRA, KE_S_F31)
#define K_S_F32			TERMCAP2KEY(KS_EXTRA, KE_S_F32)
#define K_S_F33			TERMCAP2KEY(KS_EXTRA, KE_S_F33)
#define K_S_F34			TERMCAP2KEY(KS_EXTRA, KE_S_F34)
#define K_S_F35			TERMCAP2KEY(KS_EXTRA, KE_S_F35)

#define K_HELP			TERMCAP2KEY('%', '1')
#define K_UNDO			TERMCAP2KEY('&', '8')

#define K_BS			TERMCAP2KEY('k', 'b')

#define K_INS			TERMCAP2KEY('k', 'I')
#define K_DEL			TERMCAP2KEY('k', 'D')
#define K_HOME			TERMCAP2KEY('k', 'h')
#define K_END			TERMCAP2KEY('@', '7')
#define K_PAGEUP		TERMCAP2KEY('k', 'P')
#define K_PAGEDOWN		TERMCAP2KEY('k', 'N')

#define K_MOUSE			TERMCAP2KEY(KS_MOUSE, K_FILLER)
#define K_MENU			TERMCAP2KEY(KS_MENU, K_FILLER)
#define K_SCROLLBAR		TERMCAP2KEY(KS_SCROLLBAR, K_FILLER)
#define K_HORIZ_SCROLLBAR	TERMCAP2KEY(KS_HORIZ_SCROLLBAR, K_FILLER)

/*
 * Symbols for pseudo keys which are translated from the real key symbols
 * above.
 */
#define K_LEFTMOUSE			TERMCAP2KEY(KS_EXTRA, KE_LEFTMOUSE)
#define K_LEFTDRAG			TERMCAP2KEY(KS_EXTRA, KE_LEFTDRAG)
#define K_LEFTRELEASE		TERMCAP2KEY(KS_EXTRA, KE_LEFTRELEASE)
#define K_MIDDLEMOUSE		TERMCAP2KEY(KS_EXTRA, KE_MIDDLEMOUSE)
#define K_MIDDLEDRAG		TERMCAP2KEY(KS_EXTRA, KE_MIDDLEDRAG)
#define K_MIDDLERELEASE		TERMCAP2KEY(KS_EXTRA, KE_MIDDLERELEASE)
#define K_RIGHTMOUSE		TERMCAP2KEY(KS_EXTRA, KE_RIGHTMOUSE)
#define K_RIGHTDRAG			TERMCAP2KEY(KS_EXTRA, KE_RIGHTDRAG)
#define K_RIGHTRELEASE		TERMCAP2KEY(KS_EXTRA, KE_RIGHTRELEASE)

#define K_IGNORE			TERMCAP2KEY(KS_EXTRA, KE_IGNORE)

/* Bits for modifier mask */
#define MOD_MASK_SHIFT		0x02
#define MOD_MASK_CTRL		0x04
#define MOD_MASK_ALT		0x08
#define MOD_MASK_2CLICK		0x10
#define MOD_MASK_3CLICK		0x20
#define MOD_MASK_4CLICK		0x40

#define MOD_MASK_MULTI_CLICK	(MOD_MASK_2CLICK|MOD_MASK_3CLICK|MOD_MASK_4CLICK) 

/*
 * The length of the longest special key name, including modifiers.
 * Current longest is <M-C-S-4-MiddleRelease> (length includes '<' and '>').
 */
#define MAX_KEY_NAME_LEN	23

/* Maximum length of a special key event as tokens.  This includes modifiers.
 * The longest event is something like <M-C-S-4-LeftDrag> which would be the
 * following string of tokens:
 *
 * <K_SPECIAL> <KS_MODIFIER> bitmask <K_SPECIAL> <KS_EXTRA> <KT_LEFTDRAG>.
 *
 * This is a total of 6 tokens, and is currently the longest one possible.
 */
#define MAX_KEY_CODE_LEN	6
