/*	$OpenBSD: lib_slkset.c,v 1.1 1997/12/03 05:21:35 millert Exp $	*/


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

/*
 *	lib_slkset.c
 *      Set soft label text.
 */
#include <curses.priv.h>
#include <ctype.h>

MODULE_ID("Id: lib_slkset.c,v 1.2 1997/10/18 18:09:27 tom Exp $")

int
slk_set(int i, const char *astr, int format)
{
SLK *slk = SP->_slk;
size_t len;
const char *str = astr;
const char *p;

	T((T_CALLED("slk_set(%d, \"%s\", %d)"), i, str, format));

	if (slk == NULL || i < 1 || i > slk->labcnt || format < 0 || format > 2)
		returnCode(ERR);
	if (str == NULL)
		str = "";

	while (isspace(*str)) str++; /* skip over leading spaces  */
	p = str;
	while (isprint(*p)) p++;     /* The first non-print stops */

	--i; /* Adjust numbering of labels */

	len = (size_t)(p - str);
	if (len > (unsigned)slk->maxlen)
	  len = slk->maxlen;
	if (len==0)
	  slk->ent[i].text[0] = 0;
	else
	  (void) strncpy(slk->ent[i].text, str, len);
	memset(slk->ent[i].form_text,' ', (unsigned)slk->maxlen);
	slk->ent[i].text[slk->maxlen] = 0;
	/* len = strlen(slk->ent[i].text); */

	switch(format) {
	case 0: /* left-justified */
		memcpy(slk->ent[i].form_text,
		       slk->ent[i].text,
		       len);
		break;
	case 1: /* centered */
		memcpy(slk->ent[i].form_text+(slk->maxlen - len)/2,
		       slk->ent[i].text,
		       len);
		break;
	case 2: /* right-justified */
		memcpy(slk->ent[i].form_text+ slk->maxlen - len,
		       slk->ent[i].text,
		       len);
		break;
	}
	slk->ent[i].form_text[slk->maxlen] = 0;
	slk->ent[i].dirty = TRUE;
	returnCode(OK);
}
