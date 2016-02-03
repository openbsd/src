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
 * Handling functions for command line options.
 *
 * Most options are handled by the generic code in option.c.
 * But all string options, and a few non-string options, require
 * special handling specific to the particular option.
 * This special processing is done by the "handling functions" in this file.
 *
 * Each handling function is passed a "type" and, if it is a string
 * option, the string which should be "assigned" to the option.
 * The type may be one of:
 *	INIT	The option is being initialized from the command line.
 *	TOGGLE	The option is being changed from within the program.
 *	QUERY	The setting of the option is merely being queried.
 */

#include "less.h"
#include "option.h"

extern int bufspace;
extern int pr_type;
extern int plusoption;
extern int swindow;
extern int sc_width;
extern int sc_height;
extern int secure;
extern int dohelp;
extern int any_display;
extern char openquote;
extern char closequote;
extern char *prproto[];
extern char *eqproto;
extern char *hproto;
extern char *wproto;
extern IFILE curr_ifile;
extern char version[];
extern int jump_sline;
extern int jump_sline_fraction;
extern int less_is_more;
extern char *namelogfile;
extern int force_logfile;
extern int logfile;
char *tagoption = NULL;
extern char *tags;

int shift_count;	/* Number of positions to shift horizontally */
static int shift_count_fraction = -1;

/*
 * Handler for -o option.
 */
void
opt_o(int type, char *s)
{
	PARG parg;

	if (secure) {
		error("log file support is not available", NULL);
		return;
	}
	switch (type) {
	case INIT:
		namelogfile = s;
		break;
	case TOGGLE:
		if (ch_getflags() & CH_CANSEEK) {
			error("Input is not a pipe", NULL);
			return;
		}
		if (logfile >= 0) {
			error("Log file is already in use", NULL);
			return;
		}
		s = skipsp(s);
		namelogfile = lglob(s);
		use_logfile(namelogfile);
		sync_logfile();
		break;
	case QUERY:
		if (logfile < 0) {
			error("No log file", NULL);
		} else {
			parg.p_string = namelogfile;
			error("Log file \"%s\"", &parg);
		}
		break;
	}
}

/*
 * Handler for -O option.
 */
void
opt__O(int type, char *s)
{
	force_logfile = TRUE;
	opt_o(type, s);
}

/*
 * Handlers for -j option.
 */
void
opt_j(int type, char *s)
{
	PARG parg;
	char buf[16];
	int len;
	int err;

	switch (type) {
	case INIT:
	case TOGGLE:
		if (*s == '.') {
			s++;
			jump_sline_fraction = getfraction(&s, "j", &err);
			if (err)
				error("Invalid line fraction", NULL);
			else
				calc_jump_sline();
		} else {
			int sline = getnum(&s, "j", &err);
			if (err) {
				error("Invalid line number", NULL);
			} else {
				jump_sline = sline;
				jump_sline_fraction = -1;
			}
		}
		break;
	case QUERY:
		if (jump_sline_fraction < 0) {
			parg.p_int =  jump_sline;
			error("Position target at screen line %d", &parg);
		} else {
			(void) snprintf(buf, sizeof (buf), ".%06d",
			    jump_sline_fraction);
			len = strlen(buf);
			while (len > 2 && buf[len-1] == '0')
				len--;
			buf[len] = '\0';
			parg.p_string = buf;
			error("Position target at screen position %s", &parg);
		}
		break;
	}
}

void
calc_jump_sline(void)
{
	if (jump_sline_fraction < 0)
		return;
	jump_sline = sc_height * jump_sline_fraction / NUM_FRAC_DENOM;
}

/*
 * Handlers for -# option.
 */
void
opt_shift(int type, char *s)
{
	PARG parg;
	char buf[16];
	int len;
	int err;

	switch (type) {
	case INIT:
	case TOGGLE:
		if (*s == '.') {
			s++;
			shift_count_fraction = getfraction(&s, "#", &err);
			if (err)
				error("Invalid column fraction", NULL);
			else
				calc_shift_count();
		} else {
			int hs = getnum(&s, "#", &err);
			if (err) {
				error("Invalid column number", NULL);
			} else {
				shift_count = hs;
				shift_count_fraction = -1;
			}
		}
		break;
	case QUERY:
		if (shift_count_fraction < 0) {
			parg.p_int = shift_count;
			error("Horizontal shift %d columns", &parg);
		} else {

			(void) snprintf(buf, sizeof (buf), ".%06d",
			    shift_count_fraction);
			len = strlen(buf);
			while (len > 2 && buf[len-1] == '0')
				len--;
			buf[len] = '\0';
			parg.p_string = buf;
			error("Horizontal shift %s of screen width", &parg);
		}
		break;
	}
}

void
calc_shift_count(void)
{
	if (shift_count_fraction < 0)
		return;
	shift_count = sc_width * shift_count_fraction / NUM_FRAC_DENOM;
}

void
opt_k(int type, char *s)
{
	PARG parg;

	switch (type) {
	case INIT:
		if (lesskey(s, 0)) {
			parg.p_string = s;
			error("Cannot use lesskey file \"%s\"", &parg);
		}
		break;
	}
}

/*
 * Handler for -t option.
 */
void
opt_t(int type, char *s)
{
	IFILE save_ifile;
	off_t pos;

	switch (type) {
	case INIT:
		tagoption = s;
		/* Do the rest in main() */
		break;
	case TOGGLE:
		if (secure) {
			error("tags support is not available", NULL);
			break;
		}
		findtag(skipsp(s));
		save_ifile = save_curr_ifile();
		/*
		 * Try to open the file containing the tag
		 * and search for the tag in that file.
		 */
		if (edit_tagfile() || (pos = tagsearch()) == -1) {
			/* Failed: reopen the old file. */
			reedit_ifile(save_ifile);
			break;
		}
		unsave_ifile(save_ifile);
		jump_loc(pos, jump_sline);
		break;
	}
}

/*
 * Handler for -T option.
 */
void
opt__T(int type, char *s)
{
	PARG parg;

	switch (type) {
	case INIT:
		tags = s;
		break;
	case TOGGLE:
		s = skipsp(s);
		tags = lglob(s);
		break;
	case QUERY:
		parg.p_string = tags;
		error("Tags file \"%s\"", &parg);
		break;
	}
}

/*
 * Handler for -p option.
 */
void
opt_p(int type, char *s)
{
	switch (type) {
	case INIT:
		/*
		 * Unget a search command for the specified string.
		 * {{ This won't work if the "/" command is
		 *    changed or invalidated by a .lesskey file. }}
		 */
		plusoption = TRUE;
		ungetsc(s);
		/*
		 * In "more" mode, the -p argument is a command,
		 * not a search string, so we don't need a slash.
		 */
		if (!less_is_more)
			ungetsc("/");
		break;
	}
}

/*
 * Handler for -P option.
 */
void
opt__P(int type, char *s)
{
	char **proto;
	PARG parg;

	switch (type) {
	case INIT:
	case TOGGLE:
		/*
		 * Figure out which prototype string should be changed.
		 */
		switch (*s) {
		case 's':  proto = &prproto[PR_SHORT];	s++;	break;
		case 'm':  proto = &prproto[PR_MEDIUM];	s++;	break;
		case 'M':  proto = &prproto[PR_LONG];	s++;	break;
		case '=':  proto = &eqproto;		s++;	break;
		case 'h':  proto = &hproto;		s++;	break;
		case 'w':  proto = &wproto;		s++;	break;
		default:   proto = &prproto[PR_SHORT];		break;
		}
		free(*proto);
		*proto = estrdup(s);
		break;
	case QUERY:
		parg.p_string = prproto[pr_type];
		error("%s", &parg);
		break;
	}
}

/*
 * Handler for the -b option.
 */
void
opt_b(int type, char *s)
{
	switch (type) {
	case INIT:
	case TOGGLE:
		/*
		 * Set the new number of buffers.
		 */
		ch_setbufspace(bufspace);
		break;
	case QUERY:
		break;
	}
}

/*
 * Handler for the -i option.
 */
void
opt_i(int type, char *s)
{
	switch (type) {
	case TOGGLE:
		chg_caseless();
		break;
	case QUERY:
	case INIT:
		break;
	}
}

/*
 * Handler for the -V option.
 */
void
opt__V(int type, char *s)
{
	switch (type) {
	case TOGGLE:
	case QUERY:
		dispversion();
		break;
	case INIT:
		/*
		 * Force output to stdout per GNU standard for --version output.
		 */
		any_display = 1;
		putstr("less ");
		putstr(version);
		putstr(" (");
		putstr("POSIX ");
		putstr("regular expressions)\n");
		putstr("Copyright (C) 1984-2012 Mark Nudelman\n");
		putstr("Modified for use with illumos by Garrett D'Amore.\n");
		putstr("Copyright 2014 Garrett D'Amore\n\n");
		putstr("less comes with NO WARRANTY, ");
		putstr("to the extent permitted by law.\n");
		putstr("For information about the terms of redistribution,\n");
		putstr("see the file named README in the less distribution.\n");
		putstr("Homepage: http://www.greenwoodsoftware.com/less\n");
		putstr("\n");
		quit(QUIT_OK);
		break;
	}
}

/*
 * Handler for the -x option.
 */
void
opt_x(int type, char *s)
{
	extern int tabstops[];
	extern int ntabstops;
	extern int tabdefault;
	char tabs[60+(4*TABSTOP_MAX)];
	int i;
	PARG p;

	switch (type) {
	case INIT:
	case TOGGLE:
		/* Start at 1 because tabstops[0] is always zero. */
		for (i = 1; i < TABSTOP_MAX; ) {
			int n = 0;
			s = skipsp(s);
			while (*s >= '0' && *s <= '9')
				n = (10 * n) + (*s++ - '0');
			if (n > tabstops[i-1])
				tabstops[i++] = n;
			s = skipsp(s);
			if (*s++ != ',')
				break;
		}
		if (i < 2)
			return;
		ntabstops = i;
		tabdefault = tabstops[ntabstops-1] - tabstops[ntabstops-2];
		break;
	case QUERY:
		(void) strlcpy(tabs, "Tab stops ", sizeof(tabs));
		if (ntabstops > 2) {
			for (i = 1;  i < ntabstops;  i++) {
				if (i > 1)
					strlcat(tabs, ",", sizeof(tabs));
				(void) snprintf(tabs+strlen(tabs),
				    sizeof(tabs)-strlen(tabs),
				    "%d", tabstops[i]);
			}
			(void) snprintf(tabs+strlen(tabs),
			    sizeof(tabs)-strlen(tabs), " and then ");
		}
		(void) snprintf(tabs+strlen(tabs), sizeof(tabs)-strlen(tabs),
		    "every %d spaces", tabdefault);
		p.p_string = tabs;
		error("%s", &p);
		break;
	}
}


/*
 * Handler for the -" option.
 */
void
opt_quote(int type, char *s)
{
	char buf[3];
	PARG parg;

	switch (type) {
	case INIT:
	case TOGGLE:
		if (s[0] == '\0') {
			openquote = closequote = '\0';
			break;
		}
		if (s[1] != '\0' && s[2] != '\0') {
			error("-\" must be followed by 1 or 2 chars",
			    NULL);
			return;
		}
		openquote = s[0];
		if (s[1] == '\0')
			closequote = openquote;
		else
			closequote = s[1];
		break;
	case QUERY:
		buf[0] = openquote;
		buf[1] = closequote;
		buf[2] = '\0';
		parg.p_string = buf;
		error("quotes %s", &parg);
		break;
	}
}

/*
 * "-?" means display a help message.
 * If from the command line, exit immediately.
 */
void
opt_query(int type, char *s)
{
	switch (type) {
	case QUERY:
	case TOGGLE:
		error("Use \"h\" for help", NULL);
		break;
	case INIT:
		dohelp = 1;
	}
}

/*
 * Get the "screen window" size.
 */
int
get_swindow(void)
{
	if (swindow > 0)
		return (swindow);
	return (sc_height + swindow);
}
