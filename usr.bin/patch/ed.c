/*	$OpenBSD: ed.c,v 1.1 2015/10/16 07:33:47 tobias Exp $ */

/*
 * Copyright (c) 2015 Tobias Stoeckmann <tobias@openbsd.org>
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

#include <sys/queue.h>
#include <sys/stat.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "util.h"
#include "pch.h"
#include "inp.h"

/* states of finite state machine */
#define FSM_CMD		1
#define FSM_A		2
#define FSM_C		3
#define FSM_D		4
#define FSM_I		5
#define FSM_S		6

#define SRC_INP		1	/* line's origin is input file */
#define SRC_PCH		2	/* line's origin is patch file */

#define S_PATTERN	"/.//"

static void		init_lines(void);
static void		free_lines(void);
static struct ed_line	*get_line(LINENUM);
static struct ed_line	*create_line(off_t);
static int		valid_addr(LINENUM, LINENUM);
static int		get_command(void);
static void		write_lines(char *);

LIST_HEAD(ed_head, ed_line) head;
struct ed_line {
	LIST_ENTRY(ed_line)	entries;
	int			src;
	unsigned long		subst;
	union {
		LINENUM		lineno;
		off_t		seek;
	} pos;
};

static LINENUM		first_addr;
static LINENUM		second_addr;
static LINENUM		line_count;
static struct ed_line	*cline;		/* current line */

void
do_ed_script(void)
{
	off_t linepos;
	struct ed_line *nline;
	LINENUM i, range;
	int fsm;

	init_lines();
	cline = NULL;
	fsm = FSM_CMD;

	for (;;) {
		linepos = ftello(pfp);
		if (pgets(buf, sizeof buf, pfp) == NULL)
			break;
		p_input_line++;

		if (fsm == FSM_CMD) {
			if ((fsm = get_command()) == -1)
				break;

			switch (fsm) {
			case FSM_C:
			case FSM_D:
				/* delete lines in specified range */
				if (second_addr == -1)
					range = 1;
				else
					range = second_addr - first_addr + 1;
				for (i = 0; i < range; i++) {
					nline = LIST_NEXT(cline, entries);
					LIST_REMOVE(cline, entries);
					free(cline);
					cline = nline;
					line_count--;
				}
				fsm = (fsm == FSM_C) ? FSM_I : FSM_CMD;
				break;
			case FSM_S:
				cline->subst++;
				fsm = FSM_CMD;
				break;
			default:
				break;
			}

			continue;
		}

		if (strcmp(buf, ".\n") == 0) {
			fsm = FSM_CMD;
			continue;
		}

		if (fsm == FSM_A) {
			nline = create_line(linepos);
			if (cline == NULL)
				LIST_INSERT_HEAD(&head, nline, entries);
			else
				LIST_INSERT_AFTER(cline, nline, entries);
			cline = nline;
			line_count++;
		} else if (fsm == FSM_I) {
			nline = create_line(linepos);
			if (cline == NULL) {
				LIST_INSERT_HEAD(&head, nline, entries);
				cline = nline;
			} else
				LIST_INSERT_BEFORE(cline, nline, entries);
			line_count++;
		}
	}

	next_intuit_at(linepos, p_input_line);

	if (skip_rest_of_patch) {
		free_lines();
		return;
	}

	write_lines(TMPOUTNAME);
	free_lines();

	ignore_signals();
	if (!check_only) {
		if (move_file(TMPOUTNAME, outname) < 0) {
			toutkeep = true;
			chmod(TMPOUTNAME, filemode);
		} else
			chmod(outname, filemode);
	}
	set_signals(1);
}

static int
get_command(void)
{
	char *p;
	LINENUM min_addr;
	int fsm;

	min_addr = 0;
	fsm = -1;
	p = buf;

	/* maybe garbage encountered at end of patch */
	if (!isdigit((unsigned char)*p))
		return -1;

	first_addr = strtolinenum(buf, &p);
	second_addr = (*p == ',') ? strtolinenum(p + 1, &p) : -1;

	switch (*p++) {
	case 'a':
		if (second_addr != -1)
			fatal("invalid address at line %ld: %s",
			    p_input_line, buf);
		fsm = FSM_A;
		break;
	case 'c':
		fsm = FSM_C;
		min_addr = 1;
		break;
	case 'd':
		fsm = FSM_D;
		min_addr = 1;
		break;
	case 'i':
		if (second_addr != -1)
			fatal("invalid address at line %ld: %s",
			    p_input_line, buf);
		fsm = FSM_I;
		break;
	case 's':
		if (second_addr != -1)
			fatal("unsupported address range at line %ld: %s",
			    p_input_line, buf);
		if (strncmp(p, S_PATTERN, sizeof(S_PATTERN) - 1) != 0)
			fatal("unsupported substitution at "
			    "line %ld: %s", p_input_line, buf);
		p += sizeof(S_PATTERN) - 1;
		fsm = FSM_S;
		min_addr = 1;
		break;
	default:
		return -1;
		/* NOTREACHED */
	}

	if (*p != '\n')
		return -1;

	if (!valid_addr(first_addr, min_addr) ||
	    (second_addr != -1 && !valid_addr(second_addr, first_addr)))
		fatal("invalid address at line %ld: %s", p_input_line, buf);

	cline = get_line(first_addr);

	return fsm;
}

static void
write_lines(char *filename)
{
	FILE *ofp;
	char *p;
	struct ed_line *line;
	off_t linepos;

	linepos = ftello(pfp);
	ofp = fopen(filename, "w");
	if (ofp == NULL)
		pfatal("can't create %s", filename);

	LIST_FOREACH(line, &head, entries) {
		if (line->src == SRC_INP) {
			p = ifetch(line->pos.lineno, 0);
			/* Note: string is not NUL terminated. */
			for (; *p != '\n'; p++)
				if (line->subst != 0)
					line->subst--;
				else
					putc(*p, ofp);
			putc('\n', ofp);
		} else if (line->src == SRC_PCH) {
			fseeko(pfp, line->pos.seek, SEEK_SET);
			if (pgets(buf, sizeof buf, pfp) == NULL)
				fatal("unexpected end of file");
			p = buf;
			if (line->subst != 0)
				for (; *p != '\0' && *p != '\n'; p++)
					if (line->subst-- == 0)
						break;
			fputs(p, ofp);
			if (strchr(p, '\n') == NULL)
				putc('\n', ofp);
		}
	}
	fclose(ofp);

	/* restore patch file position to match p_input_line */
	fseeko(pfp, linepos, SEEK_SET);
}

/* initialize list with input file */
static void
init_lines(void)
{
	struct ed_line *line;
	LINENUM i;

	LIST_INIT(&head);
	for (i = input_lines; i > 0; i--) {
		line = malloc(sizeof(*line));
		if (line == NULL)
			fatal("cannot allocate memory");
		line->src = SRC_INP;
		line->subst = 0;
		line->pos.lineno = i;
		LIST_INSERT_HEAD(&head, line, entries);
	}
	line_count = input_lines;
}

static void
free_lines(void)
{
	struct ed_line *line;

	while (!LIST_EMPTY(&head)) {
		line = LIST_FIRST(&head);
		LIST_REMOVE(line, entries);
		free(line);
	}
}

static struct ed_line *
get_line(LINENUM lineno)
{
	struct ed_line *line;
	LINENUM i;

	if (lineno == 0)
		return NULL;

	i = 0;
	LIST_FOREACH(line, &head, entries)
		if (++i == lineno)
			return line;

	return NULL;
}

static struct ed_line *
create_line(off_t seek)
{
	struct ed_line *line;

	line = malloc(sizeof(*line));
	if (line == NULL)
		fatal("cannot allocate memory");
	line->src = SRC_PCH;
	line->subst = 0;
	line->pos.seek = seek;

	return line;
}

static int
valid_addr(LINENUM lineno, LINENUM min)
{
	return lineno >= min && lineno <= line_count;
}
