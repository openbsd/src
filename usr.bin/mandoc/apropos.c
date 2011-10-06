/*	$Id: apropos.c,v 1.1 2011/10/06 23:04:16 schwarze Exp $ */
/*
* Copyright (c) 2011 Kristaps Dzonsons <kristaps@bsd.lv>
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
#include <sys/types.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <regex.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <db.h>

#include "mandoc.h"

#define	MAXRESULTS	 100

#define TYPE_NAME	0x01
#define TYPE_FUNCTION	0x02
#define TYPE_UTILITY	0x04
#define TYPE_INCLUDES	0x08
#define TYPE_VARIABLE	0x10
#define TYPE_STANDARD	0x20
#define TYPE_AUTHOR	0x40
#define TYPE_CONFIG	0x80
#define TYPE_DESC	0x100
#define TYPE_XREF	0x200
#define TYPE_PATH	0x400
#define	TYPE_ENV	0x800
#define	TYPE_ERR	0x1000

enum	match {
	MATCH_SUBSTR = 0,
	MATCH_REGEX,
	MATCH_EXACT
};

enum	sort {
	SORT_TITLE = 0,
	SORT_CAT,
	SORT__MAX
};

struct	opts {
	enum sort	 sort; /* output sorting */
	const char	*arch; /* restrict to architecture */
	const char	*cat; /* restrict to category */
	int		 types; /* only types in bitmask */
	int		 insens; /* case-insensitive match */
	enum match	 match; /* match type */
};

struct	type {
	int		 mask;
	const char	*name;
};

struct	rec {
	char		*file;
	char		*cat;
	char		*title;
	char		*arch;
	char		*desc;
	recno_t		 rec;
};

struct	res {
	char		*arch; /* architecture */
	char		*desc; /* free-form description */
	char		*keyword; /* matched keyword */
	int	 	 types; /* bitmask of field selectors */
	char		*cat; /* manual section */
	char		*title; /* manual section */
	char		*uri; /* formatted uri of file */
	recno_t		 rec; /* unique id of underlying manual */
};

struct	state {
	DB		 *db; /* database */
	DB		 *idx; /* index */
	const char	 *dbf; /* database name */
	const char	 *idxf; /* index name */
	void		(*err)(const char *);
	void		(*errx)(const char *, ...);
};

static	const char * const sorts[SORT__MAX] = {
	"cat", /* SORT_CAT */
	"title", /* SORT_TITLE */
};

static	const struct type types[] = {
	{ TYPE_NAME, "name" },
	{ TYPE_FUNCTION, "func" },
	{ TYPE_UTILITY, "utility" },
	{ TYPE_INCLUDES, "incl" },
	{ TYPE_VARIABLE, "var" },
	{ TYPE_STANDARD, "stand" },
	{ TYPE_AUTHOR, "auth" },
	{ TYPE_CONFIG, "conf" },
	{ TYPE_DESC, "desc" },
	{ TYPE_XREF, "xref" },
	{ TYPE_PATH, "path" },
	{ TYPE_ENV, "env" },
	{ TYPE_ERR, "err" },
	{ INT_MAX, "all" },
	{ 0, NULL }
};

static	void	 buf_alloc(char **, size_t *, size_t);
static	void	 buf_dup(struct mchars *, char **, const char *);
static	void	 buf_redup(struct mchars *, char **, 
			size_t *, const char *);
static	void	 error(const char *, ...);
static	int	 sort_cat(const void *, const void *);
static	int	 sort_title(const void *, const void *);
static	void	 state_destroy(struct state *);
static	int	 state_getrecord(struct state *, recno_t, struct rec *);
static	int	 state_init(struct state *, 
			const char *, const char *,
			void (*err)(const char *),
			void (*errx)(const char *, ...));
static	void	 state_output(const struct res *, int);
static	void	 state_search(struct state *, 
			const struct opts *, char *);

static	void	 usage(void);

static	const char	 *progname;

int
apropos(int argc, char *argv[])
{
	int		 ch, i;
	const char	*dbf, *idxf;
	struct state	 state;
	char		*q, *v;
	struct opts	 opts;
	extern int	 optind;
	extern char	*optarg;

	memset(&opts, 0, sizeof(struct opts));

	dbf = "mandoc.db";
	idxf = "mandoc.index";
	q = NULL;

	progname = strrchr(argv[0], '/');
	if (progname == NULL)
		progname = argv[0];
	else
		++progname;

	opts.match = MATCH_SUBSTR;

	while (-1 != (ch = getopt(argc, argv, "a:c:eIrs:t:"))) 
		switch (ch) {
		case ('a'):
			opts.arch = optarg;
			break;
		case ('c'):
			opts.cat = optarg;
			break;
		case ('e'):
			opts.match = MATCH_EXACT;
			break;
		case ('I'):
			opts.insens = 1;
			break;
		case ('r'):
			opts.match = MATCH_REGEX;
			break;
		case ('s'):
			for (i = 0; i < SORT__MAX; i++) {
				if (strcmp(optarg, sorts[i])) 
					continue;
				opts.sort = (enum sort)i;
				break;
			}

			if (i < SORT__MAX)
				break;

			error("%s: Bad sort\n", optarg);
			return(EXIT_FAILURE);
		case ('t'):
			while (NULL != (v = strsep(&optarg, ","))) {
				if ('\0' == *v)
					continue;
				for (i = 0; types[i].mask; i++) {
					if (strcmp(types[i].name, v))
						continue;
					break;
				}
				if (0 == types[i].mask)
					break;
				opts.types |= types[i].mask;
			}
			if (NULL == v)
				break;
			
			error("%s: Bad type\n", v);
			return(EXIT_FAILURE);
		default:
			usage();
			return(EXIT_FAILURE);
		}

	argc -= optind;
	argv += optind;

	if (0 == argc || '\0' == **argv) {
		usage();
		return(EXIT_FAILURE);
	} else
		q = *argv;

	if (0 == opts.types)
		opts.types = TYPE_NAME | TYPE_DESC;

	if ( ! state_init(&state, dbf, idxf, perror, error)) {
		state_destroy(&state);
		return(EXIT_FAILURE);
	}

	state_search(&state, &opts, q);
	state_destroy(&state);

	return(EXIT_SUCCESS);
}

static void
state_search(struct state *p, const struct opts *opts, char *q)
{
	int		 i, len, ch, rflags, dflag;
	struct mchars	*mc;
	char		*buf;
	size_t		 bufsz;
	recno_t		 rec;
	uint32_t	 fl;
	DBT		 key, val;
	struct res	 res[MAXRESULTS];
	regex_t		 reg;
	regex_t		*regp;
	char		 filebuf[10];
	struct rec	 record;

	len = 0;
	buf = NULL;
	bufsz = 0;
	ch = 0;
	regp = NULL;

	switch (opts->match) {
	case (MATCH_REGEX):
		rflags = REG_EXTENDED | REG_NOSUB | 
			(opts->insens ? REG_ICASE : 0);

		if (0 != regcomp(&reg, q, rflags)) {
			error("%s: Bad pattern\n", q);
			return;
		}

		regp = &reg;
		dflag = R_FIRST;
		break;
	case (MATCH_EXACT):
		key.data = q;
		key.size = strlen(q) + 1;
		dflag = R_CURSOR;
		break;
	default:
		dflag = R_FIRST;
		break;
	}

	if (NULL == (mc = mchars_alloc())) {
		perror(NULL);
		exit(EXIT_FAILURE);
	}

	/*
	 * Iterate over the entire keyword database.
	 * For each record, we must first translate the key into UTF-8.
	 * Following that, make sure it's acceptable.
	 * Lastly, add it to the available records.
	 */

	while (len < MAXRESULTS) {
		if ((ch = (*p->db->seq)(p->db, &key, &val, dflag)))
			break;

		dflag = R_NEXT;

		/* 
		 * Keys must be sized as such: the keyword must be
		 * non-empty (nil terminator plus one character) and the
		 * value must be 8 (recno_t---uint32_t---index reference
		 * and a uint32_t flag field).
		 */

		if (key.size < 2 || 8 != val.size) {
			error("%s: Corrupt database\n", p->dbf);
			exit(EXIT_FAILURE);
		}

		buf_redup(mc, &buf, &bufsz, (char *)key.data);

		fl = *(uint32_t *)val.data;

		if ( ! (fl & opts->types))
			continue;

		switch (opts->match) {
		case (MATCH_REGEX):
			if (regexec(regp, buf, 0, NULL, 0))
				continue;
			break;
		case (MATCH_EXACT):
			if (opts->insens && strcasecmp(buf, q))
				goto send;
			if ( ! opts->insens && strcmp(buf, q))
				goto send;
			break;
		default:
			if (opts->insens && NULL == strcasestr(buf, q))
				continue;
			if ( ! opts->insens && NULL == strstr(buf, q))
				continue;
			break;
		}

		/*
		 * Now look up the file itself in our index.  The file's
		 * indexed by its recno for fast lookups.
		 */

		memcpy(&rec, val.data + 4, sizeof(recno_t));

		if ( ! state_getrecord(p, rec, &record))
			exit(EXIT_FAILURE);

		/* If we're in a different section, skip... */

		if (opts->cat && strcasecmp(opts->cat, record.cat))
			continue;
		if (opts->arch && strcasecmp(opts->arch, record.arch))
			continue;

		/* FIXME: this needs to be changed.  Ugh.  Linear. */

		for (i = 0; i < len; i++)
			if (res[i].rec == record.rec)
				break;

		if (i < len)
			continue;

		/*
		 * Now we have our filename, keywords, types, and all
		 * other necessary information.  
		 * Process it and add it to our list of results.
		 */

		filebuf[9] = '\0';
		snprintf(filebuf, 10, "%u", record.rec);
		assert('\0' == filebuf[9]);

		res[len].rec = record.rec;
		res[len].types = fl;

		buf_dup(mc, &res[len].keyword, buf);
		buf_dup(mc, &res[len].uri, filebuf);
		buf_dup(mc, &res[len].cat, record.cat);
		buf_dup(mc, &res[len].arch, record.arch);
		buf_dup(mc, &res[len].title, record.title);
		buf_dup(mc, &res[len].desc, record.desc);
		len++;
	}

send:
	if (ch < 0) {
		perror(p->dbf);
		exit(EXIT_FAILURE);
	} 

	switch (opts->sort) {
	case (SORT_CAT):
		qsort(res, len, sizeof(struct res), sort_cat);
		break;
	default:
		qsort(res, len, sizeof(struct res), sort_title);
		break;
	}

	state_output(res, len);

	for (len-- ; len >= 0; len--) {
		free(res[len].keyword);
		free(res[len].title);
		free(res[len].cat);
		free(res[len].arch);
		free(res[len].desc);
		free(res[len].uri);
	}

	free(buf);
	mchars_free(mc);

	if (regp)
		regfree(regp);
}

/*
 * Track allocated buffer size for buf_redup().
 */
static inline void
buf_alloc(char **buf, size_t *bufsz, size_t sz)
{

	if (sz < *bufsz) 
		return;

	*bufsz = sz + 1024;
	if (NULL == (*buf = realloc(*buf, *bufsz))) {
		perror(NULL);
		exit(EXIT_FAILURE);
	}
}

/*
 * Like buf_redup() but throwing away the buffer size.
 */
static void
buf_dup(struct mchars *mc, char **buf, const char *val)
{
	size_t		 bufsz;

	bufsz = 0;
	*buf = NULL;
	buf_redup(mc, buf, &bufsz, val);
}

/*
 * Normalise strings from the index and database.
 * These strings are escaped as defined by mandoc_char(7) along with
 * other goop in mandoc.h (e.g., soft hyphens).
 */
static void
buf_redup(struct mchars *mc, char **buf, 
		size_t *bufsz, const char *val)
{
	size_t		 sz;
	const char	*seq, *cpp;
	int		 len, pos;
	enum mandoc_esc	 esc;
	const char	 rsv[] = { '\\', ASCII_NBRSP, ASCII_HYPH, '\0' };

	/* Pre-allocate by the length of the input */

	buf_alloc(buf, bufsz, strlen(val) + 1);

	pos = 0;

	while ('\0' != *val) {
		/*
		 * Halt on the first escape sequence.
		 * This also halts on the end of string, in which case
		 * we just copy, fallthrough, and exit the loop.
		 */
		if ((sz = strcspn(val, rsv)) > 0) {
			memcpy(&(*buf)[pos], val, sz);
			pos += (int)sz;
			val += (int)sz;
		}

		if (ASCII_HYPH == *val) {
			(*buf)[pos++] = '-';
			val++;
			continue;
		} else if (ASCII_NBRSP == *val) {
			(*buf)[pos++] = ' ';
			val++;
			continue;
		} else if ('\\' != *val)
			break;

		/* Read past the slash. */

		val++;

		/*
		 * Parse the escape sequence and see if it's a
		 * predefined character or special character.
		 */

		esc = mandoc_escape(&val, &seq, &len);
		if (ESCAPE_ERROR == esc)
			break;

		cpp = ESCAPE_SPECIAL == esc ? 
			mchars_spec2str(mc, seq, len, &sz) : NULL;

		if (NULL == cpp)
			continue;

		/* Copy the rendered glyph into the stream. */

		buf_alloc(buf, bufsz, sz);

		memcpy(&(*buf)[pos], cpp, sz);
		pos += (int)sz;
	}

	(*buf)[pos] = '\0';
}

static void
error(const char *fmt, ...)
{
	va_list		 ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}

static void
state_output(const struct res *res, int sz)
{
	int		 i;

	for (i = 0; i < sz; i++)
		printf("%s(%s%s%s) - %s\n", res[i].title, 
				res[i].cat, 
				*res[i].arch ? "/" : "",
				*res[i].arch ? res[i].arch : "",
				res[i].desc);
}

static void
usage(void)
{

	fprintf(stderr, "usage: %s "
			"[-eIr] "
			"[-a arch] "
			"[-c cat] "
			"[-s sort] "
			"[-t type[,...]] "
			"key\n", progname);
}

static int
state_init(struct state *p, 
		const char *dbf, const char *idxf,
		void (*err)(const char *),
		void (*errx)(const char *, ...))
{
	BTREEINFO	 info;

	memset(p, 0, sizeof(struct state));
	memset(&info, 0, sizeof(BTREEINFO));

	info.flags = R_DUP;

	p->dbf = dbf;
	p->idxf = idxf;
	p->err = err;

	p->db = dbopen(p->dbf, O_RDONLY, 0, DB_BTREE, &info);
	if (NULL == p->db) {
		(*err)(p->dbf);
		return(0);
	}

	p->idx = dbopen(p->idxf, O_RDONLY, 0, DB_RECNO, NULL);
	if (NULL == p->idx) {
		(*err)(p->idxf);
		return(0);
	}

	return(1);
}

static void
state_destroy(struct state *p)
{

	if (p->db)
		(*p->db->close)(p->db);
	if (p->idx)
		(*p->idx->close)(p->idx);
}

static int
state_getrecord(struct state *p, recno_t rec, struct rec *rp)
{
	DBT		key, val;
	size_t		sz;
	int		rc;

	key.data = &rec;
	key.size = sizeof(recno_t);

	rc = (*p->idx->get)(p->idx, &key, &val, 0);
	if (rc < 0) {
		(*p->err)(p->idxf);
		return(0);
	} else if (rc > 0) {
		(*p->errx)("%s: Corrupt index\n", p->idxf);
		return(0);
	}

	rp->file = (char *)val.data;
	if ((sz = strlen(rp->file) + 1) >= val.size) {
		(*p->errx)("%s: Corrupt index\n", p->idxf);
		return(0);
	}

	rp->cat = (char *)val.data + (int)sz;
	if ((sz += strlen(rp->cat) + 1) >= val.size) {
		(*p->errx)("%s: Corrupt index\n", p->idxf);
		return(0);
	}

	rp->title = (char *)val.data + (int)sz;
	if ((sz += strlen(rp->title) + 1) >= val.size) {
		(*p->errx)("%s: Corrupt index\n", p->idxf);
		return(0);
	}

	rp->arch = (char *)val.data + (int)sz;
	if ((sz += strlen(rp->arch) + 1) >= val.size) {
		(*p->errx)("%s: Corrupt index\n", p->idxf);
		return(0);
	}

	rp->desc = (char *)val.data + (int)sz;
	rp->rec = rec;
	return(1);
}

static int
sort_title(const void *p1, const void *p2)
{

	return(strcmp(((const struct res *)p1)->title,
		      ((const struct res *)p2)->title));
}

static int
sort_cat(const void *p1, const void *p2)
{
	int		 rc;

	rc = strcmp(((const struct res *)p1)->cat,
			((const struct res *)p2)->cat);

	return(0 == rc ? sort_title(p1, p2) : rc);
}
