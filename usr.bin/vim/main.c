/*	$OpenBSD: main.c,v 1.5 1996/10/14 03:55:14 downsj Exp $	*/
/* vi:set ts=4 sw=4:
 *
 * VIM - Vi IMproved		by Bram Moolenaar
 *
 * Do ":help uganda"  in Vim to read copying and usage conditions.
 * Do ":help credits" in Vim to see a list of people who contributed.
 */

#define EXTERN
#include "vim.h"
#include "globals.h"
#include "proto.h"
#include "option.h"

#ifdef SPAWNO
# include <spawno.h>			/* special MSDOS swapping library */
#endif

static void usage __PARMS((int, char_u *));
static int stdout_notty = FALSE;			/* is stdout not a terminal? */

/*
 * Types of usage message required.  These must match the array of error
 * messages in usage().
 */
#define USAGE_UNKNOWN_OPTION	0
#define USAGE_TOO_MANY_ARGS		1
#define USAGE_ARG_MISSING		2
#define USAGE_GARBAGE			3

	static void
usage(n, str)
	int		n;
	char_u	*str;
{
	register int i;
	static char_u *(use[]) = {(char_u *)"[file ..]",
							(char_u *)"-t tag",
							(char_u *)"-e [errorfile]"};
	static char_u *(errors[]) =  {(char_u *)"Unknown option",
								(char_u *)"Too many arguments",
								(char_u *)"Argument missing after",
								(char_u *)"Garbage after option",
								};

#if defined(UNIX) || defined(__EMX__)
	reset_signals();		/* kill us with CTRL-C here, if you like */
#endif

	fprintf(stderr, longVersion);
	fprintf(stderr, "\n");
	fprintf(stderr, (char *)errors[n]);
	if (str != NULL)
		fprintf(stderr, ": \"%s\"", str);
	fprintf(stderr, "\nusage:");
	for (i = 0; ; ++i)
	{
		fprintf(stderr, " vim [options] ");
		fprintf(stderr, (char *)use[i]);
		if (i == (sizeof(use) / sizeof(char_u *)) - 1)
			break;
		fprintf(stderr, "\n   or:");
	}

	fprintf(stderr, "\n\nOptions:\n");
#ifdef USE_GUI
	fprintf(stderr, "   -g\t\t\tRun using GUI\n");
	fprintf(stderr, "   -f\t\t\tForeground: Don't fork when starting GUI\n");
#endif
	fprintf(stderr, "   -R  or  -v\t\tReadonly mode (view mode)\n");
	fprintf(stderr, "   -b\t\t\tBinary mode\n");
	fprintf(stderr, "   -l\t\t\tLisp mode\n");
	fprintf(stderr, "   -n\t\t\tNo swap file, use memory only\n");
	fprintf(stderr, "   -r\t\t\tList swap files\n");
	fprintf(stderr, "   -r (with file name)\tRecover crashed session\n");
	fprintf(stderr, "   -L\t\t\tSame as -r\n");
#ifdef AMIGA
	fprintf(stderr, "   -x\t\t\tDon't use newcli to open window\n");
	fprintf(stderr, "   -d <device>\t\tUse <device> for I/O\n");
#endif
#ifdef RIGHTLEFT
	fprintf(stderr, "   -H\t\t\tstart in Hebrew mode\n");
#endif
	fprintf(stderr, "   -T <terminal>\tSet terminal type to <terminal>\n");
	fprintf(stderr, "   -o[N]\t\tOpen N windows (default: one for each file)\n");
	fprintf(stderr, "   +\t\t\tStart at end of file\n");
	fprintf(stderr, "   +<lnum>\t\tStart at line <lnum>\n");
	fprintf(stderr, "   -c <command>\t\tExecute <command> first\n");
	fprintf(stderr, "   -s <scriptin>\tRead commands from script file <scriptin>\n");
	fprintf(stderr, "   -w <scriptout>\tAppend commands to script file <scriptout>\n");
	fprintf(stderr, "   -W <scriptout>\tWrite commands to script file <scriptout>\n");
	fprintf(stderr, "   -u <vimrc>\t\tUse <vimrc> instead of any .vimrc\n");
	fprintf(stderr, "   -i <viminfo>\t\tUse <viminfo> instead of .viminfo\n");
	fprintf(stderr, "   --\t\t\tEnd of options\n");

#ifdef USE_GUI_X11
# ifdef USE_GUI_MOTIF
	fprintf(stderr, "\nOptions recognised by gvim (Motif version):\n");
# else
#  ifdef USE_GUI_ATHENA
	fprintf(stderr, "\nOptions recognised by gvim (Athena version):\n");
#  endif /* USE_GUI_ATHENA */
# endif /* USE_GUI_MOTIF */
	fprintf(stderr, "   -display <display>\tRun vim on <display>\n");
	fprintf(stderr, "   -iconic\t\tStart vim iconified\n");
# if 0
	fprintf(stderr, "   -name <name>\t\tUse resource as if vim was <name>\n");
	fprintf(stderr, "\t\t\t  (Unimplemented)\n");
# endif
	fprintf(stderr, "   -background <color>\tUse <color> for the background (also: -bg)\n");
	fprintf(stderr, "   -foreground <color>\tUse <color> for normal text (also: -fg)\n");
	fprintf(stderr, "   -bold <color>\tUse <color> for bold text\n");
	fprintf(stderr, "   -italic <color>\tUse <color> for italic text\n");
	fprintf(stderr, "   -underline <color>\tUse <color> for underlined text (also: -ul)\n");
	fprintf(stderr, "   -cursor <color>\tUse <color> for cursor\n");
	fprintf(stderr, "   -font <font>\t\tUse <font> for normal text (also: -fn)\n");
	fprintf(stderr, "   -boldfont <font>\tUse <font> for bold text\n");
	fprintf(stderr, "   -italicfont <font>\tUse <font> for italic text\n");
	fprintf(stderr, "   -geometry <geom>\tUse <geom> for initial geometry (also: -geom)\n");
	fprintf(stderr, "   -borderwidth <width>\tUse a border width of <width> (also: -bw)\n");
	fprintf(stderr, "   -scrollbarwidth <width>\tUse a scrollbar width of <width> (also: -sw)\n");
	fprintf(stderr, "   -menuheight <height>\tUse a menu bar height of <height> (also: -mh)\n");
	fprintf(stderr, "   -reverse\t\tUse reverse video (also: -rv)\n");
	fprintf(stderr, "   +reverse\t\tDon't use reverse video (also: +rv)\n");
	fprintf(stderr, "   -xrm <resource>\tSet the specified resource\n");
#endif /* USE_GUI_X11 */

	mch_windexit(1);
}

#ifdef HAVE_LOCALE_H
# include <locale.h>
#endif

	void
main(argc, argv)
	int				argc;
	char		  **argv;
{
	char_u		   *initstr;		/* init string from the environment */
	char_u		   *term = NULL;	/* specified terminal name */
	char_u		   *fname = NULL;	/* file name from command line */
	char_u		   *command = NULL;	/* command from + or -c option */
	char_u		   *tagname = NULL;	/* tag from -t option */
	char_u		   *use_vimrc = NULL;	/* vimrc from -u option */
	int 			c;
	int				doqf = 0;
	int				i;
	int				bin_mode = FALSE;		/* -b option used */
	int				vi_mode = FALSE;		/* run as vi */
	int				window_count = 1;		/* number of windows to use */
	int				arg_idx = 0;			/* index for arg_files[] */
	int				check_version = FALSE;	/* check .vimrc version number */
	int				argv_idx;				/* index in argv[n][] */
	int             invoked_as_ex = FALSE;  /* argv[0] is "ex" */

#if defined(MSDOS) || defined(WIN32) || defined(OS2)
	static struct initmap
	{
		char_u		*arg;
		int			mode;
	} initmappings[] =
	{
		/* normal and visual mode */
#ifdef MSDOS
		{(char_u *)"\316w H", NORMAL+VISUAL},		/* CTRL-HOME is 'H' */
		{(char_u *)"\316u L", NORMAL+VISUAL},		/* CTRL-END is 'L' */
		{(char_u *)"\316\204 1G", NORMAL+VISUAL},	/* CTRL-PageUp is '1G' */
		{(char_u *)"\316v G", NORMAL+VISUAL},		/* CTRL-PageDown is 'G' */
#else /* WIN32 */
		/* Use the Windows (CUA) keybindings */
		{(char_u *)"\316w 1G", NORMAL+VISUAL},		/* CTRL-HOME is '1G' */
		{(char_u *)"\316u G$", NORMAL+VISUAL},		/* CTRL-END is 'G$' */
		{(char_u *)"\316\204 H", NORMAL+VISUAL},	/* CTRL-PageUp is 'H' */
		{(char_u *)"\316v L$", NORMAL+VISUAL},		/* CTRL-PageDown is 'L$' */
		{(char_u *)"\316s B", NORMAL+VISUAL},		/* CTRL-Left is 'B' */
		{(char_u *)"\316t W", NORMAL+VISUAL},		/* CTRL-Right is 'W' */
#endif /* WIN32 */

		/* insert mode */
#ifdef MSDOS
		{(char_u *)"\316w \017H", INSERT},			/* CTRL-HOME is '^OH' */
		{(char_u *)"\316u \017L", INSERT},			/* CTRL-END is '^OL' */
		{(char_u *)"\316\204 \017\061G", INSERT},	/* CTRL-PageUp is '^O1G' */
		{(char_u *)"\316v \017G", INSERT},			/* CTRL-PageDown is '^OG' */
#else /* WIN32 */
		/* Use the Windows (CUA) keybindings */
		{(char_u *)"\316w \017\061G", INSERT},		/* CTRL-HOME is '^O1G' */
		{(char_u *)"\316u \017G\017$", INSERT},		/* CTRL-END is '^OG^O$' */
		{(char_u *)"\316\204 \017H",INSERT},		/* CTRL-PageUp is '^OH'*/
		{(char_u *)"\316v \017L\017$", INSERT},		/* CTRL-PageDown ='^OL^O$'*/
		{(char_u *)"\316s \017B", INSERT},			/* CTRL-Left is '^OB' */
		{(char_u *)"\316t \017W", INSERT},			/* CTRL-Right is '^OW' */
#endif /* WIN32 */
	};
#endif

#ifdef __EMX__
	_wildcard(&argc, &argv);
#endif

#ifdef HAVE_LOCALE_H
	setlocale(LC_ALL, "");		/* for ctype() and the like */
#endif

#ifdef USE_GUI
	gui_prepare(&argc, argv);	/* Prepare for possibly starting GUI sometime */
#endif

/*
 * Check if we have an interactive window.
 * On the Amiga: If there is no window, we open one with a newcli command
 * (needed for :! to * work). mch_check_win() will also handle the -d argument.
 */
	stdout_notty = (mch_check_win(argc, argv) == FAIL);

/*
 * allocate the first window and buffer. Can't do anything without it
 */
	if ((curwin = win_alloc(NULL)) == NULL ||
						(curbuf = buflist_new(NULL, NULL, 1L, FALSE)) == NULL)
		mch_windexit(0);
	curwin->w_buffer = curbuf;
	screen_start();					/* don't know where cursor is yet */

/*
 * Allocate space for the generic buffers (needed for set_init_1()).
 */
	if ((IObuff = alloc(IOSIZE)) == NULL ||
							(NameBuff = alloc(MAXPATHL)) == NULL)
		mch_windexit(0);

/*
 * Set the default values for the options.
 * First find out the home directory, needed to expand "~" in options.
 */
	init_homedir();				/* find real value of $HOME */
	set_init_1();

/*
 * If the executable is called "view" we start in readonly mode.
 */
	if (STRCMP(gettail((char_u *)argv[0]), (char_u *)"view") == 0)
	{
		readonlymode = TRUE;
		curbuf->b_p_ro = TRUE;
		if (p_uc)					/* if we are doing any updating.. */
			p_uc = 10000;			/* ..don't update very often */
	}

/*
 * If the executable is called "ex" we start in ex mode.
 */

	if (STRCMP(gettail((char_u *)argv[0]), (char_u *)"ex") == 0)
	{
		invoked_as_ex = TRUE;
	}

/*
 * If the executable is called "gvim" we run the GUI version.
 */
	if (STRCMP(gettail((char_u *)argv[0]), (char_u *)"gvim") == 0)
	{
#ifdef USE_GUI
		gui.starting = TRUE;
#else
		fprintf(stderr, (char *)e_nogvim);
		mch_windexit(2);
#endif
	}

/*
 * If the executable is called "vi" we switch to compat mode.
 */
 	if (STRCMP(gettail((char_u *)argv[0]), (char_u *)"vi") == 0)
	{
		vi_mode = TRUE;
	}

	++argv;
	/*
	 * Process the command line arguments
	 *		'+{command}'	execute command
	 *		'-b'			binary
	 *		'-c {command}'	execute command
	 *		'-d {device}'	device (for Amiga)
	 *		'-f'			Don't fork when starging GUI. (if USE_GUI defined)
 	 *		'-g'			Run with GUI. (if USE_GUI defined)
	 *		'-H'			Start in right-left mode
	 *		'-i viminfo'	use instead of p_viminfo
	 *		'-n'			no .vim file
	 *		'-o[N]'			open N windows (default: number of files)
	 *		'-r'			recovery mode
	 *		'-L'			recovery mode
	 * 		'-s scriptin'	read from script file
	 *		'-T terminal'	terminal name
	 *		'-u vimrc'		read initializations from a file
	 *		'-v' 			view or Readonly mode
	 *		'-R'			view or Readonly mode
	 *		'-w scriptout'	write to script file (append)
	 *		'-W scriptout'	write to script file (overwrite)
	 *		'-x'			open window directly, not with newcli
	 */
	argv_idx = 1;			/* active option letter is argv[0][argv_idx] */

	while (argc > 1 && ((c = argv[0][0]) == '+' || (c == '-' &&
								   vim_strchr((char_u *)"bcdfgHilLnorRsTuvwWx",
											 c = argv[0][argv_idx]) != NULL)))
	{
		++argv_idx;			/* advance to next option letter by default */
		switch (c)
		{
		case '+': 			/* + or +{number} or +/{pat} or +{command} */
			argv_idx = -1;			/* skip to next argument */
			if (argv[0][1] == NUL)
				command = (char_u *)"$";
			else
				command = (char_u *)&(argv[0][1]);
			break;

		case 'b':
			bin_mode = TRUE;		/* postpone to after reading .exrc files */
			break;

#ifdef USE_GUI
		case 'f':
			gui.dofork = FALSE;		/* don't fork() when starting GUI */
			break;
#endif

		case 'g':
#ifdef USE_GUI
			gui.starting = TRUE;	/* start GUI a bit later */
#else
			fprintf(stderr, (char *)e_nogvim);
			mch_windexit(2);
#endif
			break;

		case 'H':				/* start in Hebrew mode: rl + hkmap set */
#ifdef RIGHTLEFT
			curwin->w_p_rl = p_hkmap = TRUE;
#else
			fprintf(stderr, (char *)e_nohebrew);
			mch_windexit(2);
#endif
			break;

		case 'l':			/* -l: lisp mode, 'lisp' and 'showmatch' on */
			curbuf->b_p_lisp = TRUE;
			p_sm = TRUE;
			break;

		case 'n':
			p_uc = 0;
			break;

		case 'o':
			window_count = 0;		/* default: open window for each file */
			if (isdigit(argv[0][argv_idx]))
			{
				window_count = atoi(&(argv[0][argv_idx]));
				while (isdigit(argv[0][argv_idx]))
					++argv_idx;
			}
			break;

		case 'r':
		case 'L':
			recoverymode = 1;
			break;
		
		case 'v':
		case 'R':
			readonlymode = TRUE;
			curbuf->b_p_ro = TRUE;
			if (p_uc)					/* if we are doing any updating.. */
				p_uc = 10000;			/* ..don't update very often */
			break;

		case 'x':
			break;	/* This is ignored as it is handled in mch_check_win() */


		case 'w':
			if (isdigit(argv[0][argv_idx]))	/* -w{number}; set window height */
			{
				argv_idx = -1;
				break;						/* not implemented, ignored */
			}
			/* FALLTHROUGH */

		default:	/* options with argument */
			/*
			 * Check there's no garbage immediately after the option letter.
			 */
			if (argv[0][argv_idx] != NUL)
				usage(USAGE_GARBAGE, (char_u *)argv[0]);

			--argc;
			if (argc < 2)
				usage(USAGE_ARG_MISSING, (char_u *)argv[0]);
			++argv;
			argv_idx = -1;

			switch (c)
			{
			case 'c':			/* -c {command} */
				command = (char_u *)argv[0];
				break;

		/*	case 'd':	This is ignored as it is handled in mch_check_win() */

			case 'i':			/* -i {viminfo} */
				use_viminfo = (char_u *)argv[0];
				break;
			
			case 's':			/* -s {scriptin} */
				if (scriptin[0] != NULL)
				{
					fprintf(stderr,
							"Attempt to open script file again: \"%s %s\"\n",
							argv[-1], argv[0]);
					mch_windexit(2);
				}
				if ((scriptin[0] = fopen(argv[0], READBIN)) == NULL)
				{
					fprintf(stderr, "Cannot open \"%s\" for reading\n", argv[0]);
					mch_windexit(2);
				}
				break;
			
/*
 * The -T term option is always available and when HAVE_TERMLIB is supported
 * it overrides the environment variable TERM.
 */
			case 'T':			/* -T {terminal} */
				term = (char_u *)argv[0];
				break;
			
			case 'u':			/* -u {vimrc} */
				use_vimrc = (char_u *)argv[0];
				break;
			
			case 'w':			/* -w {scriptout} (append) */
			case 'W':			/* -W {scriptout} (overwrite) */
				if (scriptout != NULL)
				{
					fprintf(stderr,
							"Attempt to open script file again: \"%s %s\"\n",
							argv[-1], argv[0]);
					mch_windexit(2);
				}
				if ((scriptout = fopen(argv[0],
									c == 'w' ? APPENDBIN : WRITEBIN)) == NULL)
				{
					fprintf(stderr, "cannot open \"%s\" for output\n", argv[0]);
					mch_windexit(2);
				}
				break;
			}
		}
		/*
		 * If there are no more letters after the current "-", go to next
		 * argument.  argv_idx is set to -1 when the current argument is to be
		 * skipped.
		 */
		if (argv_idx <= 0 || argv[0][argv_idx] == NUL)
		{
			--argc;
			++argv;
			argv_idx = 1;
		}
	}

	/* note that we may use mch_windexit() before mch_windinit()! */
	mch_windinit();				/* inits Rows and Columns */
/*
 * Set the default values for the options that use Rows and Columns.
 */
	set_init_2();

	firstwin->w_height = Rows - 1;
	cmdline_row = Rows - 1;

	/*
	 * Process the other command line arguments.
	 * -e[errorfile]	quickfix mode
	 * -t[tagname]		jump to tag
	 * [--] [file ..]	file names
	 */
	if (argc > 1)
	{
		if (argv[0][0] == '-' && (argv[0][1] != '-' || argv[0][2] != NUL))
		{
		    switch (argv[0][1])
			{
		  	case 'e':			/* -e QuickFix mode */
				switch (argc)
				{
					case 2:
							if (argv[0][2])	/* -eerrorfile */
								p_ef = (char_u *)argv[0] + 2;
							break;				/* -e */

					case 3:					/* -e errorfile */
							if (argv[0][2] != NUL)
								usage(USAGE_GARBAGE, (char_u *)argv[0]);
							++argv;
							p_ef = (char_u *)argv[0];
							break;

					default:				/* argc > 3: too many arguments */
							usage(USAGE_TOO_MANY_ARGS, NULL);
				}
				doqf = 1;
				break;

			case 't':			/* -t tag  or -ttag */
				switch (argc)
				{
					case 2:
							if (argv[0][2])		/* -ttag */
							{
								tagname = (char_u *)argv[0] + 2;
								break;
							}
							usage(USAGE_ARG_MISSING, (char_u *)argv[0]);
							break;

					case 3:						/* -t tag */
							if (argv[0][2] != NUL)	/* also -ttag?! */
								usage(USAGE_GARBAGE, (char_u *)argv[0]);
							++argv;
							tagname = (char_u *)argv[0];
							break;

					default:				/* argc > 3: too many arguments */
							usage(USAGE_TOO_MANY_ARGS, NULL);
				}
				break;

			default:
				usage(USAGE_UNKNOWN_OPTION, (char_u *)argv[0]);
			}
		}
		else				/* must be a file name */
		{
			/*
			 * Skip a single "--" argument, used in front of a file name that
			 * starts with '-'.
			 */
			if (argc > 2 && STRCMP(argv[0], "--") == 0)
			{
				++argv;
				--argc;
			}

#if (!defined(UNIX) && !defined(__EMX__)) || defined(ARCHIE)
			if (ExpandWildCards(argc - 1, (char_u **)argv, &arg_count,
					&arg_files, TRUE, TRUE) == OK && arg_count != 0)
			{
				fname = arg_files[0];
				arg_exp = TRUE;
			}
#else
			arg_files = (char_u **)argv;
			arg_count = argc - 1;
			fname = (char_u *)argv[0];
#endif
			if (arg_count > 1)
			{
				printf("%d files to edit\n", arg_count);
				screen_start();			/* don't know where cursor is now */
			}
		}
	}

	RedrawingDisabled = TRUE;

	/*
	 * When listing swap file names, don't do cursor positioning et. al.
	 */
	if (recoverymode && fname == NULL)
		full_screen = FALSE;

#ifdef USE_GUI
	/*
	 * We don't want to open the GUI window until after we've read .vimrc,
	 * otherwise we don't know what font we will use, and hence we don't know
	 * what size the window should be.  So if there are errors in the .vimrc
	 * file, they will have to go to the terminal -- webb
	 */
	if (gui.starting)
		full_screen = FALSE;
#endif

	/*
	 * Now print a warning if stdout is not a terminal.
	 */
	if (full_screen && (stdout_notty || mch_check_input() == FAIL))
	{
		if (stdout_notty)
			fprintf(stderr, "Vim: Warning: Output is not to a terminal\n");
		if (mch_check_input() == FAIL)
			fprintf(stderr, "Vim: Warning: Input is not from a terminal\n");
		mch_delay(2000L, TRUE);
		screen_start();			/* don't know where cursor is now */
	}

	curbuf->b_nwindows = 1;		/* there is one window */
	win_init(curwin);			/* init current window */
	init_yank();				/* init yank buffers */
	if (full_screen)
		termcapinit(term);		/* set terminal name and get terminal
								   capabilities */
	screenclear();				/* clear screen (just inits screen structures,
									because starting is TRUE) */

	if (full_screen)
		msg_start();		/* in case a mapping or error message is printed */
	msg_scroll = TRUE;
	no_wait_return = TRUE;

#if defined(MSDOS) || defined(WIN32) || defined(OS2)
/*
 * Default mapping for some often used keys.
 * Need to put string in allocated memory, because do_map() will modify it.
 */
	for (i = 0; i < sizeof(initmappings) / sizeof(struct initmap); ++i)
	{
		initstr = strsave(initmappings[i].arg);
		if (initstr != NULL)
		{
			do_map(0, initstr, initmappings[i].mode);
			vim_free(initstr);
		}
	}
#endif

/*
 * If -u option give, use only the initializations from that file and nothing
 * else.
 */
	if (use_vimrc != NULL)
	{
		if (STRCMP(use_vimrc, "NONE") != 0)
		{
			if (do_source(use_vimrc, FALSE) == OK)
				check_version = TRUE;
			else
				EMSG2("Cannot read from \"%s\"", use_vimrc);
		}
	}
	else
	{

	/*
	 * get system wide defaults (for unix)
	 */
#if defined(HAVE_CONFIG_H) || defined(OS2)
		if (do_source(((vi_mode == TRUE) ? sys_compatrc_fname
		               : sys_vimrc_fname), TRUE) == OK)
			check_version = TRUE;
#endif

	/*
	 * Try to read initialization commands from the following places:
	 * - environment variable VIMINIT
	 * - user vimrc file (s:.vimrc for Amiga, ~/.vimrc for Unix)
	 * - environment variable EXINIT
	 * - user exrc file (s:.exrc for Amiga, ~/.exrc for Unix)
	 * The first that exists is used, the rest is ignored.
	 */
		if ((initstr = vim_getenv((char_u *)"VIMINIT")) != NULL &&
														*initstr != NUL)
		{
			sourcing_name = (char_u *)"VIMINIT";
			do_cmdline(initstr, TRUE, TRUE);
			sourcing_name = NULL;
		}
		else if (do_source((char_u *)USR_VIMRC_FILE, TRUE) == FAIL)
		{
			if ((initstr = vim_getenv((char_u *)"EXINIT")) != NULL)
			{
				sourcing_name = (char_u *)"EXINIT";
				do_cmdline(initstr, TRUE, TRUE);
				sourcing_name = NULL;
			}
			else
				(void)do_source((char_u *)USR_EXRC_FILE, FALSE);
		}
		else
			check_version = TRUE;

	/*
	 * Read initialization commands from ".vimrc" or ".exrc" in current
	 * directory.  This is only done if the 'exrc' option is set.
	 * Because of security reasons we disallow shell and write commands now,
	 * except for unix if the file is owned by the user or 'secure' option has
	 * been reset in environmet of global ".exrc" or ".vimrc".
	 * Only do this if VIMRC_FILE is not the same as USR_VIMRC_FILE or
	 * sys_vimrc_fname.
	 */
		if (p_exrc)
		{
#ifdef UNIX
			{
				struct stat s;

				/* if ".vimrc" file is not owned by user, set 'secure' mode */
				if (stat(VIMRC_FILE, &s) || s.st_uid != getuid())
					secure = p_secure;
			}
#else
			secure = p_secure;
#endif

			i = FAIL;
			if (fullpathcmp((char_u *)USR_VIMRC_FILE,
											 (char_u *)VIMRC_FILE) != FPC_SAME
#if defined(HAVE_CONFIG_H) || defined(OS2)
					&& fullpathcmp(((vi_mode == TRUE) ? sys_compatrc_fname : sys_vimrc_fname),
											 (char_u *)VIMRC_FILE) != FPC_SAME
#endif
					)
				i = do_source((char_u *)VIMRC_FILE, TRUE);
#ifdef UNIX
			if (i == FAIL)
			{
				struct stat s;

					/* if ".exrc" is not owned by user set 'secure' mode */
				if (stat(EXRC_FILE, &s) || s.st_uid != getuid())
					secure = p_secure;
				else
					secure = 0;
			}
			else
				check_version = TRUE;
#endif
			if (i == FAIL && fullpathcmp((char_u *)USR_EXRC_FILE,
											 (char_u *)EXRC_FILE) != FPC_SAME)
				(void)do_source((char_u *)EXRC_FILE, FALSE);
		}
	}

	/*
	 * Recovery mode without a file name: List swap files.
	 * This uses the 'dir' option, therefore it must be after the
	 * initializations.
	 */
	if (recoverymode && fname == NULL)
	{
		recover_names(NULL, TRUE, 0);
		mch_windexit(0);
	}

	/*
	 * Set a few option defaults after reading .vimrc files:
	 * 'title' and 'icon', Unix: 'shellpipe' and 'shellredir'.
	 */
	set_init_3();

#ifdef USE_GUI
	if (gui.starting)
	{
		gui_start();
		full_screen = TRUE;
	}
#endif

	/*
	 * If we read a .vimrc but it does not contain a "version 4.0" command,
	 * give the user a pointer to the help for the new version.
	 */
	if (check_version && found_version == 0)
	{
		MSG("This is Vim version 4.0.");
		MSG("No \":version 4.0\" command found in any .vimrc.");
		MSG("Use \":help version\" for info about this new version.");
	}

#ifdef VIMINFO
/*
 * Read in registers, history etc, but not marks, from the viminfo file
 */
	if (*p_viminfo != NUL)
		read_viminfo(NULL, TRUE, FALSE, FALSE);
#endif /* VIMINFO */

#ifdef SPAWNO			/* special MSDOS swapping library */
	init_SPAWNO("", SWAP_ANY);
#endif

	if (bin_mode)					/* -b option used */
	{
		set_options_bin(curbuf->b_p_bin, 1);
		curbuf->b_p_bin = 1;		/* binary file I/O */
	}

/*
 * "-e errorfile": Load the error file now.
 * If the error file can't be read, exit before doing anything else.
 */
	if (doqf && qf_init() == FAIL)		/* if reading error file fails: exit */
	{
		outchar('\n');
		mch_windexit(3);
	}

	/* Don't set the file name if there was a command in .vimrc that already
	 * loaded the file */
	if (curbuf->b_filename == NULL)
	{
		(void)setfname(fname, NULL, TRUE);	/* includes maketitle() */
		++arg_idx;							/* used first argument name */
	}

	if (window_count == 0)
		window_count = arg_count;
	if (window_count > 1)
	{
		/* Don't change the windows if there was a command in .vimrc that
		 * already split some windows */
		if (firstwin->w_next == NULL)
			window_count = make_windows(window_count);
		else
			window_count = win_count();
	}
	else
		window_count = 1;

/*
 * Start putting things on the screen.
 * Scroll screen down before drawing over it
 * Clear screen now, so file message will not be cleared.
 */
	starting = FALSE;
	no_wait_return = FALSE;
	msg_scroll = FALSE;
#ifdef USE_GUI
	/*
	 * This seems to be required to make callbacks to be called now, instead
	 * of after things have been put on the screen, which then may be deleted
	 * when getting a resize callback.
	 */
	if (gui.in_use)
		gui_mch_wait_for_chars(50);
#endif

/*
 * When done something that is not allowed or error message call wait_return.
 * This must be done before starttermcap(), because it may switch to another
 * screen. It must be done after settmode(1), because we want to react on a
 * single key stroke.
 * Call settmode and starttermcap here, so the T_KS and T_TI may be defined
 * by termcapinit and redifined in .exrc.
 */
	settmode(1);
	if (secure == 2 || need_wait_return || msg_didany)
		wait_return(TRUE);

	starttermcap();			/* start termcap if not done by wait_return() */
#ifdef USE_MOUSE
	setmouse();							/* may start using the mouse */
#endif
	if (scroll_region)
		scroll_region_reset();			/* In case Rows changed */

	secure = 0;

	scroll_start();

	if (!invoked_as_ex) {
		screenclear();					/* clear screen */
	}

	no_wait_return = TRUE;

	if (recoverymode)					/* do recover */
	{
		msg_scroll = TRUE;				/* scroll message up */
		ml_recover();
		msg_scroll = FALSE;
		if (curbuf->b_ml.ml_mfp == NULL) /* failed */
			getout(1);
		do_modelines();					/* do modelines */
	}
	else
	{
		/*
		 * Open a buffer for windows that don't have one yet.
		 * Commands in the .vimrc might have loaded a file or split the window.
		 * Watch out for autocommands that delete a window.
		 */
#ifdef AUTOCMD
		/*
		 * Don't execute Win/Buf Enter/Leave autocommands here
		 */
		++autocmd_no_enter;
		++autocmd_no_leave;
#endif
		for (curwin = firstwin; curwin != NULL; curwin = curwin->w_next)
		{
			curbuf = curwin->w_buffer;
			if (curbuf->b_ml.ml_mfp == NULL)
			{
				(void)open_buffer();		/* create memfile and read file */
#ifdef AUTOCMD
				curwin = firstwin;			/* start again */
#endif
				if (invoked_as_ex) {		/* move to end of file if running as 
ex */
					curwin->w_cursor.lnum = curbuf->b_ml.ml_line_count;
				}
			}
			mch_breakcheck();
			if (got_int)
			{
				(void)vgetc();	/* only break the file loading, not the rest */
				break;
			}
		}
#ifdef AUTOCMD
		--autocmd_no_enter;
		--autocmd_no_leave;
#endif
		curwin = firstwin;
		curbuf = curwin->w_buffer;
	}

#ifdef AUTOCMD
	apply_autocmds(EVENT_BUFENTER, NULL, NULL);
#endif
	setpcmark();

	/*
	 * When started with "-e errorfile" jump to first error now.
	 */
	if (doqf)
		qf_jump(0, 0, FALSE);

	/*
	 * If opened more than one window, start editing files in the other windows.
	 * Make_windows() has already opened the windows.
	 */
#ifdef AUTOCMD
	/*
	 * Don't execute Win/Buf Enter/Leave autocommands here
	 */
	++autocmd_no_enter;
	++autocmd_no_leave;
#endif
	for (i = 1; i < window_count; ++i)
	{
		if (curwin->w_next == NULL)			/* just checking */
			break;
		win_enter(curwin->w_next, FALSE);

		/* Only open the file if there is no file in this window yet (that can
		 * happen when .vimrc contains ":sall") */
		if (curbuf == firstwin->w_buffer || curbuf->b_filename == NULL)
		{
			curwin->w_arg_idx = arg_idx;
			/* edit file from arg list, if there is one */
			(void)do_ecmd(0, arg_idx < arg_count ? arg_files[arg_idx] : NULL,
										  NULL, NULL, (linenr_t)0, ECMD_HIDE);
			if (arg_idx == arg_count - 1)
				arg_had_last = TRUE;
			++arg_idx;
		}
		mch_breakcheck();
		if (got_int)
		{
			(void)vgetc();		/* only break the file loading, not the rest */
			break;
		}
	}
#ifdef AUTOCMD
	--autocmd_no_enter;
#endif
	win_enter(firstwin, FALSE);				/* back to first window */
#ifdef AUTOCMD
	--autocmd_no_leave;
#endif
	if (window_count > 1)
		win_equal(curwin, FALSE);			/* adjust heights */

	/*
	 * If there are more file names in the argument list than windows,
	 * put the rest of the names in the buffer list.
	 */
	while (arg_idx < arg_count)
		(void)buflist_add(arg_files[arg_idx++]);

	/*
	 * Need to jump to the tag before executing the '-c command'.
	 * Makes "vim -c '/return' -t main" work.
	 */
	if (tagname)
	{
		STRCPY(IObuff, "ta ");
		STRCAT(IObuff, tagname);
		do_cmdline(IObuff, TRUE, TRUE);
	}

	if (command)
	{
		/*
		 * We start commands on line 0, make "vim +/pat file" match a
		 * pattern on line 1.
		 */
		curwin->w_cursor.lnum = 0;
		sourcing_name = (char_u *)"command line";
		do_cmdline(command, TRUE, TRUE);
		sourcing_name = NULL;
	}

	RedrawingDisabled = FALSE;
	redraw_later(NOT_VALID);
	no_wait_return = FALSE;

	/* start in insert mode */
	if (p_im)
		need_start_insertmode = TRUE;

/*
 * main command loop
 */
	for (;;)
	{
		if (stuff_empty())
		{
			if (need_check_timestamps)
				check_timestamps();
			if (need_wait_return)		/* if wait_return still needed ... */
				wait_return(FALSE);		/* ... call it now */
			if (need_start_insertmode)
			{
				need_start_insertmode = FALSE;
				stuffReadbuff((char_u *)"i");	/* start insert mode next */
				/* skip the fileinfo message now, because it would be shown
				 * after insert mode finishes! */
				need_fileinfo = FALSE;
			}
		}
		dont_wait_return = FALSE;
		if (got_int && !global_busy)
		{
			(void)vgetc();				/* flush all buffers */
			got_int = FALSE;
		}
		adjust_cursor();				/* put cursor on an existing line */
		msg_scroll = FALSE;
		quit_more = FALSE;
		keep_help_flag = FALSE;
		/*
		 * If skip redraw is set (for ":" in wait_return()), don't redraw now.
		 * If there is nothing in the stuff_buffer or do_redraw is TRUE,
		 * update cursor and redraw.
		 */
		if (skip_redraw || invoked_as_ex)			
			skip_redraw = FALSE;
		else if (do_redraw || stuff_empty())
		{
			cursupdate();				/* Figure out where the cursor is based
											on curwin->w_cursor. */
#ifdef SLEEP_IN_EMSG
			if (need_sleep)				/* sleep before redrawing */
			{
				mch_delay(1000L, TRUE);
				need_sleep = FALSE;
			}
#endif
			if (VIsual_active)
				update_curbuf(INVERTED);/* update inverted part */
			if (must_redraw)
				updateScreen(must_redraw);
			else if (redraw_cmdline)
				showmode();
			if (keep_msg != NULL)
			{
				if (keep_msg_highlight)
				{
					(void)set_highlight(keep_msg_highlight);
					msg_highlight = TRUE;
				}
				msg(keep_msg);			/* display message after redraw */
			}
			if (need_fileinfo)			/* used after jumping to a tag */
			{
				fileinfo(did_cd, TRUE, FALSE);
				need_fileinfo = FALSE;
			}

			emsg_on_display = FALSE;	/* can delete error message now */
			msg_didany = FALSE;			/* reset lines_left in msg_start() */
			do_redraw = FALSE;
			showruler(FALSE);

			setcursor();
			cursor_on();
		}

		/* 
		 * if we're invoked as ex, do a round of ex commands before
		 * going on to normal mode
		 */

		if (invoked_as_ex) {
			do_exmode();

			cursupdate();
			updateScreen(TRUE);
			showmode();
			setcursor();
			cursor_on();

			invoked_as_ex = FALSE;
		}

		/*
		 * get and execute a normal mode command
		 */
		normal();
	}
	/*NOTREACHED*/
}

	void
getout(r)
	int 			r;
{
	exiting = TRUE;

	/* Position the cursor on the last screen line, below all the text */
#ifdef USE_GUI
	if (!gui.in_use)
#endif
		windgoto((int)Rows - 1, 0);

#ifdef AUTOCMD
	apply_autocmds(EVENT_VIMLEAVE, NULL, NULL);

	/* Position the cursor again, the autocommands may have moved it */
# ifdef USE_GUI
	if (!gui.in_use)
# endif
		windgoto((int)Rows - 1, 0);
#endif

#ifdef VIMINFO
	msg_didany = FALSE;
	/* Write out the registers, history, marks etc, to the viminfo file */
	if (*p_viminfo != NUL)
		write_viminfo(NULL, FALSE);
	if (msg_didany)				/* make the user read the error message */
	{
		no_wait_return = FALSE;
		wait_return(FALSE);
	}
#endif /* VIMINFO */

	mch_windexit(r);
}
