
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
**	lib_set_term.c
**
**	The routine set_term().
**
*/

#include <curses.priv.h>

#include <term.h>	/* cur_term */

MODULE_ID("Id: lib_set_term.c,v 1.17 1997/05/01 23:46:18 Alexander.V.Lukyanov Exp $")

/*
 * If the output file descriptor is connected to a tty (the typical case) it
 * will probably be line-buffered.  Keith Bostic pointed out that we don't want
 * this; it hoses people running over networks by forcing out a bunch of small
 * packets instead of one big one, so screen updates on ptys look jerky. 
 * Restore block buffering to prevent this minor lossage.
 *
 * The buffer size is a compromise.  Ideally we'd like a buffer that can hold
 * the maximum possible update size (the whole screen plus cup commands to
 * change lines as it's painted).  On a 66-line xterm this can become
 * excessive.  So we min it with the amount of data we think we can get through
 * two Ethernet packets (maximum packet size - 100 for TCP/IP overhead).
 *
 * Why two ethernet packets?  It used to be one, on the theory that said
 * packets define the maximum size of atomic update.  But that's less than the
 * 2000 chars on a 25 x 80 screen, and we don't want local updates to flicker
 * either.  Two packet lengths will handle up to a 35 x 80 screen.
 *
 * The magic '6' is the estimated length of the end-of-line cup sequence to go
 * to the next line.  It's generous.  We used to mess with the buffering in
 * init_mvcur() after cost computation, but that lost the sequences emitted by
 * init_acs() in setupscreen().
 *
 * "The setvbuf function may be used only after the stream pointed to by stream
 * has been associated with an open file and before any other operation is
 * performed on the stream." (ISO 7.9.5.6.)
 *
 * Grrrr...
 */
void _nc_set_buffer(FILE *ofp, bool buffered)
{
	/* optional optimization hack -- do before any output to ofp */
#if HAVE_SETVBUF || HAVE_SETBUFFER  
	unsigned buf_len;
	char *buf_ptr;

	if (buffered) {
		buf_len = min(LINES * (COLS + 6), 2800);
		buf_ptr = malloc(buf_len);
	} else {
		buf_len = 0;
		buf_ptr = 0;
	}

#if HAVE_SETVBUF
#ifdef SETVBUF_REVERSED	/* pre-svr3? */
	(void) setvbuf(ofp, buf_ptr, buf_len, buf_len ? _IOFBF : _IONBF);
#else
	(void) setvbuf(ofp, buf_ptr, buf_len ? _IOFBF : _IONBF, buf_len);
#endif
#elif HAVE_SETBUFFER
	(void) setbuffer(ofp, buf_ptr, (int)buf_len);
#endif

	if (!buffered) {
		FreeIfNeeded(SP->_setbuf);
	}
	SP->_setbuf = buf_ptr;

#endif /* HAVE_SETVBUF || HAVE_SETBUFFER */
}

SCREEN * set_term(SCREEN *screen)
{
SCREEN	*oldSP;

	T((T_CALLED("set_term(%p)"), screen));

	oldSP = SP;
	_nc_set_screen(screen);

	cur_term    = SP->_term;
	curscr      = SP->_curscr;
	newscr      = SP->_newscr;
	stdscr      = SP->_stdscr;
	COLORS      = SP->_color_count;
	COLOR_PAIRS = SP->_pair_count;

	T((T_RETURN("%p"), oldSP));
	return(oldSP);
}

static void _nc_free_keytry(struct tries *kt)
{
	if (kt != 0) {
		_nc_free_keytry(kt->child);
		_nc_free_keytry(kt->sibling);
		free(kt);
	}
}

/*
 * Free the storage associated with the given SCREEN sp.
 */
void delscreen(SCREEN *sp)
{
	T((T_CALLED("delscreen(%p)"), sp));

	_nc_freewin(sp->_curscr);
	_nc_freewin(sp->_newscr);
	_nc_freewin(sp->_stdscr);
	_nc_free_keytry(sp->_keytry);

	FreeIfNeeded(sp->_color_table);
	FreeIfNeeded(sp->_color_pairs);

	free(sp);

	/*
	 * If this was the current screen, reset everything that the
	 * application might try to use (except cur_term, which may have
	 * multiple references in different screens).
	 */
	if (sp == SP) {
		curscr = 0;
		newscr = 0;
		stdscr = 0;
		COLORS = 0;
		COLOR_PAIRS = 0;
		_nc_set_screen(0);
	}
	returnVoid;
}

ripoff_t rippedoff[5], *rsp = rippedoff;
#define N_RIPS (int)(sizeof(rippedoff)/sizeof(rippedoff[0]))

int _nc_setupscreen(short slines, short const scolumns, FILE *output)
/* OS-independent screen initializations */
{
int	bottom_stolen = 0, i;

	if (!_nc_alloc_screen())
		return ERR;

	_nc_set_buffer(output, TRUE);
	SP->_term        = cur_term;
	SP->_lines       = slines;
	SP->_lines_avail = slines;
	SP->_columns     = scolumns;
	SP->_cursrow     = -1;
	SP->_curscol     = -1;
	SP->_keytry      = UNINITIALISED;
	SP->_nl          = TRUE;
	SP->_raw         = FALSE;
	SP->_cbreak      = FALSE;
	SP->_echo        = FALSE;
	SP->_fifohead    = -1;
	SP->_fifotail    = 0;
	SP->_fifopeek    = 0;
	SP->_endwin      = TRUE;
	SP->_ofp         = output;
	SP->_coloron     = 0;
	SP->_curscr      = 0;
	SP->_newscr      = 0;
	SP->_stdscr      = 0;
	SP->_topstolen   = 0;
	SP->_cursor      = -1;	/* cannot know real cursor shape */

	init_acs();

	T(("creating newscr"));
	if ((newscr = newwin(slines, scolumns, 0, 0)) == 0)
		return ERR;

	T(("creating curscr"));
	if ((curscr = newwin(slines, scolumns, 0, 0)) == 0)
		return ERR;

	SP->_newscr = newscr;
	SP->_curscr = curscr;

	newscr->_clear = TRUE;
	curscr->_clear = FALSE;

	for (i=0, rsp = rippedoff; rsp->line && (i < N_RIPS); rsp++, i++) {
	  if (rsp->hook) {
	      WINDOW *w;
	      int count = (rsp->line < 0) ? -rsp->line : rsp->line;

	      if (rsp->line < 0) {
		  w = newwin(count,scolumns,SP->_lines_avail - count,0);
		  if (w) {
		      rsp->w = w;
		      rsp->hook(w, scolumns);
		      bottom_stolen += count;
		  }
		  else
		    return ERR;
	      } else {
		  w = newwin(count,scolumns, 0, 0);
		  if (w) {
		      rsp->w = w;
		      rsp->hook(w, scolumns);
		      SP->_topstolen += count;
		  }
		  else
		    return ERR;
	      }
	      SP->_lines_avail -= count;
	  }
	}

	T(("creating stdscr"));
	assert ((SP->_lines_avail + SP->_topstolen + bottom_stolen) == slines);
	if ((stdscr = newwin(LINES = SP->_lines_avail, scolumns, 0, 0)) == 0)
		return ERR;
	SP->_stdscr = stdscr;

	def_shell_mode();
	def_prog_mode();

	return OK;
}

/* The internal implementation interprets line as the number of
   lines to rip off from the top or bottom.
   */
int
_nc_ripoffline(int line, int (*init)(WINDOW *,int))
{
    if (line == 0)
	return(OK);

    if (rsp >= rippedoff + N_RIPS)
	return(ERR);

    rsp->line = line;
    rsp->hook = init;
    rsp->w    = 0;
    rsp++;

    return(OK);
}

int
ripoffline(int line, int (*init)(WINDOW *, int))
{
    T((T_CALLED("ripoffline(%d,%p)"), line, init));

    if (line == 0)
	returnCode(OK);

    returnCode(_nc_ripoffline ((line<0) ? -1 : 1, init));
}
