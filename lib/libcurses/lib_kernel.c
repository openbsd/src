
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
 *		baudrate()
 *		delay_output()
 *		erasechar()
 *		killchar()
 *		flushinp()
 *		savetty()
 *		resetty()
 *
 *
 */

#include "curses.priv.h"
#include "term.h"	/* cur_term, pad_char */
#include <errno.h>
#if !HAVE_EXTERN_ERRNO
extern int errno;
#endif

int napms(int ms)
{
	T(("napms(%d) called", ms));

	usleep(1000*(unsigned)ms);
	return OK;
}

#ifndef EXTERN_TERMINFO
int reset_prog_mode(void)
{
	T(("reset_prog_mode() called"));

#ifdef TERMIOS
	tcsetattr(cur_term->Filedes, TCSANOW, &cur_term->Nttyb);
#else
	stty(cur_term->Filedes, &cur_term->Nttyb);
#endif
	if (SP && stdscr && stdscr->_use_keypad)
		_nc_keypad(TRUE);

	return OK; 
}


int reset_shell_mode(void)
{
	T(("reset_shell_mode() called"));

	if (SP)
	{
		fflush(SP->_ofp);
		_nc_keypad(FALSE);
	}

#ifdef TERMIOS
	tcsetattr(cur_term->Filedes, TCSANOW, &cur_term->Ottyb);
#else
	stty(cur_term->Filedes, &cur_term->Ottyb);
#endif
	return OK; 
}
#endif /* EXTERN_TERMINFO */

int delay_output(float ms)
{
	T(("delay_output(%f) called", ms));

    	if (SP == 0 || SP->_baudrate == ERR)
		return(ERR);
#ifdef no_pad_char
    	else if (no_pad_char)
		_nc_timed_wait(0, (int)ms, (int *)NULL);
#endif /* no_pad_char */
	else {
		register int	nullcount;
		char	null = '\0';

#ifdef pad_char
		if (pad_char)
	    		null = pad_char[0];
#endif /* pad_char */

		for (nullcount = ms * 1000 / SP->_baudrate; nullcount > 0; nullcount--)
		    	putc(null, SP->_ofp);
		(void) fflush(SP->_ofp);
    	}

    	return OK;
}
  
/*
 *	erasechar()
 *
 *	Return erase character as given in cur_term->Ottyb.
 *
 */

char
erasechar(void)
{
	T(("erasechar() called"));

#ifdef TERMIOS
    	return(cur_term->Ottyb.c_cc[VERASE]);
#else
    	return(cur_term->Ottyb.sg_erase);
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
	T(("killchar() called"));

#ifdef TERMIOS
    	return(cur_term->Ottyb.c_cc[VKILL]);
#else
    	return(cur_term->Ottyb.sg_kill);
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
	T(("flushinp() called"));

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
	return OK;

}



/*
 *	int
 *	baudrate()
 *
 *	Returns the current terminal's baud rate.
 *
 */

struct speed {
	speed_t s;
	int sp;
};

static struct speed const speeds[] = {
	{B0, 0},
	{B50, 50},
	{B75, 75},
	{B110, 110},
	{B134, 134},
	{B150, 150},
	{B200, 200},
	{B300, 300},
	{B600, 600},
	{B1200, 1200},
	{B1800, 1800},
	{B2400, 2400},
	{B4800, 4800},
	{B9600, 9600},
#define MAX_BAUD	B9600
#ifdef B19200
#undef MAX_BAUD
#define MAX_BAUD	B19200
	{B19200, 19200},
#else 
#ifdef EXTA
#define MAX_BAUD	EXTA
	{EXTA, 19200},
#endif
#endif
#ifdef B38400
#undef MAX_BAUD
#define MAX_BAUD	B38400
	{B38400, 38400},
#else 
#ifdef EXTB
#define MAX_BAUD	EXTB
	{EXTB, 38400},
#endif
#endif
#ifdef B57600
#undef MAX_BAUD
#define MAX_BAUD        B57600
	{B57600, 57600},
#endif
#ifdef B115200
#undef MAX_BAUD
#define MAX_BAUD        B115200
	{B115200, 115200},
#endif
};

int
baudrate(void)
{
int i, ret;

	T(("baudrate() called"));

#ifdef TERMIOS
	ret = cfgetospeed(&cur_term->Nttyb);
#else
	ret = cur_term->Nttyb.sg_ospeed;
#endif
	if(ret < 0 || ret > MAX_BAUD)
		return ERR;
	SP->_baudrate = ERR;
	for (i = 0; i < (sizeof(speeds) / sizeof(struct speed)); i++)
		if (speeds[i].s == ret)
		{
			SP->_baudrate = speeds[i].sp;
			break;
		}
	return(SP->_baudrate);
}


/*
**	savetty()  and  resetty()
**
*/

static TTY   buf;

int savetty(void)
{
	T(("savetty() called"));

#ifdef TERMIOS
	tcgetattr(cur_term->Filedes, &buf);
#else
	gtty(cur_term->Filedes, &buf);
#endif
	return OK;
}

int resetty(void)
{
	T(("resetty() called"));

#ifdef TERMIOS
	tcsetattr(cur_term->Filedes, TCSANOW, &buf);
#else
        stty(cur_term->Filedes, &buf);
#endif
	return OK;
}

