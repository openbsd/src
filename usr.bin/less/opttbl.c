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
 * The option table.
 */

#include "less.h"
#include "option.h"

/*
 * Variables controlled by command line options.
 */
public int quiet;		/* Should we suppress the audible bell? */
public int how_search;		/* Where should forward searches start? */
public int top_scroll;		/* Repaint screen from top?
				   (alternative is scroll from bottom) */
public int pr_type;		/* Type of prompt (short, medium, long) */
public int bs_mode;		/* How to process backspaces */
public int know_dumb;		/* Don't complain about dumb terminals */
public int quit_at_eof;		/* Quit after hitting end of file twice */
public int be_helpful;		/* more(1) style -d */
public int squeeze;		/* Squeeze multiple blank lines into one */
public int tabstop;		/* Tab settings */
public int back_scroll;		/* Repaint screen on backwards movement */
public int forw_scroll;		/* Repaint screen on forward movement */
public int twiddle;		/* Display "~" for lines after EOF */
public int caseless;		/* Do "caseless" searches */
public int linenums;		/* Use line numbers */
public int cbufs;		/* Current number of buffers */
public int autobuf;		/* Automatically allocate buffers as needed */
public int nohelp;		/* Disable the HELP command */
public int ctldisp;		/* Send control chars to screen untranslated */
public int force_open;		/* Open the file even if not regular file */
public int swindow;		/* Size of scrolling window */
public int jump_sline;		/* Screen line of "jump target" */
public int chopline;		/* Truncate displayed lines at screen width */
public int no_init;		/* Disable sending ti/te termcap strings */
#if HILITE_SEARCH
public int hilite_search;	/* Highlight matched search patterns? */
#endif

/*
 * Table of all options and their semantics.
 */
static struct option option[] =
{
	{ 'a', BOOL, OPT_OFF, &how_search, NULL,
		"Search includes displayed screen",
		"Search skips displayed screen",
		NULL
	},
	{ 'b', NUMBER, 10, &cbufs, opt_b, 
		"Buffers: ",
		"%d buffers",
		NULL
	},
	{ 'B', BOOL, OPT_ON, &autobuf, NULL,
		"Don't automatically allocate buffers",
		"Automatically allocate buffers when needed",
		NULL
	},
	{ 'c', TRIPLE, OPT_OFF, &top_scroll, NULL,
		"Repaint by scrolling from bottom of screen",
		"Repaint by clearing each line",
		"Repaint by painting from top of screen"
	},
#if 0
	{ 'd', BOOL|NO_TOGGLE, OPT_OFF, &know_dumb, NULL,
		"Assume intelligent terminal",
		"Assume dumb terminal",
		NULL
	},
#else
	{ 'd', BOOL, OPT_OFF, &be_helpful, NULL,
		"Be less helpful in prompts",
		"Be helpful in prompts",
		NULL,
	},
#endif
#if MSOFTC
	{ 'D', STRING|REPAINT, 0, NULL, opt_D,
		"color desc: ", NULL, NULL
	},
#endif
	{ 'e', TRIPLE, OPT_OFF, &quit_at_eof, NULL,
		"Don't quit at end-of-file",
		"Quit at end-of-file",
		"Quit immediately at end-of-file"
	},
	{ 'f', BOOL, OPT_OFF, &force_open, NULL,
		"Open only regular files",
		"Open even non-regular files",
		NULL
	},
#if HILITE_SEARCH
	{ 'g', TRIPLE|HL_REPAINT, OPT_ONPLUS, &hilite_search, NULL,
		"Don't highlight search matches",
		"Highlight matches for previous search only",
		"Highlight all matches for previous search pattern",
	},
#endif
	{ 'h', NUMBER, -1, &back_scroll, NULL,
		"Backwards scroll limit: ",
		"Backwards scroll limit is %d lines",
		NULL
	},
	{ 'H', BOOL|NO_TOGGLE, OPT_OFF, &nohelp, NULL,
		"Allow help command",
		"Don't allow help command",
		NULL
	},
	{ 'i', TRIPLE|HL_REPAINT, OPT_OFF, &caseless, opt_i,
		"Case is significant in searches",
		"Ignore case in searches",
		"Ignore case in searches and in patterns"
	},
	{ 'j', NUMBER, 1, &jump_sline, NULL,
		"Target line: ",
		"Position target at screen line %d",
		NULL
	},
#if USERFILE
	{ 'k', STRING|NO_TOGGLE|NO_QUERY, 0, NULL, opt_k,
		NULL, NULL, NULL
	},
#endif
	{ 'l', STRING|NO_TOGGLE|NO_QUERY, 0, NULL, opt_l,
		NULL, NULL, NULL
	},
	{ 'm', TRIPLE, OPT_OFF, &pr_type, NULL,
		"Short prompt",
		"Medium prompt",
		"Long prompt"
	},
	{ 'n', TRIPLE|REPAINT, OPT_ON, &linenums, NULL,
		"Don't use line numbers",
		"Use line numbers",
		"Constantly display line numbers"
	},
#if LOGFILE
	{ 'o', STRING, 0, NULL, opt_o,
		"log file: ", NULL, NULL
	},
	{ 'O', STRING, 0, NULL, opt__O,
		"Log file: ", NULL, NULL
	},
#endif
	{ 'p', STRING|NO_TOGGLE|NO_QUERY, 0, NULL, opt_p,
		NULL, NULL, NULL
	},
	{ 'P', STRING, 0, NULL, opt__P,
		"prompt: ", NULL, NULL
	},
	{ 'q', TRIPLE, OPT_OFF, &quiet, NULL,
		"Ring the bell for errors AND at eof/bof",
		"Ring the bell for errors but not at eof/bof",
		"Never ring the bell"
	},
	{ 'r', BOOL|REPAINT, OPT_ON, &ctldisp, NULL,
		"Display control characters directly",
		"Display control characters as ^X",
		NULL
	},
	{ 's', BOOL|REPAINT, OPT_OFF, &squeeze, NULL,
		"Display all blank lines",
		"Squeeze multiple blank lines",
		NULL
	},
	{ 'S', BOOL|REPAINT, OPT_OFF, &chopline, NULL,
		"Fold long lines",
		"Chop long lines",
		NULL
	},
#if TAGS
	{ 't', STRING|NO_QUERY, 0, NULL, opt_t,
		"tag: ", NULL, NULL
	},
	{ 'T', STRING, 0, NULL, opt__T,
		"tags file: ", NULL, NULL
	},
#endif
	{ 'u', TRIPLE|REPAINT, OPT_OFF, &bs_mode, NULL,
		"Display underlined text in underline mode",
		"Backspaces cause overstrike",
		"Print backspace as ^H"
	},
	{ 'V', NOVAR, 0, NULL, opt__V,
		NULL, NULL, NULL
	},
	{ 'w', BOOL|REPAINT, OPT_ON, &twiddle, NULL,
		"Display nothing for lines after end-of-file",
		"Display ~ for lines after end-of-file",
		NULL
	},
	{ 'x', NUMBER|REPAINT, 8, &tabstop, NULL,
		"Tab stops: ",
		"Tab stops every %d spaces", 
		NULL
	},
	{ 'X', BOOL|NO_TOGGLE, OPT_OFF, &no_init, NULL,
		"Send init/deinit strings to terminal",
		"Don't use init/deinit strings",
		NULL
	},
	{ 'y', NUMBER, -1, &forw_scroll, NULL,
		"Forward scroll limit: ",
		"Forward scroll limit is %d lines",
		NULL
	},
	{ 'z', NUMBER, -1, &swindow, NULL,
		"Scroll window size: ",
		"Scroll window size is %d lines",
		NULL
	},
	{ '?', NOVAR, 0, NULL, opt_query,
		NULL, NULL, NULL
	},
	{ '\0' }
};


/*
 * Initialize each option to its default value.
 */
	public void
init_option()
{
	register struct option *o;

	for (o = option;  o->oletter != '\0';  o++)
	{
		/*
		 * Set each variable to its default.
		 */
		if (o->ovar != NULL)
			*(o->ovar) = o->odefault;
	}
}

/*
 * Find an option in the option table.
 */
	public struct option *
findopt(c)
	int c;
{
	register struct option *o;

	for (o = option;  o->oletter != '\0';  o++)
	{
		if (o->oletter == c)
			return (o);
		if ((o->otype & TRIPLE) && toupper(o->oletter) == c)
			return (o);
	}
	return (NULL);
}
