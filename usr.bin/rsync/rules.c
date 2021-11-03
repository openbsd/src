/*	$OpenBSD: rules.c,v 1.4 2021/11/03 14:42:12 deraadt Exp $ */
/*
 * Copyright (c) 2021 Claudio Jeker <claudio@openbsd.org>
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
#include <err.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "extern.h"

struct rule {
	char			*pattern;
	enum rule_type		 type;
#ifdef NOTYET
	unsigned int		 modifiers;
#endif
	short			 numseg;
	unsigned char		 anchored;
	unsigned char		 fileonly;
	unsigned char		 nowild;
	unsigned char		 onlydir;
	unsigned char		 leadingdir;
};

static struct rule	*rules;
static size_t		 numrules;	/* number of rules */
static size_t		 rulesz;	/* available size */

/* up to protocol 29 filter rules only support - + ! and no modifiers */

const struct command {
	enum rule_type		type;
	char			sopt;
	const char		*lopt;
} commands[] = {
	{ RULE_EXCLUDE,		'-',	"exclude" },
	{ RULE_INCLUDE,		'+',	"include" },
	{ RULE_CLEAR,		'!',	"clear" },
#ifdef NOTYET
	{ RULE_MERGE,		'.',	"merge" },
	{ RULE_DIR_MERGE,	':',	"dir-merge" },
	{ RULE_SHOW,		'S',	"show" },
	{ RULE_HIDE,		'H',	"hide" },
	{ RULE_PROTECT,		'P',	"protect" },
	{ RULE_RISK,		'R',	"risk" },
#endif
	{ 0 }
};

#ifdef NOTYET
#define MOD_ABSOLUTE			0x0001
#define MOD_NEGATE			0x0002
#define MOD_CVSEXCLUDE			0x0004
#define MOD_SENDING			0x0008
#define MOD_RECEIVING			0x0010
#define MOD_PERISHABLE			0x0020
#define MOD_XATTR			0x0040
#define MOD_MERGE_EXCLUDE		0x0080
#define MOD_MERGE_INCLUDE		0x0100
#define MOD_MERGE_CVSCOMPAT		0x0200
#define MOD_MERGE_EXCLUDE_FILE		0x0400
#define MOD_MERGE_NO_INHERIT		0x0800
#define MOD_MERGE_WORDSPLIT		0x1000

/* maybe support absolute and negate */
const struct modifier {
	unsigned int		modifier;
	char			sopt;
} modifiers[] = {
	{ MOD_ABSOLUTE,			'/' },
	{ MOD_NEGATE,			'!' },
	{ MOD_CVSEXCLUDE,		'C' },
	{ MOD_SENDING,			's' },
	{ MOD_RECEIVING,		'r' },
	{ MOD_PERISHABLE,		'p' },
	{ MOD_XATTR,			'x' },
	/* for '.' and ':' types */
	{ MOD_MERGE_EXCLUDE,		'-' },
	{ MOD_MERGE_INCLUDE,		'+' },
	{ MOD_MERGE_CVSCOMPAT,		'C' },
	{ MOD_MERGE_EXCLUDE_FILE,	'e' },
	{ MOD_MERGE_NO_INHERIT,		'n' },
	{ MOD_MERGE_WORDSPLIT,		'w' },
	{ 0 }
}
#endif

static struct rule *
get_next_rule(void)
{
	struct rule *new;
	size_t newsz;

	if (++numrules > rulesz) {
		if (rulesz == 0)
			newsz = 16;
		else
			newsz = rulesz * 2;

		new = recallocarray(rules, rulesz, newsz, sizeof(*rules));
		if (new == NULL)
			err(ERR_NOMEM, NULL);

		rules = new;
		rulesz = newsz;
	}

	return rules + numrules - 1;
}

static enum rule_type
parse_command(const char *command, size_t len)
{
	const char *mod;
	size_t	i;

	mod = memchr(command, ',', len);
	if (mod != NULL) {
		/* XXX modifiers not yet implemented */
		return RULE_NONE;
	}

	for (i = 0; commands[i].type != RULE_NONE; i++) {
		if (strncmp(commands[i].lopt, command, len) == 0)
			return commands[i].type;
		if (len == 1 && commands[i].sopt == *command)
			return commands[i].type;
	}

	return RULE_NONE;
}

static void
parse_pattern(struct rule *r, char *pattern)
{
	size_t plen;
	char *p;
	short nseg = 1;

	/*
	 * check for / at start and end of pattern both are special and
	 * can bypass full path matching.
	 */
	if (*pattern == '/') {
		pattern++;
		r->anchored = 1;
	}
	plen = strlen(pattern);
	/*
	 * check for patterns ending in '/' and '/'+'***' and handle them
	 * specially. Because of this and the check above pattern will never
	 * start or end with a '/'.
	 */
	if (plen > 1 && pattern[plen - 1] == '/') {
		r->onlydir = 1;
		pattern[plen - 1] = '\0';
	}
	if (plen > 4 && strcmp(pattern + plen - 4, "/***") == 0) {
		r->leadingdir = 1;
		pattern[plen - 4] = '\0';
	}

	/* count how many segments the pattern has. */
	for (p = pattern; *p != '\0'; p++)
		if (*p == '/')
			nseg++;
	r->numseg = nseg;

	/* check if this pattern only matches against the basename */
	if (nseg == 1 && !r->anchored)
		r->fileonly = 1;

	if (strpbrk(pattern, "*?[") == NULL) {
		/* no wildchar matching */
		r->nowild = 1;
	} else {
		/* requires wildchar matching */
		if (strstr(pattern, "**") != NULL)
			r->numseg = -1;
	}

	r->pattern = strdup(pattern);
	if (r->pattern == NULL)
		err(ERR_NOMEM, NULL);
}

int
parse_rule(char *line, enum rule_type def)
{
	enum rule_type type;
	struct rule *r;
	char *pattern;
	size_t len;

	switch (*line) {
	case '#':
	case ';':
		/* comment */
		return 0;
	case '\0':
		/* ingore empty lines */
		return 0;
	default:
		len = strcspn(line, " _");
		type = parse_command(line, len);
		if (type == RULE_NONE) {
			if (def == RULE_NONE)
				return -1;
			type = def;
			pattern = line;
		} else
			pattern = line + len + 1;

		if (*pattern == '\0' && type != RULE_CLEAR)
			return -1;
		if (*pattern != '\0' && type == RULE_CLEAR)
			return -1;
		break;
	}

	r = get_next_rule();
	r->type = type;
	parse_pattern(r, pattern);

	return 0;
}

void
parse_file(const char *file, enum rule_type def)
{
	FILE *fp;
	char *line = NULL;
	size_t linesize = 0, linenum = 0;
	ssize_t linelen;

	if ((fp = fopen(file, "r")) == NULL)
		err(ERR_SYNTAX, "open: %s", file);

	while ((linelen = getline(&line, &linesize, fp)) != -1) {
		linenum++;
		line[linelen - 1] = '\0';
		if (parse_rule(line, def) == -1)
			errx(ERR_SYNTAX, "syntax error in %s at entry %zu",
			    file, linenum);
	}

	free(line);
	if (ferror(fp))
		err(ERR_SYNTAX, "failed to parse file %s", file);
	fclose(fp);
}

static const char *
send_command(struct rule *r)
{
	static char buf[16];
	char *b = buf;
	char *ep = buf + sizeof(buf);

	switch (r->type) {
	case RULE_EXCLUDE:
		*b++ = '-';
		break;
	case RULE_INCLUDE:
		*b++ = '+';
		break;
	case RULE_CLEAR:
		*b++ = '!';
		break;
#ifdef NOTYET
	case RULE_MERGE:
		*b++ = '.';
		break;
	case RULE_DIR_MERGE:
		*b++ = ':';
		break;
	case RULE_SHOW:
		*b++ = 'S';
		break;
	case RULE_HIDE:
		*b++ = 'H';
		break;
	case RULE_PROTECT:
		*b++ = 'P';
		break;
	case RULE_RISK:
		*b++ = 'R';
		break;
#endif
	default:
		err(ERR_SYNTAX, "unknown rule type %d", r->type);
	}

#ifdef NOTYET
	for (i = 0; modifiers[i].modifier != 0; i++) {
		if (rule->modifiers & modifiers[i].modifier)
			*b++ = modifiers[i].sopt;
		if (b >= ep - 3)
			err(ERR_SYNTAX, "rule modifiers overflow");
	}
#endif
	if (b >= ep - 3)
		err(ERR_SYNTAX, "rule prefix overflow");
	*b++ = ' ';

	/* include the stripped root '/' for anchored patterns */
	if (r->anchored)
		*b++ = '/';
	*b++ = '\0';
	return buf;
}

static const char *
postfix_command(struct rule *r)
{
	static char buf[8];

	buf[0] = '\0';
	if (r->onlydir)
		strlcpy(buf, "/", sizeof(buf));
	if (r->leadingdir)
		strlcpy(buf, "/***", sizeof(buf));

	return buf;
}

void
send_rules(struct sess *sess, int fd)
{
	const char *cmd;
	const char *postfix;
	struct rule *r;
	size_t cmdlen, len, postlen, i;

	for (i = 0; i < numrules; i++) {
		r = &rules[i];
		cmd = send_command(r);
		if (cmd == NULL)
			err(ERR_PROTOCOL,
			    "rules are incompatible with remote rsync");
		postfix = postfix_command(r);
		cmdlen = strlen(cmd);
		len = strlen(r->pattern);
		postlen = strlen(postfix);

		if (!io_write_int(sess, fd, cmdlen + len + postlen))
			err(ERR_SOCK_IO, "send rules");
		if (!io_write_buf(sess, fd, cmd, cmdlen))
			err(ERR_SOCK_IO, "send rules");
		if (!io_write_buf(sess, fd, r->pattern, len))
			err(ERR_SOCK_IO, "send rules");
		/* include the '/' stripped by onlydir */
		if (postlen > 0)
			if (!io_write_buf(sess, fd, postfix, postlen))
				err(ERR_SOCK_IO, "send rules");
	}

	if (!io_write_int(sess, fd, 0))
		err(ERR_SOCK_IO, "send rules");
}

void
recv_rules(struct sess *sess, int fd)
{
	char line[8192];
	size_t len;

	do {
		if (!io_read_size(sess, fd, &len))
			err(ERR_SOCK_IO, "receive rules");

		if (len == 0)
			return;
		if (len >= sizeof(line) - 1)
			errx(ERR_SOCK_IO, "received rule too long");
		if (!io_read_buf(sess, fd, line, len))
			err(ERR_SOCK_IO, "receive rules");
		line[len] = '\0';
		if (parse_rule(line, RULE_NONE) == -1)
			errx(ERR_PROTOCOL, "syntax error in received rules");
	} while (1);
}

static inline int
rule_matched(struct rule *r)
{
	/* TODO apply negation once modifiers are added */

	if (r->type == RULE_EXCLUDE)
		return -1;
	else
		return 1;
}

int
rules_match(const char *path, int isdir)
{
	const char *basename, *p = NULL;
	struct rule *r;
	size_t i;

	basename = strrchr(path, '/');
	if (basename != NULL)
		basename += 1;
	else
		basename = path;

	for (i = 0; i < numrules; i++) {
		r = &rules[i];

		if (r->onlydir && !isdir)
			continue;

		if (r->nowild) {
			/* fileonly and anchored are mutually exclusive */
			if (r->fileonly) {
				if (strcmp(basename, r->pattern) == 0)
					return rule_matched(r);
			} else if (r->anchored) {
				/*
				 * assumes that neither path nor pattern
				 * start with a '/'.
				 */
				if (strcmp(path, r->pattern) == 0)
					return rule_matched(r);
			} else if (r->leadingdir) {
				size_t plen = strlen(r->pattern);

				p = strstr(path, r->pattern);
				/*
				 * match from start or dir boundary also
				 * match to end or to dir boundary
				 */
				if (p != NULL && (p == path || p[-1] == '/') &&
				    (p[plen] == '\0' || p[plen] == '/'))
					return rule_matched(r);
			} else {
				size_t len = strlen(path);
				size_t plen = strlen(r->pattern);

				if (len >= plen && strcmp(path + len - plen,
				    r->pattern) == 0) {
					/* match all or start on dir boundary */
					if (len == plen ||
					    path[len - plen - 1] == '/')
						return rule_matched(r);
				}
			}
		} else {
			if (r->fileonly) {
				p = basename;
			} else if (r->anchored || r->numseg == -1) {
				/* full path matching */
				p = path;
			} else {
				short nseg = 1;

				/* match against the last numseg elements */
				for (p = path; *p != '\0'; p++)
					if (*p == '/')
						nseg++;
				if (nseg < r->numseg) {
					p = NULL;
				} else {
					nseg -= r->numseg;
					for (p = path; *p != '\0' && nseg > 0;
					    p++) {
						if (*p == '/')
							nseg--;
					}
				}
			}

			if (p != NULL) {
				if (rmatch(r->pattern, p, r->leadingdir) == 0)
					return rule_matched(r);
			}
		}
	}

	return 0;
}
