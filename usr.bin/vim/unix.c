/*	$OpenBSD: unix.c,v 1.4 1996/10/15 08:07:58 downsj Exp $	*/
/* vi:set ts=4 sw=4:
 *
 * VIM - Vi IMproved		by Bram Moolenaar
 *                OS/2 port by Paul Slootman
 *
 * Do ":help uganda"  in Vim to read copying and usage conditions.
 * Do ":help credits" in Vim to see a list of people who contributed.
 */

/*
 * unix.c -- code for all flavors of Unix (BSD, SYSV, SVR4, POSIX, ...)
 *           Also for OS/2, using the excellent EMX package!!!
 *
 * A lot of this file was originally written by Juergen Weigert and later
 * changed beyond recognition.
 */

/*
 * Some systems have a prototype for select() that has (int *) instead of
 * (fd_set *), which is wrong. This define removes that prototype. We include
 * our own prototype in osdef.h.
 */
#define select select_declared_wrong

#include "vim.h"
#include "globals.h"
#include "option.h"
#include "proto.h"

#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif

#include "unixunix.h"		/* unix includes for unix.c only */

/*
 * Use this prototype for select, some include files have a wrong prototype
 */
#undef select

#if defined(HAVE_SELECT)
extern int   select __ARGS((int, fd_set *, fd_set *, fd_set *, struct timeval *));
#endif

/*
 * end of autoconf section. To be extended...
 */

/* Are the following #ifdefs still required? And why? Is that for X11? */

#if defined(ESIX) || defined(M_UNIX) && !defined(SCO)
# ifdef SIGWINCH
#  undef SIGWINCH
# endif
# ifdef TIOCGWINSZ
#  undef TIOCGWINSZ
# endif
#endif

#if defined(SIGWINDOW) && !defined(SIGWINCH)	/* hpux 9.01 has it */
# define SIGWINCH SIGWINDOW
#endif

#if defined(HAVE_X11) && defined(WANT_X11)
# include <X11/Xlib.h>
# include <X11/Xutil.h>
# include <X11/Xatom.h>

Window		x11_window = 0;
Display		*x11_display = NULL;
int			got_x_error = FALSE;

static int	get_x11_windis __ARGS((void));
static void set_x11_title __ARGS((char_u *));
static void set_x11_icon __ARGS((char_u *));
#endif

static int get_x11_title __ARGS((int));
static int get_x11_icon __ARGS((int));

static void may_core_dump __ARGS((void));

static int	Read __ARGS((char_u *, long));
static int	WaitForChar __ARGS((long));
static int	RealWaitForChar __ARGS((int, long));
static void fill_inbuf __ARGS((int));

#if defined(SIGWINCH)
static RETSIGTYPE sig_winch __ARGS(SIGPROTOARG);
#endif
#if defined(SIGALRM) && defined(HAVE_X11) && defined(WANT_X11)
static RETSIGTYPE sig_alarm __ARGS(SIGPROTOARG);
#endif
static RETSIGTYPE deathtrap __ARGS(SIGPROTOARG);

static void catch_signals __ARGS((RETSIGTYPE (*func)()));
#ifndef __EMX__
static int	have_wildcard __ARGS((int, char_u **));
static int	have_dollars __ARGS((int, char_u **));
#endif

static int		do_resize = FALSE;
static char_u	*oldtitle = NULL;
static char_u	*fixedtitle = (char_u *)"Thanks for flying Vim";
static char_u	*oldicon = NULL;
#ifndef __EMX__
static char_u	*extra_shell_arg = NULL;
static int		show_shell_mess = TRUE;
#endif
static int		deadly_signal = 0;			/* The signal we caught */

#ifdef SYS_SIGLIST_DECLARED
/*
 * I have seen 
 * 	extern char *_sys_siglist[NSIG];
 * on Irix, Linux, NetBSD and Solaris. It contains a nice list of strings
 * that describe the signals. That is nearly what we want here.  But
 * autoconf does only check for sys_siglist (without the underscore), I
 * do not want to change everything today.... jw.
 * This is why AC_DECL_SYS_SIGLIST is commented out in configure.in
 */
#endif

static struct
{
	int		sig;		/* Signal number, eg. SIGSEGV etc */
	char	*name;		/* Signal name (not char_u!). */
} signal_info[] =
{
#ifdef SIGHUP
	{SIGHUP,		"HUP"},
#endif
#ifdef SIGINT
	{SIGINT,		"INT"},
#endif
#ifdef SIGQUIT
	{SIGQUIT,		"QUIT"},
#endif
#ifdef SIGILL
	{SIGILL,		"ILL"},
#endif
#ifdef SIGTRAP
	{SIGTRAP,		"TRAP"},
#endif
#ifdef SIGABRT
	{SIGABRT,		"ABRT"},
#endif
#ifdef SIGEMT
	{SIGEMT,		"EMT"},
#endif
#ifdef SIGFPE
	{SIGFPE,		"FPE"},
#endif
#ifdef SIGBUS
	{SIGBUS,		"BUS"},
#endif
#ifdef SIGSEGV
	{SIGSEGV,		"SEGV"},
#endif
#ifdef SIGSYS
	{SIGSYS,		"SYS"},
#endif
#ifdef SIGALRM
	{SIGALRM,		"ALRM"},
#endif
#ifdef SIGTERM
	{SIGTERM,		"TERM"},
#endif
#ifdef SIGVTALRM
	{SIGVTALRM,		"VTALRM"},
#endif
#ifdef SIGPROF
	{SIGPROF,		"PROF"},
#endif
#ifdef SIGXCPU
	{SIGXCPU,		"XCPU"},
#endif
#ifdef SIGXFSZ
	{SIGXFSZ,		"XFSZ"},
#endif
#ifdef SIGUSR1
	{SIGUSR1,		"USR1"},
#endif
#ifdef SIGUSR2
	{SIGUSR2,		"USR2"},
#endif
	{-1,			"Unknown!"}
};

	void
mch_write(s, len)
	char_u	*s;
	int		len;
{
#ifdef USE_GUI
	if (gui.in_use && !gui.dying)
	{
		gui_write(s, len);
		if (p_wd)
			gui_mch_wait_for_chars(p_wd);
	}
	else
#endif
	{
		write(1, (char *)s, len);
		if (p_wd)			/* Unix is too fast, slow down a bit more */
			RealWaitForChar(0, p_wd);
	}
}

/*
 * mch_inchar(): low level input funcion.
 * Get a characters from the keyboard.
 * Return the number of characters that are available.
 * If wtime == 0 do not wait for characters.
 * If wtime == n wait a short time for characters.
 * If wtime == -1 wait forever for characters.
 */
	int
mch_inchar(buf, maxlen, wtime)
	char_u	*buf;
	int		maxlen;
	long	wtime;			/* don't use "time", MIPS cannot handle it */
{
	int			len;

#ifdef USE_GUI
	if (gui.in_use)
	{
		if (!gui_mch_wait_for_chars(wtime))
			return 0;
		return Read(buf, (long)maxlen);
	}
#endif

	if (wtime >= 0)
	{
		while (WaitForChar(wtime) == 0)		/* no character available */
		{
			if (!do_resize)			/* return if not interrupted by resize */
				return 0;
			set_winsize(0, 0, FALSE);
			do_resize = FALSE;
		}
	}
	else		/* wtime == -1 */
	{
	/*
	 * If there is no character available within 'updatetime' seconds
	 * flush all the swap files to disk
	 * Also done when interrupted by SIGWINCH.
	 */
		if (WaitForChar(p_ut) == 0)
			updatescript(0);
	}

	for (;;)	/* repeat until we got a character */
	{
		if (do_resize)		/* window changed size */
		{
			set_winsize(0, 0, FALSE);
			do_resize = FALSE;
		}
		/* 
		 * we want to be interrupted by the winch signal
		 */
		WaitForChar(-1L);
		if (do_resize)		/* interrupted by SIGWINCHsignal */
			continue;

		/*
		 * For some terminals we only get one character at a time.
		 * We want the get all available characters, so we could keep on
		 * trying until none is available
		 * For some other terminals this is quite slow, that's why we don't do
		 * it.
		 */
		len = Read(buf, (long)maxlen);
		if (len > 0)
		{
#ifdef OS2
			int i;

			for (i = 0; i < len; i++)
				if (buf[i] == 0)
					buf[i] = K_NUL;
#endif
			return len;
		}
	}
}

/*
 * return non-zero if a character is available
 */
	int
mch_char_avail()
{
#ifdef USE_GUI
	if (gui.in_use)
	{
		gui_mch_update();
		return !is_input_buf_empty();
	}
#endif
	return WaitForChar(0L);
}

	long
mch_avail_mem(special)
	int special;
{
#ifdef __EMX__
	return ulimit(3, 0L);	/* always 32MB? */
#else
 	return 0x7fffffff;		/* virtual memory eh */
#endif
}

	void
mch_delay(msec, ignoreinput)
	long		msec;
	int			ignoreinput;
{
	if (ignoreinput)
#ifndef HAVE_SELECT
		poll(NULL, 0, (int)msec);
#else
# ifdef __EMX__
	_sleep2(msec);
# else
	{
		struct timeval tv;

		tv.tv_sec = msec / 1000;
		tv.tv_usec = (msec % 1000) * 1000;
		select(0, NULL, NULL, NULL, &tv);
	}
# endif	/* __EMX__ */
#endif	/* HAVE_SELECT */
	else
#ifdef USE_GUI
		if (gui.in_use)
			gui_mch_wait_for_chars(msec);
		else
#endif
			WaitForChar(msec);
}

#if defined(SIGWINCH)
/*
 * We need correct potatotypes, otherwise mean compilers will barf when the
 * second argument to signal() is ``wrong''.
 * Let me try it with a few tricky defines from my own osdef.h  (jw).
 */
	static RETSIGTYPE
sig_winch SIGDEFARG(sigarg)
{
	/* this is not required on all systems, but it doesn't hurt anybody */
	signal(SIGWINCH, (RETSIGTYPE (*)())sig_winch);
	do_resize = TRUE;
	SIGRETURN;
}
#endif
 
#if defined(SIGALRM) && defined(HAVE_X11) && defined(WANT_X11)
/*
 * signal function for alarm().
 */
	static RETSIGTYPE
sig_alarm SIGDEFARG(sigarg)
{
	/* doesn't do anything, just to break a system call */
	SIGRETURN;
}
#endif
 
	void
mch_resize()
{
	do_resize = TRUE;
}

/*
 * This function handles deadly signals.
 * It tries to preserve any swap file and exit properly.
 * (partly from Elvis).
 */
	static RETSIGTYPE
deathtrap SIGDEFARG(sigarg)
{
	static int		entered = 0;
#ifdef SIGHASARG
	int		i;

	/* try to find the name of this signal */
	for (i = 0; signal_info[i].sig != -1; i++)
		if (sigarg == signal_info[i].sig)
			break;
	deadly_signal = sigarg;
#endif

	/*
	 * If something goes wrong after entering here, we may get here again.
	 * When this happens, give a message and try to exit nicely (resetting the
	 * terminal mode, etc.)
	 * When this happens twice, just exit, don't even try to give a message,
	 * stack may be corrupt or something weird.
	 */
	if (entered == 2)
	{
		may_core_dump();
		exit(7);
	}
	if (entered++)
	{
		OUTSTR("Vim: Double signal, exiting\n");
		flushbuf();
		getout(1);
	}

	sprintf((char *)IObuff, "Vim: Caught %s %s\n",
#ifdef SIGHASARG
					"deadly signal", signal_info[i].name);
#else
					"some", "deadly signal");
#endif

	preserve_exit();				/* preserve files and exit */

	SIGRETURN;
}

/*
 * If the machine has job control, use it to suspend the program,
 * otherwise fake it by starting a new shell.
 * When running the GUI iconify the window.
 */
	void
mch_suspend()
{
#ifdef USE_GUI
	if (gui.in_use)
	{
		gui_mch_iconify();
		return;
	}
#endif
#ifdef SIGTSTP
	flushbuf();				/* needed to make cursor visible on some systems */
	settmode(0);
	flushbuf();				/* needed to disable mouse on some systems */
	kill(0, SIGTSTP);		/* send ourselves a STOP signal */
	
	/*
	 * Set oldtitle to NULL, so the current title is obtained again.
	 */
	if (oldtitle != fixedtitle)
	{
		vim_free(oldtitle);
		oldtitle = NULL;
	}
	settmode(1);
#else
	MSG_OUTSTR("new shell started\n");
	(void)call_shell(NULL, SHELL_COOKED);
#endif
	need_check_timestamps = TRUE;
}

	void
mch_windinit()
{
	Columns = 80;
	Rows = 24;

	flushbuf();

	(void)mch_get_winsize();

#if defined(SIGWINCH)
	/*
	 * WINDOW CHANGE signal is handled with sig_winch().
	 */
	signal(SIGWINCH, (RETSIGTYPE (*)())sig_winch);
#endif

	/*
	 * We want the STOP signal to work, to make mch_suspend() work
	 */
#ifdef SIGTSTP
	signal(SIGTSTP, SIG_DFL);
#endif

	/*
	 * We want to ignore breaking of PIPEs.
	 */
#ifdef SIGPIPE
	signal(SIGPIPE, SIG_IGN);
#endif

	/*
	 * Arrange for other signals to gracefully shutdown Vim.
	 */
	catch_signals(deathtrap);
}

	static void
catch_signals(func)
	RETSIGTYPE (*func)();
{
	int		i;

	for (i = 0; signal_info[i].sig != -1; i++)
		signal(signal_info[i].sig, func);
}

	void
reset_signals()
{
	catch_signals(SIG_DFL);
}

/*
 * Check_win checks whether we have an interactive window.
 */
	int
mch_check_win(argc, argv)
	int		argc;
	char	**argv;
{
	if (isatty(1))
		return OK;
	return FAIL;
}

	int
mch_check_input()
{
	if (isatty(0))
		return OK;
	return FAIL;
}

#if defined(HAVE_X11) && defined(WANT_X11)
/*
 * X Error handler, otherwise X just exits!  (very rude) -- webb
 */
	static int
x_error_handler(dpy, error_event)
	Display		*dpy;
	XErrorEvent	*error_event;
{
	XGetErrorText(dpy, error_event->error_code, (char *)IObuff, IOSIZE);
	STRCAT(IObuff, "\nVim: Got X error\n");

#if 1
	preserve_exit();				/* preserve files and exit */
#else
	printf(IObuff);					/* print error message and continue */
									/* Makes my system hang */
#endif

	return 0;			/* NOTREACHED */
}

/*
 * Another X Error handler, just used to check for errors.
 */
	static int
x_error_check(dpy, error_event)
	Display	*dpy;
	XErrorEvent	*error_event;
{
	got_x_error = TRUE;
	return 0;
}

/*
 * try to get x11 window and display
 *
 * return FAIL for failure, OK otherwise
 */
	static int
get_x11_windis()
{
	char			*winid;
	XTextProperty	text_prop;
	int				(*old_handler)();
	static int		result = -1;
	static int		x11_display_opened_here = FALSE;

	/* X just exits if it finds an error otherwise! */
	XSetErrorHandler(x_error_handler);

#ifdef USE_GUI_X11
	if (gui.in_use)
	{
		/*
		 * If the X11 display was opened here before, for the window where Vim
		 * was started, close that one now to avoid a memory leak.
		 */
		if (x11_display_opened_here && x11_display != NULL)
		{
			XCloseDisplay(x11_display);
			x11_display = NULL;
			x11_display_opened_here = FALSE;
		}
		return gui_get_x11_windis(&x11_window, &x11_display);
	}
#endif

	if (result != -1)		/* Have already been here and set this */
		return result;		/* Don't do all these X calls again */

	/*
	 * If WINDOWID not set, should try another method to find out
	 * what the current window number is. The only code I know for
	 * this is very complicated.
	 * We assume that zero is invalid for WINDOWID.
	 */
	if (x11_window == 0 && (winid = getenv("WINDOWID")) != NULL) 
		x11_window = (Window)atol(winid);
	if (x11_window != 0 && x11_display == NULL)
	{
#ifdef SIGALRM
		RETSIGTYPE (*sig_save)();

		/*
		 * Opening the Display may hang if the DISPLAY setting is wrong, or
		 * the network connection is bad.  Set an alarm timer to get out.
		 */
		sig_save = (RETSIGTYPE (*)())signal(SIGALRM,
												 (RETSIGTYPE (*)())sig_alarm);
		alarm(2);
#endif
		x11_display = XOpenDisplay(NULL);
#ifdef SIGALRM
		alarm(0);
		signal(SIGALRM, (RETSIGTYPE (*)())sig_save);
#endif
		if (x11_display != NULL)
		{
			/*
			 * Try to get the window title.  I don't actually want it yet, so
			 * there may be a simpler call to use, but this will cause the
			 * error handler x_error_check() to be called if anything is wrong,
			 * such as the window pointer being invalid (as can happen when the
			 * user changes his DISPLAY, but not his WINDOWID) -- webb
			 */
			old_handler = XSetErrorHandler(x_error_check);
			got_x_error = FALSE;
			if (XGetWMName(x11_display, x11_window, &text_prop))
				XFree((void *)text_prop.value);
			XSync(x11_display, False);
			if (got_x_error)
			{
				/* Maybe window id is bad */
				x11_window = 0;
				XCloseDisplay(x11_display);
				x11_display = NULL;
			}
			else
				x11_display_opened_here = TRUE;
			XSetErrorHandler(old_handler);
		}
	}
	if (x11_window == 0 || x11_display == NULL)
		return (result = FAIL);
	return (result = OK);
}

/*
 * Determine original x11 Window Title
 */
	static int
get_x11_title(test_only)
	int		test_only;
{
	XTextProperty	text_prop;
	int				retval = FALSE;

	if (get_x11_windis() == OK)
	{
			/* Get window name if any */
		if (XGetWMName(x11_display, x11_window, &text_prop))
		{
			if (text_prop.value != NULL)
			{
				retval = TRUE;
				if (!test_only)
					oldtitle = strsave((char_u *)text_prop.value);
			}
			XFree((void *)text_prop.value);
		}
	}
	if (oldtitle == NULL && !test_only)		/* could not get old title */
		oldtitle = fixedtitle;

	return retval;
}

/*
 * Determine original x11 Window icon
 */

	static int
get_x11_icon(test_only)
	int		test_only;
{
	XTextProperty	text_prop;
	int				retval = FALSE;

	if (get_x11_windis() == OK)
	{
			/* Get icon name if any */
		if (XGetWMIconName(x11_display, x11_window, &text_prop))
		{
			if (text_prop.value != NULL)
			{
				retval = TRUE;
				if (!test_only)
					oldicon = strsave((char_u *)text_prop.value);
			}
			XFree((void *)text_prop.value);
		}
	}

		/* could not get old icon, use terminal name */
	if (oldicon == NULL && !test_only)
	{
		if (STRNCMP(term_strings[KS_NAME], "builtin_", 8) == 0)
			oldicon = term_strings[KS_NAME] + 8;
		else
			oldicon = term_strings[KS_NAME];
	}

	return retval;
}

/*
 * Set x11 Window Title
 *
 * get_x11_windis() must be called before this and have returned OK
 */
    static void
set_x11_title(title)
    char_u      *title;
{
#if XtSpecificationRelease >= 4
    XTextProperty text_prop;

    text_prop.value = title;
    text_prop.nitems = STRLEN(title);
    text_prop.encoding = XA_STRING;
    text_prop.format = 8;
    XSetWMName(x11_display, x11_window, &text_prop);
#else
    XStoreName(x11_display, x11_window, (char *)title);
#endif
    XFlush(x11_display);
}

/*
 * Set x11 Window icon
 *
 * get_x11_windis() must be called before this and have returned OK
 */
    static void
set_x11_icon(icon)
    char_u      *icon;
{
#if XtSpecificationRelease >= 4
    XTextProperty text_prop;

    text_prop.value = icon;
    text_prop.nitems = STRLEN(icon);
    text_prop.encoding = XA_STRING;
    text_prop.format = 8;
    XSetWMIconName(x11_display, x11_window, &text_prop);
#else
    XSetIconName(x11_display, x11_window, (char *)icon);
#endif
    XFlush(x11_display);
}

#else	/* HAVE_X11 && WANT_X11 */

	static int
get_x11_title(test_only)
	int		test_only;
{
	if (!test_only)
		oldtitle = fixedtitle;
	return FALSE;
}

	static int
get_x11_icon(test_only)
	int		test_only;
{
	if (!test_only)
	{
		if (STRNCMP(term_strings[KS_NAME], "builtin_", 8) == 0)
			oldicon = term_strings[KS_NAME] + 8;
		else
			oldicon = term_strings[KS_NAME];
	}
	return FALSE;
}

#endif	/* HAVE_X11 && WANT_X11 */

	int
mch_can_restore_title()
{
#ifdef USE_GUI
	/*
	 * If GUI is (going to be) used, we can always set the window title.
	 * Saves a bit of time, because the X11 display server does not need to be
	 * contacted.
	 */
	if (gui.starting || gui.in_use)
		return TRUE;
#endif
	return get_x11_title(TRUE);
}

	int
mch_can_restore_icon()
{
#ifdef USE_GUI
	/*
	 * If GUI is (going to be) used, we can always set the icon name.
	 * Saves a bit of time, because the X11 display server does not need to be
	 * contacted.
	 */
	if (gui.starting || gui.in_use)
		return TRUE;
#endif
	return get_x11_icon(TRUE);
}

/*
 * Set the window title and icon.
 * Currently only works for x11.
 */
	void
mch_settitle(title, icon)
	char_u *title;
	char_u *icon;
{
	int			type = 0;

	if (term_strings[KS_NAME] == NULL)		/* no terminal name (yet) */
		return;
	if (title == NULL && icon == NULL)		/* nothing to do */
		return;

/*
 * if the window ID and the display is known, we may use X11 calls
 */
#if defined(HAVE_X11) && defined(WANT_X11)
	if (get_x11_windis() == OK)
		type = 1;
#endif

	/*
	 * Note: if terminal is xterm, title is set with escape sequence rather
	 * 		 than x11 calls, because the x11 calls don't always work
	 */
	if (is_xterm(term_strings[KS_NAME]))
		type = 2;

	if (is_iris_ansi(term_strings[KS_NAME]))
		type = 3;

	if (type)
	{
		if (title != NULL)
		{
			if (oldtitle == NULL)				/* first call, save title */
				(void)get_x11_title(FALSE);

			switch(type)
			{
#if defined(HAVE_X11) && defined(WANT_X11)
			case 1:	set_x11_title(title);				/* x11 */
					break;
#endif
			case 2: outstrn((char_u *)"\033]2;");		/* xterm */
					outstrn(title);
					outchar(Ctrl('G'));
					flushbuf();
					break;

			case 3: outstrn((char_u *)"\033P1.y");		/* iris-ansi */
					outstrn(title);
					outstrn((char_u *)"\234");
					flushbuf();
					break;
			}
		}

		if (icon != NULL)
		{
			if (oldicon == NULL)				/* first call, save icon */
				get_x11_icon(FALSE);

			switch(type)
			{
#if defined(HAVE_X11) && defined(WANT_X11)
			case 1:	set_x11_icon(icon);					/* x11 */
					break;
#endif
			case 2: outstrn((char_u *)"\033]1;");		/* xterm */
					outstrn(icon);
					outchar(Ctrl('G'));
					flushbuf();
					break;

			case 3: outstrn((char_u *)"\033P3.y");		/* iris-ansi */
					outstrn(icon);
					outstrn((char_u *)"\234");
					flushbuf();
					break;
			}
		}
	}
}

	int
is_xterm(name)
	char_u *name;
{
	if (name == NULL)
		return FALSE;
	return (vim_strnicmp(name, (char_u *)"xterm", (size_t)5) == 0 ||
						STRCMP(name, "builtin_xterm") == 0);
}

	int
is_iris_ansi(name)
	char_u	*name;
{
	if (name == NULL)
		return FALSE;
	return (vim_strnicmp(name, (char_u *)"iris-ansi", (size_t)9) == 0 ||
						STRCMP(name, "builtin_iris-ansi") == 0);
}

/*
 * Return TRUE if "name" is a terminal for which 'ttyfast' should be set.
 * This should include all windowed terminal emulators.
 */
	int
is_fastterm(name)
	char_u	*name;
{
	if (name == NULL)
		return FALSE;
	if (is_xterm(name) || is_iris_ansi(name))
		return TRUE;
	return (vim_strnicmp(name, (char_u *)"hpterm", (size_t)6) == 0 ||
		    vim_strnicmp(name, (char_u *)"sun-cmd", (size_t)7) == 0 ||
		    vim_strnicmp(name, (char_u *)"screen", (size_t)6) == 0 ||
		    vim_strnicmp(name, (char_u *)"dtterm", (size_t)6) == 0);
}

/*
 * Restore the window/icon title.
 * which is one of:
 *	1  Just restore title
 *  2  Just restore icon
 *	3  Restore title and icon
 */
	void
mch_restore_title(which)
	int which;
{
	mch_settitle((which & 1) ? oldtitle : NULL, (which & 2) ? oldicon : NULL);
}

/*
 * Insert user name in s[len].
 * Return OK if a name found.
 */
	int
mch_get_user_name(s, len)
	char_u	*s;
	int		len;
{
#if defined(HAVE_PWD_H) && defined(HAVE_GETPWUID)
	struct passwd	*pw;
#endif
	uid_t			uid;

	uid = getuid();
#if defined(HAVE_PWD_H) && defined(HAVE_GETPWUID)
	if ((pw = getpwuid(uid)) != NULL &&
								   pw->pw_name != NULL && *pw->pw_name != NUL)
	{
		STRNCPY(s, pw->pw_name, len);
		return OK;
	}
#endif
	sprintf((char *)s, "%d", (int)uid);		/* assumes s is long enough */
	return FAIL;							/* a number is not a name */
}

/*
 * Insert host name is s[len].
 */

#ifdef HAVE_SYS_UTSNAME_H
	void
mch_get_host_name(s, len)
	char_u	*s;
	int		len;
{
    struct utsname vutsname;

    uname(&vutsname);
    STRNCPY(s, vutsname.nodename, len);
}
#else /* HAVE_SYS_UTSNAME_H */

# ifdef HAVE_SYS_SYSTEMINFO_H
#  define gethostname(nam, len) sysinfo(SI_HOSTNAME, nam, len)
# endif

	void
mch_get_host_name(s, len)
	char_u	*s;
	int		len;
{
	gethostname((char *)s, len);
}
#endif /* HAVE_SYS_UTSNAME_H */

/*
 * return process ID
 */
	long
mch_get_pid()
{
	return (long)getpid();
}

#if !defined(HAVE_STRERROR) && defined(USE_GETCWD)
static char *strerror __ARGS((int));

	static char *
strerror(err)
	int err;
{
	extern int		sys_nerr;
	extern char		*sys_errlist[];
	static char		er[20];

	if (err > 0 && err < sys_nerr)
		return (sys_errlist[err]);
	sprintf(er, "Error %d", err);
	return er;
}
#endif

/*
 * Get name of current directory into buffer 'buf' of length 'len' bytes.
 * Return OK for success, FAIL for failure.
 */
	int 
mch_dirname(buf, len)
	char_u	*buf;
	int		len;
{
#if defined(USE_GETCWD)
	if (getcwd((char *)buf, len) == NULL)
	{
	    STRCPY(buf, strerror(errno));
	    return FAIL;
	}
    return OK;
#else
	return (getwd((char *)buf) != NULL ? OK : FAIL);
#endif
}

#ifdef __EMX__
/*
 * Replace all slashes by backslashes.
 */
	static void
slash_adjust(p)
	char_u	*p;
{
	while (*p)
	{
		if (*p == '/')
			*p = '\\';
		++p;
	}
}
#endif

/*
 * Get absolute filename into buffer 'buf' of length 'len' bytes.
 *
 * return FAIL for failure, OK for success
 */
	int 
FullName(fname, buf, len, force)
	char_u *fname, *buf;
	int len;
	int	force;			/* also expand when already absolute path name */
{
	int		l;
#ifdef OS2
	int		only_drive;	/* only a drive letter is specified in file name */
#endif
#ifdef HAVE_FCHDIR
	int		fd = -1;
	static int	dont_fchdir = FALSE;	/* TRUE when fchdir() doesn't work */
#endif
	char_u	olddir[MAXPATHL];
	char_u	*p;
	char_u	c;
	int		retval = OK;

	if (fname == NULL)	/* always fail */
	{
		*buf = NUL;
		return FAIL;
	}

	*buf = 0;
	if (force || !isFullName(fname))	/* if forced or not an absolute path */
	{
		/*
		 * If the file name has a path, change to that directory for a moment,
		 * and then do the getwd() (and get back to where we were).
		 * This will get the correct path name with "../" things.
		 */
#ifdef OS2
		only_drive = 0;
		if (((p = vim_strrchr(fname, '/')) != NULL) ||
		    ((p = vim_strrchr(fname, '\\')) != NULL) ||
		    (((p = vim_strchr(fname,  ':')) != NULL) && ++only_drive))
#else
		if ((p = vim_strrchr(fname, '/')) != NULL)
#endif
		{
#ifdef HAVE_FCHDIR
			/*
			 * Use fchdir() if possible, it's said to be faster and more
			 * reliable.  But on SunOS 4 it might not work.  Check this by
			 * doing a fchdir() right now.
			 */
			if (!dont_fchdir)
			{
				fd = open(".", O_RDONLY | O_EXTRA);
				if (fd >= 0 && fchdir(fd) < 0)
				{
					close(fd);
					fd = -1;
					dont_fchdir = TRUE;		/* don't try again */
				}
			}
#endif
			if (
#ifdef HAVE_FCHDIR
				fd < 0 &&
#endif
							mch_dirname(olddir, MAXPATHL) == FAIL)
			{
				p = NULL;		/* can't get current dir: don't chdir */
				retval = FAIL;
			}
			else
			{
#ifdef OS2
				/*
				 * compensate for case where ':' from "D:" was the only
				 * path separator detected in the file name; the _next_
				 * character has to be removed, and then restored later.
				 */
				if (only_drive)
					p++;
#endif
				c = *p;
				*p = NUL;
				if (vim_chdir((char *)fname))
					retval = FAIL;
				else
					fname = p + 1;
				*p = c;
#ifdef OS2
				if (only_drive)
				{
					p--;
					if (retval != FAIL)
						fname--;
				}
#endif
			}
		}
		if (mch_dirname(buf, len) == FAIL)
		{
			retval = FAIL;
			*buf = NUL;
		}
		l = STRLEN(buf);
		if (l && buf[l - 1] != '/')
			STRCAT(buf, "/");
		if (p != NULL)
		{
#ifdef HAVE_FCHDIR
			if (fd >= 0)
			{
				fchdir(fd);
				close(fd);
			}
			else
#endif
				vim_chdir((char *)olddir);
		}
	}
	STRCAT(buf, fname);
#ifdef OS2
	slash_adjust(buf);
#endif
	return retval;
}

/*
 * return TRUE is fname is an absolute path name
 */
	int
isFullName(fname)
	char_u		*fname;
{
#ifdef __EMX__
	return _fnisabs(fname);
#else
	return (*fname == '/' || *fname == '~');
#endif
}

/*
 * get file permissions for 'name'
 */
	long 
getperm(name)
	char_u *name;
{
	struct stat statb;

	if (stat((char *)name, &statb))
		return -1;
	return statb.st_mode;
}

/*
 * set file permission for 'name' to 'perm'
 *
 * return FAIL for failure, OK otherwise
 */
	int
setperm(name, perm)
	char_u *name;
	int perm;
{
	return (chmod((char *)name, (mode_t)perm) == 0 ? OK : FAIL);
}

/*
 * return TRUE if "name" is a directory
 * return FALSE if "name" is not a directory
 * return FALSE for error
 */
	int 
mch_isdir(name)
	char_u *name;
{
	struct stat statb;

	if (stat((char *)name, &statb))
		return FALSE;
#ifdef _POSIX_SOURCE
	return (S_ISDIR(statb.st_mode) ? TRUE : FALSE);
#else
	return ((statb.st_mode & S_IFMT) == S_IFDIR ? TRUE : FALSE);
#endif
}

	void
mch_windexit(r)
	int r;
{
	settmode(0);
	exiting = TRUE;
	mch_settitle(oldtitle, oldicon);	/* restore xterm title */
	stoptermcap();
	outchar('\n');
	flushbuf();
	ml_close_all(TRUE); 				/* remove all memfiles */
	may_core_dump();
	exit(r);
}

	static void
may_core_dump()
{
	if (deadly_signal != 0)
	{
		signal(deadly_signal, SIG_DFL);
		kill(getpid(), deadly_signal);	/* Die using the signal we caught */
	}
}

static int curr_tmode = 0;	/* contains current raw/cooked mode (0 = cooked) */

	void
mch_settmode(raw)
	int				raw;
{
	static int first = TRUE;

	/* Why is NeXT excluded here (and not in unixunix.h)? */
#if defined(ECHOE) && defined(ICANON) && (defined(HAVE_TERMIO_H) || defined(HAVE_TERMIOS_H)) && !defined(__NeXT__)
	/* for "new" tty systems */
# ifdef HAVE_TERMIOS_H
	static struct termios told;
		   struct termios tnew;
# else
	static struct termio told;
		   struct termio tnew;
# endif

# ifdef TIOCLGET
	static unsigned long tty_local;
# endif

	if (raw)
	{
		if (first)
		{
			first = FALSE;
# ifdef TIOCLGET
			ioctl(0, TIOCLGET, &tty_local);
# endif
# if defined(HAVE_TERMIOS_H)
			tcgetattr(0, &told);
# else
			ioctl(0, TCGETA, &told);
# endif
		}
		tnew = told;
		/*
		 * ICRNL enables typing ^V^M
		 */
		tnew.c_iflag &= ~ICRNL;
		tnew.c_lflag &= ~(ICANON | ECHO | ISIG | ECHOE
# if defined(IEXTEN) && !defined(MINT)
					| IEXTEN		/* IEXTEN enables typing ^V on SOLARIS */
									/* but it breaks function keys on MINT */
# endif
								);
# ifdef ONLCR		/* don't map NL -> CR NL, we do it ourselves */
		tnew.c_oflag &= ~ONLCR;
# endif
		tnew.c_cc[VMIN] = 1;			/* return after 1 char */
		tnew.c_cc[VTIME] = 0;			/* don't wait */
# if defined(HAVE_TERMIOS_H)
		tcsetattr(0, TCSANOW, &tnew);
# else
		ioctl(0, TCSETA, &tnew);
# endif
	}
	else
	{
# if defined(HAVE_TERMIOS_H)
		tcsetattr(0, TCSANOW, &told);
# else
		ioctl(0, TCSETA, &told);
# endif
# ifdef TIOCLGET
		ioctl(0, TIOCLSET, &tty_local);
# endif
	}
#else
# ifndef TIOCSETN
#  define TIOCSETN TIOCSETP		/* for hpux 9.0 */
# endif
	/* for "old" tty systems */
	static struct sgttyb ttybold;
		   struct sgttyb ttybnew;

	if (raw)
	{
		if (first)
		{
			first = FALSE;
			ioctl(0, TIOCGETP, &ttybold);
		}
		ttybnew = ttybold;
		ttybnew.sg_flags &= ~(CRMOD | ECHO);
		ttybnew.sg_flags |= RAW;
		ioctl(0, TIOCSETN, &ttybnew);
	}
	else
		ioctl(0, TIOCSETN, &ttybold);
#endif
	curr_tmode = raw;
}

/*
 * Try to get the code for "t_kb" from the stty setting
 *
 * Even if termcap claims a backspace key, the user's setting *should*
 * prevail.  stty knows more about reality than termcap does, and if
 * somebody's usual erase key is DEL (which, for most BSD users, it will
 * be), they're going to get really annoyed if their erase key starts
 * doing forward deletes for no reason. (Eric Fischer)
 */
	void
get_stty()
{
	char_u	buf[2];
	char_u	*p;

	/* Why is NeXT excluded here (and not in unixunix.h)? */
#if defined(ECHOE) && defined(ICANON) && (defined(HAVE_TERMIO_H) || defined(HAVE_TERMIOS_H)) && !defined(__NeXT__)
	/* for "new" tty systems */
# ifdef HAVE_TERMIOS_H
	struct termios keys;
# else
	struct termio keys;
# endif

# if defined(HAVE_TERMIOS_H)
	if (tcgetattr(0, &keys) != -1)
# else
	if (ioctl(0, TCGETA, &keys) != -1)
# endif
	{
		buf[0] = keys.c_cc[VERASE];
#else
	/* for "old" tty systems */
	struct sgttyb keys;

	if (ioctl(0, TIOCGETP, &keys) != -1)
	{
		buf[0] = keys.sg_erase;
#endif
		buf[1] = NUL;
		add_termcode((char_u *)"kb", buf);

		/*
		 * If <BS> and <DEL> are now the same, redefine <DEL>.
		 */
		p = find_termcode((char_u *)"kD");
		if (p != NULL && p[0] == buf[0] && p[1] == buf[1])
			do_fixdel();
	}
#if 0
	}		/* to keep cindent happy */
#endif
}

#ifdef USE_MOUSE
/*
 * set mouse clicks on or off (only works for xterms)
 */
	void
mch_setmouse(on)
	int		on;
{
	static int	ison = FALSE;

	if (on == ison)		/* return quickly if nothing to do */
		return;

	if (is_xterm(term_strings[KS_NAME]))
	{
		if (on)
			outstrn((char_u *)"\033[?1000h"); /* xterm: enable mouse events */
		else
			outstrn((char_u *)"\033[?1000l"); /* xterm: disable mouse events */
	}
	ison = on;
}
#endif

/*
 * set screen mode, always fails.
 */
	int
mch_screenmode(arg)
	char_u	 *arg;
{
	EMSG("Screen mode setting not supported");
	return FAIL;
}

/*
 * Try to get the current window size:
 * 1. with an ioctl(), most accurate method
 * 2. from the environment variables LINES and COLUMNS
 * 3. from the termcap
 * 4. keep using the old values
 */
	int
mch_get_winsize()
{
	int			old_Rows = Rows;
	int			old_Columns = Columns;
	char_u		*p;

#ifdef USE_GUI
	if (gui.in_use)
		return gui_mch_get_winsize();
#endif

	Columns = 0;
	Rows = 0;

/*
 * For OS/2 use _scrsize().
 */
# ifdef __EMX__
	{
		int s[2];
		_scrsize(s);
		Columns = s[0];
		Rows = s[1];
	}
# endif

/*
 * 1. try using an ioctl. It is the most accurate method.
 *
 * Try using TIOCGWINSZ first, some systems that have it also define TIOCGSIZE
 * but don't have a struct ttysize.
 */
# ifdef TIOCGWINSZ
	{
		struct winsize	ws;

	    if (ioctl(0, TIOCGWINSZ, &ws) == 0)
	    {
			Columns = ws.ws_col;
			Rows = ws.ws_row;
	    }
	}
# else /* TIOCGWINSZ */
#  ifdef TIOCGSIZE
	{
		struct ttysize	ts;

	    if (ioctl(0, TIOCGSIZE, &ts) == 0)
	    {
			Columns = ts.ts_cols;
			Rows = ts.ts_lines;
	    }
	}
#  endif /* TIOCGSIZE */
# endif /* TIOCGWINSZ */

/*
 * 2. get size from environment
 */
	if (Columns == 0 || Rows == 0)
	{
	    if ((p = (char_u *)getenv("LINES")))
			Rows = atoi((char *)p);
	    if ((p = (char_u *)getenv("COLUMNS")))
			Columns = atoi((char *)p);
	}

#ifdef HAVE_TGETENT
/*
 * 3. try reading the termcap
 */
	if (Columns == 0 || Rows == 0)
		getlinecol();	/* get "co" and "li" entries from termcap */
#endif

/*
 * 4. If everything fails, use the old values
 */
	if (Columns <= 0 || Rows <= 0)
	{
		Columns = old_Columns;
		Rows = old_Rows;
		return FAIL;
	}

	check_winsize();

/* if size changed: screenalloc will allocate new screen buffers */
	return OK;
}

	void
mch_set_winsize()
{
	char_u	string[10];

#ifdef USE_GUI
	if (gui.in_use)
	{
		gui_mch_set_winsize();
		return;
	}
#endif

	/* try to set the window size to Rows and Columns */
	if (is_iris_ansi(term_strings[KS_NAME]))
	{
		sprintf((char *)string, "\033[203;%ld;%ld/y", Rows, Columns);
		outstrn(string);
		flushbuf();
		screen_start();					/* don't know where cursor is now */
	}
}

	int 
call_shell(cmd, options)
	char_u	*cmd;
	int		options;		/* SHELL_FILTER if called by do_filter() */
							/* SHELL_COOKED if term needs cooked mode */
							/* SHELL_EXPAND if called by ExpandWildCards() */
{
#ifdef USE_SYSTEM		/* use system() to start the shell: simple but slow */

	int		x;
#ifndef __EMX__
	char_u	newcmd[1024];	/* only needed for unix */
#else /* __EMX__ */
	/*
	 * Set the preferred shell in the EMXSHELL environment variable (but
	 * only if it is different from what is already in the environment).
	 * Emx then takes care of whether to use "/c" or "-c" in an
	 * intelligent way. Simply pass the whole thing to emx's system() call.
	 * Emx also starts an interactive shell if system() is passed an empty
	 * string.
	 */
	char_u *p, *old;

	if (((old = getenv("EMXSHELL")) == NULL) || strcmp(old, p_sh))
	{
		/* should check HAVE_SETENV, but I know we don't have it. */
		p = alloc(10 + strlen(p_sh));
		if (p)
		{
			sprintf(p, "EMXSHELL=%s", p_sh);
			putenv(p);	/* don't free the pointer! */
		}
	}
#endif

	flushbuf();

	if (options & SHELL_COOKED)
		settmode(0); 				/* set to cooked mode */

#ifdef __EMX__
	if (cmd == NULL)
		x = system("");	/* this starts an interactive shell in emx */
	else
		x = system(cmd);
	if (x == -1) /* system() returns -1 when error occurs in starting shell */
	{
		MSG_OUTSTR("\nCannot execute shell ");
		msg_outstr(p_sh);
		msg_outchar('\n');
	}
#else /* not __EMX__ */
	if (cmd == NULL)
		x = system(p_sh);
	else
	{
		sprintf(newcmd, "%s %s %s \"%s\"", p_sh,
					extra_shell_arg == NULL ? "" : (char *)extra_shell_arg,
					(char *)p_shcf,
					(char *)cmd);
		x = system(newcmd);
	}
	if (x == 127)
	{
 		MSG_OUTSTR("\nCannot execute shell sh\n");
	}
#endif	/* __EMX__ */
	else if (x && !expand_interactively)
	{
		msg_outchar('\n');
		msg_outnum((long)x);
		MSG_OUTSTR(" returned\n");
	}

	settmode(1); 						/* set to raw mode */
#ifdef OS2
	/* external command may change the window size in OS/2, so check it */
	mch_get_winsize();
#endif
	resettitle();
	return (x ? FAIL : OK);

#else /* USE_SYSTEM */		/* don't use system(), use fork()/exec() */

#define EXEC_FAILED 122		/* Exit code when shell didn't execute.  Don't use
							   127, some shell use that already */

	char_u	newcmd[1024];
	int		pid;
#ifdef HAVE_UNION_WAIT
	union wait status;
#else
	int		status = -1;
#endif
	int		retval = FAIL;
	char	**argv = NULL;
	int		argc;
	int		i;
	char_u	*p;
	int		inquote;
#ifdef USE_GUI
	int		pty_master_fd = -1;		/* for pty's */
	int		pty_slave_fd = -1;
	char	*tty_name;
	int		fd_toshell[2];			/* for pipes */
	int		fd_fromshell[2];
	int		pipe_error = FALSE;
# ifdef HAVE_SETENV
	char	envbuf[50];
# else
	static char	envbuf_Rows[20];
	static char	envbuf_Columns[20];
# endif
#endif
	int		did_settmode = FALSE;	/* TRUE when settmode(1) called */

	flushbuf();
	if (options & SHELL_COOKED)
		settmode(0);			/* set to cooked mode */

	/*
	 * 1: find number of arguments
	 * 2: separate them and built argv[]
	 */
	STRCPY(newcmd, p_sh);
	for (i = 0; i < 2; ++i)	
	{
		p = newcmd;
		inquote = FALSE;
		argc = 0;
		for (;;)
		{
			if (i == 1)
				argv[argc] = (char *)p;
			++argc;
			while (*p && (inquote || (*p != ' ' && *p != TAB)))
			{
				if (*p == '"')
					inquote = !inquote;
				++p;
			}
			if (*p == NUL)
				break;
			if (i == 1)
				*p++ = NUL;
			p = skipwhite(p);
		}
		if (i == 0)
		{
			argv = (char **)alloc((unsigned)((argc + 4) * sizeof(char *)));
			if (argv == NULL)		/* out of memory */
				goto error;
		}
	}
	if (cmd != NULL)
	{
		if (extra_shell_arg != NULL)
			argv[argc++] = (char *)extra_shell_arg;
		argv[argc++] = (char *)p_shcf;
		argv[argc++] = (char *)cmd;
	}
	argv[argc] = NULL;

#ifdef tower32
	/*
	 * reap lost children (seems necessary on NCR Tower,
	 * although I don't have a clue why...) (Slootman)
	 */
	while (wait(&status) != 0 && errno != ECHILD)
		;	/* do it again, if necessary */
#endif

#ifdef USE_GUI
/*
 * First try at using a pseudo-tty to get the stdin/stdout of the executed
 * command into the current window for the GUI.
 */

	if (gui.in_use && show_shell_mess)
	{
		/*
		 * Try to open a master pty.
		 * If this works, open the slave pty.
		 * If the slave can't be opened, close the master pty.
		 */
		if (p_guipty)
		{
			pty_master_fd = OpenPTY(&tty_name);		/* open pty */
			if (pty_master_fd >= 0 && ((pty_slave_fd =
									   open(tty_name, O_RDWR | O_EXTRA)) < 0))
			{
				close(pty_master_fd);
				pty_master_fd = -1;
			}
		}
		/*
		 * If opening a pty didn't work, try using pipes.
		 */
		if (pty_master_fd < 0)
		{
			pipe_error = (pipe(fd_toshell) < 0);
			if (!pipe_error)						/* pipe create OK */
			{
				pipe_error = (pipe(fd_fromshell) < 0);
				if (pipe_error)						/* pipe create failed */
				{
					close(fd_toshell[0]);
					close(fd_toshell[1]);
				}
			}
			if (pipe_error)
			{
				MSG_OUTSTR("\nCannot create pipes\n");
				flushbuf();
			}
		}
	}

	if (!pipe_error)					/* pty or pipe opened or not used */
#endif

	{
		if ((pid = fork()) == -1)		/* maybe we should use vfork() */
		{
			MSG_OUTSTR("\nCannot fork\n");
#ifdef USE_GUI
			if (gui.in_use && show_shell_mess)
			{
				if (pty_master_fd >= 0)			/* close the pseudo tty */
				{
					close(pty_master_fd);
					close(pty_slave_fd);
				}
				else							/* close the pipes */
				{
					close(fd_toshell[0]);
					close(fd_toshell[1]);
					close(fd_fromshell[0]);
					close(fd_fromshell[1]);
				}
			}
#endif
		}
		else if (pid == 0)		/* child */
		{
			reset_signals();			/* handle signals normally */
			if (!show_shell_mess)
			{
				int fd;

				/*
				 * Don't want to show any message from the shell.  Can't just
				 * close stdout and stderr though, because some systems will
				 * break if you try to write to them after that, so we must
				 * use dup() to replace them with something else -- webb
				 */
				fd = open("/dev/null", O_WRONLY | O_EXTRA);
				fclose(stdout);
				fclose(stderr);

				/*
				 * If any of these open()'s and dup()'s fail, we just continue
				 * anyway.  It's not fatal, and on most systems it will make
				 * no difference at all.  On a few it will cause the execvp()
				 * to exit with a non-zero status even when the completion
				 * could be done, which is nothing too serious.  If the open()
				 * or dup() failed we'd just do the same thing ourselves
				 * anyway -- webb
				 */
				if (fd >= 0)
				{
					/* To replace stdout (file descriptor 1) */
					dup(fd);

					/* To replace stderr (file descriptor 2) */
					dup(fd);

					/* Don't need this now that we've duplicated it */
					close(fd);
				}
			}
#ifdef USE_GUI
			else if (gui.in_use)
			{

#ifdef HAVE_SETSID
				(void)setsid();
#endif
#ifdef TIOCSCTTY
				/* try to become controlling tty (probably doesn't work,
				 * unless run by root) */
				ioctl(pty_slave_fd, TIOCSCTTY, (char *)NULL);
#endif
				/* Simulate to have a dumb terminal (for now) */
#ifdef HAVE_SETENV
				setenv("TERM", "dumb", 1);
				sprintf((char *)envbuf, "%ld", Rows);
				setenv("ROWS", (char *)envbuf, 1);
				sprintf((char *)envbuf, "%ld", Columns);
				setenv("COLUMNS", (char *)envbuf, 1);
#else
				/*
				 * Putenv does not copy the string, it has to remain valid.
				 * Use a static array to avoid loosing allocated memory.
				 */
				putenv("TERM=dumb");
				sprintf(envbuf_Rows, "ROWS=%ld", Rows);
				putenv(envbuf_Rows);
				sprintf(envbuf_Columns, "COLUMNS=%ld", Columns);
				putenv(envbuf_Columns);
#endif

				if (pty_master_fd >= 0)
				{
					close(pty_master_fd);	/* close master side of pty */

					/* set up stdin/stdout/stderr for the child */
					close(0);
					dup(pty_slave_fd);
					close(1);
					dup(pty_slave_fd);
					close(2);
					dup(pty_slave_fd);

					close(pty_slave_fd);	/* has been dupped, close it now */
				}
				else
				{
					/* set up stdin for the child */
					close(fd_toshell[1]);
					close(0);
					dup(fd_toshell[0]);
					close(fd_toshell[0]);

					/* set up stdout for the child */
					close(fd_fromshell[0]);
					close(1);
					dup(fd_fromshell[1]);
					close(fd_fromshell[1]);

					/* set up stderr for the child */
					close(2);
					dup(1);
				}
			}
#endif
			/*
			 * There is no type cast for the argv, because the type may be
			 * different on different machines. This may cause a warning
			 * message with strict compilers, don't worry about it.
			 */
			execvp(argv[0], argv);
			exit(EXEC_FAILED);		/* exec failed, return failure code */
		}
		else					/* parent */
		{
			/*
			 * While child is running, ignore terminating signals.
			 */
			catch_signals(SIG_IGN);

#ifdef USE_GUI

			/*
			 * For the GUI we redirect stdin, stdout and stderr to our window.
			 */
			if (gui.in_use && show_shell_mess)
			{
#define BUFLEN 100				/* length for buffer, pseudo tty limit is 128 */
				char_u		buffer[BUFLEN];
				int			len;
				int			p_more_save;
				int			old_State;
				int			read_count;
				int			c;
				int			toshell_fd;
				int			fromshell_fd;

				if (pty_master_fd >= 0)
				{
					close(pty_slave_fd);		/* close slave side of pty */
					fromshell_fd = pty_master_fd;
					toshell_fd = dup(pty_master_fd);
				}
				else
				{
					close(fd_toshell[0]);
					close(fd_fromshell[1]);
					toshell_fd = fd_toshell[1];
					fromshell_fd = fd_fromshell[0];
				}

				/*
				 * Write to the child if there are typed characters.
				 * Read from the child if there are characters available.
				 *   Repeat the reading a few times if more characters are
				 *   available. Need to check for typed keys now and then, but
				 *   not too often (delays when no chars are available).
				 * This loop is quit if no characters can be read from the pty
				 * (WaitForChar detected special condition), or there are no
				 * characters available and the child has exited.
				 * Only check if the child has exited when there is no more
				 * output. The child may exit before all the output has
				 * been printed.
				 *
				 * Currently this busy loops!
				 * This can probably dead-lock when the write blocks!
				 */
				p_more_save = p_more;
				p_more = FALSE;
				old_State = State;
				State = EXTERNCMD;		/* don't redraw at window resize */

				for (;;)
				{
					/*
					 * Check if keys have been typed, write them to the child
					 * if there are any.  Don't do this if we are expanding
					 * wild cards (would eat typeahead).
					 */
					if (!(options & SHELL_EXPAND) &&
							  (len = mch_inchar(buffer, BUFLEN - 1, 10)) != 0)
					{
						/*
						 * For pipes:
						 * Check for CTRL-C: sent interrupt signal to child.
						 * Check for CTRL-D: EOF, close pipe to child.
						 */
						if (len == 1 && (pty_master_fd < 0 || cmd != NULL))
						{
#ifdef SIGINT
							if (buffer[0] == Ctrl('C'))
								/* send SIGINT to all processes in our group */
								kill(0, SIGINT);
#endif
							if (pty_master_fd < 0 && toshell_fd >= 0 &&
													   buffer[0] == Ctrl('D'))
							{
								close(toshell_fd);
								toshell_fd = -1;
							}
						}

						/* replace K_BS by <BS> and K_DEL by <DEL> */
						for (i = 0; i < len; ++i)
						{
							if (buffer[i] == CSI && len - i > 2)
							{
								c = TERMCAP2KEY(buffer[i + 1], buffer[i + 2]);
								if (c == K_DEL || c == K_BS)
								{
									vim_memmove(buffer + i + 1, buffer + i + 3,
													   (size_t)(len - i - 2));
									if (c == K_DEL)
										buffer[i] = DEL;
									else
										buffer[i] = Ctrl('H');
									len -= 2;
								}
							}
							else if (buffer[i] == '\r')
								buffer[i] = '\n';
						}

						/*
						 * For pipes: echo the typed characters.
						 * For a pty this does not seem to work.
						 */
						if (pty_master_fd < 0)
						{
							for (i = 0; i < len; ++i)
								if (buffer[i] == '\n' || buffer[i] == '\b')
									msg_outchar(buffer[i]);
								else
									msg_outtrans_len(buffer + i, 1);
							windgoto(msg_row, msg_col);
							flushbuf();
						}

						/*
						 * Write the characters to the child, unless EOF has
						 * been typed for pipes.  Ignore errors.
						 */
						if (toshell_fd >= 0)
							write(toshell_fd, (char *)buffer, (size_t)len);
					}

					/*
					 * Check if the child has any characters to be printed.
					 * Read them and write them to our window.
					 * Repeat this a few times as long as there is something
					 * to do, avoid the 10ms wait for mch_inchar().
					 * TODO: This should handle escape sequences.
					 */
					for (read_count = 0; read_count < 10 &&
							 RealWaitForChar(fromshell_fd, 10); ++read_count)
					{
						len = read(fromshell_fd, (char *)buffer,
														(size_t)(BUFLEN - 1));
						if (len <= 0)				/* end of file or error */
							goto finished;
						buffer[len] = NUL;
						msg_outstr(buffer);
						windgoto(msg_row, msg_col);
						cursor_on();
						flushbuf();
					}

					/*
					 * Check if the child still exists when we finished
					 * outputting all characters.
					 */
					if (read_count == 0 &&
#ifdef __NeXT__
							wait4(pid, &status, WNOHANG, (struct rusage *) 0) &&
#else
							waitpid(pid, &status, WNOHANG) &&
#endif
															WIFEXITED(status))
						break;
				}
finished:
				p_more = p_more_save;
				State = old_State;
				if (toshell_fd >= 0)
					close(toshell_fd);
				close(fromshell_fd);
			}
#endif /* USE_GUI */

			/*
			 * Wait until child has exited.
			 */
#ifdef ECHILD
			/* Don't stop waiting when a signal (e.g. SIGWINCH) is received. */
			while (wait(&status) == -1 && errno != ECHILD)
				;
#else
			wait(&status);
#endif
			/*
			 * Set to raw mode right now, otherwise a CTRL-C after
			 * catch_signals will kill Vim.
			 */
			settmode(1);
			did_settmode = TRUE;
			catch_signals(deathtrap);

			/*
			 * Check the window size, in case it changed while executing the
			 * external command.
			 */
			mch_get_winsize();

			if (WIFEXITED(status))
			{
				i = WEXITSTATUS(status);
				if (i)
				{
					if (i == EXEC_FAILED)
					{
						MSG_OUTSTR("\nCannot execute shell ");
						msg_outtrans(p_sh);
						msg_outchar('\n');
					}
					else if (!expand_interactively)
					{
						msg_outchar('\n');
						msg_outnum((long)i);
						MSG_OUTSTR(" returned\n");
					}
				}
				else
					retval = OK;
			}
			else
				MSG_OUTSTR("\nCommand terminated\n");
		}
	}
	vim_free(argv);

error:
	if (!did_settmode)
		settmode(1); 						/* always set to raw mode */
	resettitle();

	return retval;

#endif /* USE_SYSTEM */
}

/*
 * The input characters are buffered to be able to check for a CTRL-C.
 * This should be done with signals, but I don't know how to do that in
 * a portable way for a tty in RAW mode.
 */

/*
 * Internal typeahead buffer.  Includes extra space for long key code
 * descriptions which would otherwise overflow.  The buffer is considered full
 * when only this extra space (or part of it) remains.
 */
#define INBUFLEN 250

static char_u	inbuf[INBUFLEN + MAX_KEY_CODE_LEN];
static int		inbufcount = 0;		/* number of chars in inbuf[] */

/*
 * is_input_buf_full(), is_input_buf_empty(), add_to_input_buf(), and
 * trash_input_buf() are functions for manipulating the input buffer.  These
 * are used by the gui_* calls when a GUI is used to handle keyboard input.
 *
 * NOTE: These functions will be identical in msdos.c etc, and should probably
 * be taken out and put elsewhere, but at the moment inbuf is only local.
 */

	int
is_input_buf_full()
{
	return (inbufcount >= INBUFLEN);
}

	int
is_input_buf_empty()
{
	return (inbufcount == 0);
}

/* Add the given bytes to the input buffer */
	void
add_to_input_buf(s, len)
	char_u	*s;
	int		len;
{
	if (inbufcount + len > INBUFLEN + MAX_KEY_CODE_LEN)
		return;		/* Shouldn't ever happen! */
	
	while (len--)
		inbuf[inbufcount++] = *s++;
}

/* Remove everything from the input buffer.  Called when ^C is found */
	void
trash_input_buf()
{
	inbufcount = 0;
}

	static int
Read(buf, maxlen)
	char_u	*buf;
	long	maxlen;
{
	if (inbufcount == 0)		/* if the buffer is empty, fill it */
		fill_inbuf(TRUE);
	if (maxlen > inbufcount)
		maxlen = inbufcount;
	vim_memmove(buf, inbuf, (size_t)maxlen);
	inbufcount -= maxlen;
	if (inbufcount)
		vim_memmove(inbuf, inbuf + maxlen, (size_t)inbufcount);
	return (int)maxlen;
}

	void
mch_breakcheck()
{
#ifdef USE_GUI
	if (gui.in_use)
	{
		gui_mch_update();
		return;
	}
#endif /* USE_GUI */

/*
 * Check for CTRL-C typed by reading all available characters.
 * In cooked mode we should get SIGINT, no need to check.
 */
	if (curr_tmode && RealWaitForChar(0, 0L))	/* if characters available */
		fill_inbuf(FALSE);
}

	static void
fill_inbuf(exit_on_error)
	int	exit_on_error;
{
	int		len;
	int		try;

#ifdef USE_GUI
	if (gui.in_use)
	{
		gui_mch_update();
		return;
	}
#endif
	if (is_input_buf_full())
		return;
	/*
	 * Fill_inbuf() is only called when we really need a character.
	 * If we can't get any, but there is some in the buffer, just return.
	 * If we can't get any, and there isn't any in the buffer, we give up and
	 * exit Vim.
	 */
	for (try = 0; try < 100; ++try)
	{
		len = read(0, (char *)inbuf + inbufcount,
											 (size_t)(INBUFLEN - inbufcount));
		if (len > 0)
			break;
		if (!exit_on_error)
			return;
	}
	if (len <= 0)
	{
		windgoto((int)Rows - 1, 0);
		fprintf(stderr, "Vim: Error reading input, exiting...\n");
		ml_sync_all(FALSE, TRUE);		/* preserve all swap files */
		getout(1);
	}
	while (len-- > 0)
	{
		/*
		 * if a CTRL-C was typed, remove it from the buffer and set got_int
		 */
		if (inbuf[inbufcount] == 3)
		{
			/* remove everything typed before the CTRL-C */
			vim_memmove(inbuf, inbuf + inbufcount, (size_t)(len + 1));
			inbufcount = 0;
			got_int = TRUE;
		}
		++inbufcount;
	}
}

/* 
 * Wait "msec" msec until a character is available from the keyboard or from
 * inbuf[]. msec == -1 will block forever.
 * When a GUI is being used, this will never get called -- webb 
 */

	static	int
WaitForChar(msec)
	long	msec;
{
	if (inbufcount)		/* something in inbuf[] */
		return 1;
	return RealWaitForChar(0, msec);
}

/* 
 * Wait "msec" msec until a character is available from file descriptor "fd".
 * Time == -1 will block forever.
 * When a GUI is being used, this will not be used for input -- webb 
 */
	static	int
RealWaitForChar(fd, msec)
	int		fd;
	long	msec;
{
#ifndef HAVE_SELECT
	struct pollfd fds;

	fds.fd = fd;
	fds.events = POLLIN;
	return (poll(&fds, 1, (int)msec) > 0);	/* is this correct when fd != 0?? */
#else
	struct timeval tv;
	fd_set rfds, efds;

# ifdef __EMX__
	/* don't check for incoming chars if not in raw mode, because select()
	 * always returns TRUE then (in some version of emx.dll) */
	if (curr_tmode == 0)
		return 0;
# endif

	if (msec >= 0)
    {
   		tv.tv_sec = msec / 1000;
		tv.tv_usec = (msec % 1000) * (1000000/1000);
    }

	/*
	 * Select on ready for reading and exceptional condition (end of file).
	 */
	FD_ZERO(&rfds);	/* calls bzero() on a sun */
	FD_ZERO(&efds);
	FD_SET(fd, &rfds);
#ifndef __QNX__
	/* For QNX select() always returns 1 if this is set.  Why? */
	FD_SET(fd, &efds);
#endif
	return (select(fd + 1, &rfds, NULL, &efds, (msec >= 0) ? &tv : NULL) > 0);
#endif
}

/*
 * ExpandWildCards() - this code does wild-card pattern matching using the shell
 *
 * return OK for success, FAIL for error (you may lose some memory) and put
 * an error message in *file.
 *
 * num_pat is number of input patterns
 * pat is array of pointers to input patterns
 * num_file is pointer to number of matched file names
 * file is pointer to array of pointers to matched file names
 * On Unix we do not check for files only yet
 * list_notfound is ignored
 */

#ifndef SEEK_SET
# define SEEK_SET 0
#endif
#ifndef SEEK_END
# define SEEK_END 2
#endif

	int
ExpandWildCards(num_pat, pat, num_file, file, files_only, list_notfound)
	int 			num_pat;
	char_u		  **pat;
	int 		   *num_file;
	char_u		 ***file;
	int				files_only;
	int				list_notfound;
{
	int		i;
	size_t	len;
	char_u	*p;
#ifdef __EMX__
# define EXPL_ALLOC_INC	16
	char_u	**expl_files;
	size_t	files_alloced, files_free;
	char_u	*buf;
	int		has_wildcard;

	*num_file = 0;		/* default: no files found */
	files_alloced = EXPL_ALLOC_INC;	/* how much space is allocated */
	files_free = EXPL_ALLOC_INC;	/* how much space is not used  */
	*file = (char_u **) alloc(sizeof(char_u **) * files_alloced);
	if (*file == NULL)
		return FAIL;

	for (; num_pat > 0; num_pat--, pat++)
	{
		expl_files = NULL;
		if (vim_strchr(*pat, '$') || vim_strchr(*pat, '~'))
		{
			/* expand environment var or home dir */
			buf = alloc(MAXPATHL);
			if (buf == NULL)
				return FAIL;
			expand_env(*pat, buf, MAXPATHL);
		}
		else
		{
			buf = strsave(*pat);
		}
        expl_files = NULL;
		has_wildcard = mch_has_wildcard(buf);  /* (still) wildcards in there? */
		if (has_wildcard)   /* yes, so expand them */
			expl_files = (char_u **)_fnexplode(buf);
        /*
         * return value of buf if no wildcards left,
         * OR if no match AND list_notfound is true.
         */
		if (!has_wildcard || (expl_files == NULL && list_notfound))
		{	/* simply save the current contents of *buf */
			expl_files = (char_u **)alloc(sizeof(char_u **) * 2);
			if (expl_files != NULL)
			{
				expl_files[0] = strsave(buf);
				expl_files[1] = NULL;
			}
		}
        vim_free(buf);

		/*
		 * Count number of names resulting from expansion,
		 * At the same time add a backslash to the end of names that happen to
		 * be directories, and replace slashes with backslashes.
		 */
		if (expl_files)
		{
			for (i = 0; (p = expl_files[i]) != NULL; i++, (*num_file)++)
			{
				if (--files_free == 0)
				{
					/* need more room in table of pointers */
					files_alloced += EXPL_ALLOC_INC;
					*file = (char_u **) realloc(*file,
												sizeof(char_u **) * files_alloced);
					if (*file == NULL)
					{
						emsg(e_outofmem);
						*num_file = 0;
						return FAIL;
					}
					files_free = EXPL_ALLOC_INC;
				}
				slash_adjust(p);
				if (mch_isdir(p))
				{
					len = strlen(p);
					if (((*file)[*num_file] = alloc(len + 2)) != NULL)
					{
						strcpy((*file)[*num_file], p);
						(*file)[*num_file][len] = '\\';
						(*file)[*num_file][len+1] = 0;
					}
				}
				else
				{
					(*file)[*num_file] = strsave(p);
				}

				/*
				 * Error message already given by either alloc or strsave.
				 * Should return FAIL, but returning OK works also.
				 */
				if ((*file)[*num_file] == NULL)
					break;
			}
		_fnexplodefree((char **)expl_files);
		}
	}
	return OK;

#else /* __EMX__ */

	int		dir;
	char_u	*tempname;
	char_u	*command;
	FILE	*fd;
	char_u	*buffer;
	int		use_glob = FALSE;

	*num_file = 0;		/* default: no files found */
	*file = (char_u **)"";

	/*
	 * If there are no wildcards, just copy the names to allocated memory.
	 * Saves a lot of time, because we don't have to start a new shell.
	 */
	if (!have_wildcard(num_pat, pat))
	{
		*file = (char_u **)alloc(num_pat * sizeof(char_u *));
		if (*file == NULL)
		{
			*file = (char_u **)"";
			return FAIL;
		}
		for (i = 0; i < num_pat; i++)
			(*file)[i] = strsave(pat[i]);
		*num_file = num_pat;
		return OK;
	}

/*
 * get a name for the temp file
 */
	if ((tempname = vim_tempname('o')) == NULL)
	{
		emsg(e_notmp);
	    return FAIL;
	}

/*
 * let the shell expand the patterns and write the result into the temp file
 * If we use csh, glob will work better than echo.
 */
	if ((len = STRLEN(p_sh)) >= 3 && STRCMP(p_sh + len - 3, "csh") == 0)
		use_glob = TRUE;

	len = STRLEN(tempname) + 12;
	for (i = 0; i < num_pat; ++i)		/* count the length of the patterns */
		len += STRLEN(pat[i]) + 3;
	command = alloc(len);
	if (command == NULL)
	{
		vim_free(tempname);
		return FAIL;
	}
	if (use_glob)
		STRCPY(command, "glob >");		/* build the shell command */
	else
		STRCPY(command, "echo >");		/* build the shell command */
	STRCAT(command, tempname);
	for (i = 0; i < num_pat; ++i)
	{
#ifdef USE_SYSTEM
		STRCAT(command, " \"");				/* need extra quotes because we */
		STRCAT(command, pat[i]);			/*   start the shell twice */
		STRCAT(command, "\"");
#else
		STRCAT(command, " ");
		STRCAT(command, pat[i]);
#endif
	}
	if (expand_interactively)
		show_shell_mess = FALSE;
	/*
	 * If we use -f then shell variables set in .cshrc won't get expanded.
	 * vi can do it, so we will too, but it is only necessary if there is a "$"
	 * in one of the patterns, otherwise we can still use the fast option.
	 */
	if (use_glob && !have_dollars(num_pat, pat))	/* Use csh fast option */
		extra_shell_arg = (char_u *)"-f";
	i = call_shell(command, SHELL_EXPAND);		/* execute it */
	extra_shell_arg = NULL;
	show_shell_mess = TRUE;
	vim_free(command);
	if (i == FAIL)							/* call_shell failed */
	{
		vim_remove(tempname);
		vim_free(tempname);
		/*
		 * With interactive completion, the error message is not printed.
		 * However with USE_SYSTEM, I don't know how to turn off error messages
		 * from the shell, so screen may still get messed up -- webb.
		 */
#ifndef USE_SYSTEM
		if (!expand_interactively)
#endif
		{
			must_redraw = CLEAR;			/* probably messed up screen */
			msg_outchar('\n');				/* clear bottom line quickly */
			cmdline_row = Rows - 1;			/* continue on last line */
		}
		return FAIL;
	}

/*
 * read the names from the file into memory
 */
 	fd = fopen((char *)tempname, "r");
	if (fd == NULL)
	{
		emsg2(e_notopen, tempname);
		vim_free(tempname);
		return FAIL;
	}
	fseek(fd, 0L, SEEK_END);
	len = ftell(fd);				/* get size of temp file */
	fseek(fd, 0L, SEEK_SET);
	buffer = alloc(len + 1);
	if (buffer == NULL)
	{
		vim_remove(tempname);
		vim_free(tempname);
		fclose(fd);
		return FAIL;
	}
	i = fread((char *)buffer, 1, len, fd);
	fclose(fd);
	vim_remove(tempname);
	if (i != len)
	{
		emsg2(e_notread, tempname);
		vim_free(tempname);
		vim_free(buffer);
		return FAIL;
	}
	vim_free(tempname);

	if (use_glob)		/* file names are separated with NUL */
	{
		buffer[len] = NUL;				/* make sure the buffers ends in NUL */
		i = 0;
		for (p = buffer; p < buffer + len; ++p)
			if (*p == NUL)				/* count entry */
				++i;
		if (len)
			++i;						/* count last entry */
	}
	else				/* file names are separated with SPACE */
	{
		buffer[len] = '\n';				/* make sure the buffers ends in NL */
		p = buffer;
		for (i = 0; *p != '\n'; ++i)	/* count number of entries */
		{
			while (*p != ' ' && *p != '\n')	/* skip entry */
				++p;
			p = skipwhite(p);			/* skip to next entry */
		}
	}
	if (i == 0)
	{
		/*
		 * Can happen when using /bin/sh and typing ":e $NO_SUCH_VAR^I".
		 * /bin/sh will happily expand it to nothing rather than returning an
		 * error; and hey, it's good to check anyway -- webb.
		 */
		vim_free(buffer);
		*file = (char_u **)"";
		return FAIL;
	}
	*num_file = i;
	*file = (char_u **)alloc(sizeof(char_u *) * i);
	if (*file == NULL)
	{
		vim_free(buffer);
		*file = (char_u **)"";
		return FAIL;
	}
	
	/*
	 * Isolate the individual file names.
	 */
	p = buffer;
	for (i = 0; i < *num_file; ++i)
	{
		(*file)[i] = p;
		if (use_glob)
		{
			while (*p && p < buffer + len)		/* skip entry */
				++p;
			++p;								/* skip NUL */
		}
		else
		{
			while (*p != ' ' && *p != '\n')		/* skip entry */
				++p;
			if (*p == '\n')						/* last entry */
				*p = NUL;
			else
			{
				*p++ = NUL;
				p = skipwhite(p);				/* skip to next entry */
			}
		}
	}

	/*
	 * Move the file names to allocated memory.
	 */
	for (i = 0; i < *num_file; ++i)
	{
		/* Require the files to exist.  Helps when using /bin/sh */
		if (expand_interactively)
		{
			struct stat		st;
			int				j;

			if (stat((char *)((*file)[i]), &st) < 0)
			{
				for (j = i; j + 1 < *num_file; ++j)
					(*file)[j] = (*file)[j + 1];
				--*num_file;
				--i;
				continue;
			}
		}

		/* if file doesn't exist don't add '/' */
		dir = (mch_isdir((*file)[i]));
		p = alloc((unsigned)(STRLEN((*file)[i]) + 1 + dir));
		if (p)
		{
			STRCPY(p, (*file)[i]);
			if (dir)
				STRCAT(p, "/");
		}
		(*file)[i] = p;
	}
	vim_free(buffer);

	if (*num_file == 0)		/* rejected all entries */
	{
		vim_free(*file);
		*file = (char_u **)"";
		return FAIL;
	}

	return OK;

#endif /* __EMX__ */
}

	int
mch_has_wildcard(p)
	char_u	*p;
{
	for ( ; *p; ++p)
	{
		if (*p == '\\' && p[1] != NUL)
			++p;
		else if (vim_strchr((char_u *)"*?[{`~$", *p) != NULL)
			return TRUE;
	}
	return FALSE;
}

#ifndef __EMX__
	static int
have_wildcard(num, file)
	int		num;
	char_u	**file;
{
	register int i;

	for (i = 0; i < num; i++)
		if (mch_has_wildcard(file[i]))
			return 1;
	return 0;
}

	static int
have_dollars(num, file)
	int		num;
	char_u	**file;
{
	register int i;

	for (i = 0; i < num; i++)
		if (vim_strchr(file[i], '$') != NULL)
			return TRUE;
	return FALSE;
}
#endif	/* ifndef __EMX__ */

#ifndef HAVE_RENAME
/*
 * Scaled-down version of rename, which is missing in Xenix.
 * This version can only move regular files and will fail if the
 * destination exists.
 */
	int
rename(src, dest)
	const char *src, *dest;
{
	struct stat		st;

	if (stat(dest, &st) >= 0)		/* fail if destination exists */
		return -1;
	if (link(src, dest) != 0)		/* link file to new name */
		return -1;
	if (vim_remove(src) == 0)		/* delete link to old name */
		return 0;
	return -1;
}
#endif /* !HAVE_RENAME */
