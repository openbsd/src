/*
 * Copyright (C) 1984-2012  Mark Nudelman
 * Modified for use with illumos by Garrett D'Amore.
 * Copyright 2014 Garrett D'Amore <garrett@damore.org>
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Less License, as specified in the README file.
 *
 * For more information, see the README file.
 */

/*
 * The option table.
 */

#include "less.h"
#include "option.h"

/*
 * Variables controlled by command line options.
 */
int quiet;		/* Should we suppress the audible bell? */
int how_search;		/* Where should forward searches start? */
int top_scroll;		/* Repaint screen from top? (vs scroll from bottom) */
int pr_type;		/* Type of prompt (short, medium, long) */
int bs_mode;		/* How to process backspaces */
int know_dumb;		/* Don't complain about dumb terminals */
int quit_at_eof;	/* Quit after hitting end of file twice */
int quit_if_one_screen;	/* Quit if EOF on first screen */
int squeeze;		/* Squeeze multiple blank lines into one */
int back_scroll;	/* Repaint screen on backwards movement */
int forw_scroll;	/* Repaint screen on forward movement */
int caseless;		/* Do "caseless" searches */
int linenums;		/* Use line numbers */
int autobuf;		/* Automatically allocate buffers as needed */
int bufspace;		/* Max buffer space per file (K) */
int ctldisp;		/* Send control chars to screen untranslated */
int force_open;		/* Open the file even if not regular file */
int swindow;		/* Size of scrolling window */
int jump_sline;		/* Screen line of "jump target" */
long jump_sline_fraction = -1;
int chopline;		/* Truncate displayed lines at screen width */
int no_init;		/* Disable sending ti/te termcap strings */
int no_keypad;		/* Disable sending ks/ke termcap strings */
int twiddle;		/* Show tildes after EOF */
int show_attn;		/* Hilite first unread line */
int status_col;		/* Display a status column */
int use_lessopen;	/* Use the LESSOPEN filter */
int quit_on_intr;	/* Quit on interrupt */
int follow_mode;	/* F cmd Follows file desc or file name? */
int oldbot;		/* Old bottom of screen behavior {{REMOVE}} */
int opt_use_backslash;	/* Use backslash escaping in option parsing */
int hilite_search;	/* Highlight matched search patterns? */

int less_is_more = 0;	/* Make compatible with POSIX more */

/*
 * Long option names.
 */
static struct optname a_optname		= { "search-skip-screen",	NULL };
static struct optname b_optname		= { "buffers",			NULL };
static struct optname B__optname	= { "auto-buffers",		NULL };
static struct optname c_optname		= { "clear-screen",		NULL };
static struct optname d_optname		= { "dumb",			NULL };
static struct optname e_optname		= { "quit-at-eof",		NULL };
static struct optname f_optname		= { "force",			NULL };
static struct optname F__optname	= { "quit-if-one-screen",	NULL };
static struct optname g_optname		= { "hilite-search",		NULL };
static struct optname h_optname		= { "max-back-scroll",		NULL };
static struct optname i_optname		= { "ignore-case",		NULL };
static struct optname j_optname		= { "jump-target",		NULL };
static struct optname J__optname	= { "status-column",		NULL };
static struct optname k_optname		= { "lesskey-file",		NULL };
static struct optname K__optname	= { "quit-on-intr",		NULL };
static struct optname L__optname	= { "no-lessopen",		NULL };
static struct optname m_optname		= { "long-prompt",		NULL };
static struct optname n_optname		= { "line-numbers",		NULL };
static struct optname o_optname		= { "log-file",			NULL };
static struct optname O__optname	= { "LOG-FILE",			NULL };
static struct optname p_optname		= { "pattern",			NULL };
static struct optname P__optname	= { "prompt",			NULL };
static struct optname q2_optname	= { "silent",			NULL };
static struct optname q_optname		= { "quiet",		&q2_optname };
static struct optname r_optname		= { "raw-control-chars",	NULL };
static struct optname s_optname		= { "squeeze-blank-lines",	NULL };
static struct optname S__optname	= { "chop-long-lines",		NULL };
static struct optname t_optname		= { "tag",			NULL };
static struct optname T__optname	= { "tag-file",			NULL };
static struct optname u_optname		= { "underline-special",	NULL };
static struct optname V__optname	= { "version",			NULL };
static struct optname w_optname		= { "hilite-unread",		NULL };
static struct optname x_optname		= { "tabs",			NULL };
static struct optname X__optname	= { "no-init",			NULL };
static struct optname y_optname		= { "max-forw-scroll",		NULL };
static struct optname z_optname		= { "window",			NULL };
static struct optname quote_optname	= { "quotes",			NULL };
static struct optname tilde_optname	= { "tilde",			NULL };
static struct optname query_optname	= { "help",			NULL };
static struct optname pound_optname	= { "shift",			NULL };
static struct optname keypad_optname	= { "no-keypad",		NULL };
static struct optname oldbot_optname	= { "old-bot",			NULL };
static struct optname follow_optname	= { "follow-name",		NULL };
static struct optname use_backslash_optname = { "use-backslash",	NULL };


/*
 * Table of all options and their semantics.
 *
 * For BOOL and TRIPLE options, odesc[0], odesc[1], odesc[2] are
 * the description of the option when set to 0, 1 or 2, respectively.
 * For NUMBER options, odesc[0] is the prompt to use when entering
 * a new value, and odesc[1] is the description, which should contain
 * one %d which is replaced by the value of the number.
 * For STRING options, odesc[0] is the prompt to use when entering
 * a new value, and odesc[1], if not NULL, is the set of characters
 * that are valid in the string.
 */
static struct loption option[] = {
	{ 'a', &a_optname,
		TRIPLE, OPT_ONPLUS, &how_search, NULL,
		{
			"Search includes displayed screen",
			"Search skips displayed screen",
			"Search includes all of displayed screen"
		}
	},

	{ 'b', &b_optname,
		NUMBER|INIT_HANDLER, 64, &bufspace, opt_b,
		{
			"Max buffer space per file (K): ",
			"Max buffer space per file: %dK",
			NULL
		}
	},
	{ 'B', &B__optname,
		BOOL, OPT_ON, &autobuf, NULL,
		{
			"Don't automatically allocate buffers",
			"Automatically allocate buffers when needed",
			NULL
		}
	},
	{ 'c', &c_optname,
		TRIPLE|MORE_OK, OPT_ON, &top_scroll, NULL,
		{
			"Repaint by scrolling from bottom of screen",
			"Repaint by painting from top of screen",
			"Repaint by painting from top of screen"
		}
	},
	{ 'd', &d_optname,
		BOOL|MORE_OK|NO_TOGGLE, OPT_OFF, &know_dumb, NULL,
		{
			"Assume intelligent terminal",
			"Assume dumb terminal",
			NULL
		}
	},
	{ 'e', &e_optname,
		TRIPLE, OPT_OFF, &quit_at_eof, NULL,
		{
			"Don't quit at end-of-file",
			"Quit at end-of-file",
			"Quit immediately at end-of-file"
		}
	},
	{ 'f', &f_optname,
		BOOL, OPT_OFF, &force_open, NULL,
		{
			"Open only regular files",
			"Open even non-regular files",
			NULL
		}
	},
	{ 'F', &F__optname,
		BOOL, OPT_OFF, &quit_if_one_screen, NULL,
		{
			"Don't quit if end-of-file on first screen",
			"Quit if end-of-file on first screen",
			NULL
		}
	},
	{ 'g', &g_optname,
		TRIPLE|HL_REPAINT, OPT_ONPLUS, &hilite_search, NULL,
		{
			"Don't highlight search matches",
			"Highlight matches for previous search only",
			"Highlight all matches for previous search pattern",
		}
	},
	{ 'h', &h_optname,
		NUMBER, -1, &back_scroll, NULL,
		{
			"Backwards scroll limit: ",
			"Backwards scroll limit is %d lines",
			NULL
		}
	},
	{ 'i', &i_optname,
		TRIPLE|HL_REPAINT, OPT_OFF, &caseless, opt_i,
		{
			"Case is significant in searches",
			"Ignore case in searches",
			"Ignore case in searches and in patterns"
		}
	},
	{ 'j', &j_optname,
		STRING, 0, NULL, opt_j,
		{
			"Target line: ",
			"0123456789.-",
			NULL
		}
	},
	{ 'J', &J__optname,
		BOOL|REPAINT, OPT_OFF, &status_col, NULL,
		{
			"Don't display a status column",
			"Display a status column",
			NULL
		}
	},
	{ 'k', &k_optname,
		STRING|NO_TOGGLE|NO_QUERY, 0, NULL, opt_k,
		{ NULL, NULL, NULL }
	},
	{ 'K', &K__optname,
		BOOL, OPT_OFF, &quit_on_intr, NULL,
		{
			"Interrupt (ctrl-C) returns to prompt",
			"Interrupt (ctrl-C) exits less",
			NULL
		}
	},
	{ 'L', &L__optname,
		BOOL, OPT_ON, &use_lessopen, NULL,
		{
			"Don't use the LESSOPEN filter",
			"Use the LESSOPEN filter",
			NULL
		}
	},
	{ 'm', &m_optname,
		TRIPLE, OPT_OFF, &pr_type, NULL,
		{
			"Short prompt",
			"Medium prompt",
			"Long prompt"
		}
	},
	{ 'n', &n_optname,
		TRIPLE|REPAINT, OPT_ON, &linenums, NULL,
		{
			"Don't use line numbers",
			"Use line numbers",
			"Constantly display line numbers"
		}
	},
	{ 'o', &o_optname,
		STRING, 0, NULL, opt_o,
		{ "log file: ", NULL, NULL }
	},
	{ 'O', &O__optname,
		STRING, 0, NULL, opt__O,
		{ "Log file: ", NULL, NULL }
	},
	{ 'p', &p_optname,
		STRING|NO_TOGGLE|NO_QUERY|MORE_OK, 0, NULL, opt_p,
		{ NULL, NULL, NULL }
	},
	{ 'P', &P__optname,
		STRING, 0, NULL, opt__P,
		{ "prompt: ", NULL, NULL }
	},
	{ 'q', &q_optname,
		TRIPLE, OPT_OFF, &quiet, NULL,
		{
			"Ring the bell for errors AND at eof/bof",
			"Ring the bell for errors but not at eof/bof",
			"Never ring the bell"
		}
	},
	{ 'r', &r_optname,
		TRIPLE|REPAINT, OPT_OFF, &ctldisp, NULL,
		{
			"Display control characters as ^X",
			"Display control characters directly",
			"Display control characters directly, "
			"processing ANSI sequences"
		}
	},
	{ 's', &s_optname,
		BOOL|REPAINT|MORE_OK, OPT_OFF, &squeeze, NULL,
		{
			"Display all blank lines",
			"Squeeze multiple blank lines",
			NULL
		}
	},
	{ 'S', &S__optname,
		BOOL|REPAINT, OPT_OFF, &chopline, NULL,
		{
			"Fold long lines",
			"Chop long lines",
			NULL
		}
	},
	{ 't', &t_optname,
		STRING|NO_QUERY|MORE_OK, 0, NULL, opt_t,
		{ "tag: ", NULL, NULL }
	},
	{ 'T', &T__optname,
		STRING|MORE_OK, 0, NULL, opt__T,
		{ "tags file: ", NULL, NULL }
	},
	{ 'u', &u_optname,
		TRIPLE|REPAINT, OPT_OFF, &bs_mode, NULL,
		{
			"Display underlined text in underline mode",
			"Backspaces cause overstrike",
			"Print backspace as ^H"
		}
	},
	{ 'V', &V__optname,
		NOVAR, 0, NULL, opt__V,
		{ NULL, NULL, NULL }
	},
	{ 'w', &w_optname,
		TRIPLE|REPAINT, OPT_OFF, &show_attn, NULL,
		{
			"Don't highlight first unread line",
			"Highlight first unread line after forward-screen",
			"Highlight first unread line after any "
			"forward movement",
		}
	},
	{ 'x', &x_optname,
		STRING|REPAINT, 0, NULL, opt_x,
		{
			"Tab stops: ",
			"0123456789,",
			NULL
		}
	},
	{ 'X', &X__optname,
		BOOL|NO_TOGGLE, OPT_OFF, &no_init, NULL,
		{
			"Send init/deinit strings to terminal",
			"Don't use init/deinit strings",
			NULL
		}
	},
	{ 'y', &y_optname,
		NUMBER, -1, &forw_scroll, NULL,
		{
			"Forward scroll limit: ",
			"Forward scroll limit is %d lines",
			NULL
		}
	},
	{ 'z', &z_optname,
		NUMBER, -1, &swindow, NULL,
		{
			"Scroll window size: ",
			"Scroll window size is %d lines",
			NULL
		}
	},
	{ '"', &quote_optname,
		STRING, 0, NULL, opt_quote,
		{ "quotes: ", NULL, NULL }
	},
	{ '~', &tilde_optname,
		BOOL|REPAINT, OPT_ON, &twiddle, NULL,
		{
			"Don't show tildes after end of file",
			"Show tildes after end of file",
			NULL
		}
	},
	{ '?', &query_optname,
		NOVAR, 0, NULL, opt_query,
		{ NULL, NULL, NULL }
	},
	{ '#', &pound_optname,
		STRING, 0, NULL, opt_shift,
		{
			"Horizontal shift: ",
			"0123456789.",
			NULL
		}
	},
	{ OLETTER_NONE, &keypad_optname,
		BOOL|NO_TOGGLE, OPT_OFF, &no_keypad, NULL,
		{
			"Use keypad mode",
			"Don't use keypad mode",
			NULL
		}
	},
	{ OLETTER_NONE, &oldbot_optname,
		BOOL, OPT_OFF, &oldbot, NULL,
		{
			"Use new bottom of screen behavior",
			"Use old bottom of screen behavior",
			NULL
		}
	},
	{ OLETTER_NONE, &follow_optname,
		BOOL, FOLLOW_DESC, &follow_mode, NULL,
		{
			"F command follows file descriptor",
			"F command follows file name",
			NULL
		}
	},
	{ OLETTER_NONE, &use_backslash_optname,
		BOOL, OPT_OFF, &opt_use_backslash, NULL,
		{
			"Use backslash escaping in command line parameters",
			"Don't use backslash escaping in command line "
			"parameters",
			NULL
		}
	},
	{ '\0', NULL, NOVAR, 0, NULL, NULL, { NULL, NULL, NULL } }
};


/*
 * Initialize each option to its default value.
 */
void
init_option(void)
{
	struct loption *o;

	for (o = option; o->oletter != '\0'; o++) {
		/*
		 * Set each variable to its default.
		 */
		if (o->ovar != NULL)
			*(o->ovar) = o->odefault;
		if (o->otype & INIT_HANDLER)
			(*(o->ofunc))(INIT, NULL);
	}
}

/*
 * Find an option in the option table, given its option letter.
 */
struct loption *
findopt(int c)
{
	struct loption *o;

	for (o = option; o->oletter != '\0'; o++) {
		if (o->oletter == c)
			return (o);
		if ((o->otype & TRIPLE) &&
		    (toupper((unsigned char)o->oletter) == c))
			return (o);
	}
	return (NULL);
}

/*
 *
 */
static int
is_optchar(unsigned char c)
{
	if (isupper(c) || islower(c) || c == '-')
		return (1);
	else
		return (0);
}

/*
 * Find an option in the option table, given its option name.
 * p_optname is the (possibly partial) name to look for, and
 * is updated to point after the matched name.
 * p_oname if non-NULL is set to point to the full option name.
 */
struct loption *
findopt_name(char **p_optname, char **p_oname, int *p_err)
{
	char *optname = *p_optname;
	struct loption *o;
	struct optname *oname;
	int len;
	int uppercase;
	struct loption *maxo = NULL;
	struct optname *maxoname = NULL;
	int maxlen = 0;
	int ambig = 0;
	int exact = 0;

	/*
	 * Check all options.
	 */
	for (o = option; o->oletter != '\0'; o++) {
		/*
		 * Check all names for this option.
		 */
		for (oname = o->onames; oname != NULL; oname = oname->onext) {
			/*
			 * Try normal match first (uppercase == 0),
			 * then, then if it's a TRIPLE option,
			 * try uppercase match (uppercase == 1).
			 */
			for (uppercase = 0; uppercase <= 1; uppercase++) {
				len = sprefix(optname, oname->oname, uppercase);
				if (len <= 0 || is_optchar(optname[len])) {
					/*
					 * We didn't use all of the option name.
					 */
					continue;
				}
				if (!exact && len == maxlen) {
					/*
					 * Already had a partial match,
					 * and now there's another one that
					 * matches the same length.
					 */
					ambig = 1;
				} else if (len > maxlen) {
					/*
					 * Found a better match than
					 * the one we had.
					 */
					maxo = o;
					maxoname = oname;
					maxlen = len;
					ambig = 0;
					exact = (len == strlen(oname->oname));
				}
				if (!(o->otype & TRIPLE))
					break;
			}
		}
	}
	if (ambig) {
		/*
		 * Name matched more than one option.
		 */
		if (p_err != NULL)
			*p_err = OPT_AMBIG;
		return (NULL);
	}
	*p_optname = optname + maxlen;
	if (p_oname != NULL)
		*p_oname = maxoname == NULL ? NULL : maxoname->oname;
	return (maxo);
}
