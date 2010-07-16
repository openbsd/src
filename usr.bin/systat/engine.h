/* $Id: engine.h,v 1.6 2010/07/16 05:22:48 lum Exp $	 */
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

#ifndef _ENGINE_H_
#define _ENGINE_H_

#include <curses.h>

#define DEFAULT_WIDTH  80
#define DEFAULT_HEIGHT 25

/* XXX do not hardcode! */
#define HEADER_LINES 4


#define CTRL_A  1
#define CTRL_B  2
#define CTRL_E  5
#define CTRL_F  6
#define CTRL_G  7
#define CTRL_H  8
#define CTRL_L  12
#define CTRL_N  14
#define CTRL_P  16
#define CTRL_V  22

#define META_V  246

#define MAX_LINE_BUF 1024


#define FLD_ALIGN_LEFT   0
#define FLD_ALIGN_RIGHT  1
#define FLD_ALIGN_CENTER 2
#define FLD_ALIGN_COLUMN 3
#define FLD_ALIGN_BAR    4

#define FLD_FLAG_HIDDEN 1


typedef struct {
	char *title;
	int norm_width;
	int max_width;
	int increment;
	int align;
	int start;
	int width;
	unsigned flags;
	int arg;
} field_def;

typedef struct {
	char *name;
	char *match;
	int hotkey;
	int (*func) (const void *, const void *);
} order_type;

struct view_manager {
	char *name;
	int  (*select_fn) (void);
	int  (*read_fn)   (void);
	void (*sort_fn)   (void);
	int  (*header_fn) (void);
	void (*print_fn)  (void);
	int  (*key_fn)    (int);
	order_type *order_list;
	order_type *order_curr;
};

typedef struct {
	field_def **view;
	char *name;
	int hotkey;
	struct view_manager *mgr;
} field_view;

struct command {
	char *prompt;
	void ( *exec)(const char *);
};


void tb_start(void);

void tb_end(void);

int tbprintf(char *format, ...) GCC_PRINTFLIKE(1,2);

void end_line(void);
void end_page(void);

void print_fld_str(field_def *, const char *);
void print_fld_age(field_def *, unsigned int);
void print_fld_sdiv(field_def *, u_int64_t, int);
void print_fld_size(field_def *, u_int64_t);
void print_fld_ssdiv(field_def *, int64_t, int);
void print_fld_ssize(field_def *, int64_t);
void print_fld_bw(field_def *, double);
void print_fld_rate(field_def *, double);
void print_fld_uint(field_def *, unsigned int);
void print_fld_float(field_def *, double, int);
void print_fld_bar(field_def *, int);
void print_fld_tb(field_def *);

void print_title(void);

void hide_field(field_def *fld);
void show_field(field_def *fld);
void field_setup(void);

void add_view(field_view *fv);
int set_view(const char *opt);
void next_view(void);
void prev_view(void);

void set_order(const char *opt);
void next_order(void);

void setup_term(int maxpr);
int check_termcap(void);

void engine_initialize(void);
void engine_loop(int countmax);

struct command *command_set(struct command *cmd, const char *init);
const char *message_set(const char *msg);

void foreach_view(void (*callback)(field_view *));

extern int sortdir;
extern useconds_t udelay;
extern int dispstart;
extern int interactive;
extern int maxprint;
extern int paused;
extern int rawmode;
extern int rawwidth;
extern int columns, lines;

extern int need_update;
extern int need_sort;

extern volatile sig_atomic_t gotsig_close;
extern volatile sig_atomic_t gotsig_resize;
extern volatile sig_atomic_t gotsig_alarm;

extern field_view *curr_view;
extern struct view_manager *curr_mgr;

extern char tmp_buf[MAX_LINE_BUF];

extern int curr_line; /* XXX temp */
extern u_int32_t num_disp;
#endif
