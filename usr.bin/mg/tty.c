/*	$OpenBSD: tty.c,v 1.7 2001/01/29 01:58:10 niklas Exp $	*/

/*
 * Terminfo display driver
 *
 * Terminfo is a terminal information database and routines to describe
 * terminals on most modern UNIX systems.  Many other systems have adopted
 * this as a reasonable way to allow for widly varying and ever changing
 * varieties of terminal types.	 This should be used where practical.
 */
/*
 * Known problems: If you have a terminal with no clear to end of screen and
 * memory of lines below the ones visible on the screen, display will be
 * wrong in some cases.  I doubt that any such terminal was ever made, but I
 * thought everyone with delete line would have clear to end of screen too...
 *
 * Code for terminals without clear to end of screen and/or clear to end of line
 * has not been extensivly tested.
 *
 * Cost calculations are very rough.  Costs of insert/delete line may be far
 * from the truth.  This is accentuated by display.c not knowing about
 * multi-line insert/delete.
 *
 * Using scrolling region vs insert/delete line should probably be based on cost
 * rather than the assuption that scrolling region operations look better.
 */

#include "def.h"

#include <term.h>

#ifdef NO_RESIZE
static int	 setttysize	__P((void));
#endif /* NO_RESIZE */

static int	 charcost	__P((char *));

static int	 cci;
static int	 insdel;	/* Do we have both insert & delete line? */
static char	*scroll_fwd;	/* How to scroll forward. */

/*
 * Initialize the terminal when the editor
 * gets started up.
 */
VOID
ttinit()
{
	char	*tv_stype, *p;

/* system dependent function to determine terminal type, if necessary. */
#ifndef gettermtype
	char	*gettermtype();
#endif /* gettermtype */

	if ((tv_stype = gettermtype()) == NULL)
		panic("Could not determine terminal type!");

	if (setupterm(tv_stype, 1, NULL)) {
		(void)asprintf(&p, "Unknown terminal type: %s", tv_stype);
		panic(p);
	}

	scroll_fwd = scroll_forward;
	if (scroll_fwd == NULL || *scroll_fwd == '\0') {
		/* this is what GNU Emacs does */
		scroll_fwd = parm_down_cursor;
		if (scroll_fwd == NULL || *scroll_fwd == '\0')
			scroll_fwd = "\n";
	}

	if (cursor_address == NULL || cursor_up == NULL)
		panic("This terminal is too stupid to run mg");

	/* set nrow & ncol */
	ttresize();

	if (!clr_eol)
		tceeol = ncol;
	else
		tceeol = charcost(clr_eol);

	/* Estimate cost of inserting a line */
	if (change_scroll_region && scroll_reverse)
		tcinsl = charcost(change_scroll_region) * 2 +
		    charcost(scroll_reverse);
	else if (parm_insert_line)
		tcinsl = charcost(parm_insert_line);
	else if (insert_line)
		tcinsl = charcost(insert_line);
	else
		/* make this cost high enough */
		tcinsl = NROW * NCOL;

	/* Estimate cost of deleting a line */
	if (change_scroll_region)
		tcdell = charcost(change_scroll_region) * 2 +
		    charcost(scroll_fwd);
	else if (parm_delete_line)
		tcdell = charcost(parm_delete_line);
	else if (delete_line)
		tcdell = charcost(delete_line);
	else
		/* make this cost high enough */
		tcdell = NROW * NCOL;

	/* Flag to indicate that we can both insert and delete lines */
	insdel = (insert_line || parm_insert_line) && 
	    (delete_line || parm_delete_line);

	if (enter_ca_mode)
		/* enter application mode */
		putpad(enter_ca_mode, 1);

	setttysize();
}

/*
 * Re-initialize the terminal when the editor is resumed.
 * The keypad_xmit doesn't really belong here but...
 */
VOID
ttreinit()
{
	if (enter_ca_mode)
		/* enter application mode */
		putpad(enter_ca_mode, 1);
	if (keypad_xmit)
		/* turn on keypad */
		putpad(keypad_xmit, 1);

	setttysize();
}

/*
 * Clean up the terminal, in anticipation of a return to the command 
 * interpreter. This is a no-op on the ANSI display. On the SCALD display, 
 * it sets the window back to half screen scrolling. Perhaps it should
 * query the display for the increment, and put it back to what it was.
 */
VOID
tttidy()
{
#ifdef	XKEYS
	ttykeymaptidy();
#endif /* XKEYS */

	/* set the term back to normal mode */
	if (exit_ca_mode)
		putpad(exit_ca_mode, 1);
}

/*
 * Move the cursor to the specified origin 0 row and column position. Try to
 * optimize out extra moves; redisplay may have left the cursor in the right
 * location last time!
 */
VOID
ttmove(row, col)
	int row, col;
{
	if (ttrow != row || ttcol != col) {
		putpad(tgoto(cursor_address, col, row), 1);
		ttrow = row;
		ttcol = col;
	}
}

/*
 * Erase to end of line.
 */
VOID
tteeol()
{
	int	i;

	if (clr_eol)
		putpad(clr_eol, 1);
	else {
		i = ncol - ttcol;
		while (i--)
			ttputc(' ');
		ttrow = ttcol = HUGE;
	}
}

/*
 * Erase to end of page.
 */
VOID
tteeop()
{
	int	line;

	if (clr_eos)
		putpad(clr_eos, nrow - ttrow);
	else {
		putpad(clr_eol, 1);
		if (insdel)
			ttdell(ttrow + 1, lines, lines - ttrow - 1);
		else {
			/* do it by hand */
			for (line = ttrow + 1; line <= lines; ++line) {
				ttmove(line, 0);
				tteeol();
			}
		}
		ttrow = ttcol = HUGE;
	}
}

/*
 * Make a noise.
 */
VOID
ttbeep()
{
	putpad(bell, 1);
	ttflush();
}

/*
 * Insert nchunk blank line(s) onto the screen, scrolling the last line on 
 * the screen off the bottom.  Use the scrolling region if possible for a 
 * smoother display.  If there is no scrolling region, use a set of insert 
 * and delete line sequences.
 */
VOID
ttinsl(row, bot, nchunk)
	int row, bot, nchunk;
{
	int	i, nl;

	/* Case of one line insert is special. */
	if (row == bot) {
		ttmove(row, 0);
		tteeol();
		return;
	}
	if (change_scroll_region && scroll_reverse) {
		/* Use scroll region and back index	 */
		nl = bot - row;
		ttwindow(row, bot);
		ttmove(row, 0);
		while (nchunk--)
			putpad(scroll_reverse, nl);
		ttnowindow();
		return;
	} else if (insdel) {
		ttmove(1 + bot - nchunk, 0);
		nl = nrow - ttrow;
		if (parm_delete_line)
			putpad(tgoto(parm_delete_line, 0, nchunk), nl);
		else
			/* For all lines in the chunk... */
			for (i = 0; i < nchunk; i++)
				putpad(delete_line, nl);
		ttmove(row, 0);

		/* ttmove() changes ttrow */
		nl = nrow - ttrow;

		if (parm_insert_line)
			putpad(tgoto(parm_insert_line, 0, nchunk), nl);
		else
			/* For all lines in the chunk */
			for (i = 0; i < nchunk; i++)
				putpad(insert_line, nl);
		ttrow = HUGE;
		ttcol = HUGE;
	} else
		panic("ttinsl: Can't insert/delete line");
}

/*
 * Delete nchunk line(s) from "row", replacing the bottom line on the 
 * screen with a blank line.  Unless we're using the scrolling region, 
 * this is done with crafty sequences of insert and delete lines.  The 
 * presence of the echo area makes a boundry condition go away.
 */
VOID
ttdell(row, bot, nchunk)
	int row, bot, nchunk;
{
	int	i, nl;

	/* One line special cases */
	if (row == bot) {
		ttmove(row, 0);
		tteeol();
		return;
	}
	/* scrolling region */
	if (change_scroll_region) {
		nl = bot - row;
		ttwindow(row, bot);
		ttmove(bot, 0);
		while (nchunk--)
			putpad(scroll_fwd, nl);
		ttnowindow();
	/* else use insert/delete line */
	} else if (insdel) {
		ttmove(row, 0);
		nl = nrow - ttrow;
		if (parm_delete_line)
			putpad(tgoto(parm_delete_line, 0, nchunk), nl);
		else
			/* For all lines in the chunk	 */
			for (i = 0; i < nchunk; i++)
				putpad(delete_line, nl);
		ttmove(1 + bot - nchunk, 0);

		/* ttmove() changes ttrow */
		nl = nrow - ttrow;
		if (parm_insert_line)
			putpad(tgoto(parm_insert_line, 0, nchunk), nl);
		else
			/* For all lines in the chunk */
			for (i = 0; i < nchunk; i++)
				putpad(insert_line, nl);
		ttrow = HUGE;
		ttcol = HUGE;
	} else
		panic("ttdell: Can't insert/delete line");
}

/*
 * This routine sets the scrolling window on the display to go from line 
 * "top" to line "bot" (origin 0, inclusive).  The caller checks for the 
 * pathological 1-line scroll window which doesn't work right and avoids 
 * it.  The "ttrow" and "ttcol" variables are set to a crazy value to 
 * ensure that the next call to "ttmove" does not turn into a no-op (the 
 * window adjustment moves the cursor).
 */
VOID
ttwindow(top, bot)
	int top, bot;
{
	if (change_scroll_region && (tttop != top || ttbot != bot)) {
		putpad(tgoto(change_scroll_region, bot, top), nrow - ttrow);
		ttrow = HUGE;	/* Unknown.		 */
		ttcol = HUGE;
		tttop = top;	/* Remember region.	 */
		ttbot = bot;
	}
}

/*
 * Switch to full screen scroll. This is used by "spawn.c" just before it 
 * suspends the editor and by "display.c" when it is getting ready to 
 * exit.  This function does a full screen scroll by telling the terminal 
 * to set a scrolling region that is lines or nrow rows high, whichever is 
 * larger.  This behavior seems to work right on systems where you can set 
 * your terminal size.
 */
VOID
ttnowindow()
{
	if (change_scroll_region) {
		putpad(tgoto(change_scroll_region,
		       (nrow > lines ? nrow : lines) - 1, 0), nrow - ttrow);
		ttrow = HUGE;	/* Unknown.		 */
		ttcol = HUGE;
		tttop = HUGE;	/* No scroll region.	 */
		ttbot = HUGE;
	}
}

/*
 * Set the current writing color to the specified color. Watch for color 
 * changes that are not going to do anything (the color is already right)
 * and don't send anything to the display.  The rainbow version does this 
 * in putline.s on a line by line basis, so don't bother sending out the 
 * color shift.
 */
VOID
ttcolor(color)
	int color;
{
	if (color != tthue) {
		if (color == CTEXT)
			/* normal video */
			putpad(exit_standout_mode, 1);
		else if (color == CMODE)
			/* reverse video */
			putpad(enter_standout_mode, 1);
		/* save the color */
		tthue = color;
	}
}

/*
 * This routine is called by the "refresh the screen" command to try 
 * to resize the display. The new size, which must not exceed the NROW 
 * and NCOL limits, is stored back into "nrow" and * "ncol". Display can 
 * always deal with a screen NROW by NCOL. Look in "window.c" to see how 
 * the caller deals with a change.
 */
VOID
ttresize()
{
	/* found in "ttyio.c" */
	setttysize();

	/* ask OS for tty size and check limits */
	if (nrow < 1)
		nrow = 1;
	else if (nrow > NROW)
		nrow = NROW;
	if (ncol < 1)
		ncol = 1;
	else if (ncol > NCOL)
		ncol = NCOL;
}

#ifdef NO_RESIZE
static
setttysize()
{
	nrow = lines;
	ncol = columns;
}
#endif /* NO_RESIZE */

/*
 * fake char output for charcost() 
 */
/* ARGSUSED */
static int
fakec(c)
	char c;
{
	cci++;
	return 0;
}

/* calculate the cost of doing string s */
static int
charcost(s)
	char *s;
{
	int	cci = 0;

	tputs(s, nrow, fakec);
	return (cci);
}
