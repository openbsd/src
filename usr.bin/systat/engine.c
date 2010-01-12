/* $Id: engine.c,v 1.11 2010/01/12 23:22:14 nicm Exp $	 */
/*
 * Copyright (c) 2001, 2007 Can Erkin Acar <canacar@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */


#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/queue.h>

#include <ctype.h>
#include <curses.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <term.h>
#include <unistd.h>

/* XXX These are defined in term.h and conflict with our variable names */
#ifdef columns
#undef columns
#endif

#ifdef lines
#undef lines
#endif

#include "engine.h"

#ifndef MIN
#define MIN(a,b) (((a)<(b))?(a):(b))
#endif

/* circular linked list of views */
CIRCLEQ_HEAD(view_list, view_ent) view_head =
				  CIRCLEQ_HEAD_INITIALIZER(view_head);
struct view_ent {
	field_view *view;
	CIRCLEQ_ENTRY(view_ent) entries;
};

useconds_t udelay = 5000000;
int dispstart = 0;
int interactive = 1;
int maxprint = 0;
int paused = 0;
int rawmode = 0;
int rawwidth = DEFAULT_WIDTH;
int sortdir = 1;
int columns, lines;
u_int32_t num_disp = 0;
int max_disp = -1;

volatile sig_atomic_t gotsig_close = 0;
volatile sig_atomic_t gotsig_resize = 0;
volatile sig_atomic_t gotsig_alarm = 0;
int need_update = 0;
int need_sort = 0;

SCREEN *screen;

field_view *curr_view = NULL;
struct view_ent *curr_view_ent = NULL;
struct view_manager *curr_mgr = NULL;

int curr_line = 0;
int home_line = 0;

/* line buffer for raw mode */
char linebuf[MAX_LINE_BUF];
int linepos = 0;

/* temp storage for state printing */
char tmp_buf[MAX_LINE_BUF];

char cmdbuf[MAX_LINE_BUF];
int cmd_len = -1;
struct command *curr_cmd = NULL;
char *curr_message = NULL;

void print_cmdline(void);


/* screen output functions */

char * tb_ptr = NULL;
int tb_len = 0;

void
tb_start(void)
{
	tb_ptr = tmp_buf;
	tb_len = sizeof(tmp_buf);
	tb_ptr[0] = '\0';
}

void
tb_end(void)
{
	tb_ptr = NULL;
	tb_len = 0;
}

int
tbprintf(char *format, ...)
	GCC_PRINTFLIKE(1,2)       /* defined in curses.h */
{
	int len;
	va_list arg;

	if (tb_ptr == NULL || tb_len <= 0)
		return 0;

	va_start(arg, format);
	len=vsnprintf(tb_ptr, tb_len, format, arg);
	va_end(arg);
	
	if (len > tb_len)
		tb_end();
	else if (len > 0) {
		tb_ptr += len;
		tb_len -= len;
	}
	
	return len;
}

void
move_horiz(int offset)
{
	if (rawmode) {
		if (offset <= 0)
			linepos = 0;
		else if (offset >= MAX_LINE_BUF)
			linepos = MAX_LINE_BUF - 1;
		else
			linepos = offset;
	} else {
		move(curr_line, offset);
	}
}

void
print_str(int len, const char *str)
{
	if (len <= 0)
		return;

	if (rawmode) {
		int length = MIN(len, MAX_LINE_BUF - linepos);
		if (length <= 0)
			return;
		bcopy(str, &linebuf[linepos], length);
		linepos += length;
	} else
		addnstr(str, len);
}

void
clear_linebuf(void)
{
	memset(linebuf, ' ', MAX_LINE_BUF);
}

void
end_line(void)
{
	if (rawmode) {
		linebuf[rawwidth] = '\0';
		printf("%s\n", linebuf);
		clear_linebuf();
	}
	curr_line++;
}

void
end_page(void)
{
	if (rawmode) {
		linepos = 0;
		clear_linebuf();
	} else {
		move(home_line, 0);
		print_cmdline();
		refresh();
	}
	curr_line = 0;
}

void
rawaddstr(char *s)
{
	if (rawmode)
		printf("%s", s);
	else
		addstr(s);
}

/* field output functions */

void
print_fld_str(field_def *fld, const char *str)
{
	int len, offset;
	char *cpos;

	if (str == NULL || fld == NULL)
		return;

	if (fld->start < 0)
		return;

	len = strlen(str);

	if (len >= fld->width) {
		move_horiz(fld->start);
		print_str(fld->width, str);
	} else {
		switch (fld->align) {
		case FLD_ALIGN_RIGHT:
			move_horiz(fld->start + (fld->width - len));
			break;
		case FLD_ALIGN_CENTER:
			move_horiz(fld->start + (fld->width - len) / 2);
			break;
		case FLD_ALIGN_COLUMN:
			if ((cpos = strchr(str, ':')) == NULL) {
				offset = (fld->width - len) / 2;
			} else {
				offset = (fld->width / 2) - (cpos - str);
				if (offset < 0)
					offset = 0;
				else if (offset > (fld->width - len))
					offset = fld->width - len;
			}
			move_horiz(fld->start + offset);
			break;
		default:
			move_horiz(fld->start);
			break;
		}
		print_str(len, str);
	}
}

void
print_bar_title(field_def *fld)
{
	char buf[16];
	int len, i, d, tr, tw, val, pos, cur;

	int divs[] = {20, 10, 5, 4, 3, 2, 1, 0};

	if (fld->width < 1)
		return;

	len = snprintf(buf, sizeof(buf), " %d\\", fld->arg);
	if (len >= sizeof(buf))
		return;

	for (i = 0; divs[i]; i++)
		if (divs[i] * len <= fld->width)
			break;

	if (divs[i] == 0) {
		print_fld_str(fld, "*****");
		return;
	}

	d = divs[i];

	val = 0;
	pos = 0;
	tr = fld->arg % d;
	tw = fld->width % d;

	tb_start();
	cur = 0;
	for(i = 0; i < d; i++) {
		tw += fld->width;
		tr += fld->arg;

		while (tr >= d) {
			val++;
			tr -= d;
		}
		while (tw >= d) {
			pos++;
			tw -= d;
		}

		len = snprintf(buf, sizeof(buf), "%d\\", val);
		while (cur < pos - len) {
			tbprintf(" ");
			cur++;
		}
		tbprintf("%s", buf);
		cur += len;
	}

	print_fld_tb(fld);
}

void
print_fld_bar(field_def *fld, int value)
{
	int i, tw, val, cur;

	if (fld->width < 1)
		return;

	val = 0;
	tw = fld->arg / 2;

	tb_start();
	cur = 0;
	for(i = 0; i < fld->width; i++) {
		tw += fld->arg;

		while (tw >= fld->width) {
			val++;
			tw -= fld->width;
		}
		if (val > value)
			break;
		tbprintf("#");
	}

	print_fld_tb(fld);
}

void
print_fld_tb(field_def *fld)
{
	print_fld_str(fld, tmp_buf);
	tb_end();
}

void
print_title(void)
{
	field_def **fp;

	if (curr_view != NULL && curr_view->view != NULL) {
		for (fp = curr_view->view; *fp != NULL; fp++) {
			switch((*fp)->align) {
			case FLD_ALIGN_LEFT:
			case FLD_ALIGN_RIGHT:
			case FLD_ALIGN_CENTER:
			case FLD_ALIGN_COLUMN:
				print_fld_str(*fp, (*fp)->title);
				break;
			case FLD_ALIGN_BAR:
				print_bar_title(*fp);
				break;
			}
		}
	}
	end_line();
}

/* view related functions */
void
hide_field(field_def *fld)
{
	if (fld == NULL)
		return;

	fld->flags |= FLD_FLAG_HIDDEN;
}

void
show_field(field_def *fld)
{
	if (fld == NULL)
		return;

	fld->flags &= ~((unsigned int) FLD_FLAG_HIDDEN);
}

void
reset_fields(void)
{
	field_def **fp;
	field_def *fld;

	if (curr_view == NULL)
		return;

	if (curr_view->view == NULL)
		return;

	for (fp = curr_view->view; *fp != NULL; fp++) {
		fld = *fp;
		fld->start = -1;
		fld->width = fld->norm_width;
	}
}

void
field_setup(void)
{
	field_def **fp;
	field_def *fld;
	int st, fwid, change;
	int width = columns;

	reset_fields();

	dispstart = 0;
	st = 0;

	for (fp = curr_view->view; *fp != NULL; fp++) {
		fld = *fp;
		if (fld->flags & FLD_FLAG_HIDDEN)
			continue;

		if (width <= 1)
			break;

		if (st != 1)
			width--;

		fld->start = 1;
		fwid = fld->width;
		st++;
		if (fwid >= width) {
			fld->width = width;
			width = 0;
		} else
			width -= fwid;
	}

	change = 0;
	while (width > 0) {
		change = 0;
		for (fp = curr_view->view; *fp != NULL; fp++) {
			fld = *fp;
			if (fld->flags & FLD_FLAG_HIDDEN)
				continue;
			if ((fld->width < fld->max_width) &&
			    (fld->increment <= width)) {
				int w = fld->width + fld->increment;
				if (w > fld->max_width)
					w = fld->max_width;
				width += fld->width - w;
				fld->width = w;
				change = 1;
			}
			if (width <= 0) break;
		}
		if (change == 0) break;
	}

	st = 0;
	for (fp = curr_view->view; *fp != NULL; fp++) {
		fld = *fp;
		if (fld->flags & FLD_FLAG_HIDDEN)
			continue;
		if (fld->start < 0) break;
		fld->start = st;
		st += fld->width + 1;
	}
}

void
set_curr_view(struct view_ent *ve)
{
	field_view *v;

	reset_fields();

	if (ve == NULL) {
		curr_view_ent = NULL;
		curr_view = NULL;
		curr_mgr = NULL;
		return;
	}

	v = ve->view;
	
	if ((curr_view != NULL) && (curr_mgr != v->mgr)) {
		gotsig_alarm = 1;
		if (v->mgr != NULL && v->mgr->select_fn != NULL)
			v->mgr->select_fn();
	}

	curr_view_ent = ve;
	curr_view = v;
	curr_mgr = v->mgr;
	field_setup();
	need_update = 1;
}

void
add_view(field_view *fv)
{
	struct view_ent *ent;

	if (fv == NULL)
		return;

	if (fv->view == NULL || fv->name == NULL || fv->mgr == NULL)
		return;

	ent = malloc(sizeof(struct view_ent));
	if (ent == NULL)
		return;

	ent->view = fv;
	CIRCLEQ_INSERT_TAIL(&view_head, ent, entries);

	if (curr_view == NULL)
		set_curr_view(ent);
}

int
set_view(const char *opt)
{
	struct view_ent *ve, *vm = NULL;
	field_view *v;
	int len;

	if (opt == NULL || (len = strlen(opt)) == 0)
		return 1;

	CIRCLEQ_FOREACH(ve, &view_head, entries) {
		v = ve->view;
		if (strncasecmp(opt, v->name, len) == 0) {
			if (vm)
				return 1;
			vm = ve;
		}
	}

	if (vm) {
		set_curr_view(vm);
		return 0;
	}

	return 1;
}

void
foreach_view(void (*callback)(field_view *))
{
	struct view_ent *ve;

	CIRCLEQ_FOREACH(ve, &view_head, entries) {
		callback(ve->view);
	}
}

int
set_view_hotkey(int ch)
{
	struct view_ent *ve;
	field_view *v;
	int key = tolower(ch);

	CIRCLEQ_FOREACH(ve, &view_head, entries) {
		v = ve->view;
		if (key == v->hotkey) {
			set_curr_view(ve);
			return 1;
		}
	}

	return 0;
}

void
next_view(void)
{
	struct view_ent *ve;

	if (CIRCLEQ_EMPTY(&view_head) || curr_view_ent == NULL)
		return;

	ve = CIRCLEQ_NEXT(curr_view_ent, entries);
	if (ve == CIRCLEQ_END(&view_head))
		ve = CIRCLEQ_FIRST(&view_head);

	set_curr_view(ve);
}

void
prev_view(void)
{
	struct view_ent *ve;

	if (CIRCLEQ_EMPTY(&view_head) || curr_view_ent == NULL)
		return;

	ve = CIRCLEQ_PREV(curr_view_ent, entries);
	if (ve == CIRCLEQ_END(&view_head))
		ve = CIRCLEQ_LAST(&view_head);

	set_curr_view(ve);
}

/* generic field printing */

void
print_fld_age(field_def *fld, unsigned int age)
{
	int len;
	unsigned int h, m, s;

	if (fld == NULL)
		return;
	len = fld->width;

	if (len < 1)
		return;

	s = age % 60;
	m = age / 60;
	h = m / 60;
	m %= 60;

	tb_start();
	if (tbprintf("%02u:%02u:%02u", h, m, s) <= len)
		goto ok;
	
	tb_start();
	if (tbprintf("%u", age) <= len)
		goto ok;

	tb_start();
	age /= 60;
	if (tbprintf("%um", age) <= len)
		goto ok;
	if (age == 0)
		goto err;
	
	tb_start();
	age /= 60;
	if (tbprintf("%uh", age) <= len)
		goto ok;
	if (age == 0)
		goto err;
	
	tb_start();
	age /= 24;
	if (tbprintf("%ud", age) <= len)
		goto ok;
	
err:
	print_fld_str(fld, "*");
	tb_end();
	return;
	
ok:
	print_fld_tb(fld);
}

void
print_fld_sdiv(field_def *fld, u_int64_t size, int d)
{
	int len;

	if (fld == NULL)
		return;

	len = fld->width;
	if (len < 1)
		return;

	tb_start();
	if (tbprintf("%llu", size) <= len)
		goto ok;

	tb_start();
	size /= d;
	if (tbprintf("%lluK", size) <= len)
		goto ok;
	if (size == 0)
		goto err;

	tb_start();
	size /= d;
	if (tbprintf("%lluM", size) <= len)
		goto ok;
	if (size == 0)
		goto err;

	tb_start();
	size /= d;
	if (tbprintf("%lluG", size) <= len)
		goto ok;
	if (size == 0)
		goto err;

	tb_start();
	size /= d;
	if (tbprintf("%lluT", size) <= len)
		goto ok;
	
err:
	print_fld_str(fld, "*");
	tb_end();
	return;

ok:
	print_fld_tb(fld);
}

void
print_fld_size(field_def *fld, u_int64_t size)
{
	print_fld_sdiv(fld, size, 1024);
}

void
print_fld_ssdiv(field_def *fld, int64_t size, int d)
{
	int len;

	if (fld == NULL)
		return;

	len = fld->width;
	if (len < 1)
		return;

	tb_start();
	if (tbprintf("%lld", size) <= len)
		goto ok;

	tb_start();
	size /= d;
	if (tbprintf("%lldK", size) <= len)
		goto ok;
	if (size == 0)
		goto err;

	tb_start();
	size /= d;
	if (tbprintf("%lldM", size) <= len)
		goto ok;
	if (size == 0)
		goto err;

	tb_start();
	size /= d;
	if (tbprintf("%lldG", size) <= len)
		goto ok;
	if (size == 0)
		goto err;

	tb_start();
	size /= d;
	if (tbprintf("%lldT", size) <= len)
		goto ok;

err:
	print_fld_str(fld, "*");
	tb_end();
	return;

ok:
	print_fld_tb(fld);
}

void
print_fld_ssize(field_def *fld, int64_t size)
{
	print_fld_ssdiv(fld, size, 1024);
}

void
print_fld_rate(field_def *fld, double rate)
{
	if (rate < 0) {
		print_fld_str(fld, "*");
	} else {
		print_fld_size(fld, rate);
	}
}

void
print_fld_bw(field_def *fld, double bw)
{
	if (bw < 0) {
		print_fld_str(fld, "*");
	} else {
		print_fld_sdiv(fld, bw, 1000);
	}
}

void
print_fld_uint(field_def *fld, unsigned int size)
{
	int len;

	if (fld == NULL)
		return;

	len = fld->width;
	if (len < 1)
		return;

	tb_start();
	if (tbprintf("%u", size) > len)
		print_fld_str(fld, "*");
	else
		print_fld_tb(fld);
	tb_end();
}

void
print_fld_float(field_def *fld, double f, int prec)
{
	int len;

	if (fld == NULL)
		return;

	len = fld->width;
	if (len < 1)
		return;

	tb_start();
	if (tbprintf("%*.*f", len, prec, f) > len)
		print_fld_str(fld, "*");
	else
		print_fld_tb(fld);
	tb_end();
}


/* ordering */

void
set_order(const char *opt)
{
	order_type *o;

	if (curr_view == NULL || curr_view->mgr == NULL)
		return;

	curr_view->mgr->order_curr = curr_view->mgr->order_list;

	if (opt == NULL)
		return;

	o = curr_view->mgr->order_list;

	if (o == NULL)
		return;

	for (;o->name != NULL; o++) {
		if (strcasecmp(opt, o->match) == 0) {
			curr_view->mgr->order_curr = o;
			return;
		}
	}
}

int
set_order_hotkey(int ch)
{
	order_type *o;
	int key = ch;

	if (curr_view == NULL || curr_view->mgr == NULL)
		return 0;

	o = curr_view->mgr->order_list;

	if (o == NULL)
		return 0;

	for (;o->name != NULL; o++) {
		if (key == o->hotkey) {
			if (curr_view->mgr->order_curr == o) {
				sortdir *= -1;
			} else {
				curr_view->mgr->order_curr = o;
			}
			return 1;
		}
	}

	return 0;
}

void
next_order(void)
{
	order_type *o, *oc;

	if (curr_view->mgr->order_list == NULL)
		return;

	oc = curr_view->mgr->order_curr;

	for (o = curr_view->mgr->order_list; o->name != NULL; o++) {
		if (oc == o) {
			o++;
			if (o->name == NULL)
				break;
			curr_view->mgr->order_curr = o;
			return;
		}
	}

	curr_view->mgr->order_curr = curr_view->mgr->order_list;
}


/* main program functions */

int
read_view(void)
{
	if (curr_mgr == NULL)
		return (0);

	if (paused)
		return (0);

	if (curr_mgr->read_fn != NULL)
		return (curr_mgr->read_fn());

	return (0);
}


int
disp_update(void)
{
	int li;

	if (maxprint < 0)
		dispstart = 0;
	else if (dispstart + maxprint > num_disp)
		dispstart = num_disp - maxprint;
	
	if (dispstart < 0)
		dispstart = 0;

	if (curr_view == NULL)
		return 0;

	if (curr_mgr != NULL) {
		curr_line = 0;

		if (curr_mgr->header_fn != NULL) {
			li = curr_mgr->header_fn();
			if (li < 0)
				return (1);
			curr_line = ++li;
			home_line = li + maxprint + 1;
		}

		print_title();

		if (curr_mgr->print_fn != NULL)
			curr_mgr->print_fn();
	}

	return (0);
}

void
sort_view(void)
{
	if (curr_mgr != NULL)
		if (curr_mgr->sort_fn != NULL)
			curr_mgr->sort_fn();
}

void
sig_close(int sig)
{
	gotsig_close = 1;
}

void
sig_resize(int sig)
{
	gotsig_resize = 1;
}

void
sig_alarm(int sig)
{
	gotsig_alarm = 1;
}

void
setup_term(int dmax)
{
	max_disp = dmax;
	maxprint = dmax;

	if (rawmode) {
		columns = rawwidth;
		lines = DEFAULT_HEIGHT;
		clear_linebuf();
	} else {
		if (dmax < 0)
			dmax = 0;

		screen = newterm(NULL, stdout, stdin);
		if (screen == NULL) {
			rawmode = 1;
			interactive = 0;
			setup_term(dmax);
			return;
		}
		columns = COLS;
		lines = LINES;

		if (maxprint > lines - HEADER_LINES)
			maxprint = lines - HEADER_LINES;

		nonl();
		keypad(stdscr, TRUE);
		intrflush(stdscr, FALSE);

		halfdelay(10);
		noecho();
	}

	if (dmax == 0)
		maxprint = lines - HEADER_LINES;

	field_setup();
}

void
do_resize_term(void)
{
	struct winsize ws;

	if (rawmode)
		return;

	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1)
		return;

	resizeterm(ws.ws_row, ws.ws_col);

	columns = COLS;
	lines = LINES;

	maxprint = max_disp;

	if (maxprint == 0 || maxprint > lines - HEADER_LINES)
		maxprint = lines - HEADER_LINES;

	clear();

	field_setup();
}

struct command *
command_set(struct command *cmd, const char *init)
{
	struct command *prev = curr_cmd;

	if (cmd) {
		if (init) {
			cmd_len = strlcpy(cmdbuf, init, sizeof(cmdbuf));
			if (cmd_len >= sizeof(cmdbuf)) {
				cmdbuf[0] = '\0';
				cmd_len = 0;
			}
		} else {
			cmd_len = 0;
			cmdbuf[0] = 0;
		}
	}
	curr_message = NULL;
	curr_cmd = cmd;
	need_update = 1;
	return prev;
}

const char *
message_set(const char *msg) {
	char *prev = curr_message;
	if (msg)
		curr_message = strdup(msg);
	else
		curr_message = NULL;
	free(prev);
	return NULL;
}

void
print_cmdline(void)
{
	if (curr_cmd) {
		attron(A_STANDOUT);
		mvprintw(home_line, 0, "%s: ", curr_cmd->prompt);
		attroff(A_STANDOUT);
		printw("%s", cmdbuf);
	} else if (curr_message) {
		mvprintw(home_line, 0, "> %s", curr_message);
	}
	clrtoeol();
}


void
cmd_keyboard(int ch)
{
	if (curr_cmd == NULL)
		return;

	if (ch > 0 && isprint(ch)) {
		if (cmd_len < sizeof(cmdbuf) - 1) {
			cmdbuf[cmd_len++] = ch;
			cmdbuf[cmd_len] = 0;
		} else
			beep();
	}
	
	switch (ch) {
	case KEY_ENTER:
	case 0x0a:
	case 0x0d:
	{
		struct command * c = command_set(NULL, NULL);
		c->exec(cmdbuf);
		break;
	}
	case KEY_BACKSPACE:
	case KEY_DC:
	case CTRL_H:
		if (cmd_len > 0) {
			cmdbuf[--cmd_len] = 0;
		} else
			beep();
		break;
	case 0x1b:
	case CTRL_G:
		if (cmd_len > 0) {
			cmdbuf[0] = '\0';
			cmd_len = 0;
		} else
			command_set(NULL, NULL);
		break;
	default:
		break;
	}
}

void
keyboard(void)
{
	int ch;

	ch = getch();

	if (curr_cmd) {
		cmd_keyboard(ch);
		print_cmdline();
		return;
	}

	if (curr_mgr != NULL)
		if (curr_mgr->key_fn != NULL)
			if (curr_mgr->key_fn(ch))
				return;

	if (curr_message != NULL) {
		if (ch > 0) {
			curr_message = NULL;
			need_update = 1;
		}
	}

	switch (ch) {
	case ' ':
		gotsig_alarm = 1;
		break;
	case 'o':
		next_order();
		need_sort = 1;
		break;
	case 'p':
		paused = !paused;
		gotsig_alarm = 1;
		break;
	case 'q':
		gotsig_close = 1;
		break;
	case 'r':
		sortdir *= -1;
		need_sort = 1;
		break;
	case 'v':
		/* FALLTHROUGH */
	case KEY_RIGHT:
		/* FALLTHROUGH */
	case CTRL_F:
		next_view();
		break;
	case KEY_LEFT:
		/* FALLTHROUGH */
	case CTRL_B:
		prev_view();
		break;
	case KEY_DOWN:
		/* FALLTHROUGH */
	case CTRL_N:
		dispstart++;
		need_update = 1;
		break;
	case KEY_UP:
		/* FALLTHROUGH */
	case CTRL_P:
		dispstart--;
		need_update = 1;
		break;
	case KEY_NPAGE:
		/* FALLTHROUGH */
	case CTRL_V:
		dispstart += maxprint;
		need_update = 1;
		break;
	case KEY_PPAGE:
		/* FALLTHROUGH */
	case META_V:
		dispstart -= maxprint;
		need_update = 1;
		break;
	case KEY_HOME:
		/* FALLTHROUGH */
	case CTRL_A:
		dispstart = 0;
		need_update = 1;
		break;
	case KEY_END:
		/* FALLTHROUGH */
	case CTRL_E:
		dispstart = num_disp;
		need_update = 1;
		break;
	case CTRL_L:
		clear();
		need_update = 1;
		break;
	default:
		break;
	}

	if (set_order_hotkey(ch))
		need_sort = 1;
	else
		set_view_hotkey(ch);
}

void
engine_initialize(void)
{
	signal(SIGTERM, sig_close);
	signal(SIGINT, sig_close);
	signal(SIGQUIT, sig_close);
	signal(SIGWINCH, sig_resize);
	signal(SIGALRM, sig_alarm);
}

void
engine_loop(int countmax)
{
	int count = 0;

	for (;;) {
		if (gotsig_alarm) {
			read_view();
			need_sort = 1;
			gotsig_alarm = 0;
			ualarm(udelay, 0);
		}

		if (need_sort) {
			sort_view();
			need_sort = 0;
			need_update = 1;
			
			/* XXX if sort took too long */
			if (gotsig_alarm) {
				gotsig_alarm = 0;
				ualarm(udelay, 0);
			}
		}

		if (need_update) {
			erase();
			disp_update();
			end_page();
			need_update = 0;
			if (countmax && ++count >= countmax)
				break;
		}

		if (gotsig_close)
			break;
		if (gotsig_resize) {
			do_resize_term();
			gotsig_resize = 0;
			need_update = 1;
		}

		if (interactive && need_update == 0)
			keyboard();
		else if (interactive == 0)
			usleep(udelay);
	}

	if (rawmode == 0)
		endwin();
}
