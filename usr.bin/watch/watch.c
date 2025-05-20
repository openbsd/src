/*	$OpenBSD: watch.c,v 1.10 2025/05/20 08:24:16 job Exp $ */
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

#define DEFAULT_INTERVAL 1
#define MAXLINE 300
#define MAXCOLUMN 180

#define NUM_FRAQ_DIGITS_USEC	6	/* number of fractal digits for usec */
#define MAX_FRAQ_DIGITS		3	/* max number of fractal digits */

typedef enum {
	HIGHLIGHT_NONE,
	HIGHLIGHT_CHAR,
	HIGHLIGHT_WORD,
	HIGHLIGHT_LINE
}    highlight_mode_t;

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

highlight_mode_t highlight_mode = HIGHLIGHT_NONE;
highlight_mode_t last_highlight_mode = HIGHLIGHT_CHAR;

int start_line = 0, start_column = 0;	/* display offset coordinates */
int decimal_point = -1;		/* position of decimal point.  */

int pause_status = 0;		/* pause status */
time_t lastupdate;		/* last updated time */
int xflag = 0;

#define	addwch(_x)	addnwstr(&(_x), 1);
#define	WCWIDTH(_x)	((wcwidth((_x)) > 0)? wcwidth((_x)) : 1)

static char	 *cmdstr;
static char	**cmdv;

typedef wchar_t BUFFER[MAXLINE][MAXCOLUMN + 1];

#define MAXIMUM(a, b)	(((a) > (b)) ? (a) : (b))
#define MINIMUM(a, b)	(((a) < (b)) ? (a) : (b))

#define ctrl(c)		((c) & 037)
int main(int, char *[]);
void command_loop(void);
int display(BUFFER *, BUFFER *, highlight_mode_t);
void read_result(BUFFER *);
kbd_result_t kbd_command(int);
void show_help(void);
void untabify(wchar_t *, int);
void on_signal(int);
void quit(void);
void usage(void);

int
main(int argc, char *argv[])
{
	int i, ch, cmdsiz = 0;
	char *e, *s, *endp;
	double intvl;

	while ((ch = getopt(argc, argv, "ers:wx")) != -1)
		switch (ch) {
		case 'r':
			highlight_mode = HIGHLIGHT_CHAR;
			break;
		case 's':
			intvl = strtod(optarg, &endp);
			if (intvl >= 0 && intvl <= 1000000 && *endp == '\0') {
				opt_interval.tv_sec = (int)intvl;
				opt_interval.tv_usec =
				    (u_long)(intvl * 1000000UL) % 1000000UL;
				break;
			} else
				errx(1, "-n: bad interval value: %s", optarg);
			break;
		case 'w':
			highlight_mode = HIGHLIGHT_WORD;
			break;
		case 'e':
			highlight_mode = HIGHLIGHT_LINE;
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

	initscr();

	if (unveil(xflag ? cmdv[0] : _PATH_BSHELL, "x") == -1)
		err(1, "unveil");

	if (pledge("stdio rpath tty proc exec", NULL) == -1)
		err(1, "pledge");

	noecho();
	crmode();

	/*
	 * Initialize signal
	 */
	(void) signal(SIGINT, on_signal);
	(void) signal(SIGTERM, on_signal);
	(void) signal(SIGHUP, on_signal);

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
		display(cur, prev, highlight_mode);

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
display(BUFFER * cur, BUFFER * prev, highlight_mode_t hm)
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

	move(1, 1);

	if (!prev || (cur == prev))
		hm = HIGHLIGHT_NONE;

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

		switch (hm) {
		case HIGHLIGHT_LINE:
			if (wcscmp(cur_line, prev_line)) {
				standout();
				rl = 1;
				for (i = 0; i < screen_x; i++) {
					move(screen_y, i);
					addch(' ');
				}
			}
			/* FALLTHROUGH */

		case HIGHLIGHT_NONE:
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
			standend();
			break;

		case HIGHLIGHT_WORD:
		case HIGHLIGHT_CHAR:
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
				 * If the word highlight option is specified and
				 * the current character is not a space, track
				 * back to the beginning of the word.
				 */
				if (hm == HIGHLIGHT_WORD && !iswspace(*p)) {
					while (cur_line + start_column < p &&
					    !iswspace(*(p - 1))) {
						p--;
						pp--;
						screen_x -= wcwidth(*p);
					}
					move(screen_y, screen_x);
				}
				standout();

				/* Print character itself.  */
				cw = wcwidth(*p);
				addwch(*p++);
				pp++;
				screen_x += cw;

				/*
				 * If the word highlight option is specified, and
				 * the current character is not a space, print
				 * the whole word which includes current
				 * character.
				 */
				if (hm == HIGHLIGHT_WORD) {
					while (*p && !iswspace(*p) &&
					    screen_x < COLS) {
						cw = wcwidth(*p);
						addwch(*p++);
						pp++;
						screen_x += cw;
					}
				}
				standend();
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

kbd_result_t
kbd_command(int ch)
{
	char buf[10], *endp;
	double intvl;

	switch (ch) {

	case '?':
		show_help();
		refresh();
		return (RSLT_REDRAW);

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

	case 't':
		if (highlight_mode != HIGHLIGHT_NONE) {
			last_highlight_mode = highlight_mode;
			highlight_mode = HIGHLIGHT_NONE;
		} else {
			highlight_mode = last_highlight_mode;
		}
		break;
	case 'r':
		if (highlight_mode == HIGHLIGHT_CHAR)
			highlight_mode = HIGHLIGHT_NONE;
		else
			highlight_mode = HIGHLIGHT_CHAR;
		break;
	case 'w':
		if (highlight_mode == HIGHLIGHT_WORD)
			highlight_mode = HIGHLIGHT_NONE;
		else
			highlight_mode = HIGHLIGHT_WORD;
		break;
	case 'e':
		if (highlight_mode == HIGHLIGHT_LINE)
			highlight_mode = HIGHLIGHT_NONE;
		else
			highlight_mode = HIGHLIGHT_LINE;
		break;

	case 's':
		move(1, 0);

		standout();
		printw("New interval: ");
		standend();

		echo();
		getnstr(buf, sizeof(buf));
		noecho();

		intvl = strtod(buf, &endp);
		if (intvl >= 0 && intvl <= 1000000 && *endp == '\0') {
			opt_interval.tv_sec = (int)intvl;
			opt_interval.tv_usec =
			    (u_long)(intvl * 1000000UL) % 1000000UL;
		} else {
			move(1, 0);
			standout();
			printw("Interval should be a non-negative number");
			standend();
			refresh();
			return (RSLT_ERROR);
		}

		return (RSLT_REDRAW);

	case '\n':
	case '+':
	case 'j':
		start_line = MINIMUM(start_line + 1, MAXLINE - 1);
		break;
	case '-':
	case 'k':
		start_line = MAXIMUM(start_line - 1, 0);
		break;
	case 'd':
	case 'D':
	case ctrl('d'):
		start_line = MINIMUM(start_line + ((LINES - 2) / 2), MAXLINE - 1);
		break;
	case 'u':
	case 'U':
	case ctrl('u'):
		start_line = MAXIMUM(start_line - ((LINES - 2) / 2), 0);
		break;
	case 'f':
	case ctrl('f'):
		start_line = MINIMUM(start_line + (LINES - 2), MAXLINE - 1);
		break;
	case 'b':
	case ctrl('b'):
		start_line = MAXIMUM(start_line - (LINES - 2), 0);
		break;
	case 'g':
		start_line = 0;
		break;

		/*
		 * horizontal motion
		 */
	case 'l':
		start_column = MINIMUM(start_column + 1, MAXCOLUMN - 1);
		break;
	case ctrl('l'):
		clear();
		break;
	case 'h':
		start_column = MAXIMUM(start_column - 1, 0);
		break;
	case 'L':
		start_column = MINIMUM(start_column + ((COLS - 2) / 2),
		    MAXCOLUMN - 1);
		break;
	case 'H':
		start_column = MAXIMUM(start_column - ((COLS - 2) / 2), 0);
		break;
	case ']':
	case '\t':
		start_column = MINIMUM(start_column + 8, MAXCOLUMN - 1);
		break;
	case '[':
	case '\b':
		start_column = MAXIMUM(start_column - 8, 0);
		break;
	case '>':
		start_column = MINIMUM(start_column + (COLS - 2), MAXCOLUMN - 1);
		break;
	case '<':
		start_column = MAXIMUM(start_column - (COLS - 2), 0);
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

	return (RSLT_REDRAW);
}

void
show_help(void)
{
	int ch;
	ssize_t len;

	clear();
	nl();

	printw("These commands are available:\n"
	    "\n"
	    "Movement:\n"
	    "1-char    half-win  full-win  8-char\n"
	    "UP       k, -      u, ^u     b, ^b\n"
	    "DOWN     j, RET    d, ^d     f, ^f\n"
	    "RIGHT    l         L         >         ]\n"
	    "LEFT     h         H         <         [\n"
	    "\n"
	    "g        go to top\n"
	    "\n"
	    "Others:\n"
	    "space    update buffer\n"
	    "ctrl-l   refresh screen\n"
	    "r        highlight changed characters\n"
	    "e        highlight changed lines\n"
	    "w        highlight changed words\n"
	    "t        toggle higlighting mode\n"
	    "n        change update interval\n"
	    "p        pause and restart\n"
	    "?        show this message\n"
	    "q        quit\n\n");

	standout();
	printw("Hit any key to continue.");
	standend();
	refresh();

	while (1) {
		len = read(STDIN_FILENO, &ch, 1);
		if (len == -1 && errno == EINTR)
			continue;
		if (len == 0)
			exit(1);
		break;
	}
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
			len = MINIMUM(maxcnt - (p + spaces - buf),
			    (int)wcslen(p + 1) + 1);
			if (len > 0)
				memmove(p + spaces, p + 1,
				    len * sizeof(wchar_t));
			len = MINIMUM(spaces, maxcnt - 1 - (p - buf));
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

	fprintf(stderr, "usage: %s [-erw] [-s seconds] command [arg ...]\n",
	    __progname);
}
