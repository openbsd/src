
/***************************************************************************
*                            COPYRIGHT NOTICE                              *
****************************************************************************
*                ncurses is copyright (C) 1992-1995                        *
*                          Zeyd M. Ben-Halim                               *
*                          zmbenhal@netcom.com                             *
*                          Eric S. Raymond                                 *
*                          esr@snark.thyrsus.com                           *
*                                                                          *
*        Permission is hereby granted to reproduce and distribute ncurses  *
*        by any means and for any fee, whether alone or as part of a       *
*        larger distribution, in source or in binary form, PROVIDED        *
*        this notice is included with any such distribution, and is not    *
*        removed from any of its header files. Mention of ncurses in any   *
*        applications linked with it is highly appreciated.                *
*                                                                          *
*        ncurses comes AS IS with no warranty, implied or expressed.       *
*                                                                          *
***************************************************************************/



#include "curses.priv.h"
#include "term.h"	/* ena_acs, acs_chars */
#include <string.h>

chtype acs_map[128];

void init_acs(void)
{
	T(("initializing ACS map"));

	/*
	 * Initializations for a UNIX-like multi-terminal environment.  Use
	 * ASCII chars and count on the terminfo description to do better.
	 */
	ACS_ULCORNER = '+';	/* should be upper left corner */
	ACS_LLCORNER = '+';	/* should be lower left corner */
	ACS_URCORNER = '+';	/* should be upper right corner */
	ACS_LRCORNER = '+';	/* should be lower right corner */
	ACS_RTEE     = '+';	/* should be tee pointing left */
	ACS_LTEE     = '+';	/* should be tee pointing right */
	ACS_BTEE     = '+';	/* should be tee pointing up */
	ACS_TTEE     = '+';	/* should be tee pointing down */
	ACS_HLINE    = '-';	/* should be horizontal line */
	ACS_VLINE    = '|';	/* should be vertical line */
	ACS_PLUS     = '+';	/* should be large plus or crossover */
	ACS_S1       = '~';	/* should be scan line 1 */
	ACS_S9       = '_';	/* should be scan line 9 */
	ACS_DIAMOND  = '+';	/* should be diamond */
	ACS_CKBOARD  = ':';	/* should be checker board (stipple) */
	ACS_DEGREE   = '\'';	/* should be degree symbol */
	ACS_PLMINUS  = '#';	/* should be plus/minus */
	ACS_BULLET   = 'o';	/* should be bullet */
	ACS_LARROW   = '<';	/* should be arrow pointing left */
	ACS_RARROW   = '>';	/* should be arrow pointing right */
	ACS_DARROW   = 'v';	/* should be arrow pointing down */
	ACS_UARROW   = '^';	/* should be arrow pointing up */
	ACS_BOARD    = '#';	/* should be board of squares */
	ACS_LANTERN  = '#';	/* should be lantern symbol */
	ACS_BLOCK    = '#';	/* should be solid square block */
	/* these defaults were invented for ncurses */
	ACS_S3       = '-';	/* should be scan line 3 */  
	ACS_S7       = '-';	/* should be scan line 7 */
	ACS_LEQUAL   = '<';	/* should be less-than-or-equal-to */
	ACS_GEQUAL   = '>';	/* should be greater-than-or-equal-to */
	ACS_PI       = '*';	/* should be greek pi */
        ACS_NEQUAL   = '!';	/* should be not-equal */
        ACS_STERLING = 'f';	/* should be pound-sterling symbol */

#ifdef ena_acs
	if (ena_acs != NULL)
	{
		TPUTS_TRACE("ena_acs");
		putp(ena_acs);
	}
#endif /* ena_acs */

#ifdef acs_chars
#define ALTCHAR(c)	((chtype)(c) & A_CHARTEXT) | A_ALTCHARSET

	if (acs_chars != NULL) {
	    size_t i = 0;
	    size_t length = strlen(acs_chars);
	    
		while (i < length) 
			switch (acs_chars[i]) {
			case 'l':case 'm':case 'k':case 'j':
			case 'u':case 't':case 'v':case 'w':
			case 'q':case 'x':case 'n':case 'o':
			case 's':case '`':case 'a':case 'f':
			case 'g':case '~':case ',':case '+':
			case '.':case '-':case 'h':case 'I':
			case '0':case 'p':case 'r':case 'y':
			case 'z':case '{':case '|':case '}':
				acs_map[(unsigned int)acs_chars[i]] = 
					ALTCHAR(acs_chars[i+1]);
				i++;
				/* FALLTHRU */
			default:
				i++;
				break;
			}
	}
#ifdef TRACE
	else {
		T(("acsc not defined, using default mapping"));
	}
#endif /* TRACE */
#endif /* acs_char */
}

