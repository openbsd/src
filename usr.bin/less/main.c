/*
 * Copyright (c) 1984,1985,1989,1994,1995  Mark Nudelman
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice in the documentation and/or other materials provided with 
 *    the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR 
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT 
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR 
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE 
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN 
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


/*
 * Entry point, initialization, miscellaneous routines.
 */

#include "less.h"
#include "position.h"

public char *	every_first_cmd = NULL;
public int	new_file;
public int	is_tty;
public IFILE	curr_ifile = NULL_IFILE;
public IFILE	old_ifile = NULL_IFILE;
public struct scrpos initial_scrpos;
public int	any_display = FALSE;
public int	wscroll;
public char *	progname;
public int	quitting;
public int	more_mode = 0;

extern int	quit_at_eof;
extern int	cbufs;
extern int	errmsgs;
extern int	screen_trashed;
extern int	force_open;

#if LOGFILE
public int	logfile = -1;
public int	force_logfile = FALSE;
public char *	namelogfile = NULL;
#endif

#if EDITOR
public char *	editor;
public char *	editproto;
#endif

#if TAGS
extern char *	tagfile;
extern char *	tagoption;
extern int	jump_sline;
#endif



/*
 * Entry point.
 */
int
main(argc, argv)
	int argc;
	char *argv[];
{
	IFILE ifile;
	extern char *__progname;

#ifdef __EMX__
	_response(&argc, &argv);
	_wildcard(&argc, &argv);
#endif

	progname = *argv++;

	/*
	 * Process command line arguments and LESS environment arguments.
	 * Command line arguments override environment arguments.
	 */
	if (strcmp(__progname, "more") == 0)
		more_mode = 1;

	get_term();
	init_cmds();
	init_prompt();
	init_charset();
	init_option();

	if (more_mode) {
		scan_option("-E");
		scan_option("-m");
		scan_option("-G");
		scan_option(getenv("MORE"));
	} else
		scan_option(getenv("LESS"));

#if GNU_OPTIONS
	/*
	 * Special case for "less --help" and "less --version".
	 */
	if (argc == 2)
	{
		if (strcmp(argv[0], "--help") == 0)
			scan_option("-?");
		if (strcmp(argv[0], "--version") == 0)
			scan_option("-V");
	}
#endif
#define	isoptstring(s)	(((s)[0] == '-' || (s)[0] == '+') && (s)[1] != '\0')
	while (--argc > 0 && (isoptstring(argv[0]) || isoptpending())) {
		if (strcmp(argv[0], "--") == 0) {
			argv++;
			argc--;
			break;
		}
		scan_option(*argv++);
	}
#undef isoptstring

	if (isoptpending())
	{
		/*
		 * Last command line option was a flag requiring a
		 * following string, but there was no following string.
		 */
		nopendopt();
		quit(QUIT_OK);
	}

#if EDITOR
	editor = getenv("VISUAL");
	if (editor == NULL || *editor == '\0')
	{
		editor = getenv("EDITOR");
		if (editor == NULL || *editor == '\0')
			editor = EDIT_PGM;
	}
	editproto = getenv("LESSEDIT");
	if (editproto == NULL || *editproto == '\0')
		editproto = "%E ?lm+%lm. %f";
#endif

	/*
	 * Call get_ifile with all the command line filenames
	 * to "register" them with the ifile system.
	 */
	ifile = NULL_IFILE;
	while (--argc >= 0)
	{
#if MSOFTC || OS2
		/*
		 * Because the "shell" doesn't expand filename patterns,
		 * treat each argument as a filename pattern rather than
		 * a single filename.  
		 * Expand the pattern and iterate over the expanded list.
		 */
		struct textlist tlist;
		char *gfilename;
		char *filename;
		
		gfilename = glob(*argv++);
		init_textlist(&tlist, gfilename);
		filename = NULL;
		while ((filename = forw_textlist(&tlist, filename)) != NULL)
			ifile = get_ifile(filename, ifile);
		free(gfilename);
#else
		ifile = get_ifile(*argv++, ifile);
#endif
	}
	/*
	 * Set up terminal, etc.
	 */
	is_tty = isatty(1);
	if (!is_tty)
	{
		/*
		 * Output is not a tty.
		 * Just copy the input file(s) to output.
		 */
		if (nifile() == 0)
		{
			if (edit_stdin() == 0)
				cat_file();
		} else if (edit_first() == 0)
		{
			do {
				cat_file();
			} while (edit_next(1) == 0);
		}
		quit(QUIT_OK);
	}

	init_mark();
	raw_mode(1);
	open_getchr();
	init_signals(1);

	/*
	 * Select the first file to examine.
	 */
#if TAGS
	if (tagoption != NULL)
	{
		/*
		 * A -t option was given.
		 * Verify that no filenames were also given.
		 * Edit the file selected by the "tags" search,
		 * and search for the proper line in the file.
		 */
		if (nifile() > 0)
		{
			error("No filenames allowed with -t option", NULL_PARG);
			quit(QUIT_ERROR);
		}
		findtag(tagoption);
		if (tagfile == NULL)
			quit(QUIT_ERROR);
		if (edit(tagfile))  /* Edit file which contains the tag */
			quit(QUIT_ERROR);
		/*
		 * Search for the line which contains the tag.
		 * Set up initial_scrpos so we display that line.
		 */
		initial_scrpos.pos = tagsearch();
		if (initial_scrpos.pos == NULL_POSITION)
			quit(QUIT_ERROR);
		initial_scrpos.ln = jump_sline;
	} else
#endif
	if (nifile() == 0)
	{
		if (edit_stdin())  /* Edit standard input */
			quit(QUIT_ERROR);
	} else 
	{
		if (edit_first())  /* Edit first valid file in cmd line */
			quit(QUIT_ERROR);
	}

	init();
	commands();
	quit(QUIT_OK);
	/*NOTREACHED*/
}

/*
 * Copy a string, truncating to the specified length if necessary.
 * Unlike strncpy(), the resulting string is guaranteed to be null-terminated.
 */
	public void
strtcpy(to, from, len)
	char *to;
	char *from;
	unsigned int len;
{
	strncpy(to, from, len);
	to[len-1] = '\0';
}

/*
 * Copy a string to a "safe" place
 * (that is, to a buffer allocated by calloc).
 */
	public char *
save(s)
	char *s;
{
	register char *p;

	p = (char *) ecalloc(strlen(s)+1, sizeof(char));
	strcpy(p, s);
	return (p);
}

/*
 * Allocate memory.
 * Like calloc(), but never returns an error (NULL).
 */
	public VOID_POINTER
ecalloc(count, size)
	int count;
	unsigned int size;
{
	register VOID_POINTER p;

	p = (VOID_POINTER) calloc(count, size);
	if (p != NULL)
		return (p);
	error("Cannot allocate memory", NULL_PARG);
	quit(QUIT_ERROR);
	/*NOTREACHED*/
}

/*
 * Skip leading spaces in a string.
 */
	public char *
skipsp(s)
	register char *s;
{
	while (*s == ' ' || *s == '\t')	
		s++;
	return (s);
}

/*
 * Exit the program.
 */
	public void
quit(status)
	int status;
{
	static int save_status;

	/*
	 * Put cursor at bottom left corner, clear the line,
	 * reset the terminal modes, and exit.
	 */
	if (status < 0)
		status = save_status;
	else
		save_status = status;
	quitting = 1;
	edit((char*)NULL);
	if (is_tty && any_display)
		clear_bot();
	deinit();
	flush();
	raw_mode(0);
#if MSOFTC
	/* 
	 * If we don't close 2, we get some garbage from
	 * 2's buffer when it flushes automatically.
	 * I cannot track this one down  RB
	 * The same bug shows up if we use ^C^C to abort.
	 */
	close(2);
#endif
	exit(status);
}
