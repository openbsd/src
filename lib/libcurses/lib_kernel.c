
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
 *	lib_kernel.c
 *
 *	Misc. low-level routines:
 *		napms()
 *		reset_prog_mode()
 *		reset_shell_mode()
 *		erasechar()
 *		killchar()
 *		flushinp()
 *		savetty()
 *		resetty()
 *
 * The baudrate() and delay_output() functions could logically live here,
 * but are in other modules to reduce the static-link size of programs
 * that use only these facilities.
 */

#include <curses.priv.h>
#include <term.h>	/* cur_term */

MODULE_ID("Id: lib_kernel.c,v 1.13 1997/02/02 00:33:14 tom Exp $")

int napms(int ms)
{
	T((T_CALLED("napms(%d)"), ms));

	usleep(1000*(unsigned)ms);
	returnCode(OK);
}

#ifndef EXTERN_TERMINFO
int reset_prog_mode(void)
{
	T((T_CALLED("reset_prog_mode()")));

	SET_TTY(cur_term->Filedes, &cur_term->Nttyb);
	if (SP && stdscr && stdscr->_use_keypad)
		_nc_keypad(TRUE);

	returnCode(OK);
}


int reset_shell_mode(void)
{
	T((T_CALLED("reset_shell_mode()")));

	if (SP)
	{
		fflush(SP->_ofp);
		_nc_keypad(FALSE);
	}

	SET_TTY(cur_term->Filedes, &cur_term->Ottyb);
	returnCode(OK);
}
#endif /* EXTERN_TERMINFO */

/*
 *	erasechar()
 *
 *	Return erase character as given in cur_term->Ottyb.
 *
 */

char
erasechar(void)
{
	T((T_CALLED("erasechar()")));

#ifdef TERMIOS
	returnCode(cur_term->Ottyb.c_cc[VERASE]);
#else
	returnCode(cur_term->Ottyb.sg_erase);
#endif

}



/*
 *	killchar()
 *
 *	Return kill character as given in cur_term->Ottyb.
 *
 */

char
killchar(void)
{
	T((T_CALLED("killchar()")));

#ifdef TERMIOS
	returnCode(cur_term->Ottyb.c_cc[VKILL]);
#else
	returnCode(cur_term->Ottyb.sg_kill);
#endif
}



/*
 *	flushinp()
 *
 *	Flush any input on cur_term->Filedes
 *
 */

int flushinp(void)
{
	T((T_CALLED("flushinp()")));

#ifdef TERMIOS
	tcflush(cur_term->Filedes, TCIFLUSH);
#else
	errno = 0;
	do {
	    ioctl(cur_term->Filedes, TIOCFLUSH, 0);
	} while
	    (errno == EINTR);
#endif
	if (SP) {
		SP->_fifohead = -1;
		SP->_fifotail = 0;
		SP->_fifopeek = 0;
	}
	returnCode(OK);

}

/*
**	savetty()  and  resetty()
**
*/

static TTY   buf;

int savetty(void)
{
	T((T_CALLED("savetty()")));

	GET_TTY(cur_term->Filedes, &buf);
	returnCode(OK);
}

int resetty(void)
{
	T((T_CALLED("resetty()")));

	SET_TTY(cur_term->Filedes, &buf);
	returnCode(OK);
}
