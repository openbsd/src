
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
 *	raw.c
 *
 *	Routines:
 *		raw()
 *		echo()
 *		nl()
 *		cbreak()
 *		noraw()
 *		noecho()
 *		nonl()
 *		nocbreak()
 *		qiflush()
 *		noqiflush()
 *		intrflush()
 *
 */

#include "curses.priv.h"
#include "term.h"	/* cur_term */

/* 
 * COOKED_INPUT defines the collection of input mode bits to be
 * cleared when entering raw mode, then re-set by noraw().  
 *
 * We used to clear ISTRIP and INPCK when going to raw mode.  Keith
 * Bostic says that's wrong, because those are hardware bits that the
 * user has to get right in his/her initial environment -- he says
 * curses can't do any good by clearing these, and may do harm.  In
 * 1995's world of 8N1 connections over error-correcting modems, all
 * the parity-check stuff is pretty nearly irrelevant anyway.
 *
 * What's supposed to happen when noraw() executes has never been very
 * well-defined.  Yes, it should reset ISIG/ICANON/OPOST (historical
 * practice is for it to attempt to take the driver back to cooked
 * mode, rather going to some half-baked cbreak-like intermediate
 * level).
 *
 * We make a design choice here to turn off CR/LF translation a la BSD
 * when raw() is enabled, on the theory that a programmer requesting
 * raw() ideally wants an 8-bit data stream that's been messed with as
 * little as possible.  The man pages document this.
 *
 * We originally opted for the simplest way to handle noraw(); just set all
 * the flags we cleared.  Unfortunately, having noraw() set IGNCR
 * turned out to be too painful.  So raw() now clears the COOKED_INPUT
 * flags, but also clears (ICRNL|INLCR|IGNCR) which noraw() doesn't
 * restore.
 *
 * Unfortunately, this means noraw() may still force some COOKED_INPUT
 * flags on that the user had initially cleared via stty.  It'll all
 * come out in the wash when endwin() restores the user's original
 * input bits (we hope...)
 *
 */
#define COOKED_INPUT	(IXON|IGNBRK|BRKINT|PARMRK)

int raw(void)
{
	T(("raw() called"));

	SP->_raw = TRUE;
	SP->_cbreak = TRUE;

#ifdef TERMIOS
	cur_term->Nttyb.c_lflag &= ~(ICANON|ISIG|IEXTEN);
	cur_term->Nttyb.c_iflag &= ~(COOKED_INPUT|ICRNL|INLCR|IGNCR);
	cur_term->Nttyb.c_oflag &= ~(OPOST);
	cur_term->Nttyb.c_cc[VMIN] = 1;
	cur_term->Nttyb.c_cc[VTIME] = 0;
	if((tcsetattr(cur_term->Filedes, TCSANOW, &cur_term->Nttyb)) == -1)
		return ERR;
	else
		return OK;
#else
	cur_term->Nttyb.sg_flags |= RAW;
	stty(cur_term->Filedes, &cur_term->Nttyb);
	return OK;
#endif
}

int cbreak(void)
{
	T(("cbreak() called"));

	SP->_cbreak = TRUE;

#ifdef TERMIOS
	cur_term->Nttyb.c_lflag &= ~ICANON; 
	cur_term->Nttyb.c_lflag |= ISIG;
	cur_term->Nttyb.c_cc[VMIN] = 1;
	cur_term->Nttyb.c_cc[VTIME] = 0;
	if((tcsetattr(cur_term->Filedes, TCSANOW, &cur_term->Nttyb)) == -1)
		return ERR;
	else
		return OK;
#else
	cur_term->Nttyb.sg_flags |= CBREAK;
	stty(cur_term->Filedes, &cur_term->Nttyb);
	return OK;
#endif
}

int echo(void)
{
	T(("echo() called"));

	SP->_echo = TRUE;
    
#ifdef TERMIOS
	cur_term->Nttyb.c_lflag |= ECHO;
	if((tcsetattr(cur_term->Filedes, TCSANOW, &cur_term->Nttyb)) == -1)
		return ERR;
	else
		return OK;
#else
	cur_term->Nttyb.sg_flags |= ECHO;
	stty(cur_term->Filedes, &cur_term->Nttyb);
	return OK;
#endif
}


int nl(void)
{
	T(("nl() called"));

	SP->_nl = TRUE;

#ifdef TERMIOS
	/* the code used to set IXON|IXOFF here, Ghod knows why... */
	cur_term->Nttyb.c_iflag |= ICRNL;
	cur_term->Nttyb.c_oflag |= OPOST|ONLCR;
	if((tcsetattr(cur_term->Filedes, TCSANOW, &cur_term->Nttyb)) == -1)
		return ERR;
	else
		return OK;
#else
	cur_term->Nttyb.sg_flags |= CRMOD;
	stty(cur_term->Filedes, &cur_term->Nttyb);
	return OK;
#endif
}


int qiflush(void)
{
	T(("qiflush() called"));

	/*
	 * Note: this implementation may be wrong.  See the comment under
	 * intrflush().
	 */

#ifdef TERMIOS
	cur_term->Nttyb.c_lflag &= ~(NOFLSH);
	if((tcsetattr(cur_term->Filedes, TCSANOW, &cur_term->Nttyb)) == -1)
		return ERR;
	else
		return OK;
#else
	return ERR;
#endif
}


int noraw(void)
{
	T(("noraw() called"));

	SP->_raw = FALSE;
	SP->_cbreak = FALSE;

#ifdef TERMIOS
	cur_term->Nttyb.c_lflag |= ISIG|ICANON|IEXTEN;
	cur_term->Nttyb.c_iflag |= COOKED_INPUT;
	cur_term->Nttyb.c_oflag |= OPOST;
	if((tcsetattr(cur_term->Filedes, TCSANOW, &cur_term->Nttyb)) == -1)
		return ERR;
	else
		return OK;
#else
	cur_term->Nttyb.sg_flags &= ~(RAW|CBREAK);
	stty(cur_term->Filedes, &cur_term->Nttyb);
	return OK;
#endif

}


int nocbreak(void)
{
	T(("nocbreak() called"));

	SP->_cbreak = 0;
	
#ifdef TERMIOS
	cur_term->Nttyb.c_lflag |= ICANON;
	if((tcsetattr(cur_term->Filedes, TCSANOW, &cur_term->Nttyb)) == -1)
		return ERR;
	else
		return OK;
#else 
	cur_term->Nttyb.sg_flags &= ~CBREAK;
	stty(cur_term->Filedes, &cur_term->Nttyb);
	return OK;
#endif
}

int noecho(void)
{
	T(("noecho() called"));

	SP->_echo = FALSE;
	
#ifdef TERMIOS
	/* 
	 * Turn off ECHONL to avoid having \n still be echoed when
	 * cooked mode is in effect (that is, ICANON is on).
	 */
	cur_term->Nttyb.c_lflag &= ~(ECHO|ECHONL);
	if((tcsetattr(cur_term->Filedes, TCSANOW, &cur_term->Nttyb)) == -1)
		return ERR;
	else
		return OK;
#else
	cur_term->Nttyb.sg_flags &= ~ECHO;
	stty(cur_term->Filedes, &cur_term->Nttyb);
	return OK;
#endif
}


int nonl(void)
{
	T(("nonl() called"));

	SP->_nl = FALSE;
	
#ifdef TERMIOS
	cur_term->Nttyb.c_iflag &= ~ICRNL;
	cur_term->Nttyb.c_oflag &= ~ONLCR;
	if((tcsetattr(cur_term->Filedes, TCSANOW, &cur_term->Nttyb)) == -1)
		return ERR;
	else
		return OK;
#else
	cur_term->Nttyb.sg_flags &= ~CRMOD;
	stty(cur_term->Filedes, &cur_term->Nttyb);
	return OK;
#endif
}

int noqiflush(void)
{
	T(("noqiflush() called"));

	/*
	 * Note: this implementation may be wrong.  See the comment under
	 * intrflush().
	 */

#ifdef TERMIOS
	cur_term->Nttyb.c_lflag |= NOFLSH;
	if((tcsetattr(cur_term->Filedes, TCSANOW, &cur_term->Nttyb)) == -1)
		return ERR;
	else
		return OK;
#else
	return ERR;
#endif
}

int intrflush(WINDOW *win, bool flag)
{
	T(("intrflush() called"));

	/*
	 * This call does the same thing as the qiflush()/noqiflush()
	 * pair.  We know for certain that SVr3 intrflush() tweaks the
	 * NOFLSH bit; on the other hand, the match (in the SVr4 man
	 * pages) between the language describing NOFLSH in termio(7)
	 * and the language describing qiflush()/noqiflush() in
	 * curs_inopts(3x) is too exact to be coincidence.
	 */

#ifdef TERMIOS
	if (flag)
		cur_term->Nttyb.c_lflag &= ~(NOFLSH);
	else
		cur_term->Nttyb.c_lflag |= (NOFLSH);
	if((tcsetattr(cur_term->Filedes, TCSANOW, &cur_term->Nttyb)) == -1)
		return ERR;
	else
		return OK;
#else
	return ERR;
#endif
}

