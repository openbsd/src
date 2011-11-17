/*	$Id: mandocdb.c,v 1.7 2011/11/17 14:52:32 schwarze Exp $ */
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
#include <sys/param.h>

#include <assert.h>
#include <dirent.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <db.h>

#include "man.h"
#include "mdoc.h"
#include "mandoc.h"
#include "mandocdb.h"
#include "man_conf.h"

#define	MANDOC_BUFSZ	  BUFSIZ
#define	MANDOC_SLOP	  1024

/* Tiny list for files.  No need to bring in QUEUE. */

struct	of {
	char		 *fname; /* heap-allocated */
	char		 *sec;
	char		 *arch;
	char		 *title;
	struct of	 *next; /* NULL for last one */
	struct of	 *first; /* first in list */
};

/* Buffer for storing growable data. */

struct	buf {
	char		 *cp;
	size_t		  len; /* current length */
	size_t		  size; /* total buffer size */
};

/* Operation we're going to perform. */

enum	op {
	OP_NEW = 0, /* new database */
	OP_UPDATE, /* delete/add entries in existing database */
	OP_DELETE /* delete entries from existing database */
};

#define	MAN_ARGS	  DB *hash, \
			  struct buf *buf, \
			  struct buf *dbuf, \
			  const struct man_node *n
#define	MDOC_ARGS	  DB *hash, \
			  struct buf *buf, \
			  struct buf *dbuf, \
			  const struct mdoc_node *n, \
			  const struct mdoc_meta *m

static	void		  buf_appendmdoc(struct buf *, 
				const struct mdoc_node *, int);
static	void		  buf_append(struct buf *, const char *);
static	void		  buf_appendb(struct buf *, 
				const void *, size_t);
static	void		  dbt_put(DB *, const char *, DBT *, DBT *);
static	void		  hash_put(DB *, const struct buf *, int);
static	void		  hash_reset(DB **);
static	void		  index_merge(const struct of *, struct mparse *,
				struct buf *, struct buf *,
				DB *, DB *, const char *, 
				DB *, const char *, int, int,
				recno_t, const recno_t *, size_t);
static	void		  index_prune(const struct of *, DB *, 
				const char *, DB *, const char *, 
				int, recno_t *, recno_t **, size_t *);
static	void		  ofile_argbuild(char *[], int, int, int,
				struct of **);
static	int		  ofile_dirbuild(const char *, const char *,
				const char *, int, int, struct of **);
static	void		  ofile_free(struct of *);
static	int		  pman_node(MAN_ARGS);
static	void		  pmdoc_node(MDOC_ARGS);
static	void		  pmdoc_An(MDOC_ARGS);
static	void		  pmdoc_Cd(MDOC_ARGS);
static	void		  pmdoc_Er(MDOC_ARGS);
static	void		  pmdoc_Ev(MDOC_ARGS);
static	void		  pmdoc_Fd(MDOC_ARGS);
static	void		  pmdoc_In(MDOC_ARGS);
static	void		  pmdoc_Fn(MDOC_ARGS);
static	void		  pmdoc_Fo(MDOC_ARGS);
static	void		  pmdoc_Nd(MDOC_ARGS);
static	void		  pmdoc_Nm(MDOC_ARGS);
static	void		  pmdoc_Pa(MDOC_ARGS);
static	void		  pmdoc_St(MDOC_ARGS);
static	void		  pmdoc_Vt(MDOC_ARGS);
static	void		  pmdoc_Xr(MDOC_ARGS);
static	void		  usage(void);

typedef	void		(*pmdoc_nf)(MDOC_ARGS);

static	const pmdoc_nf	  mdocs[MDOC_MAX] = {
	NULL, /* Ap */
	NULL, /* Dd */
	NULL, /* Dt */
	NULL, /* Os */
	NULL, /* Sh */ 
	NULL, /* Ss */ 
	NULL, /* Pp */ 
	NULL, /* D1 */
	NULL, /* Dl */
	NULL, /* Bd */
	NULL, /* Ed */
	NULL, /* Bl */ 
	NULL, /* El */
	NULL, /* It */
	NULL, /* Ad */ 
	pmdoc_An, /* An */ 
	NULL, /* Ar */
	pmdoc_Cd, /* Cd */ 
	NULL, /* Cm */
	NULL, /* Dv */ 
	pmdoc_Er, /* Er */ 
	pmdoc_Ev, /* Ev */ 
	NULL, /* Ex */ 
	NULL, /* Fa */ 
	pmdoc_Fd, /* Fd */
	NULL, /* Fl */
	pmdoc_Fn, /* Fn */ 
	NULL, /* Ft */ 
	NULL, /* Ic */ 
	pmdoc_In, /* In */ 
	NULL, /* Li */
	pmdoc_Nd, /* Nd */
	pmdoc_Nm, /* Nm */
	NULL, /* Op */
	NULL, /* Ot */
	pmdoc_Pa, /* Pa */
	NULL, /* Rv */
	pmdoc_St, /* St */ 
	pmdoc_Vt, /* Va */
	pmdoc_Vt, /* Vt */ 
	pmdoc_Xr, /* Xr */ 
	NULL, /* %A */
	NULL, /* %B */
	NULL, /* %D */
	NULL, /* %I */
	NULL, /* %J */
	NULL, /* %N */
	NULL, /* %O */
	NULL, /* %P */
	NULL, /* %R */
	NULL, /* %T */
	NULL, /* %V */
	NULL, /* Ac */
	NULL, /* Ao */
	NULL, /* Aq */
	NULL, /* At */ 
	NULL, /* Bc */
	NULL, /* Bf */
	NULL, /* Bo */
	NULL, /* Bq */
	NULL, /* Bsx */
	NULL, /* Bx */
	NULL, /* Db */
	NULL, /* Dc */
	NULL, /* Do */
	NULL, /* Dq */
	NULL, /* Ec */
	NULL, /* Ef */ 
	NULL, /* Em */ 
	NULL, /* Eo */
	NULL, /* Fx */
	NULL, /* Ms */ 
	NULL, /* No */
	NULL, /* Ns */
	NULL, /* Nx */
	NULL, /* Ox */
	NULL, /* Pc */
	NULL, /* Pf */
	NULL, /* Po */
	NULL, /* Pq */
	NULL, /* Qc */
	NULL, /* Ql */
	NULL, /* Qo */
	NULL, /* Qq */
	NULL, /* Re */
	NULL, /* Rs */
	NULL, /* Sc */
	NULL, /* So */
	NULL, /* Sq */
	NULL, /* Sm */ 
	NULL, /* Sx */
	NULL, /* Sy */
	NULL, /* Tn */
	NULL, /* Ux */
	NULL, /* Xc */
	NULL, /* Xo */
	pmdoc_Fo, /* Fo */ 
	NULL, /* Fc */ 
	NULL, /* Oo */
	NULL, /* Oc */
	NULL, /* Bk */
	NULL, /* Ek */
	NULL, /* Bt */
	NULL, /* Hf */
	NULL, /* Fr */
	NULL, /* Ud */
	NULL, /* Lb */
	NULL, /* Lp */ 
	NULL, /* Lk */ 
	NULL, /* Mt */ 
	NULL, /* Brq */ 
	NULL, /* Bro */ 
	NULL, /* Brc */ 
	NULL, /* %C */
	NULL, /* Es */
	NULL, /* En */
	NULL, /* Dx */
	NULL, /* %Q */
	NULL, /* br */
	NULL, /* sp */
	NULL, /* %U */
	NULL, /* Ta */
};

static	const char	 *progname;

int
mandocdb(int argc, char *argv[])
{
	struct man_conf	 dirs;
	struct mparse	*mp; /* parse sequence */
	enum op		 op; /* current operation */
	const char	*dir;
	char		 ibuf[MAXPATHLEN], /* index fname */
			 fbuf[MAXPATHLEN];  /* btree fname */
	int		 verb, /* output verbosity */
			 use_all, /* use all directories and files */
			 ch, i, flags;
	DB		*idx, /* index database */
			*db, /* keyword database */
			*hash; /* temporary keyword hashtable */
	BTREEINFO	 info; /* btree configuration */
	recno_t		 maxrec; /* supremum of all records */
	recno_t		*recs; /* buffer of empty records */
	size_t		 sz1, sz2,
			 recsz, /* buffer size of recs */
			 reccur; /* valid number of recs */
	struct buf	 buf, /* keyword buffer */
			 dbuf; /* description buffer */
	struct of	*of; /* list of files for processing */
	extern int	 optind;
	extern char	*optarg;

	progname = strrchr(argv[0], '/');
	if (progname == NULL)
		progname = argv[0];
	else
		++progname;

	verb = 0;
	use_all = 0;
	of = NULL;
	db = idx = NULL;
	mp = NULL;
	hash = NULL;
	recs = NULL;
	recsz = reccur = 0;
	maxrec = 0;
	op = OP_NEW;
	dir = NULL;

	while (-1 != (ch = getopt(argc, argv, "ad:u:v")))
		switch (ch) {
		case ('a'):
			use_all = 1;
			break;
		case ('d'):
			dir = optarg;
			op = OP_UPDATE;
			break;
		case ('u'):
			dir = optarg;
			op = OP_DELETE;
			break;
		case ('v'):
			verb++;
			break;
		default:
			usage();
			return((int)MANDOCLEVEL_BADARG);
		}

	argc -= optind;
	argv += optind;

	memset(&dirs, 0, sizeof(struct man_conf));
	memset(&info, 0, sizeof(BTREEINFO));
	info.flags = R_DUP;

	mp = mparse_alloc(MPARSE_AUTO, MANDOCLEVEL_FATAL, NULL, NULL);

	memset(&buf, 0, sizeof(struct buf));
	memset(&dbuf, 0, sizeof(struct buf));

	buf.size = dbuf.size = MANDOC_BUFSZ;

	buf.cp = mandoc_malloc(buf.size);
	dbuf.cp = mandoc_malloc(dbuf.size);

	flags = OP_NEW == op ? O_CREAT|O_TRUNC|O_RDWR : O_CREAT|O_RDWR;

	if (OP_UPDATE == op || OP_DELETE == op) {
		ibuf[0] = fbuf[0] = '\0';

		strlcat(fbuf, dir, MAXPATHLEN);
		strlcat(fbuf, "/", MAXPATHLEN);
		sz1 = strlcat(fbuf, MANDOC_DB, MAXPATHLEN);

		strlcat(ibuf, dir, MAXPATHLEN);
		strlcat(ibuf, "/", MAXPATHLEN);
		sz2 = strlcat(ibuf, MANDOC_IDX, MAXPATHLEN);

		if (sz1 >= MAXPATHLEN || sz2 >= MAXPATHLEN) {
			fprintf(stderr, "%s: Path too long\n", dir);
			exit((int)MANDOCLEVEL_BADARG);
		}

		db = dbopen(fbuf, flags, 0644, DB_BTREE, &info);
		idx = dbopen(ibuf, flags, 0644, DB_RECNO, NULL);

		if (NULL == db) {
			perror(fbuf);
			exit((int)MANDOCLEVEL_SYSERR);
		} else if (NULL == db) {
			perror(ibuf);
			exit((int)MANDOCLEVEL_SYSERR);
		}

		if (verb > 2) {
			printf("%s: Opened\n", fbuf);
			printf("%s: Opened\n", ibuf);
		}

		ofile_argbuild(argv, argc, use_all, verb, &of);
		if (NULL == of)
			goto out;

		of = of->first;

		index_prune(of, db, fbuf, idx, ibuf, verb,
				&maxrec, &recs, &recsz);

		if (OP_UPDATE == op) {
			chdir(dir);
			index_merge(of, mp, &dbuf, &buf, hash, 
					db, fbuf, idx, ibuf, use_all,
					verb, maxrec, recs, reccur);
		}

		goto out;
	}

	if (0 == argc) {
		man_conf_parse(&dirs);
		argc = dirs.argc;
		argv = dirs.argv;
	}

	for (i = 0; i < argc; i++) {
		ibuf[0] = fbuf[0] = '\0';

		strlcat(fbuf, argv[i], MAXPATHLEN);
		strlcat(fbuf, "/", MAXPATHLEN);
		sz1 = strlcat(fbuf, MANDOC_DB, MAXPATHLEN);

		strlcat(ibuf, argv[i], MAXPATHLEN);
		strlcat(ibuf, "/", MAXPATHLEN);
		sz2 = strlcat(ibuf, MANDOC_IDX, MAXPATHLEN);

		if (sz1 >= MAXPATHLEN || sz2 >= MAXPATHLEN) {
			fprintf(stderr, "%s: Path too long\n", argv[i]);
			exit((int)MANDOCLEVEL_BADARG);
		}

		if (db)
			(*db->close)(db);
		if (idx)
			(*idx->close)(idx);

		db = dbopen(fbuf, flags, 0644, DB_BTREE, &info);
		idx = dbopen(ibuf, flags, 0644, DB_RECNO, NULL);

		if (NULL == db) {
			perror(fbuf);
			exit((int)MANDOCLEVEL_SYSERR);
		} else if (NULL == db) {
			perror(ibuf);
			exit((int)MANDOCLEVEL_SYSERR);
		}

		if (verb > 2) {
			printf("%s: Truncated\n", fbuf);
			printf("%s: Truncated\n", ibuf);
		}

		ofile_free(of);
		of = NULL;

		if ( ! ofile_dirbuild(argv[i], NULL, NULL,
				use_all, verb, &of)) 
			exit((int)MANDOCLEVEL_SYSERR);

		if (NULL == of)
			continue;

		of = of->first;

		chdir(argv[i]);
		index_merge(of, mp, &dbuf, &buf, hash, db, fbuf, 
				idx, ibuf, use_all, verb,
				maxrec, recs, reccur);
	}

out:
	if (db)
		(*db->close)(db);
	if (idx)
		(*idx->close)(idx);
	if (hash)
		(*hash->close)(hash);
	if (mp)
		mparse_free(mp);
	if (dirs.argc)
		man_conf_free(&dirs);

	ofile_free(of);
	free(buf.cp);
	free(dbuf.cp);
	free(recs);

	return(MANDOCLEVEL_OK);
}

void
index_merge(const struct of *of, struct mparse *mp,
		struct buf *dbuf, struct buf *buf,
		DB *hash, DB *db, const char *dbf, 
		DB *idx, const char *idxf, int use_all, int verb,
		recno_t maxrec, const recno_t *recs, size_t reccur)
{
	recno_t		 rec;
	int		 ch;
	DBT		 key, val;
	struct mdoc	*mdoc;
	struct man	*man;
	const char	*fn, *msec, *mtitle, *arch;
	size_t		 sv;
	unsigned	 seq;
	char		 vbuf[8];

	for (rec = 0; of; of = of->next) {
		fn = of->fname;
		if (reccur > 0) {
			--reccur;
			rec = recs[(int)reccur];
		} else if (maxrec > 0) {
			rec = maxrec;
			maxrec = 0;
		} else
			rec++;

		mparse_reset(mp);
		hash_reset(&hash);

		if (mparse_readfd(mp, -1, fn) >= MANDOCLEVEL_FATAL) {
			fprintf(stderr, "%s: Parse failure\n", fn);
			continue;
		}

		mparse_result(mp, &mdoc, &man);
		if (NULL == mdoc && NULL == man)
			continue;

		/*
		 * Make sure the manual section and architecture
		 * agree with the directory where the file is located
		 * or man(1) will not be able to find it.
		 */

		msec = NULL != mdoc ? 
			mdoc_meta(mdoc)->msec : man_meta(man)->msec;
		arch = NULL != mdoc ? 
			mdoc_meta(mdoc)->arch : NULL;

		if (0 == use_all) {
			assert(of->sec);
			assert(msec);
			if (strcmp(msec, of->sec))
				continue;

			if (NULL == arch) {
				if (NULL != of->arch)
					continue;
			} else if (NULL == of->arch ||
					strcmp(arch, of->arch))
				continue;
		}

		if (NULL == arch)
			arch = "";

		/* 
		 * Case is relevant for man(1), so use the file name
		 * instead of the (usually) all caps page title,
		 * if the two agree.
		 */

		mtitle = NULL != mdoc ? 
			mdoc_meta(mdoc)->title : man_meta(man)->title;

		assert(of->title);
		assert(mtitle);

		if (0 == strcasecmp(mtitle, of->title))
			mtitle = of->title;
		else if (0 == use_all)
			continue;

		/* 
		 * The index record value consists of a nil-terminated
		 * filename, a nil-terminated manual section, and a
		 * nil-terminated description.  Since the description
		 * may not be set, we set a sentinel to see if we're
		 * going to write a nil byte in its place.
		 */

		dbuf->len = 0;
		buf_appendb(dbuf, fn, strlen(fn) + 1);
		buf_appendb(dbuf, msec, strlen(msec) + 1);
		buf_appendb(dbuf, mtitle, strlen(mtitle) + 1);
		buf_appendb(dbuf, arch, strlen(arch) + 1);

		sv = dbuf->len;

		/* Fix the record number in the btree value. */

		if (mdoc)
			pmdoc_node(hash, buf, dbuf,
				mdoc_node(mdoc), mdoc_meta(mdoc));
		else 
			pman_node(hash, buf, dbuf, man_node(man));

		/*
		 * Copy from the in-memory hashtable of pending keywords
		 * into the database.
		 */
		
		memset(vbuf, 0, sizeof(uint32_t));
		memcpy(vbuf + 4, &rec, sizeof(uint32_t));

		seq = R_FIRST;
		while (0 == (ch = (*hash->seq)(hash, &key, &val, seq))) {
			seq = R_NEXT;

			memcpy(vbuf, val.data, sizeof(uint32_t));
			val.size = sizeof(vbuf);
			val.data = vbuf;

			if (verb > 1)
				printf("%s: Added keyword: %s\n", 
						fn, (char *)key.data);
			dbt_put(db, dbf, &key, &val);
		}
		if (ch < 0) {
			perror("hash");
			exit((int)MANDOCLEVEL_SYSERR);
		}
		
		/*
		 * Apply to the index.  If we haven't had a description
		 * set, put an empty one in now.
		 */

		if (dbuf->len == sv)
			buf_appendb(dbuf, "", 1);

		key.data = &rec;
		key.size = sizeof(recno_t);

		val.data = dbuf->cp;
		val.size = dbuf->len;

		if (verb)
			printf("%s: Added index\n", fn);
		dbt_put(idx, idxf, &key, &val);
	}
}

/*
 * Scan through all entries in the index file `idx' and prune those
 * entries in `ofile'.
 * Pruning consists of removing from `db', then invalidating the entry
 * in `idx' (zeroing its value size).
 */
static void
index_prune(const struct of *ofile, DB *db, const char *dbf, 
		DB *idx, const char *idxf, int verb,
		recno_t *maxrec, recno_t **recs, size_t *recsz)
{
	const struct of	*of;
	const char	*fn;
	unsigned	 seq, sseq;
	DBT		 key, val;
	size_t		 reccur;
	int		 ch;

	reccur = 0;
	seq = R_FIRST;
	while (0 == (ch = (*idx->seq)(idx, &key, &val, seq))) {
		seq = R_NEXT;
		*maxrec = *(recno_t *)key.data;
		if (0 == val.size) {
			if (reccur >= *recsz) {
				*recsz += MANDOC_SLOP;
				*recs = mandoc_realloc(*recs, 
					*recsz * sizeof(recno_t));
			}
			(*recs)[(int)reccur] = *maxrec;
			reccur++;
			continue;
		}

		fn = (char *)val.data;
		for (of = ofile; of; of = of->next)
			if (0 == strcmp(fn, of->fname))
				break;

		if (NULL == of)
			continue;

		sseq = R_FIRST;
		while (0 == (ch = (*db->seq)(db, &key, &val, sseq))) {
			sseq = R_NEXT;
			assert(8 == val.size);
			if (*maxrec != *(recno_t *)(val.data + 4))
				continue;
			if (verb)
				printf("%s: Deleted keyword: %s\n", 
						fn, (char *)key.data);
			ch = (*db->del)(db, &key, R_CURSOR);
			if (ch < 0)
				break;
		}
		if (ch < 0) {
			perror(dbf);
			exit((int)MANDOCLEVEL_SYSERR);
		}

		if (verb)
			printf("%s: Deleted index\n", fn);

		val.size = 0;
		ch = (*idx->put)(idx, &key, &val, R_CURSOR);
		if (ch < 0) {
			perror(idxf);
			exit((int)MANDOCLEVEL_SYSERR);
		}

		if (reccur >= *recsz) {
			*recsz += MANDOC_SLOP;
			*recs = mandoc_realloc
				(*recs, *recsz * sizeof(recno_t));
		}

		(*recs)[(int)reccur] = *maxrec;
		reccur++;
	}
	(*maxrec)++;
}

/*
 * Grow the buffer (if necessary) and copy in a binary string.
 */
static void
buf_appendb(struct buf *buf, const void *cp, size_t sz)
{

	/* Overshoot by MANDOC_BUFSZ. */

	while (buf->len + sz >= buf->size) {
		buf->size = buf->len + sz + MANDOC_BUFSZ;
		buf->cp = mandoc_realloc(buf->cp, buf->size);
	}

	memcpy(buf->cp + (int)buf->len, cp, sz);
	buf->len += sz;
}

/*
 * Append a nil-terminated string to the buffer.  
 * This can be invoked multiple times.  
 * The buffer string will be nil-terminated.
 * If invoked multiple times, a space is put between strings.
 */
static void
buf_append(struct buf *buf, const char *cp)
{
	size_t		 sz;

	if (0 == (sz = strlen(cp)))
		return;

	if (buf->len)
		buf->cp[(int)buf->len - 1] = ' ';

	buf_appendb(buf, cp, sz + 1);
}

/*
 * Recursively add all text from a given node.  
 * This is optimised for general mdoc nodes in this context, which do
 * not consist of subexpressions and having a recursive call for n->next
 * would be wasteful.
 * The "f" variable should be 0 unless called from pmdoc_Nd for the
 * description buffer, which does not start at the beginning of the
 * buffer.
 */
static void
buf_appendmdoc(struct buf *buf, const struct mdoc_node *n, int f)
{

	for ( ; n; n = n->next) {
		if (n->child)
			buf_appendmdoc(buf, n->child, f);

		if (MDOC_TEXT == n->type && f) {
			f = 0;
			buf_appendb(buf, n->string, 
					strlen(n->string) + 1);
		} else if (MDOC_TEXT == n->type)
			buf_append(buf, n->string);

	}
}

/* ARGSUSED */
static void
pmdoc_An(MDOC_ARGS)
{
	
	if (SEC_AUTHORS != n->sec)
		return;

	buf_appendmdoc(buf, n->child, 0);
	hash_put(hash, buf, TYPE_An);
}

static void
hash_reset(DB **db)
{
	DB		*hash;

	if (NULL != (hash = *db))
		(*hash->close)(hash);

	*db = dbopen(NULL, O_CREAT|O_RDWR, 0644, DB_HASH, NULL);
	if (NULL == *db) {
		perror("hash");
		exit((int)MANDOCLEVEL_SYSERR);
	}
}

/* ARGSUSED */
static void
pmdoc_Fd(MDOC_ARGS)
{
	const char	*start, *end;
	size_t		 sz;
	
	if (SEC_SYNOPSIS != n->sec)
		return;
	if (NULL == (n = n->child) || MDOC_TEXT != n->type)
		return;

	/*
	 * Only consider those `Fd' macro fields that begin with an
	 * "inclusion" token (versus, e.g., #define).
	 */
	if (strcmp("#include", n->string))
		return;

	if (NULL == (n = n->next) || MDOC_TEXT != n->type)
		return;

	/*
	 * Strip away the enclosing angle brackets and make sure we're
	 * not zero-length.
	 */

	start = n->string;
	if ('<' == *start || '"' == *start)
		start++;

	if (0 == (sz = strlen(start)))
		return;

	end = &start[(int)sz - 1];
	if ('>' == *end || '"' == *end)
		end--;

	assert(end >= start);

	buf_appendb(buf, start, (size_t)(end - start + 1));
	buf_appendb(buf, "", 1);

	hash_put(hash, buf, TYPE_In);
}

/* ARGSUSED */
static void
pmdoc_Cd(MDOC_ARGS)
{
	
	if (SEC_SYNOPSIS != n->sec)
		return;

	buf_appendmdoc(buf, n->child, 0);
	hash_put(hash, buf, TYPE_Cd);
}

/* ARGSUSED */
static void
pmdoc_In(MDOC_ARGS)
{
	
	if (SEC_SYNOPSIS != n->sec)
		return;
	if (NULL == n->child || MDOC_TEXT != n->child->type)
		return;

	buf_append(buf, n->child->string);
	hash_put(hash, buf, TYPE_In);
}

/* ARGSUSED */
static void
pmdoc_Fn(MDOC_ARGS)
{
	const char	*cp;
	
	if (SEC_SYNOPSIS != n->sec)
		return;
	if (NULL == n->child || MDOC_TEXT != n->child->type)
		return;

	/* .Fn "struct type *arg" "foo" */

	cp = strrchr(n->child->string, ' ');
	if (NULL == cp)
		cp = n->child->string;

	/* Strip away pointer symbol. */

	while ('*' == *cp)
		cp++;

	buf_append(buf, cp);
	hash_put(hash, buf, TYPE_Fn);
}

/* ARGSUSED */
static void
pmdoc_St(MDOC_ARGS)
{
	
	if (SEC_STANDARDS != n->sec)
		return;
	if (NULL == n->child || MDOC_TEXT != n->child->type)
		return;

	buf_append(buf, n->child->string);
	hash_put(hash, buf, TYPE_St);
}

/* ARGSUSED */
static void
pmdoc_Xr(MDOC_ARGS)
{

	if (NULL == (n = n->child))
		return;

	buf_appendb(buf, n->string, strlen(n->string));

	if (NULL != (n = n->next)) {
		buf_appendb(buf, ".", 1);
		buf_appendb(buf, n->string, strlen(n->string) + 1);
	} else
		buf_appendb(buf, ".", 2);

	hash_put(hash, buf, TYPE_Xr);
}

/* ARGSUSED */
static void
pmdoc_Vt(MDOC_ARGS)
{
	const char	*start;
	size_t		 sz;
	
	if (SEC_SYNOPSIS != n->sec)
		return;
	if (MDOC_Vt == n->tok && MDOC_BODY != n->type)
		return;
	if (NULL == n->last || MDOC_TEXT != n->last->type)
		return;

	/*
	 * Strip away leading pointer symbol '*' and trailing ';'.
	 */

	start = n->last->string;

	while ('*' == *start)
		start++;

	if (0 == (sz = strlen(start)))
		return;

	if (';' == start[(int)sz - 1])
		sz--;

	if (0 == sz)
		return;

	buf_appendb(buf, start, sz);
	buf_appendb(buf, "", 1);
	hash_put(hash, buf, TYPE_Va);
}

/* ARGSUSED */
static void
pmdoc_Fo(MDOC_ARGS)
{
	
	if (SEC_SYNOPSIS != n->sec || MDOC_HEAD != n->type)
		return;
	if (NULL == n->child || MDOC_TEXT != n->child->type)
		return;

	buf_append(buf, n->child->string);
	hash_put(hash, buf, TYPE_Fn);
}


/* ARGSUSED */
static void
pmdoc_Nd(MDOC_ARGS)
{

	if (MDOC_BODY != n->type)
		return;

	buf_appendmdoc(dbuf, n->child, 1);
	buf_appendmdoc(buf, n->child, 0);

	hash_put(hash, buf, TYPE_Nd);
}

/* ARGSUSED */
static void
pmdoc_Er(MDOC_ARGS)
{

	if (SEC_ERRORS != n->sec)
		return;
	
	buf_appendmdoc(buf, n->child, 0);
	hash_put(hash, buf, TYPE_Er);
}

/* ARGSUSED */
static void
pmdoc_Ev(MDOC_ARGS)
{

	if (SEC_ENVIRONMENT != n->sec)
		return;
	
	buf_appendmdoc(buf, n->child, 0);
	hash_put(hash, buf, TYPE_Ev);
}

/* ARGSUSED */
static void
pmdoc_Pa(MDOC_ARGS)
{

	if (SEC_FILES != n->sec)
		return;
	
	buf_appendmdoc(buf, n->child, 0);
	hash_put(hash, buf, TYPE_Pa);
}

/* ARGSUSED */
static void
pmdoc_Nm(MDOC_ARGS)
{
	
	if (SEC_NAME == n->sec) {
		buf_appendmdoc(buf, n->child, 0);
		hash_put(hash, buf, TYPE_Nm);
		return;
	} else if (SEC_SYNOPSIS != n->sec || MDOC_HEAD != n->type)
		return;

	if (NULL == n->child)
		buf_append(buf, m->name);

	buf_appendmdoc(buf, n->child, 0);
	hash_put(hash, buf, TYPE_Nm);
}

static void
hash_put(DB *db, const struct buf *buf, int mask)
{
	DBT		 key, val;
	int		 rc;

	if (buf->len < 2)
		return;

	key.data = buf->cp;
	key.size = buf->len;

	if ((rc = (*db->get)(db, &key, &val, 0)) < 0) {
		perror("hash");
		exit((int)MANDOCLEVEL_SYSERR);
	} else if (0 == rc)
		mask |= *(int *)val.data;

	val.data = &mask;
	val.size = sizeof(int); 

	if ((rc = (*db->put)(db, &key, &val, 0)) < 0) {
		perror("hash");
		exit((int)MANDOCLEVEL_SYSERR);
	} 
}

static void
dbt_put(DB *db, const char *dbn, DBT *key, DBT *val)
{

	assert(key->size);
	assert(val->size);

	if (0 == (*db->put)(db, key, val, 0))
		return;
	
	perror(dbn);
	exit((int)MANDOCLEVEL_SYSERR);
	/* NOTREACHED */
}

/*
 * Call out to per-macro handlers after clearing the persistent database
 * key.  If the macro sets the database key, flush it to the database.
 */
static void
pmdoc_node(MDOC_ARGS)
{

	if (NULL == n)
		return;

	switch (n->type) {
	case (MDOC_HEAD):
		/* FALLTHROUGH */
	case (MDOC_BODY):
		/* FALLTHROUGH */
	case (MDOC_TAIL):
		/* FALLTHROUGH */
	case (MDOC_BLOCK):
		/* FALLTHROUGH */
	case (MDOC_ELEM):
		if (NULL == mdocs[n->tok])
			break;

		buf->len = 0;
		(*mdocs[n->tok])(hash, buf, dbuf, n, m);
		break;
	default:
		break;
	}

	pmdoc_node(hash, buf, dbuf, n->child, m);
	pmdoc_node(hash, buf, dbuf, n->next, m);
}

static int
pman_node(MAN_ARGS)
{
	const struct man_node *head, *body;
	const char	*start, *sv;
	size_t		 sz;

	if (NULL == n)
		return(0);

	/*
	 * We're only searching for one thing: the first text child in
	 * the BODY of a NAME section.  Since we don't keep track of
	 * sections in -man, run some hoops to find out whether we're in
	 * the correct section or not.
	 */

	if (MAN_BODY == n->type && MAN_SH == n->tok) {
		body = n;
		assert(body->parent);
		if (NULL != (head = body->parent->head) &&
				1 == head->nchild &&
				NULL != (head = (head->child)) &&
				MAN_TEXT == head->type &&
				0 == strcmp(head->string, "NAME") &&
				NULL != (body = body->child) &&
				MAN_TEXT == body->type) {

			assert(body->string);
			start = sv = body->string;

			/* 
			 * Go through a special heuristic dance here.
			 * This is why -man manuals are great!
			 * (I'm being sarcastic: my eyes are bleeding.)
			 * Conventionally, one or more manual names are
			 * comma-specified prior to a whitespace, then a
			 * dash, then a description.  Try to puzzle out
			 * the name parts here.
			 */

			for ( ;; ) {
				sz = strcspn(start, " ,");
				if ('\0' == start[(int)sz])
					break;

				buf->len = 0;
				buf_appendb(buf, start, sz);
				buf_appendb(buf, "", 1);

				hash_put(hash, buf, TYPE_Nm);

				if (' ' == start[(int)sz]) {
					start += (int)sz + 1;
					break;
				}

				assert(',' == start[(int)sz]);
				start += (int)sz + 1;
				while (' ' == *start)
					start++;
			}

			buf->len = 0;

			if (sv == start) {
				buf_append(buf, start);
				return(1);
			}

			while (' ' == *start)
				start++;

			if (0 == strncmp(start, "-", 1))
				start += 1;
			else if (0 == strncmp(start, "\\-", 2))
				start += 2;
			else if (0 == strncmp(start, "\\(en", 4))
				start += 4;
			else if (0 == strncmp(start, "\\(em", 4))
				start += 4;

			while (' ' == *start)
				start++;

			sz = strlen(start) + 1;
			buf_appendb(dbuf, start, sz);
			buf_appendb(buf, start, sz);

			hash_put(hash, buf, TYPE_Nd);
		}
	}

	for (n = n->child; n; n = n->next)
		if (pman_node(hash, buf, dbuf, n))
			return(1);

	return(0);
}

static void
ofile_argbuild(char *argv[], int argc, int use_all, int verb,
		struct of **of)
{
	char		 buf[MAXPATHLEN];
	char		*sec, *arch, *title, *p;
	int		 i;
	struct of	*nof;

	for (i = 0; i < argc; i++) {

		/*
		 * Analyze the path.
		 */

		if (strlcpy(buf, argv[i], sizeof(buf)) >= sizeof(buf)) {
			fprintf(stderr, "%s: Path too long\n", argv[i]);
			continue;
		}
		sec = arch = title = NULL;
		p = strrchr(buf, '\0');
		while (p-- > buf) {
			if (NULL == sec && '.' == *p) {
				sec = p + 1;
				*p = '\0';
				continue;
			}
			if ('/' != *p)
				continue;
			if (NULL == title) {
				title = p + 1;
				*p = '\0';
				continue;
			}
			if (strncmp("man", p + 1, 3))
				arch = p + 1;
			break;
		}
		if (NULL == title)
			title = buf;

		/*
		 * Build the file structure.
		 */

		nof = mandoc_calloc(1, sizeof(struct of));
		nof->fname = mandoc_strdup(argv[i]);
		if (NULL != sec)
			nof->sec = mandoc_strdup(sec);
		if (NULL != arch)
			nof->arch = mandoc_strdup(arch);
		nof->title = mandoc_strdup(title);

		/*
		 * Add the structure to the list.
		 */

		if (verb > 2) 
			printf("%s: Scheduling\n", argv[i]);
		if (NULL == *of) {
			*of = nof;
			(*of)->first = nof;
		} else {
			nof->first = (*of)->first;
			(*of)->next = nof;
			*of = nof;
		}
	}
}

/*
 * Recursively build up a list of files to parse.
 * We use this instead of ftw() and so on because I don't want global
 * variables hanging around.
 * This ignores the mandoc.db and mandoc.index files, but assumes that
 * everything else is a manual.
 * Pass in a pointer to a NULL structure for the first invocation.
 */
static int
ofile_dirbuild(const char *dir, const char* psec, const char *parch,
		int use_all, int verb, struct of **of)
{
	char		 buf[MAXPATHLEN];
	size_t		 sz;
	DIR		*d;
	const char	*fn, *sec, *arch;
	char		*suffix;
	struct of	*nof;
	struct dirent	*dp;

	if (NULL == (d = opendir(dir))) {
		perror(dir);
		return(0);
	}

	while (NULL != (dp = readdir(d))) {
		fn = dp->d_name;

		if ('.' == *fn)
			continue;

		if (DT_DIR == dp->d_type) {
			sec = psec;
			arch = parch;

			/*
	 		 * Don't bother parsing directories
			 * that man(1) won't find.
			 */

			if (NULL == sec) {
				if(0 == strncmp("man", fn, 3))
					sec = fn + 3;
				else if (use_all)
					sec = fn;
				else
					continue;
			} else if (NULL == arch && (use_all ||
					NULL == strchr(fn, '.')))
				arch = fn;
			else if (0 == use_all)
				continue;

			buf[0] = '\0';
			strlcat(buf, dir, MAXPATHLEN);
			strlcat(buf, "/", MAXPATHLEN);
			sz = strlcat(buf, fn, MAXPATHLEN);

			if (MAXPATHLEN <= sz) {
				fprintf(stderr, "%s: Path too long\n", dir);
				return(0);
			}
 
			if (verb > 2)
				printf("%s: Scanning\n", buf);

			if ( ! ofile_dirbuild(buf, sec, arch,
					use_all, verb, of))
				return(0);
		}
		if (DT_REG != dp->d_type ||
		    (NULL == psec && !use_all) ||
		    !strcmp(MANDOC_DB, fn) ||
		    !strcmp(MANDOC_IDX, fn))
			continue;

		/*
		 * Don't bother parsing files that man(1) won't find.
		 */

		suffix = strrchr(fn, '.');
		if (0 == use_all) {
			if (NULL == suffix)
				continue;
			if (strcmp(suffix + 1, psec))
				continue;
		}

		buf[0] = '\0';
		strlcat(buf, dir, MAXPATHLEN);
		strlcat(buf, "/", MAXPATHLEN);
		sz = strlcat(buf, fn, MAXPATHLEN);
		if (sz >= MAXPATHLEN) {
			fprintf(stderr, "%s: Path too long\n", dir);
			return(0);
		}

		nof = mandoc_calloc(1, sizeof(struct of));
		nof->fname = mandoc_strdup(buf);
		if (NULL != psec)
			nof->sec = mandoc_strdup(psec);
		if (NULL != parch)
			nof->arch = mandoc_strdup(parch);
		if (NULL != suffix)
			*suffix = '\0';
		nof->title = mandoc_strdup(fn);

		if (verb > 2)
			printf("%s: Scheduling\n", buf);

		if (NULL == *of) {
			*of = nof;
			(*of)->first = nof;
		} else {
			nof->first = (*of)->first;
			(*of)->next = nof;
			*of = nof;
		}
	}

	closedir(d);
	return(1);
}

static void
ofile_free(struct of *of)
{
	struct of	*nof;

	while (of) {
		nof = of->next;
		free(of->fname);
		free(of->sec);
		free(of->arch);
		free(of->title);
		free(of);
		of = nof;
	}
}

static void
usage(void)
{

	fprintf(stderr, "usage: %s [-v] "
			"[-d dir [files...] |"
			" -u dir [files...] |"
			" dir...]\n", progname);
}
