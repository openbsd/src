
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
 *	lib_traceatr.c - Tracing/Debugging routines (attributes)
 */

#ifndef TRACE
#define TRACE			/* turn on internal defs for this module */
#endif

#include <curses.priv.h>
#include <term.h>	/* acs_chars */

MODULE_ID("Id: lib_traceatr.c,v 1.20 1997/05/06 11:07:27 tom Exp $")

#define COLOR_OF(c) (c < 0 || c > 7 ? "default" : colors[c].name)

char * _nc_trace_buf(int bufnum, size_t want)
{
	static struct {
		char *text;
		size_t size;
	} *list;
	static size_t have;

	if (bufnum < 0)
		bufnum = 0;

	if ((size_t)(bufnum+1) > have) {
		size_t need = (bufnum + 1) * 2;
		size_t used = sizeof(*list) * need;
		list = (list == 0) ? malloc(used) : realloc(list, used);
		while (need > have)
			list[have++].text = 0;
	}

	if (list[bufnum].text == 0)
	{
		list[bufnum].text = malloc(want);
		list[bufnum].size = want;
	}
	else if (want > list[bufnum].size) {
		list[bufnum].text = realloc(list[bufnum].text, want);
		list[bufnum].size = want;
	}
	*(list[bufnum].text) = '\0';
	return list[bufnum].text;
}

char *_traceattr2(int bufnum, attr_t newmode)
{
char	*buf = _nc_trace_buf(bufnum, BUFSIZ);
char	*tmp = buf;
static const	struct {unsigned int val; const char *name;}
names[] =
    {
	{ A_STANDOUT,		"A_STANDOUT" },
	{ A_UNDERLINE,		"A_UNDERLINE" },
	{ A_REVERSE,		"A_REVERSE" },
	{ A_BLINK,		"A_BLINK" },
	{ A_DIM,		"A_DIM" },
	{ A_BOLD,		"A_BOLD" },
	{ A_ALTCHARSET,		"A_ALTCHARSET" },
	{ A_INVIS,		"A_INVIS" },
	{ A_PROTECT,		"A_PROTECT" },
	{ A_CHARTEXT,		"A_CHARTEXT" },
	{ A_NORMAL,		"A_NORMAL" },
	{ A_COLOR,		"A_COLOR" },
    },
colors[] =
    {
	{ COLOR_BLACK,		"COLOR_BLACK" },
	{ COLOR_RED,		"COLOR_RED" },
	{ COLOR_GREEN,		"COLOR_GREEN" },
	{ COLOR_YELLOW,		"COLOR_YELLOW" },
	{ COLOR_BLUE,		"COLOR_BLUE" },
	{ COLOR_MAGENTA,	"COLOR_MAGENTA" },
	{ COLOR_CYAN,		"COLOR_CYAN" },
	{ COLOR_WHITE,		"COLOR_WHITE" },
    };
size_t n;
unsigned save_nc_tracing = _nc_tracing;
	_nc_tracing = 0;

	strcpy(tmp++, "{");

	for (n = 0; n < sizeof(names)/sizeof(names[0]); n++) {
		if ((newmode & names[n].val) != 0) {
			if (buf[1] != '\0')
				strcat(tmp, "|");
			strcat(tmp, names[n].name);
			tmp += strlen(tmp);

			if (names[n].val == A_COLOR)
			{
				short pairnum = PAIR_NUMBER(newmode);
				short fg, bg;
	
				if (pair_content(pairnum, &fg, &bg) == OK)
					(void) sprintf(tmp,
						"{%d = {%s, %s}}",
						pairnum,
						COLOR_OF(fg),
						COLOR_OF(bg)
						);
				else
					(void) sprintf(tmp, "{%d}", pairnum);
			}
		}
	}
	if (AttrOf(newmode) == A_NORMAL) {
		if (buf[1] != '\0')
			strcat(tmp, "|");
		strcat(tmp, "A_NORMAL");
	}

	_nc_tracing = save_nc_tracing;
	return (strcat(buf,"}"));
}

char *_traceattr(attr_t newmode)
{
	return _traceattr2(0, newmode);
}

char *_tracechtype2(int bufnum, chtype ch)
{
char	*buf = _nc_trace_buf(bufnum, BUFSIZ);
char	*found = 0;

    strcpy(buf, "{");
    if (ch & A_ALTCHARSET)
    {
	char	*cp;
	static const	struct {unsigned int val; const char *name;}
	names[] =
	{
	    {'l', "ACS_ULCORNER"},	/* upper left corner */
	    {'m', "ACS_LLCORNER"},	/* lower left corner */
	    {'k', "ACS_URCORNER"},	/* upper right corner */
	    {'j', "ACS_LRCORNER"},	/* lower right corner */
	    {'t', "ACS_LTEE"},		/* tee pointing right */
	    {'u', "ACS_RTEE"},		/* tee pointing left */
	    {'v', "ACS_BTEE"},		/* tee pointing up */
	    {'w', "ACS_TTEE"},		/* tee pointing down */
	    {'q', "ACS_HLINE"},		/* horizontal line */
	    {'x', "ACS_VLINE"},		/* vertical line */
	    {'n', "ACS_PLUS"},		/* large plus or crossover */
	    {'o', "ACS_S1"},		/* scan line 1 */
	    {'s', "ACS_S9"},		/* scan line 9 */
	    {'`', "ACS_DIAMOND"},	/* diamond */
	    {'a', "ACS_CKBOARD"},	/* checker board (stipple) */
	    {'f', "ACS_DEGREE"},	/* degree symbol */
	    {'g', "ACS_PLMINUS"},	/* plus/minus */
	    {'~', "ACS_BULLET"},	/* bullet */
	    {',', "ACS_LARROW"},	/* arrow pointing left */
	    {'+', "ACS_RARROW"},	/* arrow pointing right */
	    {'.', "ACS_DARROW"},	/* arrow pointing down */
	    {'-', "ACS_UARROW"},	/* arrow pointing up */
	    {'h', "ACS_BOARD"},		/* board of squares */
	    {'I', "ACS_LANTERN"},	/* lantern symbol */
	    {'0', "ACS_BLOCK"},		/* solid square block */
	    {'p', "ACS_S3"},		/* scan line 3 */
	    {'r', "ACS_S7"},		/* scan line 7 */
	    {'y', "ACS_LEQUAL"},	/* less/equal */
	    {'z', "ACS_GEQUAL"},	/* greater/equal */
	    {'{', "ACS_PI"},		/* Pi */
	    {'|', "ACS_NEQUAL"},	/* not equal */
	    {'}', "ACS_STERLING"},	/* UK pound sign */
	    {'\0',(char *)0}
	},
	*sp;

	for (cp = acs_chars; cp[0] && cp[1]; cp += 2)
	{
	    if (TextOf(cp[1]) == TextOf(ch))
	    {
		found = cp;
		/* don't exit from loop - there may be redefinitions */
	    }
	}

	if (found != 0)
	{
	    ch = TextOf(*found);
	    for (sp = names; sp->val; sp++)
		if (sp->val == ch)
		{
		    (void) strcat(buf, sp->name);
		    ch &= ~A_ALTCHARSET;
		    break;
		}
	}
    }

    if (found == 0)
	(void) strcat(buf, _tracechar(TextOf(ch)));

    if (AttrOf(ch) != A_NORMAL)
	(void) sprintf(buf + strlen(buf), " | %s", _traceattr2(bufnum+20,AttrOf(ch)));

    strcat(buf, "}");
    return(buf);
}

char *_tracechtype(chtype ch)
{
	return _tracechtype2(0, ch);
}
