/*	$OpenBSD: watch.c,v 1.27 2025/05/30 15:14:56 job Exp $ */
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
#include <sys/ioctl.h>
#include <sys/wait.h>

#include <curses.h>
#include <err.h>
#include <errno.h>
#include <event.h>
#include <locale.h>
#include <paths.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <wchar.h>
#include <wctype.h>

#define DEFAULT_INTERVAL 1
#define MAXLINE 300
#define MAXCOLUMN 180

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
struct {
	struct timeval	tv;
	double		d;
} opt_interval = {
	.tv = { DEFAULT_INTERVAL, 0 },
	.d = DEFAULT_INTERVAL
};

highlight_mode_t highlight_mode = HIGHLIGHT_NONE;

int start_line = 0, start_column = 0;	/* display offset coordinates */

int pause_on_error = 0;
int paused = 0;
int last_exitcode = 0;
time_t lastupdate;
int xflag = 0;

#define	addwch(_x)	addnwstr(&(_x), 1);
#define	WCWIDTH(_x)	((wcwidth((_x)) > 0)? wcwidth((_x)) : 1)

static char	 *cmdstr;
static size_t	  cmdlen;
static char	**cmdv;

typedef wchar_t	  BUFFER[MAXLINE][MAXCOLUMN + 1];
BUFFER		  buf0, buf1;
BUFFER		 *cur_buf, *prev_buf;
struct event	  ev_timer;

#define MAXIMUM(a, b)	(((a) > (b)) ? (a) : (b))
#define MINIMUM(a, b)	(((a) < (b)) ? (a) : (b))

#define ctrl(c)		((c) & 037)
int display(BUFFER *, BUFFER *, highlight_mode_t);
void read_result(BUFFER *);
kbd_result_t kbd_command(int);
void show_help(void);
void untabify(wchar_t *, int);
void on_signal(int, short, void *);
void timer(int, short, void *);
void input(int, short, void *);
void quit(void);
void usage(void);
void swap_buffers(void);

int
set_interval(const char *str)
{
	double	 intvl;
	char	*endp;

	intvl = strtod(str, &endp);
	if (intvl < 0 || intvl > 1000000 || *endp != '\0')
		return -1;

	opt_interval.d = intvl;
	opt_interval.tv.tv_sec = (int)intvl;
	opt_interval.tv.tv_usec = (u_long)(intvl * 1000000UL) % 1000000UL;
	return 0;
}

int
main(int argc, char *argv[])
{
	struct event ev_sigint, ev_sighup, ev_sigterm, ev_sigwinch, ev_stdin;
	size_t len, rem;
	int i, ch;
	char *p;

	setlocale(LC_CTYPE, "");

	while ((ch = getopt(argc, argv, "cels:wx")) != -1) {
		switch (ch) {
		case 'c':
			highlight_mode = HIGHLIGHT_CHAR;
			break;
		case 'e':
			pause_on_error = 1;
			break;
		case 'l':
			highlight_mode = HIGHLIGHT_LINE;
			break;
		case 's':
			if (set_interval(optarg) == -1)
				errx(1, "bad interval: %s", optarg);
			break;
		case 'w':
			highlight_mode = HIGHLIGHT_WORD;
			break;
		case 'x':
			xflag = 1;
			break;
		default:
			usage();
			exit(1);
		}
	}

	argc -= optind;
	argv += optind;

	/*
	 * Build command string to give to popen
	 */
	if (argc <= 0) {
		usage();
		exit(1);
	}

	if ((cmdv = calloc(argc + 1, sizeof(char *))) == NULL)
		err(1, "calloc");

	cmdlen = 0;
	for (i = 0; i < argc; i++) {
		cmdv[i] = argv[i];
		cmdlen += strlen(argv[i]);
		if (i != 0)
			cmdlen++;
	}

	if ((cmdstr = malloc(cmdlen + 1)) == NULL)
		err(1, "malloc");

	p = cmdstr;
	rem = cmdlen + 1;

	if ((len = strlcpy(p, argv[0], rem)) >= rem)
		errx(1, "overflow");
	rem -= len;
	p += len;
	for (i = 1; i < argc; i++) {
		if ((len = strlcpy(p, " ", rem)) >= rem)
			errx(1, "overflow");
		rem -= len;
		p += len;
		if ((len = strlcpy(p, argv[i], rem)) >= rem)
			errx(1, "overflow");
		rem -= len;
		p += len;
	}
	cmdv[i++] = NULL;

	initscr();

	if (unveil(xflag ? cmdv[0] : _PATH_BSHELL, "x") == -1)
		err(1, "unveil");

	if (pledge("stdio rpath tty proc exec", NULL) == -1)
		err(1, "pledge");

	noecho();
	crmode();
	keypad(stdscr, TRUE);

	event_init();

	/*
	 * Initialize signal
	 */
	signal_set(&ev_sigint, SIGINT, on_signal, NULL);
	signal_set(&ev_sighup, SIGHUP, on_signal, NULL);
	signal_set(&ev_sigterm, SIGTERM, on_signal, NULL);
	signal_set(&ev_sigwinch, SIGWINCH, on_signal, NULL);
	signal_add(&ev_sigint, NULL);
	signal_add(&ev_sighup, NULL);
	signal_add(&ev_sigterm, NULL);
	signal_add(&ev_sigwinch, NULL);

	event_set(&ev_stdin, STDIN_FILENO, EV_READ | EV_PERSIST, input, NULL);
	event_add(&ev_stdin, NULL);
	evtimer_set(&ev_timer, timer, NULL);
	evtimer_add(&ev_timer, &opt_interval.tv);

	cur_buf = &buf0; prev_buf = &buf1;
	read_result(cur_buf);
	display(cur_buf, prev_buf, highlight_mode);

	event_dispatch();

	/* NOTREACHED */
	abort();
}

int
display(BUFFER * cur, BUFFER * prev, highlight_mode_t hm)
{
	int i, screen_x, screen_y, cw, line, rl;
	static char buf[30];

	erase();

	move(0, 0);

	if (pause_on_error && last_exitcode != 0)
		printw("--PAUSED (EXIT CODE %i)-- ", last_exitcode);
	else if (paused)
		printw("--PAUSED-- ");
	else
		printw("Every %.4gs: ", opt_interval.d);

	if (cmdlen > COLS - 47)
		printw("%-.*s...", COLS - 49, cmdstr);
	else
		printw("%s", cmdstr);

	if (pause_on_error)
		printw(" (?)");

	if (buf[0] == '\0')
		gethostname(buf, sizeof(buf));

	move(0, COLS - 8 - strlen(buf) - 1);
	printw("%s %-8.8s", buf, &(ctime(&lastupdate)[11]));

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
				 * If the word highlight option is specified and
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
		err(1, "pipe()");

	if ((pipe_pid = vfork()) == -1)
		err(1, "vfork()");
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
		_exit(1);

		/* NOTREACHED */
	}
	if ((fp = fdopen(fds[0], "r")) == NULL)
		err(1, "fdopen()");
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

	if (WIFEXITED(st))
		last_exitcode = WEXITSTATUS(st);
	if (pause_on_error && last_exitcode)
		paused = 1;
}

kbd_result_t
kbd_command(int ch)
{
	char buf[10];

	switch (ch) {

	case '?':
	case 'h':
		show_help();
		refresh();
		return (RSLT_REDRAW);

	case ' ': /* Execute the command again. */
		return (RSLT_UPDATE);

	/*
	 * XXX: redrawing with Control-l often is needed when the command
	 * emitted things to stderr. The program ought to interleave stdout
	 * and stderr.
	 */
	case ctrl('l'):
		clear();
		break;

	case KEY_DOWN:
	case 'j':
		start_line = MINIMUM(start_line + 1, MAXLINE - 1);
		break;

	case KEY_LEFT:
	case '[':
		start_column = MAXIMUM(start_column - 1, 0);
		break;

	case KEY_NPAGE:
		start_line = MINIMUM(start_line + (LINES - 2), MAXLINE - 1);
		break;

	case KEY_PPAGE:
		start_line = MAXIMUM(start_line - (LINES - 2), 0);
		break;

	case KEY_RIGHT:
	case ']':
		start_column = MINIMUM(start_column + 1, MAXCOLUMN - 1);
		break;

	case KEY_UP:
	case 'k':
		start_line = MAXIMUM(start_line - 1, 0);
		break;

	case 'G': /* jump to bottom not yet implemented */
		break;

	case 'H':
		start_column = MAXIMUM(start_column - ((COLS - 2) / 2), 0);
		break;

	case 'J':
		start_line = MINIMUM(start_line + ((LINES - 2) / 2),
		    MAXLINE - 1);
		break;

	case 'K':
		start_line = MAXIMUM(start_line - ((LINES - 2) / 2), 0);
		break;

	case 'L':
		start_column = MINIMUM(start_column + ((COLS - 2) / 2),
		    MAXCOLUMN - 1);
		break;

	case 'c':
		if (highlight_mode == HIGHLIGHT_CHAR)
			highlight_mode = HIGHLIGHT_NONE;
		else
			highlight_mode = HIGHLIGHT_CHAR;
		break;

	case 'e':
		if (pause_on_error == 1)
			pause_on_error = 0;
		else
			pause_on_error = 1;
		return (RSLT_REDRAW);

	case 'g':
		start_line = 0;
		break;

	case 'l':
		if (highlight_mode == HIGHLIGHT_LINE)
			highlight_mode = HIGHLIGHT_NONE;
		else
			highlight_mode = HIGHLIGHT_LINE;
		break;

	case 'p':
		if (paused == 1) {
			paused = 0;
			evtimer_del(&ev_timer);
			return (RSLT_REDRAW);
		} else {
			paused = 1;
			evtimer_add(&ev_timer, &opt_interval.tv);
			return (RSLT_UPDATE);
		}

	case 's':
		move(1, 0);

		standout();
		printw("New interval: ");
		standend();

		echo();
		getnstr(buf, sizeof(buf));
		noecho();

		if (set_interval(buf) == -1) {
			move(1, 0);
			standout();
			printw("Bad interval: %s", buf);
			standend();
			refresh();
			return (RSLT_ERROR);
		}
		evtimer_add(&ev_timer, &opt_interval.tv);

		return (RSLT_REDRAW);

	case 'w':
		if (highlight_mode == HIGHLIGHT_WORD)
			highlight_mode = HIGHLIGHT_NONE;
		else
			highlight_mode = HIGHLIGHT_WORD;
		break;

	case 'q':
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
	    "j | k          - scroll down/up one line\n"
	    "[ | ]          - scroll left/right one column\n"
	    "(arrow keys)   - scroll left/down/up/right one line or column\n"
	    "H | J | K | L  - scroll left/down/up/right half a screen\n"
	    "(Page Down)    - scroll down a screenful\n"
	    "(Page Up)      - scroll up a screenful\n"
	    "g              - go to top\n\n"
	    "Other:\n"
	    "(Space)        - run command again\n"
	    "e              - pause on non-zero exit code\n"
	    "c              - highlight changed characters\n"
	    "l              - highlight changed lines\n"
	    "w              - highlight changed words\n"
	    "p              - toggle pause / resume\n"
	    "s              - change the update interval\n"
	    "h | ?          - show this message\n"
	    "q              - quit\n\n");

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
swap_buffers(void)
{
	BUFFER *t;

	t = prev_buf;
	prev_buf = cur_buf;
	cur_buf = t;
}

void
on_signal(int sig, short event, void *arg)
{
	struct winsize ws;

	switch(sig) {
	case SIGWINCH:
		if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) != -1)
			resizeterm(ws.ws_row, ws.ws_col);
		doupdate();
		display(cur_buf, prev_buf, highlight_mode);
		break;
	default:
		quit();
	}
}

void
timer(int sig, short event, void *arg)
{
	swap_buffers();
	read_result(cur_buf);
	display(cur_buf, prev_buf, highlight_mode);
	if (!paused)
		evtimer_add(&ev_timer, &opt_interval.tv);
}

void
input(int sig, short event, void *arg)
{
	int ch;

	ch = getch();

	kbd_result_t result = kbd_command(ch);
	switch (result) {
	case RSLT_UPDATE:	/* update buffer */
		swap_buffers();
		read_result(cur_buf);
		display(cur_buf, prev_buf, highlight_mode);
		if (!paused)
			evtimer_add(&ev_timer, &opt_interval.tv);
		break;
	case RSLT_REDRAW:	/* scroll with current buffer */
		display(cur_buf, prev_buf, highlight_mode);
		break;
	case RSLT_NOTOUCH:	/* silently loop again */
		break;
	case RSLT_ERROR:	/* error */
		fprintf(stderr, "\007");
		break;
	}
}

void
quit(void)
{
	erase();
	refresh();
	endwin();
	free(cmdv);
	exit(0);
}

void
usage(void)
{
	extern char *__progname;

	fprintf(stderr, "usage: %s [-celwx] [-s seconds] command [arg ...]\n",
	    __progname);
}
