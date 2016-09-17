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
 * Prompting and other messages.
 * There are three flavors of prompts, SHORT, MEDIUM and LONG,
 * selected by the -m/-M options.
 * There is also the "equals message", printed by the = command.
 * A prompt is a message composed of various pieces, such as the
 * name of the file being viewed, the percentage into the file, etc.
 */

#include "less.h"
#include "position.h"

extern int pr_type;
extern int new_file;
extern int sc_width;
extern int so_s_width, so_e_width;
extern int linenums;
extern int hshift;
extern int sc_height;
extern int jump_sline;
extern int less_is_more;
extern IFILE curr_ifile;
extern char *editor;
extern char *editproto;

/*
 * Prototypes for the three flavors of prompts.
 * These strings are expanded by pr_expand().
 */
static const char s_proto[] =
	"?n?f%f .?m(%T %i of %m) ..?e(END) ?x- Next\\: %x..%t";
static const char m_proto[] =
	"?n?f%f .?m(%T %i of %m) ..?e(END) "
	"?x- Next\\: %x.:?pB%pB\\%:byte %bB?s/%s...%t";
static const char M_proto[] =
	"?f%f .?n?m(%T %i of %m) ..?"
	"ltlines %lt-%lb?L/%L. :byte %bB?s/%s. .?e(END)"
	" ?x- Next\\: %x.:?pB%pB\\%..%t";
static const char e_proto[] =
	"?f%f .?m(%T %i of %m) .?ltlines "
	"%lt-%lb?L/%L. .byte %bB?s/%s. ?e(END) :?pB%pB\\%..%t";
static const char h_proto[] =
	"HELP -- ?eEND -- Press g to see it again:"
	"Press RETURN for more., or q when done";
static const char w_proto[] =
	"Waiting for data";
static const char more_proto[] =
	"%f (?eEND ?x- Next\\: %x.:?pB%pB\\%:byte %bB?s/%s...%t)";
static const char more_M_proto[] =
	"%f (?eEND ?x- Next\\: %x.:?pB%pB\\%:byte %bB?s/%s...%t)"
	"[Press space to continue, q to quit, h for help]";

char *prproto[3];
char const *eqproto = e_proto;
char const *hproto = h_proto;
char const *wproto = w_proto;

static char message[PROMPT_SIZE];
static char *mp;

/*
 * Initialize the prompt prototype strings.
 */
void
init_prompt(void)
{
	prproto[0] = estrdup(s_proto);
	prproto[1] = estrdup(less_is_more ? more_proto : m_proto);
	prproto[2] = estrdup(less_is_more ? more_M_proto : M_proto);
	eqproto = estrdup(e_proto);
	hproto = estrdup(h_proto);
	wproto = estrdup(w_proto);
}

/*
 * Append a string to the end of the message.
 */
static void
ap_str(char *s)
{
	int len;

	len = strlen(s);
	if (mp + len >= message + PROMPT_SIZE)
		len = message + PROMPT_SIZE - mp - 1;
	(void) strncpy(mp, s, len);
	mp += len;
	*mp = '\0';
}

/*
 * Append a character to the end of the message.
 */
static void
ap_char(char c)
{
	char buf[2];

	buf[0] = c;
	buf[1] = '\0';
	ap_str(buf);
}

/*
 * Append a off_t (as a decimal integer) to the end of the message.
 */
static void
ap_pos(off_t pos)
{
	char buf[23];

	postoa(pos, buf, sizeof(buf));
	ap_str(buf);
}

/*
 * Append an integer to the end of the message.
 */
static void
ap_int(int num)
{
	char buf[13];

	inttoa(num, buf, sizeof buf);
	ap_str(buf);
}

/*
 * Append a question mark to the end of the message.
 */
static void
ap_quest(void)
{
	ap_str("?");
}

/*
 * Return the "current" byte offset in the file.
 */
static off_t
curr_byte(int where)
{
	off_t pos;

	pos = position(where);
	while (pos == -1 && where >= 0 && where < sc_height-1)
		pos = position(++where);
	if (pos == -1)
		pos = ch_length();
	return (pos);
}

/*
 * Return the value of a prototype conditional.
 * A prototype string may include conditionals which consist of a
 * question mark followed by a single letter.
 * Here we decode that letter and return the appropriate boolean value.
 */
static int
cond(char c, int where)
{
	off_t len;

	switch (c) {
	case 'a':	/* Anything in the message yet? */
		return (*message != '\0');
	case 'b':	/* Current byte offset known? */
		return (curr_byte(where) != -1);
	case 'c':
		return (hshift != 0);
	case 'e':	/* At end of file? */
		return (eof_displayed());
	case 'f':	/* Filename known? */
		return (strcmp(get_filename(curr_ifile), "-") != 0);
	case 'l':	/* Line number known? */
	case 'd':	/* Same as l */
		return (linenums);
	case 'L':	/* Final line number known? */
	case 'D':	/* Final page number known? */
		return (linenums && ch_length() != -1);
	case 'm':	/* More than one file? */
		return (ntags() ? (ntags() > 1) : (nifile() > 1));
	case 'n':	/* First prompt in a new file? */
		return (ntags() ? 1 : new_file);
	case 'p':	/* Percent into file (bytes) known? */
		return (curr_byte(where) != -1 && ch_length() > 0);
	case 'P':	/* Percent into file (lines) known? */
		return (currline(where) != 0 &&
		    (len = ch_length()) > 0 && find_linenum(len) != 0);
	case 's':	/* Size of file known? */
	case 'B':
		return (ch_length() != -1);
	case 'x':	/* Is there a "next" file? */
		if (ntags())
			return (0);
		return (next_ifile(curr_ifile) != NULL);
	}
	return (0);
}

/*
 * Decode a "percent" prototype character.
 * A prototype string may include various "percent" escapes;
 * that is, a percent sign followed by a single letter.
 * Here we decode that letter and take the appropriate action,
 * usually by appending something to the message being built.
 */
static void
protochar(int c, int where)
{
	off_t pos;
	off_t len;
	int n;
	off_t linenum;
	off_t last_linenum;
	IFILE h;

#undef	PAGE_NUM
#define	PAGE_NUM(linenum)  ((((linenum) - 1) / (sc_height - 1)) + 1)

	switch (c) {
	case 'b':	/* Current byte offset */
		pos = curr_byte(where);
		if (pos != -1)
			ap_pos(pos);
		else
			ap_quest();
		break;
	case 'c':
		ap_int(hshift);
		break;
	case 'd':	/* Current page number */
		linenum = currline(where);
		if (linenum > 0 && sc_height > 1)
			ap_pos(PAGE_NUM(linenum));
		else
			ap_quest();
		break;
	case 'D':	/* Final page number */
		/* Find the page number of the last byte in the file (len-1). */
		len = ch_length();
		if (len == -1) {
			ap_quest();
		} else if (len == 0) {
			/* An empty file has no pages. */
			ap_pos(0);
		} else {
			linenum = find_linenum(len - 1);
			if (linenum <= 0)
				ap_quest();
			else
				ap_pos(PAGE_NUM(linenum));
		}
		break;
	case 'E':	/* Editor name */
		ap_str(editor);
		break;
	case 'f':	/* File name */
		ap_str(get_filename(curr_ifile));
		break;
	case 'F':	/* Last component of file name */
		ap_str(last_component(get_filename(curr_ifile)));
		break;
	case 'i':	/* Index into list of files */
		if (ntags())
			ap_int(curr_tag());
		else
			ap_int(get_index(curr_ifile));
		break;
	case 'l':	/* Current line number */
		linenum = currline(where);
		if (linenum != 0)
			ap_pos(linenum);
		else
			ap_quest();
		break;
	case 'L':	/* Final line number */
		len = ch_length();
		if (len == -1 || len == ch_zero() ||
		    (linenum = find_linenum(len)) <= 0)
			ap_quest();
		else
			ap_pos(linenum-1);
		break;
	case 'm':	/* Number of files */
		n = ntags();
		if (n)
			ap_int(n);
		else
			ap_int(nifile());
		break;
	case 'p':	/* Percent into file (bytes) */
		pos = curr_byte(where);
		len = ch_length();
		if (pos != -1 && len > 0)
			ap_int(percentage(pos, len));
		else
			ap_quest();
		break;
	case 'P':	/* Percent into file (lines) */
		linenum = currline(where);
		if (linenum == 0 ||
		    (len = ch_length()) == -1 || len == ch_zero() ||
		    (last_linenum = find_linenum(len)) <= 0)
			ap_quest();
		else
			ap_int(percentage(linenum, last_linenum));
		break;
	case 's':	/* Size of file */
	case 'B':
		len = ch_length();
		if (len != -1)
			ap_pos(len);
		else
			ap_quest();
		break;
	case 't':	/* Truncate trailing spaces in the message */
		while (mp > message && mp[-1] == ' ')
			mp--;
		*mp = '\0';
		break;
	case 'T':	/* Type of list */
		if (ntags())
			ap_str("tag");
		else
			ap_str("file");
		break;
	case 'x':	/* Name of next file */
		h = next_ifile(curr_ifile);
		if (h != NULL)
			ap_str(get_filename(h));
		else
			ap_quest();
		break;
	}
}

/*
 * Skip a false conditional.
 * When a false condition is found (either a false IF or the ELSE part
 * of a true IF), this routine scans the prototype string to decide
 * where to resume parsing the string.
 * We must keep track of nested IFs and skip them properly.
 */
static const char *
skipcond(const char *p)
{
	int iflevel;

	/*
	 * We came in here after processing a ? or :,
	 * so we start nested one level deep.
	 */
	iflevel = 1;

	for (;;) {
		switch (*++p) {
		case '?':
			/*
			 * Start of a nested IF.
			 */
			iflevel++;
			break;
		case ':':
			/*
			 * Else.
			 * If this matches the IF we came in here with,
			 * then we're done.
			 */
			if (iflevel == 1)
				return (p);
			break;
		case '.':
			/*
			 * Endif.
			 * If this matches the IF we came in here with,
			 * then we're done.
			 */
			if (--iflevel == 0)
				return (p);
			break;
		case '\\':
			/*
			 * Backslash escapes the next character.
			 */
			++p;
			break;
		case '\0':
			/*
			 * Whoops.  Hit end of string.
			 * This is a malformed conditional, but just treat it
			 * as if all active conditionals ends here.
			 */
			return (p-1);
		}
	}
}

/*
 * Decode a char that represents a position on the screen.
 */
static const char *
wherechar(const char *p, int *wp)
{
	switch (*p) {
	case 'b': case 'd': case 'l': case 'p': case 'P':
		switch (*++p) {
		case 't':   *wp = TOP;			break;
		case 'm':   *wp = MIDDLE;		break;
		case 'b':   *wp = BOTTOM;		break;
		case 'B':   *wp = BOTTOM_PLUS_ONE;	break;
		case 'j':   *wp = adjsline(jump_sline);	break;
		default:    *wp = TOP; p--;		break;
		}
	}
	return (p);
}

/*
 * Construct a message based on a prototype string.
 */
char *
pr_expand(const char *proto, int maxwidth)
{
	const char *p;
	int c;
	int where;

	mp = message;

	if (*proto == '\0')
		return ("");

	for (p = proto; *p != '\0'; p++) {
		switch (*p) {
		default:	/* Just put the character in the message */
			ap_char(*p);
			break;
		case '\\':	/* Backslash escapes the next character */
			p++;
			ap_char(*p);
			break;
		case '?':	/* Conditional (IF) */
			if ((c = *++p) == '\0') {
				--p;
			} else {
				where = 0;
				p = wherechar(p, &where);
				if (!cond(c, where))
					p = skipcond(p);
			}
			break;
		case ':':	/* ELSE */
			p = skipcond(p);
			break;
		case '.':	/* ENDIF */
			break;
		case '%':	/* Percent escape */
			if ((c = *++p) == '\0') {
				--p;
			} else {
				where = 0;
				p = wherechar(p, &where);
				protochar(c, where);
			}
			break;
		}
	}

	if (*message == '\0')
		return ("");
	if (maxwidth > 0 && mp >= message + maxwidth) {
		/*
		 * Message is too long.
		 * Return just the final portion of it.
		 */
		return (mp - maxwidth);
	}
	return (message);
}

/*
 * Return a message suitable for printing by the "=" command.
 */
char *
eq_message(void)
{
	return (pr_expand(eqproto, 0));
}

/*
 * Return a prompt.
 * This depends on the prompt type (SHORT, MEDIUM, LONG), etc.
 * If we can't come up with an appropriate prompt, return NULL
 * and the caller will prompt with a colon.
 */
char *
prompt_string(void)
{
	char *prompt;
	int type;

	type = pr_type;
	prompt = pr_expand((ch_getflags() & CH_HELPFILE) ?
	    hproto : prproto[type], sc_width-so_s_width-so_e_width-2);
	new_file = 0;
	return (prompt);
}

/*
 * Return a message suitable for printing while waiting in the F command.
 */
char *
wait_message(void)
{
	return (pr_expand(wproto, sc_width-so_s_width-so_e_width-2));
}
