/*	$Id: apropos_db.c,v 1.5 2011/11/17 14:52:32 schwarze Exp $ */
/*
 * Copyright (c) 2011 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2011 Ingo Schwarze <schwarze@openbsd.org>
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
#include <assert.h>
#include <fcntl.h>
#include <regex.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef __linux__
# include <db_185.h>
#else
# include <db.h>
#endif

#include "mandocdb.h"
#include "apropos_db.h"
#include "mandoc.h"

struct	rectree {
	struct rec	*node;
	int		 len;
};

struct	expr {
	int		 regex;
	int		 index;
	int	 	 mask;
	int		 and;
	char		*v;
	regex_t	 	 re;
	struct expr	*next;
};

struct	type {
	int		 mask;
	const char	*name;
};

static	const struct type types[] = {
	{ TYPE_An, "An" },
	{ TYPE_Cd, "Cd" },
	{ TYPE_Er, "Er" },
	{ TYPE_Ev, "Ev" },
	{ TYPE_Fn, "Fn" },
	{ TYPE_Fn, "Fo" },
	{ TYPE_In, "In" },
	{ TYPE_Nd, "Nd" },
	{ TYPE_Nm, "Nm" },
	{ TYPE_Pa, "Pa" },
	{ TYPE_St, "St" },
	{ TYPE_Va, "Va" },
	{ TYPE_Va, "Vt" },
	{ TYPE_Xr, "Xr" },
	{ INT_MAX, "any" },
	{ 0, NULL }
};

static	DB	*btree_open(void);
static	int	 btree_read(const DBT *, const struct mchars *, char **);
static	int	 exprexecpre(const struct expr *, const char *, int);
static	void	 exprexecpost(const struct expr *, 
			const char *, int, int *, size_t);
static	struct expr *exprterm(char *, int, int);
static	DB	*index_open(void);
static	int	 index_read(const DBT *, const DBT *, 
			const struct mchars *, struct rec *);
static	void	 norm_string(const char *,
			const struct mchars *, char **);
static	size_t	 norm_utf8(unsigned int, char[7]);
static	void	 recfree(struct rec *);
static	void	 single_search(struct rectree *, const struct opts *,
			const struct expr *, size_t terms,
			struct mchars *);

/*
 * Open the keyword mandoc-db database.
 */
static DB *
btree_open(void)
{
	BTREEINFO	 info;
	DB		*db;

	memset(&info, 0, sizeof(BTREEINFO));
	info.flags = R_DUP;

	db = dbopen(MANDOC_DB, O_RDONLY, 0, DB_BTREE, &info);
	if (NULL != db) 
		return(db);

	return(NULL);
}

/*
 * Read a keyword from the database and normalise it.
 * Return 0 if the database is insane, else 1.
 */
static int
btree_read(const DBT *v, const struct mchars *mc, char **buf)
{

	/* Sanity: are we nil-terminated? */

	assert(v->size > 0);
	if ('\0' != ((char *)v->data)[(int)v->size - 1])
		return(0);

	norm_string((char *)v->data, mc, buf);
	return(1);
}

/*
 * Take a Unicode codepoint and produce its UTF-8 encoding.
 * This isn't the best way to do this, but it works.
 * The magic numbers are from the UTF-8 packaging.  
 * They're not as scary as they seem: read the UTF-8 spec for details.
 */
static size_t
norm_utf8(unsigned int cp, char out[7])
{
	size_t		 rc;

	rc = 0;

	if (cp <= 0x0000007F) {
		rc = 1;
		out[0] = (char)cp;
	} else if (cp <= 0x000007FF) {
		rc = 2;
		out[0] = (cp >> 6  & 31) | 192;
		out[1] = (cp       & 63) | 128;
	} else if (cp <= 0x0000FFFF) {
		rc = 3;
		out[0] = (cp >> 12 & 15) | 224;
		out[1] = (cp >> 6  & 63) | 128;
		out[2] = (cp       & 63) | 128;
	} else if (cp <= 0x001FFFFF) {
		rc = 4;
		out[0] = (cp >> 18 & 7) | 240;
		out[1] = (cp >> 12 & 63) | 128;
		out[2] = (cp >> 6  & 63) | 128;
		out[3] = (cp       & 63) | 128;
	} else if (cp <= 0x03FFFFFF) {
		rc = 5;
		out[0] = (cp >> 24 & 3) | 248;
		out[1] = (cp >> 18 & 63) | 128;
		out[2] = (cp >> 12 & 63) | 128;
		out[3] = (cp >> 6  & 63) | 128;
		out[4] = (cp       & 63) | 128;
	} else if (cp <= 0x7FFFFFFF) {
		rc = 6;
		out[0] = (cp >> 30 & 1) | 252;
		out[1] = (cp >> 24 & 63) | 128;
		out[2] = (cp >> 18 & 63) | 128;
		out[3] = (cp >> 12 & 63) | 128;
		out[4] = (cp >> 6  & 63) | 128;
		out[5] = (cp       & 63) | 128;
	} else
		return(0);

	out[rc] = '\0';
	return(rc);
}

/*
 * Normalise strings from the index and database.
 * These strings are escaped as defined by mandoc_char(7) along with
 * other goop in mandoc.h (e.g., soft hyphens).
 * This function normalises these into a nice UTF-8 string.
 * Returns 0 if the database is fucked.
 */
static void
norm_string(const char *val, const struct mchars *mc, char **buf)
{
	size_t		  sz, bsz;
	char		  utfbuf[7];
	const char	 *seq, *cpp;
	int		  len, u, pos;
	enum mandoc_esc	  esc;
	static const char res[] = { '\\', '\t', 
				ASCII_NBRSP, ASCII_HYPH, '\0' };

	/* Pre-allocate by the length of the input */

	bsz = strlen(val) + 1;
	*buf = mandoc_realloc(*buf, bsz);
	pos = 0;

	while ('\0' != *val) {
		/*
		 * Halt on the first escape sequence.
		 * This also halts on the end of string, in which case
		 * we just copy, fallthrough, and exit the loop.
		 */
		if ((sz = strcspn(val, res)) > 0) {
			memcpy(&(*buf)[pos], val, sz);
			pos += (int)sz;
			val += (int)sz;
		}

		if (ASCII_HYPH == *val) {
			(*buf)[pos++] = '-';
			val++;
			continue;
		} else if ('\t' == *val || ASCII_NBRSP == *val) {
			(*buf)[pos++] = ' ';
			val++;
			continue;
		} else if ('\\' != *val)
			break;

		/* Read past the slash. */

		val++;
		u = 0;

		/*
		 * Parse the escape sequence and see if it's a
		 * predefined character or special character.
		 */

		esc = mandoc_escape(&val, &seq, &len);
		if (ESCAPE_ERROR == esc)
			break;

		/* 
		 * XXX - this just does UTF-8, but we need to know
		 * beforehand whether we should do text substitution.
		 */

		switch (esc) {
		case (ESCAPE_SPECIAL):
			if (0 != (u = mchars_spec2cp(mc, seq, len)))
				break;
			/* FALLTHROUGH */
		default:
			continue;
		}

		/*
		 * If we have a Unicode codepoint, try to convert that
		 * to a UTF-8 byte string.
		 */

		cpp = utfbuf;
		if (0 == (sz = norm_utf8(u, utfbuf)))
			continue;

		/* Copy the rendered glyph into the stream. */

		sz = strlen(cpp);
		bsz += sz;

		*buf = mandoc_realloc(*buf, bsz);

		memcpy(&(*buf)[pos], cpp, sz);
		pos += (int)sz;
	}

	(*buf)[pos] = '\0';
}

/*
 * Open the filename-index mandoc-db database.
 * Returns NULL if opening failed.
 */
static DB *
index_open(void)
{
	DB		*db;

	db = dbopen(MANDOC_IDX, O_RDONLY, 0, DB_RECNO, NULL);
	if (NULL != db)
		return(db);

	return(NULL);
}

/*
 * Safely unpack from an index file record into the structure.
 * Returns 1 if an entry was unpacked, 0 if the database is insane.
 */
static int
index_read(const DBT *key, const DBT *val, 
		const struct mchars *mc, struct rec *rec)
{
	size_t		 left;
	char		*np, *cp;

#define	INDEX_BREAD(_dst) \
	do { \
		if (NULL == (np = memchr(cp, '\0', left))) \
			return(0); \
		norm_string(cp, mc, &(_dst)); \
		left -= (np - cp) + 1; \
		cp = np + 1; \
	} while (/* CONSTCOND */ 0)

	left = val->size;
	cp = (char *)val->data;

	rec->rec = *(recno_t *)key->data;

	INDEX_BREAD(rec->file);
	INDEX_BREAD(rec->cat);
	INDEX_BREAD(rec->title);
	INDEX_BREAD(rec->arch);
	INDEX_BREAD(rec->desc);
	return(1);
}

/*
 * Search the mandocdb database for the expression "expr".
 * Filter out by "opts".
 * Call "res" with the results, which may be zero.
 */
void
apropos_search(int argc, char *argv[], const struct opts *opts,
		const struct expr *expr, size_t terms, void *arg, 
		void (*res)(struct rec *, size_t, void *))
{
	struct rectree	 tree;
	struct mchars	*mc;
	struct rec	*recs;
	int		 i, mlen;

	memset(&tree, 0, sizeof(struct rectree));

	/* XXX: error out with bad regexp? */

	mc = mchars_alloc();

	for (i = 0; i < argc; i++) {
		if (chdir(argv[i]))
			continue;
		single_search(&tree, opts, expr, terms, mc);
	}

	/*
	 * Count the matching files
	 * and feed them to the output handler.
	 */

	for (mlen = i = 0; i < tree.len; i++)
		if (tree.node[i].matches[0])
			mlen++;
	recs = mandoc_malloc(mlen * sizeof(struct rec));
	for (mlen = i = 0; i < tree.len; i++)
		if (tree.node[i].matches[0])
			memcpy(&recs[mlen++], &tree.node[i], 
					sizeof(struct rec));
	(*res)(recs, mlen, arg);
	free(recs);

	for (i = 0; i < tree.len; i++)
		recfree(&tree.node[i]);

	if (mc)
		mchars_free(mc);
}

static void
single_search(struct rectree *tree, const struct opts *opts,
		const struct expr *expr, size_t terms,
		struct mchars *mc)
{
	int		 root, leaf, mask;
	DBT		 key, val;
	DB		*btree, *idx;
	int		 ch;
	char		*buf;
	recno_t		 rec;
	struct rec	*recs;
	struct rec	 srec;

	root	= -1;
	leaf	= -1;
	btree	= NULL;
	idx	= NULL;
	buf	= NULL;
	recs	= tree->node;

	memset(&srec, 0, sizeof(struct rec));

	/* XXX: return fact that we've errored? */

	if (NULL == (btree = btree_open())) 
		goto out;
	if (NULL == (idx = index_open())) 
		goto out;

	while (0 == (ch = (*btree->seq)(btree, &key, &val, R_NEXT))) {
		/* 
		 * Low-water mark for key and value.
		 * The key must have something in it, and the value must
		 * have the correct tags/recno mix.
		 */
		if (key.size < 2 || 8 != val.size) 
			break;
		if ( ! btree_read(&key, mc, &buf))
			break;

		mask = *(int *)val.data;

		/*
		 * See if this keyword record matches any of the
		 * expressions we have stored.
		 */
		if ( ! exprexecpre(expr, buf, mask))
			continue;

		memcpy(&rec, val.data + 4, sizeof(recno_t));

		/*
		 * O(log n) scan for prior records.  Since a record
		 * number is unbounded, this has decent performance over
		 * a complex hash function.
		 */

		for (leaf = root; leaf >= 0; )
			if (rec > recs[leaf].rec && recs[leaf].rhs >= 0)
				leaf = recs[leaf].rhs;
			else if (rec < recs[leaf].rec && recs[leaf].lhs >= 0)
				leaf = recs[leaf].lhs;
			else 
				break;

		if (leaf >= 0 && recs[leaf].rec == rec) {
			if (0 == recs[leaf].matches[0])
				exprexecpost
					(expr, buf, mask, 
					 recs[leaf].matches, terms);
			continue;
		}

		/*
		 * Now we actually extract the manpage's metadata from
		 * the index database.
		 */

		key.data = &rec;
		key.size = sizeof(recno_t);

		if (0 != (*idx->get)(idx, &key, &val, 0))
			break;

		srec.lhs = srec.rhs = -1;
		if ( ! index_read(&key, &val, mc, &srec))
			break;

		if (opts->cat && strcasecmp(opts->cat, srec.cat))
			continue;
		if (opts->arch && strcasecmp(opts->arch, srec.arch))
			continue;

		tree->node = recs = mandoc_realloc
			(recs, (tree->len + 1) * sizeof(struct rec));

		memcpy(&recs[tree->len], &srec, sizeof(struct rec));
		recs[tree->len].matches = 
			mandoc_calloc(terms + 1, sizeof(int));

		exprexecpost
			(expr, buf, mask, 
			 recs[tree->len].matches, terms);

		/* Append to our tree. */

		if (leaf >= 0) {
			if (rec > recs[leaf].rec)
				recs[leaf].rhs = tree->len;
			else
				recs[leaf].lhs = tree->len;
		} else
			root = tree->len;
		
		memset(&srec, 0, sizeof(struct rec));
		tree->len++;
	}

	/* XXX handle database errors? */

out:
	recfree(&srec);

	if (btree)
		(*btree->close)(btree);
	if (idx)
		(*idx->close)(idx);

	free(buf);
}

static void
recfree(struct rec *rec)
{

	free(rec->file);
	free(rec->matches);
	free(rec->cat);
	free(rec->title);
	free(rec->arch);
	free(rec->desc);
}

struct expr *
exprcomp(int argc, char *argv[], size_t *tt)
{
	struct expr	*e, *first, *next;
	int		 pos, log;

	first = next = NULL;
	(*tt) = 0;

	for (pos = 0; pos < argc; pos++) {
		e = next;
		log = 0;

		if (0 == strcmp("-a", argv[pos]))
			log = 1;			
		else if (0 == strcmp("-o", argv[pos]))
			log = 2;

		if (log > 0 && ++pos >= argc)
			goto err;

		if (0 == strcmp("-i", argv[pos])) {
			if (++pos >= argc)
				goto err;
			next = exprterm(argv[pos], 1, log == 1);
		} else
			next = exprterm(argv[pos], 0, log == 1);

		if (NULL == next)
			goto err;

		next->index = (int)(*tt)++;

		if (NULL == first) {
			assert(NULL == e);
			first = next;
		} else {
			assert(NULL != e);
			e->next = next;
		}
	}

	return(first);
err:
	exprfree(first);
	return(NULL);
}

static struct expr *
exprterm(char *buf, int cs, int and)
{
	struct expr	 e;
	struct expr	*p;
	char		*key;
	int		 i;

	memset(&e, 0, sizeof(struct expr));

	e.and = and;
 
 	/*
 	 * Choose regex or substring match.
	 */

	if (NULL == (e.v = strpbrk(buf, "=~"))) {
		e.regex = 0;
		e.v = buf;
	} else {
		e.regex = '~' == *e.v;
		*e.v++ = '\0';
	}

	/*
	 * Determine the record types to search for.
	 */

	e.mask = 0;
	if (buf < e.v) {
		while (NULL != (key = strsep(&buf, ","))) {
			i = 0;
			while (types[i].mask &&
			    strcmp(types[i].name, key))
				i++;
			e.mask |= types[i].mask;
		}
	}
	if (0 == e.mask)
		e.mask = TYPE_Nm | TYPE_Nd;

	if (e.regex) {
		i = REG_EXTENDED | REG_NOSUB | cs ? REG_ICASE : 0;
		if (regcomp(&e.re, e.v, i))
			return(NULL);
	}

	e.v = mandoc_strdup(e.v);

	p = mandoc_calloc(1, sizeof(struct expr));
	memcpy(p, &e, sizeof(struct expr));
	return(p);
}

void
exprfree(struct expr *p)
{
	struct expr	*pp;
	
	while (NULL != p) {
		if (p->regex)
			regfree(&p->re);
		free(p->v);
		pp = p->next;
		free(p);
		p = pp;
	}
}

/*
 * See if this expression evaluates to true for any terms.
 * Return 1 if any expression evaluates to true, else 0.
 */
static int
exprexecpre(const struct expr *p, const char *cp, int mask)
{

	for ( ; NULL != p; p = p->next) {
		if ( ! (mask & p->mask))
			continue;
		if (p->regex) {
			if (0 == regexec(&p->re, cp, 0, NULL, 0))
				return(1);
		} else if (NULL != strcasestr(cp, p->v))
			return(1);
	}
	return(0);
}

/*
 * First, update the array of terms for which this expression evaluates
 * to true.
 * Second, logically evaluate all terms over the updated array of truth
 * values.
 * If this evaluates to true, mark the expression as satisfied.
 */
static void
exprexecpost(const struct expr *e, const char *cp, 
		int mask, int *matches, size_t matchsz)
{
	const struct expr *p;
	int		   match;

	assert(0 == matches[0]);

	for (p = e; p; p = p->next) {
		if ( ! (mask & p->mask))
			continue;
		if (p->regex) {
			if (regexec(&p->re, cp, 0, NULL, 0))
				continue;
		} else if (NULL == strcasestr(cp, p->v))
			continue;

		matches[p->index + 1] = 1;
	}

	for (match = 0, p = e; p && ! match; p = p->next) {
		match = matches[p->index + 1];
		for ( ; p->next && p->next->and; p = p->next)
			match = match && matches[p->next->index + 1];
	}

	matches[0] = match;
}
