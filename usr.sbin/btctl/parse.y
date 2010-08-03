/*	$OpenBSD: parse.y,v 1.7 2010/08/03 18:42:40 henning Exp $ */

/*
 * Copyright (c) 2008 Uwe Stuehler <uwe@openbsd.org>
 * Copyright (c) 2003 Can Erkin Acar
 * Copyright (c) 2003 Anil Madhavapeddy <anil@recoil.org>
 * Copyright (c) 2002, 2003, 2004 Henning Brauer <henning@openbsd.org>
 * Copyright (c) 2001 Markus Friedl.  All rights reserved.
 * Copyright (c) 2001 Daniel Hartmeier.  All rights reserved.
 * Copyright (c) 2001 Theo de Raadt.  All rights reserved.
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

%{
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/queue.h>

#include <dev/bluetooth/btdev.h>

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <ctype.h>
#include <syslog.h>

#include "btctl.h"
#include "btd.h"

TAILQ_HEAD(files, file)		 files = TAILQ_HEAD_INITIALIZER(files);

static struct file {
	TAILQ_ENTRY(file)	 entry;
	FILE			*stream;
	char			*name;
	int			 lineno;
	int			 errors;
} *file, *topfile;

struct file	*pushfile(const char *);
int		 popfile(void);
int		 yyparse(void);
int		 yylex(void);
int		 yyerror(const char *, ...);
int		 kw_cmp(const void *, const void *);
int		 lookup(char *);
int		 lgetc(int);
int		 lungetc(int);
int		 findeol(void);

typedef struct {
	union {
		int64_t number;
		char *string;
		bdaddr_t bdaddr;
	} v;
	int lineno;
} YYSTYPE;

int ctl_sock;

int exec_stmt(enum btctl_stmt_type, void *, size_t);
void must_read(void *, size_t);
void must_write(const void *, size_t);

#define strbufcpy(_what, _from, _to) do {				\
	if (_from == NULL)						\
		memset(_to, 0, sizeof(_to));				\
	else if (strlen(_from) <= sizeof(_to))				\
		strncpy(_to, _from, sizeof(_to));			\
	else {								\
		yyerror(_what " too long");				\
		YYERROR;						\
	}								\
} while (0)

%}

%token	INTERFACE NAME DISABLED
%token	ATTACH TYPE HID HSET NONE HF PIN
%token	ERROR
%token	<v.string>		STRING
%token	<v.number>		NUMBER
%type	<v.bdaddr>		address
%type	<v.string>		name_opt
%type	<v.number>		disabled_opt
%type	<v.number>		type_opt
%type	<v.string>		pin_opt
%%

ruleset		: /* empty */
		| ruleset '\n'
		| ruleset main '\n'
		| ruleset error '\n'	{ file->errors++; }
		;

main		: INTERFACE address name_opt disabled_opt {
			btctl_interface_stmt stmt;

			bzero(&stmt, sizeof(stmt));
			bdaddr_copy(&stmt.addr, &$2);

			strbufcpy("interface name", $3, stmt.name);
			free($3);

			stmt.flags = $4 ? BTCTL_INTERFACE_DISABLED : 0;

			switch (exec_stmt(BTCTL_INTERFACE_STMT, &stmt, sizeof(stmt))) {
			case 0:
				break;
			case EEXIST:
				yyerror("interface %s is already defined",
				    bdaddr_any(&stmt.addr) ? "*" :
				    bt_ntoa(&stmt.addr, NULL));
				YYERROR;
			default:
				yyerror("could not add interface");
				YYERROR;
			}
		}
		| ATTACH address type_opt pin_opt {
			btctl_attach_stmt stmt;

			bzero(&stmt, sizeof(stmt));
			bdaddr_copy(&stmt.addr, &$2);

			stmt.type = $3;

			if ($4 != NULL) {
				strbufcpy("PIN code", $4, stmt.pin);
				stmt.pin_size = strlen(stmt.pin);
				free($4);
			} else
				stmt.pin_size = 0;

			switch (exec_stmt(BTCTL_ATTACH_STMT, &stmt, sizeof(stmt))) {
			case 0:
				break;
			case EEXIST:
				yyerror("device %s is already defined",
				    bdaddr_any(&stmt.addr) ? "*" :
				    bt_ntoa(&stmt.addr, NULL));
				YYERROR;
			default:
				yyerror("could not add device");
				YYERROR;
			}
		}
		;

name_opt	: /* empty */
			{ $$ = NULL; }
		| NAME STRING
			{ $$ = $2; }
		;

disabled_opt	: /* empty */
			{ $$ = 0; }
		| DISABLED
			{ $$ = 1; }
		;

type_opt	: /* empty */
			{ $$ = BTDEV_NONE; }
		| TYPE NONE
			{ $$ = BTDEV_NONE; }
		| TYPE HID
			{ $$ = BTDEV_HID; }
		| TYPE HSET
			{ $$ = BTDEV_HSET; }
		| TYPE HF
			{ $$ = BTDEV_HF; }
		;

pin_opt		: /* empty */
			{ $$ = NULL; }
		| PIN STRING {
			if (($$ = calloc(HCI_PIN_SIZE, sizeof(uint8_t))) == NULL)
				fatal("pin_opt calloc");

			strlcpy($$, $2, HCI_PIN_SIZE);
			free($2);
		}
		;

address		: STRING {
			if (strcmp($1, "*")) {
				bt_aton($1, &$$);

				if (bdaddr_any(&$$)) {
					/* 0:0:0:0:0:0 could be misinterpreted */
					yyerror("invalid address '%s'", $1);
					free($1);
					YYERROR;
				}
				free($1);
			} else
				bdaddr_copy(&$$, BDADDR_ANY);
		}
		;

%%

struct keywords {
	const char	*k_name;
	int		 k_val;
};

int
yyerror(const char *fmt, ...)
{
	va_list		 ap;
	char		*nfmt;

	file->errors++;
	va_start(ap, fmt);
	if (asprintf(&nfmt, "%s:%d: %s", file->name, yylval.lineno, fmt) == -1)
		fatalx("yyerror asprintf");
	vlog(LOG_CRIT, nfmt, ap);
	va_end(ap);
	free(nfmt);
	return (0);
}

int
kw_cmp(const void *k, const void *e)
{
	return (strcmp(k, ((const struct keywords *)e)->k_name));
}

int
lookup(char *s)
{
	/* this has to be sorted always */
	static const struct keywords keywords[] = {
		{ "attach",		ATTACH},
		{ "disabled",		DISABLED},
		{ "hf",			HF},
		{ "hid",		HID},
		{ "hset",		HSET},
		{ "interface",		INTERFACE},
		{ "name",		NAME},
		{ "none",		NONE},
		{ "pin",		PIN},
		{ "type",		TYPE}
	};
	const struct keywords	*p;

	p = bsearch(s, keywords, sizeof(keywords)/sizeof(keywords[0]),
	    sizeof(keywords[0]), kw_cmp);

	if (p)
		return (p->k_val);
	else
		return (STRING);
}

#define MAXPUSHBACK	128

char	*parsebuf;
int	 parseindex;
char	 pushback_buffer[MAXPUSHBACK];
int	 pushback_index = 0;

int
lgetc(int quotec)
{
	int		c, next;

	if (parsebuf) {
		/* Read character from the parsebuffer instead of input. */
		if (parseindex >= 0) {
			c = parsebuf[parseindex++];
			if (c != '\0')
				return (c);
			parsebuf = NULL;
		} else
			parseindex++;
	}

	if (pushback_index)
		return (pushback_buffer[--pushback_index]);

	if (quotec) {
		if ((c = getc(file->stream)) == EOF) {
			yyerror("reached end of file while parsing "
			    "quoted string");
			if (file == topfile || popfile() == EOF)
				return (EOF);
			return (quotec);
		}
		return (c);
	}

	while ((c = getc(file->stream)) == '\\') {
		next = getc(file->stream);
		if (next != '\n') {
			c = next;
			break;
		}
		yylval.lineno = file->lineno;
		file->lineno++;
	}

	while (c == EOF) {
		if (file == topfile || popfile() == EOF)
			return (EOF);
		c = getc(file->stream);
	}
	return (c);
}

int
lungetc(int c)
{
	if (c == EOF)
		return (EOF);
	if (parsebuf) {
		parseindex--;
		if (parseindex >= 0)
			return (c);
	}
	if (pushback_index < MAXPUSHBACK-1)
		return (pushback_buffer[pushback_index++] = c);
	else
		return (EOF);
}

int
findeol(void)
{
	int	c;

	parsebuf = NULL;

	/* skip to either EOF or the first real EOL */
	while (1) {
		if (pushback_index)
			c = pushback_buffer[--pushback_index];
		else
			c = lgetc(0);
		if (c == '\n') {
			file->lineno++;
			break;
		}
		if (c == EOF)
			break;
	}
	return (ERROR);
}

int
yylex(void)
{
	char	 buf[8096];
	char	*p;
	int	 quotec, next, c;
	int	 token;

	p = buf;
	while ((c = lgetc(0)) == ' ' || c == '\t')
		; /* nothing */

	yylval.lineno = file->lineno;
	if (c == '#')
		while ((c = lgetc(0)) != '\n' && c != EOF)
			; /* nothing */

	switch (c) {
	case '\'':
	case '"':
		quotec = c;
		while (1) {
			if ((c = lgetc(quotec)) == EOF)
				return (0);
			if (c == '\n') {
				file->lineno++;
				continue;
			} else if (c == '\\') {
				if ((next = lgetc(quotec)) == EOF)
					return (0);
				if (next == quotec || c == ' ' || c == '\t')
					c = next;
				else if (next == '\n') {
					file->lineno++;
					continue;
				} else
					lungetc(next);
			} else if (c == quotec) {
				*p = '\0';
				break;
			}
			if (p + 1 >= buf + sizeof(buf) - 1) {
				yyerror("string too long");
				return (findeol());
			}
			*p++ = (char)c;
		}
		yylval.v.string = strdup(buf);
		if (yylval.v.string == NULL)
			fatal("yylex: strdup");
		return (STRING);
	}

#define allowed_to_end_number(x) \
	(isspace(x) || x == ')' || x ==',' || x == '/' || x == '}' || x == '=')

	if (c == '-' || isdigit(c)) {
		do {
			*p++ = c;
			if ((unsigned)(p-buf) >= sizeof(buf)) {
				yyerror("string too long");
				return (findeol());
			}
		} while ((c = lgetc(0)) != EOF && isdigit(c));
		lungetc(c);
		if (p == buf + 1 && buf[0] == '-')
			goto nodigits;
		if (c == EOF || allowed_to_end_number(c)) {
			const char *errstr = NULL;

			*p = '\0';
			yylval.v.number = strtonum(buf, LLONG_MIN,
			    LLONG_MAX, &errstr);
			if (errstr) {
				yyerror("\"%s\" invalid number: %s",
				    buf, errstr);
				return (findeol());
			}
			return (NUMBER);
		} else {
nodigits:
			while (p > buf + 1)
				lungetc(*--p);
			c = *--p;
			if (c == '-')
				return (c);
		}
	}

#define allowed_in_string(x) \
	(isalnum(x) || (ispunct(x) && x != '(' && x != ')' && \
	x != '{' && x != '}' && x != '<' && x != '>' && \
	x != '!' && x != '=' && x != '/' && x != '#' && \
	x != ','))

	if (isalnum(c) || c == ':' || c == '_' || c == '*') {
		do {
			*p++ = c;
			if ((unsigned)(p-buf) >= sizeof(buf)) {
				yyerror("string too long");
				return (findeol());
			}
		} while ((c = lgetc(0)) != EOF && (allowed_in_string(c)));
		lungetc(c);
		*p = '\0';
		if ((token = lookup(buf)) == STRING)
			if ((yylval.v.string = strdup(buf)) == NULL)
				fatal("yylex: strdup");
		return (token);
	}
	if (c == '\n') {
		yylval.lineno = file->lineno;
		file->lineno++;
	}
	if (c == EOF)
		return (0);
	return (c);
}

struct file *
pushfile(const char *name)
{
	struct file	*nfile;

	if ((nfile = calloc(1, sizeof(struct file))) == NULL) {
		log_warn("malloc");
		return (NULL);
	}
	if ((nfile->name = strdup(name)) == NULL) {
		log_warn("malloc");
		free(nfile);
		return (NULL);
	}
	if ((nfile->stream = fopen(nfile->name, "r")) == NULL) {
		log_warn("%s", nfile->name);
		free(nfile->name);
		free(nfile);
		return (NULL);
	}
	nfile->lineno = 1;
	TAILQ_INSERT_TAIL(&files, nfile, entry);
	return (nfile);
}

int
popfile(void)
{
	struct file	*prev;

	if ((prev = TAILQ_PREV(file, files, entry)) != NULL)
		prev->errors += file->errors;

	TAILQ_REMOVE(&files, file, entry);
	fclose(file->stream);
	free(file->name);
	free(file);
	file = prev;
	return (file ? 0 : EOF);
}

int
parse_config(const char *filename, int fd)
{
	enum btctl_stmt_type stmt;
	int errors = 0;

	ctl_sock = fd;

	if ((errno = exec_stmt(BTCTL_CONFIG, NULL, 0)) != 0) {
		fatal("BTCTL_CONFIG");
		return -1;
	}

	if ((file = pushfile(filename)) == NULL)
		return (-1);

	topfile = file;
	yyparse();
	errors = file->errors;
	popfile();

	stmt = errors ? BTCTL_ROLLBACK : BTCTL_COMMIT;
	if ((errno = exec_stmt(stmt, NULL, 0)) != 0)
		fatal(errors ? "BTCTL_ROLLBACK" : "BTCTL_CONFIG");

	return (errors ? -1 : 0);
}

int
exec_stmt(enum btctl_stmt_type stmt_type, void *data, size_t size)
{
	int err;

	must_write(&stmt_type, sizeof(stmt_type));
	must_write(data, size);
	must_read(&err, sizeof(err));

	return err;
}

/* Read data with the assertion that it all must come through, or
 * else abort the process.  Based on atomicio() from openssh. */
void
must_read(void *buf, size_t n)
{
	char *s = buf;
	ssize_t res, pos = 0;

	while (n > pos) {
		res = read(ctl_sock, s + pos, n - pos);
		switch (res) {
		case -1:
			if (errno == EINTR || errno == EAGAIN)
				continue;
			/* FALLTHROUGH */
		case 0:
			fatalx("short read");
		default:
			pos += res;
		}
	}
}

/* Write data with the assertion that it all has to be written, or
 * else abort the process.  Based on atomicio() from openssh. */
void
must_write(const void *buf, size_t n)
{
	const char *s = buf;
	ssize_t res, pos = 0;

	while (n > pos) {
		res = write(ctl_sock, s + pos, n - pos);
		switch (res) {
		case -1:
			if (errno == EINTR || errno == EAGAIN)
				continue;
			/* FALLTHROUGH */
		case 0:
			fatalx("short write");
		default:
			pos += res;
		}
	}
}
