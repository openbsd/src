/*-
 * Copyright (c) 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1993, 1994, 1995, 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 *
 *	@(#)cl.h	10.17 (Berkeley) 7/12/96
 */

typedef struct _cl_private {
	CHAR_T	 ibuf[256];	/* Input keys. */

	int	 eof_count;	/* EOF count. */

	struct termios orig;	/* Original terminal values. */
	struct termios ex_enter;/* Terminal values to enter ex. */
	struct termios vi_enter;/* Terminal values to enter vi. */

	char	*el;		/* Clear to EOL terminal string. */
	char	*cup;		/* Cursor movement terminal string. */
	char	*cuu1;		/* Cursor up terminal string. */
	char	*rmso, *smso;	/* Inverse video terminal strings. */
	char	*smcup, *rmcup;	/* Terminal start/stop strings. */

	int	 in_ex;		/* XXX: Currently running ex. */

	int	 killersig;	/* Killer signal. */
#define	INDX_HUP	0
#define	INDX_INT	1
#define	INDX_TERM	2
#define	INDX_WINCH	3
#define	INDX_MAX	4	/* Original signal information. */
	struct sigaction oact[INDX_MAX];

	enum {			/* Tty group write mode. */
	    TGW_UNKNOWN=0, TGW_SET, TGW_UNSET } tgw;

	enum {			/* Terminal initialization strings. */
	    TE_SENT=0, TI_SENT } ti_te;

#define	CL_RENAME	0x001	/* X11 xterm icon/window renamed. */
#define	CL_RENAME_OK	0x002	/* User wants the windows renamed. */
#define	CL_SCR_EX_INIT	0x004	/* Ex screen initialized. */
#define	CL_SCR_VI_INIT	0x008	/* Vi screen initialized. */
#define	CL_SIGHUP	0x010	/* SIGHUP arrived. */
#define	CL_SIGINT	0x020	/* SIGINT arrived. */
#define	CL_SIGTERM	0x040	/* SIGTERM arrived. */
#define	CL_SIGWINCH	0x080	/* SIGWINCH arrived. */
	u_int32_t flags;
} CL_PRIVATE;

#define	CLP(sp)		((CL_PRIVATE *)((sp)->gp->cl_private))
#define	GCLP(gp)	((CL_PRIVATE *)gp->cl_private)

/* Return possibilities from the keyboard read routine. */
typedef enum { INP_OK=0, INP_EOF, INP_ERR, INP_INTR, INP_TIMEOUT } input_t;

/* The screen line relative to a specific window. */
#define	RLNO(sp, lno)	(sp)->woff + (lno)

/* Some functions can be safely ignored until the screen is running. */
#define	VI_INIT_IGNORE(sp)						\
	if (F_ISSET(sp, SC_VI) && !F_ISSET(sp, SC_SCR_VI))		\
		return (0);
#define	EX_INIT_IGNORE(sp)						\
	if (F_ISSET(sp, SC_EX) && !F_ISSET(sp, SC_SCR_EX))		\
		return (0);

/* X11 xterm escape sequence to rename the icon/window. */
#define	XTERM_RENAME	"\033]0;%s\007"

/*
 * XXX
 * Some implementations of curses.h don't define these for us.  Used for
 * compatibility only.
 */
#ifndef TRUE
#define	TRUE	1
#endif
#ifndef FALSE
#define	FALSE	0
#endif

#include "cl_extern.h"
