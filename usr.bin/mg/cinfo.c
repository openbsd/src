/*
 *		Character class tables.
 * Do it yourself character classification
 * macros, that understand the multinational character set,
 * and let me ask some questions the standard macros (in
 * ctype.h) don't let you ask.
 */
#include	"def.h"

/*
 * This table, indexed by a character drawn
 * from the 256 member character set, is used by my
 * own character type macros to answer questions about the
 * type of a character. It handles the full multinational
 * character set, and lets me ask some questions that the
 * standard "ctype" macros cannot ask.
 */
char	cinfo[256] = {
	_C,		_C,		_C,		_C,	/* 0x0X */
	_C,		_C,		_C,		_C,
	_C,		_C,		_C,		_C,
	_C,		_C,		_C,		_C,
	_C,		_C,		_C,		_C,	/* 0x1X */
	_C,		_C,		_C,		_C,
	_C,		_C,		_C,		_C,
	_C,		_C,		_C,		_C,
	0,		_P,		0,		0,	/* 0x2X */
	_W,		_W,		0,		_W,
	0,		0,		0,		0,
	0,		0,		_P,		0,
	_D|_W,		_D|_W,		_D|_W,		_D|_W,	/* 0x3X */
	_D|_W,		_D|_W,		_D|_W,		_D|_W,
	_D|_W,		_D|_W,		0,		0,
	0,		0,		0,		_P,
	0,		_U|_W,		_U|_W,		_U|_W,	/* 0x4X */
	_U|_W,		_U|_W,		_U|_W,		_U|_W,
	_U|_W,		_U|_W,		_U|_W,		_U|_W,
	_U|_W,		_U|_W,		_U|_W,		_U|_W,
	_U|_W,		_U|_W,		_U|_W,		_U|_W,	/* 0x5X */
	_U|_W,		_U|_W,		_U|_W,		_U|_W,
	_U|_W,		_U|_W,		_U|_W,		0,
	0,		0,		0,		0,
	0,		_L|_W,		_L|_W,		_L|_W,	/* 0x6X */
	_L|_W,		_L|_W,		_L|_W,		_L|_W,
	_L|_W,		_L|_W,		_L|_W,		_L|_W,
	_L|_W,		_L|_W,		_L|_W,		_L|_W,
	_L|_W,		_L|_W,		_L|_W,		_L|_W,	/* 0x7X */
	_L|_W,		_L|_W,		_L|_W,		_L|_W,
	_L|_W,		_L|_W,		_L|_W,		0,
	0,		0,		0,		_C,
	0,		0,		0,		0,	/* 0x8X */
	0,		0,		0,		0,
	0,		0,		0,		0,
	0,		0,		0,		0,
	0,		0,		0,		0,	/* 0x9X */
	0,		0,		0,		0,
	0,		0,		0,		0,
	0,		0,		0,		0,
	0,		0,		0,		0,	/* 0xAX */
	0,		0,		0,		0,
	0,		0,		0,		0,
	0,		0,		0,		0,
	0,		0,		0,		0,	/* 0xBX */
	0,		0,		0,		0,
	0,		0,		0,		0,
	0,		0,		0,		0,
	_U|_W,		_U|_W,		_U|_W,		_U|_W,	/* 0xCX */
	_U|_W,		_U|_W,		_U|_W,		_U|_W,
	_U|_W,		_U|_W,		_U|_W,		_U|_W,
	_U|_W,		_U|_W,		_U|_W,		_U|_W,
	0,		_U|_W,		_U|_W,		_U|_W,	/* 0xDX */
	_U|_W,		_U|_W,		_U|_W,		_U|_W,
	_U|_W,		_U|_W,		_U|_W,		_U|_W,
	_U|_W,		_U|_W,		0,		_W,
	_L|_W,		_L|_W,		_L|_W,		_L|_W,	/* 0xEX */
	_L|_W,		_L|_W,		_L|_W,		_L|_W,
	_L|_W,		_L|_W,		_L|_W,		_L|_W,
	_L|_W,		_L|_W,		_L|_W,		_L|_W,
	0,		_L|_W,		_L|_W,		_L|_W,	/* 0xFX */
	_L|_W,		_L|_W,		_L|_W,		_L|_W,
	_L|_W,		_L|_W,		_L|_W,		_L|_W,
	_L|_W,		_L|_W,		0,		0
};

/*
 * Find the name of a keystroke.  Needs to be changed to handle 8-bit printing
 * characters and function keys better.	 Returns a pointer to the terminating
 * '\0'.
 */

char *keyname(cp, k)
register char *cp;
register int k;
{
    register char *np;
#ifdef	FKEYS
    extern char *keystrings[];
#endif

    if(k < 0) k = CHARMASK(k);			/* sign extended char */
    switch(k) {
	case CCHR('@'): np = "NUL"; break;
	case CCHR('I'): np = "TAB"; break;
	case CCHR('J'): np = "LFD"; break; /* yuck, but that's what GNU calls it */
	case CCHR('M'): np = "RET"; break;
	case CCHR('['): np = "ESC"; break;
	case ' ':	np = "SPC"; break; /* yuck again */
	case CCHR('?'): np = "DEL"; break;
	default:
#ifdef	FKEYS
	    if(k >= KFIRST && k <= KLAST &&
		    (np = keystrings[k - KFIRST]) != NULL)
		break;
#endif
	    if(k > CCHR('?')) {
		*cp++ = '0';
		*cp++ = ((k>>6)&7) + '0';
		*cp++ = ((k>>3)&7) + '0';
		*cp++ = (k&7) + '0';
		*cp = '\0';
		return cp;
	    }
	    if(k < ' ') {
		*cp++ = 'C';
		*cp++ = '-';
		k = CCHR(k);
		if(ISUPPER(k)) k = TOLOWER(k);
	    }
	    *cp++ = k;
	    *cp = '\0';
	    return cp;
    }
    (VOID) strcpy(cp, np);
    return cp + strlen(cp);
}
