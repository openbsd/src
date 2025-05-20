/*
 * Copyright (c) 2000, 2001 Internet Initiative Japan Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistribution with functional modification must include
 *    prominent notice stating how and when and by whom it is
 *    modified.
 *
 * THIS SOFTWARE IS PROVIDED BY ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.
 */

#ifndef BSDMAKE
#include "includes.h"
#endif

#include <sys/types.h>
#include <sys/wait.h>

#include <curses.h>
#include <err.h>
#include <errno.h>
#include <locale.h>
#include <paths.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <time.h>
#include <unistd.h>
#include <wchar.h>
#include <wctype.h>

#define DEFAULT_INTERVAL 2
#define MAXLINE 300
#define MAXCOLUMN 180
#define MAX_COMMAND_LENGTH 128

#define NUM_FRAQ_DIGITS_USEC	6	/* number of fractal digits for usec */
#define MAX_FRAQ_DIGITS		3	/* max number of fractal digits */

typedef enum {
	REVERSE_NONE,
	REVERSE_CHAR,
	REVERSE_WORD,
	REVERSE_LINE
}    reverse_mode_t;

typedef enum {
	RSLT_UPDATE,
	RSLT_REDRAW,
	RSLT_NOTOUCH,
	RSLT_ERROR
}    kbd_result_t;

/*
 * Global symbols
 */
struct timeval	 opt_interval = { DEFAULT_INTERVAL, 0 };
reverse_mode_t
reverse_mode = REVERSE_NONE,		/* reverse mode */
last_reverse_mode = REVERSE_CHAR;	/* remember previous reverse mode */
int start_line = 0, start_column = 0;	/* display offset coordinates */
int prefix = -1;		/* command prefix argument */
int decimal_point = -1;		/* position of decimal point.  */

int pause_status = 0;		/* pause status */
time_t lastupdate;		/* last updated time */
int xflag = 0;

#define	addwch(_x)	addnwstr(&(_x), 1);
#define	WCWIDTH(_x)	((wcwidth((_x)) > 0)? wcwidth((_x)) : 1)

static char	 *cmdstr;
static char	**cmdv;
static int	  style = A_REVERSE;

typedef wchar_t BUFFER[MAXLINE][MAXCOLUMN + 1];

#ifndef MAX
#define MAX(x, y)	((x) > (y) ? (x) : (y))
#endif
#ifndef MIN
#define MIN(x, y)	((x) < (y) ? (x) : (y))
#endif
#ifndef nitems
#define nitems(_x)	(sizeof((_x)) / sizeof((_x)[0]))
#endif

#define ctrl(c)		((c) & 037)
int main(int, char *[]);
void command_loop(void);
int display(BUFFER *, BUFFER *, reverse_mode_t);
void read_result(BUFFER *);
kbd_result_t kbd_command(int);
void showhelp(void);
void untabify(wchar_t *, int);
void on_signal(int);
void quit(void);
void usage(void);
void set_attr(void);
void parse_style(void);
int get_color_num(const char *s);
int get_attr_num(const char *s);

int
main(int argc, char *argv[])
{
	int	 i, ch, cmdsiz = 0;
	char	*e, *s;
	double	 intvl;

	setlocale(LC_ALL, "");
	/*
	 * Command line option handling
	 */
	while ((ch = getopt(argc, argv, "+i:rewps:c:x")) != -1)
		switch (ch) {
		case 'i':
			intvl = strtod(optarg, &e);
			if (*optarg == '\0' || *e != '\0')
				errx(EX_USAGE, "invalid interval: %s", optarg);
			if (*optarg == '-')
				errx(EX_USAGE, "interval must be positive: %s",
					optarg);
			opt_interval.tv_sec = (int)intvl;
			opt_interval.tv_usec = (u_long)
			    (intvl * 1000000UL) % 1000000UL;
			break;
		case 'r':
			reverse_mode = REVERSE_CHAR;
			break;
		case 'w':
			reverse_mode = REVERSE_WORD;
			break;
		case 'e':
			reverse_mode = REVERSE_LINE;
			break;
		case 'p':
			pause_status = 1;
			break;
		case 's':
			start_line = atoi(optarg);
			break;
		case 'c':
			start_column = atoi(optarg);
			break;
		case 'x':
			xflag = 1;
			break;
		default:
			usage();
			exit(EX_USAGE);
		}
	argc -= optind;
	argv += optind;

	/*
	 * Build command string to give to popen
	 */
	if (argc <= 0) {
		usage();
		exit(EX_USAGE);
	}

	if ((cmdv = calloc(argc + 1, sizeof(char *))) == NULL)
		err(EX_OSERR, "calloc");

	cmdstr = "";
	for (i = 0; i < argc; i++) {
		cmdv[i] = argv[i];
		while (strlen(cmdstr) + strlen(argv[i]) + 3 > cmdsiz) {
			if (cmdsiz == 0) {
				cmdsiz = 128;
				s = calloc(cmdsiz, 1);
			} else {
				cmdsiz *= 2;
				s = realloc(cmdstr, cmdsiz);
			}
			if (s == NULL)
				err(EX_OSERR, "malloc");
			cmdstr = s;
		}
		if (i != 0)
			strlcat(cmdstr, " ", cmdsiz);
		strlcat(cmdstr, argv[i], cmdsiz);
	}
	cmdv[i++] = NULL;

	/*
	 * Initialize signal
	 */
	(void) signal(SIGINT, on_signal);
	(void) signal(SIGTERM, on_signal);
	(void) signal(SIGHUP, on_signal);

	/*
	 * Initialize curses environment
	 */
	initscr();
	start_color();
	use_default_colors();
	parse_style();
	noecho();
	crmode();

	/*
	 * Enter main processing loop and never come back here
	 */
	command_loop();

	/* NOTREACHED */
	abort();
}

void
command_loop(void)
{
	int		 i, nfds;
	BUFFER		 buf0, buf1;
	fd_set		 readfds;
	struct timeval	 to;

	for (i = 0; ; i++) {
		BUFFER *cur, *prev;

		if (i == 0) {
			cur = prev = &buf0;
		} else if (i % 2 == 0) {
			cur = &buf0;
			prev = &buf1;
		} else {
			cur = &buf1;
			prev = &buf0;
		}

		read_result(cur);

redraw:
		display(cur, prev, reverse_mode);

input:
		to = opt_interval;
		FD_ZERO(&readfds);
		FD_SET(fileno(stdin), &readfds);
		nfds = select(1, &readfds, NULL, NULL,
		    (pause_status)? NULL : &to);
		if (nfds < 0)
			switch (errno) {
			case EINTR:
				/*
				 * ncurses has changed the window size with
				 * SIGWINCH.  call doupdate() to use the
				 * updated window size.
				 */
				doupdate();
				goto redraw;
			default:
				perror("select");
			}
		else if (nfds > 0) {
			int ch = getch();
			kbd_result_t result = kbd_command(ch);

			switch (result) {
			case RSLT_UPDATE:	/* update buffer */
				break;
			case RSLT_REDRAW:	/* scroll with current buffer */
				goto redraw;
			case RSLT_NOTOUCH:	/* silently loop again */
				goto input;
			case RSLT_ERROR:	/* error */
				fprintf(stderr, "\007");
				goto input;
			}
		}
	}
}

int
display(BUFFER * cur, BUFFER * prev, reverse_mode_t reverse)
{
	int	 i, val, screen_x, screen_y, cw, line, rl;
	char	*ct;

	erase();

	move(0, 0);
	if ((int)strlen(cmdstr) > COLS - 47)
		printw("\"%-.*s..\" ", COLS - 49, cmdstr);
	else
		printw("\"%s\" ", cmdstr);
	if (pause_status)
		printw("--PAUSE--");
	else if (opt_interval.tv_sec == 1 && opt_interval.tv_usec == 0)
		printw("on every second");
	else if (opt_interval.tv_usec == 0)
		printw("on every %d seconds", (int)opt_interval.tv_sec);
	else {
		for (i = NUM_FRAQ_DIGITS_USEC, val = opt_interval.tv_usec;
		    val % 10 == 0; val /= 10)
			i--;
		printw("on every %d.%0*d seconds",
		    (int)opt_interval.tv_sec, i, val);
	}

	ct = ctime(&lastupdate);
	ct[24] = '\0';
	move(0, COLS - strlen(ct));
	addstr(ct);

#define MODELINE(HOTKEY,SWITCH,MODE)				\
	do {							\
		printw(HOTKEY);					\
		if (reverse == SWITCH) attron(style);		\
		printw(MODE);					\
		if (reverse == SWITCH) attrset(A_NORMAL);	\
	} while (0/* CONSTCOND */)

	move(1, COLS - 47);
	printw("Reverse mode:");
	MODELINE(" [w]", REVERSE_WORD, "word");
	MODELINE(" [e]", REVERSE_LINE, "line");
	MODELINE(" [r]", REVERSE_CHAR, "char");
	printw(" [t]toggle");

	move(1, 1);
	if (prefix >= 0) {
		if (decimal_point > 0) {
			int power10;

			for (i = 0, power10 = 1; i < decimal_point; i++)
				power10 *= 10;

			printw("%d.%0*d", prefix / power10, decimal_point,
			    prefix % power10);
		} else if (decimal_point == 0)
			printw("%d. ", prefix);
		else
			printw("%d ", prefix);
	}

	if (start_line != 0 || start_column != 0)
		printw("(%d, %d)", start_line, start_column);

	if (!prev || (cur == prev))
		reverse = REVERSE_NONE;

	for (line = start_line, screen_y = 2;
	    screen_y < LINES && line < MAXLINE && (*cur)[line][0];
	    line++, screen_y++) {
		wchar_t	*cur_line, *prev_line, *p, *pp;

		rl = 0;	/* reversing line */
		cur_line = (*cur)[line];
		prev_line = (*prev)[line];

		for (p = cur_line, cw = 0; cw < start_column; p++)
			cw += WCWIDTH(*p);
		screen_x = cw - start_column;
		for (pp = prev_line, cw = 0; cw < start_column; pp++)
			cw += WCWIDTH(*pp);

		switch (reverse) {
		case REVERSE_LINE:
			if (wcscmp(cur_line, prev_line)) {
				attron(style);
				rl = 1;
				for (i = 0; i < screen_x; i++) {
					move(screen_y, i);
					addch(' ');
				}
			}
			/* FALLTHROUGH */

		case REVERSE_NONE:
			move(screen_y, screen_x);
			while (screen_x < COLS) {
				if (*p && *p != L'\n') {
					cw = wcwidth(*p);
					if (screen_x + cw >= COLS)
						break;
					addwch(*p++);
					pp++;
					screen_x += cw;
				} else if (rl) {
					addch(' ');
					screen_x++;
				} else
					break;
			}
			attrset(A_NORMAL);
			break;

		case REVERSE_WORD:
		case REVERSE_CHAR:
			move(screen_y, screen_x);
			while (*p && screen_x < COLS) {
				cw = wcwidth(*p);
				if (screen_x + cw >= COLS)
					break;
				if (*p == *pp) {
					addwch(*p++);
					pp++;
					screen_x += cw;
					continue;
				}
				/*
				 * This method to reverse by word unit is not
				 * very fancy but it was easy to implement.  If
				 * you are urged to rewrite this algorithm, it
				 * is right thing and don't hesitate to do so!
				 */
				/*
				 * If the word reverse option is specified and
				 * the current character is not a space, track
				 * back to the beginning of the word.
				 */
				if (reverse == REVERSE_WORD && !iswspace(*p)) {
					while (cur_line + start_column < p &&
					    !iswspace(*(p - 1))) {
						p--;
						pp--;
						screen_x -= wcwidth(*p);
					}
					move(screen_y, screen_x);
				}
				attron(style);

				/* Print character itself.  */
				cw = wcwidth(*p);
				addwch(*p++);
				pp++;
				screen_x += cw;

				/*
				 * If the word reverse option is specified, and
				 * the current character is not a space, print
				 * the whole word which includes current
				 * character.
				 */
				if (reverse == REVERSE_WORD) {
					while (*p && !iswspace(*p) &&
					    screen_x < COLS) {
						cw = wcwidth(*p);
						addwch(*p++);
						pp++;
						screen_x += cw;
					}
				}
				attrset(A_NORMAL);
			}
			break;
		}
	}
	move(1, 0);
	refresh();
	return (1);
}

void
read_result(BUFFER *buf)
{
	FILE	*fp;
	int	 i, st, fds[2];
	pid_t	 pipe_pid, pid;

	/* Clear buffer */
	memset(buf, 0, sizeof(*buf));

	if (pipe(fds) == -1)
		err(EX_OSERR, "pipe()");

	if ((pipe_pid = vfork()) == -1)
		err(EX_OSERR, "vfork()");
	else if (pipe_pid == 0) {
		close(fds[0]);
		if (fds[1] != STDOUT_FILENO) {
			dup2(fds[1], STDOUT_FILENO);
			close(fds[1]);
		}
		if (xflag)
			execvp(cmdv[0], cmdv);
		else
			execl(_PATH_BSHELL, _PATH_BSHELL, "-c", cmdstr, NULL);

		/* use warn(3) + _exit(2) not to call exit(3) */
		warn("exec(%s)", cmdstr);
		_exit(EX_OSERR);

		/* NOTREACHED */
	}
	if ((fp = fdopen(fds[0], "r")) == NULL)
		err(EX_OSERR, "fdopen()");
	close(fds[1]);

	/* Read command output and convert tab to spaces * */
	for (i = 0; i < MAXLINE && fgetws((*buf)[i], MAXCOLUMN, fp) != NULL;
	    i++)
		untabify((*buf)[i], sizeof((*buf)[i]));
	fclose(fp);
	do {
		pid = waitpid(pipe_pid, &st, 0);
	} while (pid == -1 && errno == EINTR);

	/* Remember update time */
	time(&lastupdate);
}

/* ch: command character */
kbd_result_t
kbd_command(int ch)
{
	switch (ch) {

	case '?':
		showhelp();
		refresh();
		return (RSLT_REDRAW);

	case '\033':
		prefix = -1;
		return (RSLT_REDRAW);

	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
		if (prefix < 0) {
			prefix = 0;
			decimal_point = -1;
		}
		if (decimal_point >= MAX_FRAQ_DIGITS)
			return (RSLT_REDRAW);

		if (decimal_point >= 0)
			decimal_point++;
		prefix = prefix * 10 + (ch - '0');
		return (RSLT_REDRAW);

	case '.':
		if (decimal_point < 0)
			decimal_point = 0;
		return (RSLT_REDRAW);

		/*
		 * Update buffer
		 */
	case ' ':
		return (RSLT_UPDATE);

		/*
		 * Pause switch
		 */
	case 'p':
		if ((pause_status = !pause_status) != 0)
			return (RSLT_REDRAW);
		else
			return (RSLT_UPDATE);

		/*
		 * Reverse control
		 */
	case 't':
		if (reverse_mode != REVERSE_NONE) {
			last_reverse_mode = reverse_mode;
			reverse_mode = REVERSE_NONE;
		} else {
			reverse_mode = last_reverse_mode;
		}
		break;
	case 'r':
		reverse_mode = (reverse_mode == REVERSE_CHAR) ? REVERSE_NONE
		    : REVERSE_CHAR;
		break;
	case 'w':
		reverse_mode = (reverse_mode == REVERSE_WORD) ? REVERSE_NONE
		    : REVERSE_WORD;
		break;
	case 'e':
		reverse_mode = (reverse_mode == REVERSE_LINE) ? REVERSE_NONE
		    : REVERSE_LINE;
		break;

		/*
		 * Set interval
		 */
	case 'i':
		if (prefix >= 0) {
			int i, power10;

			for (power10 = 1, i = 0; i < decimal_point; i++)
				power10 *= 10;

			opt_interval.tv_sec = prefix / power10;
			opt_interval.tv_usec = prefix % power10;
			for (i = NUM_FRAQ_DIGITS_USEC; i < decimal_point; i++)
				opt_interval.tv_usec /= 10;
			for (i = decimal_point; i < NUM_FRAQ_DIGITS_USEC; i++)
				opt_interval.tv_usec *= 10;

			prefix = -1;
		}
		return (RSLT_REDRAW);

		/*
		 * Vertical motion
		 */
	case '\n':
	case '+':
	case 'j':
		start_line = MIN(start_line + 1, MAXLINE - 1);
		break;
	case '-':
	case 'k':
		start_line = MAX(start_line - 1, 0);
		break;
	case 'd':
	case 'D':
	case ctrl('d'):
		start_line = MIN(start_line + ((LINES - 2) / 2), MAXLINE - 1);
		break;
	case 'u':
	case 'U':
	case ctrl('u'):
		start_line = MAX(start_line - ((LINES - 2) / 2), 0);
		break;
	case 'f':
	case ctrl('f'):
		start_line = MIN(start_line + (LINES - 2), MAXLINE - 1);
		break;
	case 'b':
	case ctrl('b'):
		start_line = MAX(start_line - (LINES - 2), 0);
		break;
	case 'g':
		if (prefix < MAXLINE)
			start_line = prefix;
		prefix = -1;
		break;

		/*
		 * horizontal motion
		 */
	case 'l':
		start_column = MIN(start_column + 1, MAXCOLUMN - 1);
		break;
	case ctrl('l'):
		clear();
		break;
	case 'h':
		start_column = MAX(start_column - 1, 0);
		break;
	case 'L':
		start_column = MIN(start_column + ((COLS - 2) / 2),
		    MAXCOLUMN - 1);
		break;
	case 'H':
		start_column = MAX(start_column - ((COLS - 2) / 2), 0);
		break;
	case ']':
	case '\t':
		start_column = MIN(start_column + 8, MAXCOLUMN - 1);
		break;
	case '[':
	case '\b':
		start_column = MAX(start_column - 8, 0);
		break;
	case '>':
		start_column = MIN(start_column + (COLS - 2), MAXCOLUMN - 1);
		break;
	case '<':
		start_column = MAX(start_column - (COLS - 2), 0);
		break;
	case '{':
		start_column = 0;
		break;

		/*
		 * quit
		 */
	case 'q':
	case 'Q':
		quit();
		break;

	default:
		return (RSLT_ERROR);

	}

	prefix = -1;
	return (RSLT_REDRAW);
}

const char *helpmsg[] = {
	" Scroll:                                         ",
	"            1-char    half-win  full-win  8-char ",
	"   UP       k, -      u, ^u     b, ^b            ",
	"   DOWN     j, RET    d, ^d     f, ^f            ",
	"   RIGHT    l         L         >         ]      ",
	"   LEFT     h         H         <         [      ",
	"                                                 ",
	"   g        goto top or prefix number line       ",
	"                                                 ",
	" Others:                                         ",
	"   space    update buffer                        ",
	"   ctrl-l   refresh screen                       ",
	"   0..9     prefix number argument               ",
	"   r        reverse character                    ",
	"   e        reverse entire line                  ",
	"   w        reverse word                         ",
	"   t        toggle reverse mode                  ",
	"   i        set interval for prefix number       ",
	"   p        pause and restart                    ",
	"   ?        show this message                    ",
	"   q        quit                                 ",
	(char *) 0,
};

void
showhelp(void)
{
	size_t length = 0;
	int lines;
	int x, y;
	int i;
	const char *cp;
	WINDOW *helpwin;
	const char *cont_msg = " Hit any key to continue ";

	for (lines = 0; (cp = helpmsg[lines]) != (char *) 0; lines++)
		if (strlen(cp) > length)
			length = strlen(cp);

	x = ((COLS - length - 2) + 1) / 2;
	y = ((LINES - lines - 2) + 1) / 2;

	if (x < 0 || y < 0) {
		fprintf(stderr, "\007");
		return;
	}
	helpwin = newwin(lines + 2, length + 2, y, x);

	wclear(helpwin);
	box(helpwin, '|', '-');

	for (i = 0; i < lines; i++) {
		wmove(helpwin, i + 1, 1);
		waddstr(helpwin, helpmsg[i]);
	}

	wmove(helpwin, lines + 1, length + 1 - strlen(cont_msg));
	wstandout(helpwin);
	waddstr(helpwin, cont_msg);
	wstandend(helpwin);

	wrefresh(helpwin);
	(void) wgetch(helpwin);
	werase(helpwin);
	wrefresh(helpwin);
	delwin(helpwin);
}

void
untabify(wchar_t *buf, int maxlen)
{
	int	 i, tabstop = 8, len, spaces, width = 0, maxcnt;
	wchar_t *p = buf;

	maxcnt = maxlen / sizeof(wchar_t);
	while (*p && p - buf < maxcnt - 1) {
		if (*p != L'\t') {
			width += wcwidth(*p);
			p++;
		} else {
			spaces = tabstop - (width % tabstop);
			len = MIN(maxcnt - (p + spaces - buf),
			    (int)wcslen(p + 1) + 1);
			if (len > 0)
				memmove(p + spaces, p + 1,
				    len * sizeof(wchar_t));
			len = MIN(spaces, maxcnt - 1 - (p - buf));
			for (i = 0; i < len; i++)
				p[i] = L' ';
			p += len;
			width += len;
		}
	}
	*p = L'\0';
}

void
on_signal(int signum)
{
	quit();
}

void
quit(void)
{
	erase();
	refresh();
	endwin();
	free(cmdv);
	exit(EXIT_SUCCESS);
}

void
usage(void)
{
	extern char *__progname;

	fprintf(stderr,
	    "usage: %s [-rewp] [-i interval] [-s start_line] "
		    "[-c start_column]\n"
	    "       %*s command [arg ...]\n",
	    __progname, (int) strlen(__progname), " ");
}

void
parse_style(void)
{
	int	i;
	char	*p, *st, *st0, *token;
	struct _codestrings {
		int		 code;
		int		 no;
		const char	*string;
	} colors[] = {
		{ COLOR_BLACK,		0, "black"},
		{ COLOR_RED,		0, "red" },
		{ COLOR_GREEN,		0, "green" },
		{ COLOR_YELLOW,		0, "yellow" },
		{ COLOR_BLUE,		0, "blue" },
		{ COLOR_MAGENTA,	0, "magenta" },
		{ COLOR_CYAN,		0, "cyan" },
		{ COLOR_WHITE,		0, "white" },
	}, attrs[] = {
		{ A_UNDERLINE,		0, "underline" },
		{ A_REVERSE,		0, "reverse" },
		{ A_DIM,		0, "dim" },
		{ A_BOLD,		0, "bold" },
		{ A_UNDERLINE,		1, "nounderline" },
		{ A_REVERSE,		1, "noreverse" },
		{ A_DIM,		1, "nodim" },
		{ A_BOLD,		1, "nobold" },
	};

	if ((p = getenv("IWATCH_STYLE")) != NULL) {
		st = st0 = strdup(p);

		while ((token = strsep(&st, " ,")) != NULL) {
			if (*token == '\0')
				continue;
			for (i = 0; i < nitems(colors); i++) {
				if (strcasecmp(token, colors[i].string) == 0) {
					init_pair(1, colors[i].code, -1);
					style &= ~A_COLOR;
					style |= COLOR_PAIR(1);
					goto next_token;
				}
			}
			for (i = 0; i < nitems(attrs); i++) {
				if (strcasecmp(token, attrs[i].string) == 0) {
					if (attrs[i].no)
						style &= ~attrs[i].code;
					else
						style |= attrs[i].code;
					goto next_token;
				}
			}
 next_token:
			/* empty */;
		}
		free(st0);
	}
}
