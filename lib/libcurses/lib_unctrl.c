
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



#include <unctrl.h>

char *unctrl(register chtype uch)
{
    static char buffer[3] = "^x";

    if ((uch & 0x60) != 0 && uch != 0x7F) {
	/*
	 * Printable character. Simply return the character as a one-character
	 * string.
	 */
	buffer[1] = uch;
	return &buffer[1];
    }
    /*
     * It is a control character. DEL is handled specially (^?). All others
     * use ^x notation, where x is the character code for the control character
     * with 0x40 ORed in. (Control-A becomes ^A etc.).
     */
    buffer[1] = (uch == 0x7F ? '?' : (uch | 0x40));

    return buffer;

}
