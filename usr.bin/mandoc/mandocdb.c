/*	$Id: mandocdb.c,v 1.72 2014/03/18 16:56:06 schwarze Exp $ */
/*
 * Copyright (c) 2011, 2012 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2011, 2012, 2013, 2014 Ingo Schwarze <schwarze@openbsd.org>
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
#include <sys/stat.h>
#include <sys/wait.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <fts.h>
#include <getopt.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <ohash.h>
#include <sqlite3.h>

#include "mdoc.h"
#include "man.h"
#include "mandoc.h"
#include "manpath.h"
#include "mansearch.h"

extern int mansearch_keymax;
extern const char *const mansearch_keynames[];

#define	SQL_EXEC(_v) \
	if (SQLITE_OK != sqlite3_exec(db, (_v), NULL, NULL, NULL)) \
		fprintf(stderr, "%s\n", sqlite3_errmsg(db))
#define	SQL_BIND_TEXT(_s, _i, _v) \
	if (SQLITE_OK != sqlite3_bind_text \
		((_s), (_i)++, (_v), -1, SQLITE_STATIC)) \
		fprintf(stderr, "%s\n", sqlite3_errmsg(db))
#define	SQL_BIND_INT(_s, _i, _v) \
	if (SQLITE_OK != sqlite3_bind_int \
		((_s), (_i)++, (_v))) \
		fprintf(stderr, "%s\n", sqlite3_errmsg(db))
#define	SQL_BIND_INT64(_s, _i, _v) \
	if (SQLITE_OK != sqlite3_bind_int64 \
		((_s), (_i)++, (_v))) \
		fprintf(stderr, "%s\n", sqlite3_errmsg(db))
#define SQL_STEP(_s) \
	if (SQLITE_DONE != sqlite3_step((_s))) \
		fprintf(stderr, "%s\n", sqlite3_errmsg(db))

enum	op {
	OP_DEFAULT = 0, /* new dbs from dir list or default config */
	OP_CONFFILE, /* new databases from custom config file */
	OP_UPDATE, /* delete/add entries in existing database */
	OP_DELETE, /* delete entries from existing database */
	OP_TEST /* change no databases, report potential problems */
};

enum	form {
	FORM_NONE,  /* format is unknown */
	FORM_SRC,   /* format is -man or -mdoc */
	FORM_CAT    /* format is cat */
};

struct	str {
	char		*rendered; /* key in UTF-8 or ASCII form */
	const struct mpage *mpage; /* if set, the owning parse */
	uint64_t	 mask; /* bitmask in sequence */
	char		 key[]; /* may contain escape sequences */
};

struct	inodev {
	ino_t		 st_ino;
	dev_t		 st_dev;
};

struct	mpage {
	struct inodev	 inodev;  /* used for hashing routine */
	enum form	 form;    /* format from file content */
	char		*sec;     /* section from file content */
	char		*arch;    /* architecture from file content */
	char		*title;   /* title from file content */
	char		*desc;    /* description from file content */
	struct mlink	*mlinks;  /* singly linked list */
};

struct	mlink {
	char		 file[PATH_MAX]; /* filename rel. to manpath */
	enum form	 dform;   /* format from directory */
	enum form	 fform;   /* format from file name suffix */
	char		*dsec;    /* section from directory */
	char		*arch;    /* architecture from directory */
	char		*name;    /* name from file name (not empty) */
	char		*fsec;    /* section from file name suffix */
	struct mlink	*next;    /* singly linked list */
};

enum	stmt {
	STMT_DELETE_PAGE = 0,	/* delete mpage */
	STMT_INSERT_PAGE,	/* insert mpage */
	STMT_INSERT_LINK,	/* insert mlink */
	STMT_INSERT_KEY,	/* insert parsed key */
	STMT__MAX
};

typedef	int (*mdoc_fp)(struct mpage *, const struct mdoc_node *);

struct	mdoc_handler {
	mdoc_fp		 fp; /* optional handler */
	uint64_t	 mask;  /* set unless handler returns 0 */
};

static	void	 dbclose(int);
static	void	 dbadd(const struct mpage *, struct mchars *);
static	int	 dbopen(int);
static	void	 dbprune(void);
static	void	 filescan(const char *);
static	void	*hash_alloc(size_t, void *);
static	void	 hash_free(void *, size_t, void *);
static	void	*hash_halloc(size_t, void *);
static	void	 mlink_add(struct mlink *, const struct stat *);
static	int	 mlink_check(struct mpage *, struct mlink *);
static	void	 mlink_free(struct mlink *);
static	void	 mlinks_undupe(struct mpage *);
static	void	 mpages_free(void);
static	void	 mpages_merge(struct mchars *, struct mparse *);
static	void	 parse_cat(struct mpage *);
static	void	 parse_man(struct mpage *, const struct man_node *);
static	void	 parse_mdoc(struct mpage *, const struct mdoc_node *);
static	int	 parse_mdoc_body(struct mpage *, const struct mdoc_node *);
static	int	 parse_mdoc_head(struct mpage *, const struct mdoc_node *);
static	int	 parse_mdoc_Fd(struct mpage *, const struct mdoc_node *);
static	int	 parse_mdoc_Fn(struct mpage *, const struct mdoc_node *);
static	int	 parse_mdoc_Nd(struct mpage *, const struct mdoc_node *);
static	int	 parse_mdoc_Nm(struct mpage *, const struct mdoc_node *);
static	int	 parse_mdoc_Sh(struct mpage *, const struct mdoc_node *);
static	int	 parse_mdoc_Xr(struct mpage *, const struct mdoc_node *);
static	void	 putkey(const struct mpage *, char *, uint64_t);
static	void	 putkeys(const struct mpage *,
			const char *, size_t, uint64_t);
static	void	 putmdockey(const struct mpage *,
			const struct mdoc_node *, uint64_t);
static	void	 render_key(struct mchars *, struct str *);
static	void	 say(const char *, const char *, ...);
static	int	 set_basedir(const char *);
static	int	 treescan(void);
static	size_t	 utf8(unsigned int, char [7]);

static	char		 tempfilename[32];
static	char		*progname;
static	int		 nodb; /* no database changes */
static	int		 quick; /* abort the parse early */
static	int	 	 use_all; /* use all found files */
static	int	  	 verb; /* print what we're doing */
static	int	  	 warnings; /* warn about crap */
static	int		 write_utf8; /* write UTF-8 output; else ASCII */
static	int		 exitcode; /* to be returned by main */
static	enum op	  	 op; /* operational mode */
static	char		 basedir[PATH_MAX]; /* current base directory */
static	struct ohash	 mpages; /* table of distinct manual pages */
static	struct ohash	 mlinks; /* table of directory entries */
static	struct ohash	 strings; /* table of all strings */
static	sqlite3		*db = NULL; /* current database */
static	sqlite3_stmt	*stmts[STMT__MAX]; /* current statements */

static	const struct mdoc_handler mdocs[MDOC_MAX] = {
	{ NULL, 0 },  /* Ap */
	{ NULL, 0 },  /* Dd */
	{ NULL, 0 },  /* Dt */
	{ NULL, 0 },  /* Os */
	{ parse_mdoc_Sh, TYPE_Sh }, /* Sh */
	{ parse_mdoc_head, TYPE_Ss }, /* Ss */
	{ NULL, 0 },  /* Pp */
	{ NULL, 0 },  /* D1 */
	{ NULL, 0 },  /* Dl */
	{ NULL, 0 },  /* Bd */
	{ NULL, 0 },  /* Ed */
	{ NULL, 0 },  /* Bl */
	{ NULL, 0 },  /* El */
	{ NULL, 0 },  /* It */
	{ NULL, 0 },  /* Ad */
	{ NULL, TYPE_An },  /* An */
	{ NULL, TYPE_Ar },  /* Ar */
	{ NULL, TYPE_Cd },  /* Cd */
	{ NULL, TYPE_Cm },  /* Cm */
	{ NULL, TYPE_Dv },  /* Dv */
	{ NULL, TYPE_Er },  /* Er */
	{ NULL, TYPE_Ev },  /* Ev */
	{ NULL, 0 },  /* Ex */
	{ NULL, TYPE_Fa },  /* Fa */
	{ parse_mdoc_Fd, 0 },  /* Fd */
	{ NULL, TYPE_Fl },  /* Fl */
	{ parse_mdoc_Fn, 0 },  /* Fn */
	{ NULL, TYPE_Ft },  /* Ft */
	{ NULL, TYPE_Ic },  /* Ic */
	{ NULL, TYPE_In },  /* In */
	{ NULL, TYPE_Li },  /* Li */
	{ parse_mdoc_Nd, TYPE_Nd },  /* Nd */
	{ parse_mdoc_Nm, TYPE_Nm },  /* Nm */
	{ NULL, 0 },  /* Op */
	{ NULL, 0 },  /* Ot */
	{ NULL, TYPE_Pa },  /* Pa */
	{ NULL, 0 },  /* Rv */
	{ NULL, TYPE_St },  /* St */
	{ NULL, TYPE_Va },  /* Va */
	{ parse_mdoc_body, TYPE_Va },  /* Vt */
	{ parse_mdoc_Xr, 0 },  /* Xr */
	{ NULL, 0 },  /* %A */
	{ NULL, 0 },  /* %B */
	{ NULL, 0 },  /* %D */
	{ NULL, 0 },  /* %I */
	{ NULL, 0 },  /* %J */
	{ NULL, 0 },  /* %N */
	{ NULL, 0 },  /* %O */
	{ NULL, 0 },  /* %P */
	{ NULL, 0 },  /* %R */
	{ NULL, 0 },  /* %T */
	{ NULL, 0 },  /* %V */
	{ NULL, 0 },  /* Ac */
	{ NULL, 0 },  /* Ao */
	{ NULL, 0 },  /* Aq */
	{ NULL, TYPE_At },  /* At */
	{ NULL, 0 },  /* Bc */
	{ NULL, 0 },  /* Bf */
	{ NULL, 0 },  /* Bo */
	{ NULL, 0 },  /* Bq */
	{ NULL, TYPE_Bsx },  /* Bsx */
	{ NULL, TYPE_Bx },  /* Bx */
	{ NULL, 0 },  /* Db */
	{ NULL, 0 },  /* Dc */
	{ NULL, 0 },  /* Do */
	{ NULL, 0 },  /* Dq */
	{ NULL, 0 },  /* Ec */
	{ NULL, 0 },  /* Ef */
	{ NULL, TYPE_Em },  /* Em */
	{ NULL, 0 },  /* Eo */
	{ NULL, TYPE_Fx },  /* Fx */
	{ NULL, TYPE_Ms },  /* Ms */
	{ NULL, 0 },  /* No */
	{ NULL, 0 },  /* Ns */
	{ NULL, TYPE_Nx },  /* Nx */
	{ NULL, TYPE_Ox },  /* Ox */
	{ NULL, 0 },  /* Pc */
	{ NULL, 0 },  /* Pf */
	{ NULL, 0 },  /* Po */
	{ NULL, 0 },  /* Pq */
	{ NULL, 0 },  /* Qc */
	{ NULL, 0 },  /* Ql */
	{ NULL, 0 },  /* Qo */
	{ NULL, 0 },  /* Qq */
	{ NULL, 0 },  /* Re */
	{ NULL, 0 },  /* Rs */
	{ NULL, 0 },  /* Sc */
	{ NULL, 0 },  /* So */
	{ NULL, 0 },  /* Sq */
	{ NULL, 0 },  /* Sm */
	{ NULL, 0 },  /* Sx */
	{ NULL, TYPE_Sy },  /* Sy */
	{ NULL, TYPE_Tn },  /* Tn */
	{ NULL, 0 },  /* Ux */
	{ NULL, 0 },  /* Xc */
	{ NULL, 0 },  /* Xo */
	{ parse_mdoc_head, 0 },  /* Fo */
	{ NULL, 0 },  /* Fc */
	{ NULL, 0 },  /* Oo */
	{ NULL, 0 },  /* Oc */
	{ NULL, 0 },  /* Bk */
	{ NULL, 0 },  /* Ek */
	{ NULL, 0 },  /* Bt */
	{ NULL, 0 },  /* Hf */
	{ NULL, 0 },  /* Fr */
	{ NULL, 0 },  /* Ud */
	{ NULL, TYPE_Lb },  /* Lb */
	{ NULL, 0 },  /* Lp */
	{ NULL, TYPE_Lk },  /* Lk */
	{ NULL, TYPE_Mt },  /* Mt */
	{ NULL, 0 },  /* Brq */
	{ NULL, 0 },  /* Bro */
	{ NULL, 0 },  /* Brc */
	{ NULL, 0 },  /* %C */
	{ NULL, 0 },  /* Es */
	{ NULL, 0 },  /* En */
	{ NULL, TYPE_Dx },  /* Dx */
	{ NULL, 0 },  /* %Q */
	{ NULL, 0 },  /* br */
	{ NULL, 0 },  /* sp */
	{ NULL, 0 },  /* %U */
	{ NULL, 0 },  /* Ta */
};

int
mandocdb(int argc, char *argv[])
{
	int		  ch, i;
	size_t		  j, sz;
	const char	 *path_arg;
	struct mchars	 *mc;
	struct manpaths	  dirs;
	struct mparse	 *mp;
	struct ohash_info mpages_info, mlinks_info;

	memset(stmts, 0, STMT__MAX * sizeof(sqlite3_stmt *));
	memset(&dirs, 0, sizeof(struct manpaths));

	mpages_info.alloc  = mlinks_info.alloc  = hash_alloc;
	mpages_info.halloc = mlinks_info.halloc = hash_halloc;
	mpages_info.hfree  = mlinks_info.hfree  = hash_free;

	mpages_info.key_offset = offsetof(struct mpage, inodev);
	mlinks_info.key_offset = offsetof(struct mlink, file);

	progname = strrchr(argv[0], '/');
	if (progname == NULL)
		progname = argv[0];
	else
		++progname;

	/*
	 * We accept a few different invocations.  
	 * The CHECKOP macro makes sure that invocation styles don't
	 * clobber each other.
	 */
#define	CHECKOP(_op, _ch) do \
	if (OP_DEFAULT != (_op)) { \
		fprintf(stderr, "-%c: Conflicting option\n", (_ch)); \
		goto usage; \
	} while (/*CONSTCOND*/0)

	path_arg = NULL;
	op = OP_DEFAULT;

	while (-1 != (ch = getopt(argc, argv, "aC:d:nQT:tu:vW")))
		switch (ch) {
		case ('a'):
			use_all = 1;
			break;
		case ('C'):
			CHECKOP(op, ch);
			path_arg = optarg;
			op = OP_CONFFILE;
			break;
		case ('d'):
			CHECKOP(op, ch);
			path_arg = optarg;
			op = OP_UPDATE;
			break;
		case ('n'):
			nodb = 1;
			break;
		case ('Q'):
			quick = 1;
			break;
		case ('T'):
			if (strcmp(optarg, "utf8")) {
				fprintf(stderr, "-T%s: Unsupported "
				    "output format\n", optarg);
				goto usage;
			}
			write_utf8 = 1;
			break;
		case ('t'):
			CHECKOP(op, ch);
			dup2(STDOUT_FILENO, STDERR_FILENO);
			op = OP_TEST;
			nodb = warnings = 1;
			break;
		case ('u'):
			CHECKOP(op, ch);
			path_arg = optarg;
			op = OP_DELETE;
			break;
		case ('v'):
			verb++;
			break;
		case ('W'):
			warnings = 1;
			break;
		default:
			goto usage;
		}

	argc -= optind;
	argv += optind;

	if (OP_CONFFILE == op && argc > 0) {
		fprintf(stderr, "-C: Too many arguments\n");
		goto usage;
	}

	exitcode = (int)MANDOCLEVEL_OK;
	mp = mparse_alloc(MPARSE_AUTO, 
		MANDOCLEVEL_FATAL, NULL, NULL, quick);
	mc = mchars_alloc();

	ohash_init(&mpages, 6, &mpages_info);
	ohash_init(&mlinks, 6, &mlinks_info);

	if (OP_UPDATE == op || OP_DELETE == op || OP_TEST == op) {
		/* 
		 * Force processing all files.
		 */
		use_all = 1;

		/*
		 * All of these deal with a specific directory.
		 * Jump into that directory then collect files specified
		 * on the command-line.
		 */
		if (0 == set_basedir(path_arg))
			goto out;
		for (i = 0; i < argc; i++)
			filescan(argv[i]);
		if (0 == dbopen(1))
			goto out;
		if (OP_TEST != op)
			dbprune();
		if (OP_DELETE != op)
			mpages_merge(mc, mp);
		dbclose(1);
	} else {
		/*
		 * If we have arguments, use them as our manpaths.
		 * If we don't, grok from manpath(1) or however else
		 * manpath_parse() wants to do it.
		 */
		if (argc > 0) {
			dirs.paths = mandoc_calloc
				(argc, sizeof(char *));
			dirs.sz = (size_t)argc;
			for (i = 0; i < argc; i++)
				dirs.paths[i] = mandoc_strdup(argv[i]);
		} else
			manpath_parse(&dirs, path_arg, NULL, NULL);

		/*
		 * First scan the tree rooted at a base directory, then
		 * build a new database and finally move it into place.
		 * Ignore zero-length directories and strip trailing
		 * slashes.
		 */
		for (j = 0; j < dirs.sz; j++) {
			sz = strlen(dirs.paths[j]);
			if (sz && '/' == dirs.paths[j][sz - 1])
				dirs.paths[j][--sz] = '\0';
			if (0 == sz)
				continue;

			if (j) {
				ohash_init(&mpages, 6, &mpages_info);
				ohash_init(&mlinks, 6, &mlinks_info);
			}

			if (0 == set_basedir(dirs.paths[j]))
				goto out;
			if (0 == treescan())
				goto out;
			if (0 == set_basedir(dirs.paths[j]))
				goto out;
			if (0 == dbopen(0))
				goto out;

			mpages_merge(mc, mp);
			dbclose(0);

			if (j + 1 < dirs.sz) {
				mpages_free();
				ohash_delete(&mpages);
				ohash_delete(&mlinks);
			}
		}
	}
out:
	set_basedir(NULL);
	manpath_free(&dirs);
	mchars_free(mc);
	mparse_free(mp);
	mpages_free();
	ohash_delete(&mpages);
	ohash_delete(&mlinks);
	return(exitcode);
usage:
	fprintf(stderr, "usage: %s [-anQvW] [-C file] [-Tutf8]\n"
			"       %s [-anQvW] [-Tutf8] dir ...\n"
			"       %s [-nQvW] [-Tutf8] -d dir [file ...]\n"
			"       %s [-nvW] -u dir [file ...]\n"
			"       %s [-Q] -t file ...\n",
		       progname, progname, progname, 
		       progname, progname);

	return((int)MANDOCLEVEL_BADARG);
}

/*
 * Scan a directory tree rooted at "basedir" for manpages.
 * We use fts(), scanning directory parts along the way for clues to our
 * section and architecture.
 *
 * If use_all has been specified, grok all files.
 * If not, sanitise paths to the following:
 *
 *   [./]man*[/<arch>]/<name>.<section> 
 *   or
 *   [./]cat<section>[/<arch>]/<name>.0
 *
 * TODO: accomodate for multi-language directories.
 */
static int
treescan(void)
{
	FTS		*f;
	FTSENT		*ff;
	struct mlink	*mlink;
	int		 dform;
	char		*dsec, *arch, *fsec, *cp;
	const char	*path;
	const char	*argv[2];

	argv[0] = ".";
	argv[1] = (char *)NULL;

	/*
	 * Walk through all components under the directory, using the
	 * logical descent of files.
	 */
	f = fts_open((char * const *)argv, FTS_LOGICAL, NULL);
	if (NULL == f) {
		exitcode = (int)MANDOCLEVEL_SYSERR;
		say("", NULL);
		return(0);
	}

	dsec = arch = NULL;
	dform = FORM_NONE;

	while (NULL != (ff = fts_read(f))) {
		path = ff->fts_path + 2;
		/*
		 * If we're a regular file, add an mlink by using the
		 * stored directory data and handling the filename.
		 */
		if (FTS_F == ff->fts_info) {
			if (0 == strcmp(path, MANDOC_DB))
				continue;
			if ( ! use_all && ff->fts_level < 2) {
				if (warnings)
					say(path, "Extraneous file");
				continue;
			} else if (NULL == (fsec =
					strrchr(ff->fts_name, '.'))) {
				if ( ! use_all) {
					if (warnings)
						say(path,
						    "No filename suffix");
					continue;
				}
			} else if (0 == strcmp(++fsec, "html")) {
				if (warnings)
					say(path, "Skip html");
				continue;
			} else if (0 == strcmp(fsec, "gz")) {
				if (warnings)
					say(path, "Skip gz");
				continue;
			} else if (0 == strcmp(fsec, "ps")) {
				if (warnings)
					say(path, "Skip ps");
				continue;
			} else if (0 == strcmp(fsec, "pdf")) {
				if (warnings)
					say(path, "Skip pdf");
				continue;
			} else if ( ! use_all &&
			    ((FORM_SRC == dform && strcmp(fsec, dsec)) ||
			     (FORM_CAT == dform && strcmp(fsec, "0")))) {
				if (warnings)
					say(path, "Wrong filename suffix");
				continue;
			} else
				fsec[-1] = '\0';

			mlink = mandoc_calloc(1, sizeof(struct mlink));
			strlcpy(mlink->file, path, sizeof(mlink->file));
			mlink->dform = dform;
			mlink->dsec = dsec;
			mlink->arch = arch;
			mlink->name = ff->fts_name;
			mlink->fsec = fsec;
			mlink_add(mlink, ff->fts_statp);
			continue;
		} else if (FTS_D != ff->fts_info &&
				FTS_DP != ff->fts_info) {
			if (warnings)
				say(path, "Not a regular file");
			continue;
		}

		switch (ff->fts_level) {
		case (0):
			/* Ignore the root directory. */
			break;
		case (1):
			/*
			 * This might contain manX/ or catX/.
			 * Try to infer this from the name.
			 * If we're not in use_all, enforce it.
			 */
			cp = ff->fts_name;
			if (FTS_DP == ff->fts_info)
				break;

			if (0 == strncmp(cp, "man", 3)) {
				dform = FORM_SRC;
				dsec = cp + 3;
			} else if (0 == strncmp(cp, "cat", 3)) {
				dform = FORM_CAT;
				dsec = cp + 3;
			} else {
				dform = FORM_NONE;
				dsec = NULL;
			}

			if (NULL != dsec || use_all) 
				break;

			if (warnings)
				say(path, "Unknown directory part");
			fts_set(f, ff, FTS_SKIP);
			break;
		case (2):
			/*
			 * Possibly our architecture.
			 * If we're descending, keep tabs on it.
			 */
			if (FTS_DP != ff->fts_info && NULL != dsec)
				arch = ff->fts_name;
			else
				arch = NULL;
			break;
		default:
			if (FTS_DP == ff->fts_info || use_all)
				break;
			if (warnings)
				say(path, "Extraneous directory part");
			fts_set(f, ff, FTS_SKIP);
			break;
		}
	}

	fts_close(f);
	return(1);
}

/*
 * Add a file to the mlinks table.
 * Do not verify that it's a "valid" looking manpage (we'll do that
 * later).
 *
 * Try to infer the manual section, architecture, and page name from the
 * path, assuming it looks like
 *
 *   [./]man*[/<arch>]/<name>.<section> 
 *   or
 *   [./]cat<section>[/<arch>]/<name>.0
 *
 * See treescan() for the fts(3) version of this.
 */
static void
filescan(const char *file)
{
	char		 buf[PATH_MAX];
	struct stat	 st;
	struct mlink	*mlink;
	char		*p, *start;

	assert(use_all);

	if (0 == strncmp(file, "./", 2))
		file += 2;

	if (NULL == realpath(file, buf)) {
		exitcode = (int)MANDOCLEVEL_BADARG;
		say(file, NULL);
		return;
	}

	if (strstr(buf, basedir) == buf)
		start = buf + strlen(basedir) + 1;
	else if (OP_TEST == op)
		start = buf;
	else {
		exitcode = (int)MANDOCLEVEL_BADARG;
		say("", "%s: outside base directory", buf);
		return;
	}

	if (-1 == stat(buf, &st)) {
		exitcode = (int)MANDOCLEVEL_BADARG;
		say(file, NULL);
		return;
	} else if ( ! (S_IFREG & st.st_mode)) {
		exitcode = (int)MANDOCLEVEL_BADARG;
		say(file, "Not a regular file");
		return;
	}

	mlink = mandoc_calloc(1, sizeof(struct mlink));
	strlcpy(mlink->file, start, sizeof(mlink->file));

	/*
	 * First try to guess our directory structure.
	 * If we find a separator, try to look for man* or cat*.
	 * If we find one of these and what's underneath is a directory,
	 * assume it's an architecture.
	 */
	if (NULL != (p = strchr(start, '/'))) {
		*p++ = '\0';
		if (0 == strncmp(start, "man", 3)) {
			mlink->dform = FORM_SRC;
			mlink->dsec = start + 3;
		} else if (0 == strncmp(start, "cat", 3)) {
			mlink->dform = FORM_CAT;
			mlink->dsec = start + 3;
		}

		start = p;
		if (NULL != mlink->dsec && NULL != (p = strchr(start, '/'))) {
			*p++ = '\0';
			mlink->arch = start;
			start = p;
		}
	}

	/*
	 * Now check the file suffix.
	 * Suffix of `.0' indicates a catpage, `.1-9' is a manpage.
	 */
	p = strrchr(start, '\0');
	while (p-- > start && '/' != *p && '.' != *p)
		/* Loop. */ ;

	if ('.' == *p) {
		*p++ = '\0';
		mlink->fsec = p;
	}

	/*
	 * Now try to parse the name.
	 * Use the filename portion of the path.
	 */
	mlink->name = start;
	if (NULL != (p = strrchr(start, '/'))) {
		mlink->name = p + 1;
		*p = '\0';
	}
	mlink_add(mlink, &st);
}

static void
mlink_add(struct mlink *mlink, const struct stat *st)
{
	struct inodev	 inodev;
	struct mpage	*mpage;
	unsigned int	 slot;

	assert(NULL != mlink->file);

	mlink->dsec = mandoc_strdup(mlink->dsec ? mlink->dsec : "");
	mlink->arch = mandoc_strdup(mlink->arch ? mlink->arch : "");
	mlink->name = mandoc_strdup(mlink->name ? mlink->name : "");
	mlink->fsec = mandoc_strdup(mlink->fsec ? mlink->fsec : "");

	if ('0' == *mlink->fsec) {
		free(mlink->fsec);
		mlink->fsec = mandoc_strdup(mlink->dsec);
		mlink->fform = FORM_CAT;
	} else if ('1' <= *mlink->fsec && '9' >= *mlink->fsec)
		mlink->fform = FORM_SRC;
	else
		mlink->fform = FORM_NONE;

	slot = ohash_qlookup(&mlinks, mlink->file);
	assert(NULL == ohash_find(&mlinks, slot));
	ohash_insert(&mlinks, slot, mlink);

	inodev.st_ino = st->st_ino;
	inodev.st_dev = st->st_dev;
	slot = ohash_lookup_memory(&mpages, (char *)&inodev,
	    sizeof(struct inodev), inodev.st_ino);
	mpage = ohash_find(&mpages, slot);
	if (NULL == mpage) {
		mpage = mandoc_calloc(1, sizeof(struct mpage));
		mpage->inodev.st_ino = inodev.st_ino;
		mpage->inodev.st_dev = inodev.st_dev;
		ohash_insert(&mpages, slot, mpage);
	} else
		mlink->next = mpage->mlinks;
	mpage->mlinks = mlink;
}

static void
mlink_free(struct mlink *mlink)
{

	free(mlink->dsec);
	free(mlink->arch);
	free(mlink->name);
	free(mlink->fsec);
	free(mlink);
}

static void
mpages_free(void)
{
	struct mpage	*mpage;
	struct mlink	*mlink;
	unsigned int	 slot;

	mpage = ohash_first(&mpages, &slot);
	while (NULL != mpage) {
		while (NULL != (mlink = mpage->mlinks)) {
			mpage->mlinks = mlink->next;
			mlink_free(mlink);
		}
		free(mpage->sec);
		free(mpage->arch);
		free(mpage->title);
		free(mpage->desc);
		free(mpage);
		mpage = ohash_next(&mpages, &slot);
	}
}

/*
 * For each mlink to the mpage, check whether the path looks like
 * it is formatted, and if it does, check whether a source manual
 * exists by the same name, ignoring the suffix.
 * If both conditions hold, drop the mlink.
 */
static void
mlinks_undupe(struct mpage *mpage)
{
	char		  buf[PATH_MAX];
	struct mlink	**prev;
	struct mlink	 *mlink;
	char		 *bufp;

	mpage->form = FORM_CAT;
	prev = &mpage->mlinks;
	while (NULL != (mlink = *prev)) {
		if (FORM_CAT != mlink->dform) {
			mpage->form = FORM_NONE;
			goto nextlink;
		}
		if (strlcpy(buf, mlink->file, PATH_MAX) >= PATH_MAX) {
			if (warnings)
				say(mlink->file, "Filename too long");
			goto nextlink;
		}
		bufp = strstr(buf, "cat");
		assert(NULL != bufp);
		memcpy(bufp, "man", 3);
		if (NULL != (bufp = strrchr(buf, '.')))
			*++bufp = '\0';
		strlcat(buf, mlink->dsec, PATH_MAX);
		if (NULL == ohash_find(&mlinks,
				ohash_qlookup(&mlinks, buf)))
			goto nextlink;
		if (warnings)
			say(mlink->file, "Man source exists: %s", buf);
		if (use_all)
			goto nextlink;
		*prev = mlink->next;
		mlink_free(mlink);
		continue;
nextlink:
		prev = &(*prev)->next;
	}
}

static int
mlink_check(struct mpage *mpage, struct mlink *mlink)
{
	int	 match;

	match = 1;

	/*
	 * Check whether the manual section given in a file
	 * agrees with the directory where the file is located.
	 * Some manuals have suffixes like (3p) on their
	 * section number either inside the file or in the
	 * directory name, some are linked into more than one
	 * section, like encrypt(1) = makekey(8).
	 */

	if (FORM_SRC == mpage->form &&
	    strcasecmp(mpage->sec, mlink->dsec)) {
		match = 0;
		say(mlink->file, "Section \"%s\" manual in %s directory",
		    mpage->sec, mlink->dsec);
	}

	/*
	 * Manual page directories exist for each kernel
	 * architecture as returned by machine(1).
	 * However, many manuals only depend on the
	 * application architecture as returned by arch(1).
	 * For example, some (2/ARM) manuals are shared
	 * across the "armish" and "zaurus" kernel
	 * architectures.
	 * A few manuals are even shared across completely
	 * different architectures, for example fdformat(1)
	 * on amd64, i386, sparc, and sparc64.
	 */

	if (strcasecmp(mpage->arch, mlink->arch)) {
		match = 0;
		say(mlink->file, "Architecture \"%s\" manual in "
		    "\"%s\" directory", mpage->arch, mlink->arch);
	}

	if (strcasecmp(mpage->title, mlink->name))
		match = 0;

	return(match);
}

/*
 * Run through the files in the global vector "mpages"
 * and add them to the database specified in "basedir".
 *
 * This handles the parsing scheme itself, using the cues of directory
 * and filename to determine whether the file is parsable or not.
 */
static void
mpages_merge(struct mchars *mc, struct mparse *mp)
{
	char			 any[] = "any";
	struct ohash_info	 str_info;
	struct mpage		*mpage;
	struct mlink		*mlink;
	struct mdoc		*mdoc;
	struct man		*man;
	char			*cp;
	int			 match;
	unsigned int		 pslot;
	enum mandoclevel	 lvl;

	str_info.alloc = hash_alloc;
	str_info.halloc = hash_halloc;
	str_info.hfree = hash_free;
	str_info.key_offset = offsetof(struct str, key);

	if (0 == nodb)
		SQL_EXEC("BEGIN TRANSACTION");

	mpage = ohash_first(&mpages, &pslot);
	while (NULL != mpage) {
		mlinks_undupe(mpage);
		if (NULL == mpage->mlinks) {
			mpage = ohash_next(&mpages, &pslot);
			continue;
		}

		ohash_init(&strings, 6, &str_info);
		mparse_reset(mp);
		mdoc = NULL;
		man = NULL;

		/*
		 * Try interpreting the file as mdoc(7) or man(7)
		 * source code, unless it is already known to be
		 * formatted.  Fall back to formatted mode.
		 */
		if (FORM_CAT != mpage->mlinks->dform ||
		    FORM_CAT != mpage->mlinks->fform) {
			lvl = mparse_readfd(mp, -1, mpage->mlinks->file);
			if (lvl < MANDOCLEVEL_FATAL)
				mparse_result(mp, &mdoc, &man);
		}

		if (NULL != mdoc) {
			mpage->form = FORM_SRC;
			mpage->sec =
			    mandoc_strdup(mdoc_meta(mdoc)->msec);
			mpage->arch = mdoc_meta(mdoc)->arch;
			mpage->arch = mandoc_strdup(
			    NULL == mpage->arch ? "" : mpage->arch);
			mpage->title =
			    mandoc_strdup(mdoc_meta(mdoc)->title);
		} else if (NULL != man) {
			mpage->form = FORM_SRC;
			mpage->sec =
			    mandoc_strdup(man_meta(man)->msec);
			mpage->arch =
			    mandoc_strdup(mpage->mlinks->arch);
			mpage->title =
			    mandoc_strdup(man_meta(man)->title);
		} else {
			mpage->form = FORM_CAT;
			mpage->sec =
			    mandoc_strdup(mpage->mlinks->dsec);
			mpage->arch =
			    mandoc_strdup(mpage->mlinks->arch);
			mpage->title =
			    mandoc_strdup(mpage->mlinks->name);
		}
		putkey(mpage, mpage->sec, TYPE_sec);
		putkey(mpage, '\0' == *mpage->arch ?
		    any : mpage->arch, TYPE_arch);

		for (mlink = mpage->mlinks; mlink; mlink = mlink->next) {
			if ('\0' != *mlink->dsec)
				putkey(mpage, mlink->dsec, TYPE_sec);
			if ('\0' != *mlink->fsec)
				putkey(mpage, mlink->fsec, TYPE_sec);
			putkey(mpage, '\0' == *mlink->arch ?
			    any : mlink->arch, TYPE_arch);
			putkey(mpage, mlink->name, TYPE_Nm);
		}

		if (warnings && !use_all) {
			match = 0;
			for (mlink = mpage->mlinks; mlink;
			     mlink = mlink->next)
				if (mlink_check(mpage, mlink))
					match = 1;
		} else
			match = 1;

		if (NULL != mdoc) {
			if (NULL != (cp = mdoc_meta(mdoc)->name))
				putkey(mpage, cp, TYPE_Nm);
			assert(NULL == mpage->desc);
			parse_mdoc(mpage, mdoc_node(mdoc));
			putkey(mpage, NULL != mpage->desc ?
			    mpage->desc : mpage->mlinks->name, TYPE_Nd);
		} else if (NULL != man)
			parse_man(mpage, man_node(man));
		else
			parse_cat(mpage);

		dbadd(mpage, mc);
		ohash_delete(&strings);
		mpage = ohash_next(&mpages, &pslot);
	}

	if (0 == nodb)
		SQL_EXEC("END TRANSACTION");
}

static void
parse_cat(struct mpage *mpage)
{
	FILE		*stream;
	char		*line, *p, *title;
	size_t		 len, plen, titlesz;

	if (NULL == (stream = fopen(mpage->mlinks->file, "r"))) {
		if (warnings)
			say(mpage->mlinks->file, NULL);
		return;
	}

	/* Skip to first blank line. */

	while (NULL != (line = fgetln(stream, &len)))
		if ('\n' == *line)
			break;

	/*
	 * Assume the first line that is not indented
	 * is the first section header.  Skip to it.
	 */

	while (NULL != (line = fgetln(stream, &len)))
		if ('\n' != *line && ' ' != *line)
			break;
	
	/*
	 * Read up until the next section into a buffer.
	 * Strip the leading and trailing newline from each read line,
	 * appending a trailing space.
	 * Ignore empty (whitespace-only) lines.
	 */

	titlesz = 0;
	title = NULL;

	while (NULL != (line = fgetln(stream, &len))) {
		if (' ' != *line || '\n' != line[len - 1])
			break;
		while (len > 0 && isspace((unsigned char)*line)) {
			line++;
			len--;
		}
		if (1 == len)
			continue;
		title = mandoc_realloc(title, titlesz + len);
		memcpy(title + titlesz, line, len);
		titlesz += len;
		title[titlesz - 1] = ' ';
	}

	/*
	 * If no page content can be found, or the input line
	 * is already the next section header, or there is no
	 * trailing newline, reuse the page title as the page
	 * description.
	 */

	if (NULL == title || '\0' == *title) {
		if (warnings)
			say(mpage->mlinks->file,
			    "Cannot find NAME section");
		assert(NULL == mpage->desc);
		mpage->desc = mandoc_strdup(mpage->mlinks->name);
		putkey(mpage, mpage->mlinks->name, TYPE_Nd);
		fclose(stream);
		free(title);
		return;
	}

	title = mandoc_realloc(title, titlesz + 1);
	title[titlesz] = '\0';

	/*
	 * Skip to the first dash.
	 * Use the remaining line as the description (no more than 70
	 * bytes).
	 */

	if (NULL != (p = strstr(title, "- "))) {
		for (p += 2; ' ' == *p || '\b' == *p; p++)
			/* Skip to next word. */ ;
	} else {
		if (warnings)
			say(mpage->mlinks->file,
			    "No dash in title line");
		p = title;
	}

	plen = strlen(p);

	/* Strip backspace-encoding from line. */

	while (NULL != (line = memchr(p, '\b', plen))) {
		len = line - p;
		if (0 == len) {
			memmove(line, line + 1, plen--);
			continue;
		} 
		memmove(line - 1, line + 1, plen - len);
		plen -= 2;
	}

	assert(NULL == mpage->desc);
	mpage->desc = mandoc_strdup(p);
	putkey(mpage, mpage->desc, TYPE_Nd);
	fclose(stream);
	free(title);
}

/*
 * Put a type/word pair into the word database for this particular file.
 */
static void
putkey(const struct mpage *mpage, char *value, uint64_t type)
{
	char	 *cp;

	assert(NULL != value);
	if (TYPE_arch == type)
		for (cp = value; *cp; cp++)
			if (isupper((unsigned char)*cp))
				*cp = _tolower((unsigned char)*cp);
	putkeys(mpage, value, strlen(value), type);
}

/*
 * Grok all nodes at or below a certain mdoc node into putkey().
 */
static void
putmdockey(const struct mpage *mpage,
	const struct mdoc_node *n, uint64_t m)
{

	for ( ; NULL != n; n = n->next) {
		if (NULL != n->child)
			putmdockey(mpage, n->child, m);
		if (MDOC_TEXT == n->type)
			putkey(mpage, n->string, m);
	}
}

static void
parse_man(struct mpage *mpage, const struct man_node *n)
{
	const struct man_node *head, *body;
	char		*start, *sv, *title;
	char		 byte;
	size_t		 sz, titlesz;

	if (NULL == n)
		return;

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

			title = NULL;
			titlesz = 0;

			/*
			 * Suck the entire NAME section into memory.
			 * Yes, we might run away.
			 * But too many manuals have big, spread-out
			 * NAME sections over many lines.
			 */

			for ( ; NULL != body; body = body->next) {
				if (MAN_TEXT != body->type)
					break;
				if (0 == (sz = strlen(body->string)))
					continue;
				title = mandoc_realloc
					(title, titlesz + sz + 1);
				memcpy(title + titlesz, body->string, sz);
				titlesz += sz + 1;
				title[titlesz - 1] = ' ';
			}
			if (NULL == title)
				return;

			title = mandoc_realloc(title, titlesz + 1);
			title[titlesz] = '\0';

			/* Skip leading space.  */

			sv = title;
			while (isspace((unsigned char)*sv))
				sv++;

			if (0 == (sz = strlen(sv))) {
				free(title);
				return;
			}

			/* Erase trailing space. */

			start = &sv[sz - 1];
			while (start > sv && isspace((unsigned char)*start))
				*start-- = '\0';

			if (start == sv) {
				free(title);
				return;
			}

			start = sv;

			/* 
			 * Go through a special heuristic dance here.
			 * Conventionally, one or more manual names are
			 * comma-specified prior to a whitespace, then a
			 * dash, then a description.  Try to puzzle out
			 * the name parts here.
			 */

			for ( ;; ) {
				sz = strcspn(start, " ,");
				if ('\0' == start[sz])
					break;

				byte = start[sz];
				start[sz] = '\0';

				/*
				 * Assume a stray trailing comma in the
				 * name list if a name begins with a dash.
				 */

				if ('-' == start[0] ||
				    ('\\' == start[0] && '-' == start[1]))
					break;

				putkey(mpage, start, TYPE_Nm);

				if (' ' == byte) {
					start += sz + 1;
					break;
				}

				assert(',' == byte);
				start += sz + 1;
				while (' ' == *start)
					start++;
			}

			if (sv == start) {
				putkey(mpage, start, TYPE_Nm);
				free(title);
				return;
			}

			while (isspace((unsigned char)*start))
				start++;

			if (0 == strncmp(start, "-", 1))
				start += 1;
			else if (0 == strncmp(start, "\\-\\-", 4))
				start += 4;
			else if (0 == strncmp(start, "\\-", 2))
				start += 2;
			else if (0 == strncmp(start, "\\(en", 4))
				start += 4;
			else if (0 == strncmp(start, "\\(em", 4))
				start += 4;

			while (' ' == *start)
				start++;

			assert(NULL == mpage->desc);
			mpage->desc = mandoc_strdup(start);
			putkey(mpage, mpage->desc, TYPE_Nd);
			free(title);
			return;
		}
	}

	for (n = n->child; n; n = n->next) {
		if (NULL != mpage->desc)
			break;
		parse_man(mpage, n);
	}
}

static void
parse_mdoc(struct mpage *mpage, const struct mdoc_node *n)
{

	assert(NULL != n);
	for (n = n->child; NULL != n; n = n->next) {
		switch (n->type) {
		case (MDOC_ELEM):
			/* FALLTHROUGH */
		case (MDOC_BLOCK):
			/* FALLTHROUGH */
		case (MDOC_HEAD):
			/* FALLTHROUGH */
		case (MDOC_BODY):
			/* FALLTHROUGH */
		case (MDOC_TAIL):
			if (NULL != mdocs[n->tok].fp)
			       if (0 == (*mdocs[n->tok].fp)(mpage, n))
				       break;
			if (mdocs[n->tok].mask)
				putmdockey(mpage, n->child,
				    mdocs[n->tok].mask);
			break;
		default:
			assert(MDOC_ROOT != n->type);
			continue;
		}
		if (NULL != n->child)
			parse_mdoc(mpage, n);
	}
}

static int
parse_mdoc_Fd(struct mpage *mpage, const struct mdoc_node *n)
{
	const char	*start, *end;
	size_t		 sz;

	if (SEC_SYNOPSIS != n->sec ||
			NULL == (n = n->child) || 
			MDOC_TEXT != n->type)
		return(0);

	/*
	 * Only consider those `Fd' macro fields that begin with an
	 * "inclusion" token (versus, e.g., #define).
	 */

	if (strcmp("#include", n->string))
		return(0);

	if (NULL == (n = n->next) || MDOC_TEXT != n->type)
		return(0);

	/*
	 * Strip away the enclosing angle brackets and make sure we're
	 * not zero-length.
	 */

	start = n->string;
	if ('<' == *start || '"' == *start)
		start++;

	if (0 == (sz = strlen(start)))
		return(0);

	end = &start[(int)sz - 1];
	if ('>' == *end || '"' == *end)
		end--;

	if (end > start)
		putkeys(mpage, start, end - start + 1, TYPE_In);
	return(0);
}

static int
parse_mdoc_Fn(struct mpage *mpage, const struct mdoc_node *n)
{
	char	*cp;

	if (NULL == (n = n->child) || MDOC_TEXT != n->type)
		return(0);

	/* 
	 * Parse: .Fn "struct type *name" "char *arg".
	 * First strip away pointer symbol. 
	 * Then store the function name, then type.
	 * Finally, store the arguments. 
	 */

	if (NULL == (cp = strrchr(n->string, ' ')))
		cp = n->string;

	while ('*' == *cp)
		cp++;

	putkey(mpage, cp, TYPE_Fn);

	if (n->string < cp)
		putkeys(mpage, n->string, cp - n->string, TYPE_Ft);

	for (n = n->next; NULL != n; n = n->next)
		if (MDOC_TEXT == n->type)
			putkey(mpage, n->string, TYPE_Fa);

	return(0);
}

static int
parse_mdoc_Xr(struct mpage *mpage, const struct mdoc_node *n)
{
	char	*cp;

	if (NULL == (n = n->child))
		return(0);

	if (NULL == n->next) {
		putkey(mpage, n->string, TYPE_Xr);
		return(0);
	}

	if (-1 == asprintf(&cp, "%s(%s)", n->string, n->next->string)) {
		perror(NULL);
		exit((int)MANDOCLEVEL_SYSERR);
	}
	putkey(mpage, cp, TYPE_Xr);
	free(cp);
	return(0);
}

static int
parse_mdoc_Nd(struct mpage *mpage, const struct mdoc_node *n)
{
	size_t		 sz;

	if (MDOC_BODY != n->type)
		return(0);

	/*
	 * Special-case the `Nd' because we need to put the description
	 * into the document table.
	 */

	for (n = n->child; NULL != n; n = n->next) {
		if (MDOC_TEXT == n->type) {
			if (NULL != mpage->desc) {
				sz = strlen(mpage->desc) +
				     strlen(n->string) + 2;
				mpage->desc = mandoc_realloc(
				    mpage->desc, sz);
				strlcat(mpage->desc, " ", sz);
				strlcat(mpage->desc, n->string, sz);
			} else
				mpage->desc = mandoc_strdup(n->string);
		}
		if (NULL != n->child)
			parse_mdoc_Nd(mpage, n);
	}
	return(1);
}

static int
parse_mdoc_Nm(struct mpage *mpage, const struct mdoc_node *n)
{

	return(SEC_NAME == n->sec ||
	    (SEC_SYNOPSIS == n->sec && MDOC_HEAD == n->type));
}

static int
parse_mdoc_Sh(struct mpage *mpage, const struct mdoc_node *n)
{

	return(SEC_CUSTOM == n->sec && MDOC_HEAD == n->type);
}

static int
parse_mdoc_head(struct mpage *mpage, const struct mdoc_node *n)
{

	return(MDOC_HEAD == n->type);
}

static int
parse_mdoc_body(struct mpage *mpage, const struct mdoc_node *n)
{

	return(MDOC_BODY == n->type);
}

/*
 * Add a string to the hash table for the current manual.
 * Each string has a bitmask telling which macros it belongs to.
 * When we finish the manual, we'll dump the table.
 */
static void
putkeys(const struct mpage *mpage,
	const char *cp, size_t sz, uint64_t v)
{
	struct str	*s;
	const char	*end;
	uint64_t	 mask;
	unsigned int	 slot;
	int		 i;

	if (0 == sz)
		return;

	if (verb > 1) {
		for (i = 0, mask = 1;
		     i < mansearch_keymax;
		     i++, mask <<= 1)
			if (mask & v)
				break;
		say(mpage->mlinks->file, "Adding key %s=%*s",
		    mansearch_keynames[i], sz, cp);
	}

	end = cp + sz;
	slot = ohash_qlookupi(&strings, cp, &end);
	s = ohash_find(&strings, slot);

	if (NULL != s && mpage == s->mpage) {
		s->mask |= v;
		return;
	} else if (NULL == s) {
		s = mandoc_calloc(sizeof(struct str) + sz + 1, 1);
		memcpy(s->key, cp, sz);
		ohash_insert(&strings, slot, s);
	}
	s->mpage = mpage;
	s->mask = v;
}

/*
 * Take a Unicode codepoint and produce its UTF-8 encoding.
 * This isn't the best way to do this, but it works.
 * The magic numbers are from the UTF-8 packaging.
 * They're not as scary as they seem: read the UTF-8 spec for details.
 */
static size_t
utf8(unsigned int cp, char out[7])
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
		out[0] = (cp >> 18 &  7) | 240;
		out[1] = (cp >> 12 & 63) | 128;
		out[2] = (cp >> 6  & 63) | 128;
		out[3] = (cp       & 63) | 128;
	} else if (cp <= 0x03FFFFFF) {
		rc = 5;
		out[0] = (cp >> 24 &  3) | 248;
		out[1] = (cp >> 18 & 63) | 128;
		out[2] = (cp >> 12 & 63) | 128;
		out[3] = (cp >> 6  & 63) | 128;
		out[4] = (cp       & 63) | 128;
	} else if (cp <= 0x7FFFFFFF) {
		rc = 6;
		out[0] = (cp >> 30 &  1) | 252;
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
 * Store the rendered version of a key, or alias the pointer
 * if the key contains no escape sequences.
 */
static void
render_key(struct mchars *mc, struct str *key)
{
	size_t		 sz, bsz, pos;
	char		 utfbuf[7], res[6];
	char		*buf;
	const char	*seq, *cpp, *val;
	int		 len, u;
	enum mandoc_esc	 esc;

	assert(NULL == key->rendered);

	res[0] = '\\';
	res[1] = '\t';
	res[2] = ASCII_NBRSP;
	res[3] = ASCII_HYPH;
	res[4] = ASCII_BREAK;
	res[5] = '\0';

	val = key->key;
	bsz = strlen(val);

	/*
	 * Pre-check: if we have no stop-characters, then set the
	 * pointer as ourselvse and get out of here.
	 */
	if (strcspn(val, res) == bsz) {
		key->rendered = key->key;
		return;
	} 

	/* Pre-allocate by the length of the input */

	buf = mandoc_malloc(++bsz);
	pos = 0;

	while ('\0' != *val) {
		/*
		 * Halt on the first escape sequence.
		 * This also halts on the end of string, in which case
		 * we just copy, fallthrough, and exit the loop.
		 */
		if ((sz = strcspn(val, res)) > 0) {
			memcpy(&buf[pos], val, sz);
			pos += sz;
			val += sz;
		}

		switch (*val) {
		case (ASCII_HYPH):
			buf[pos++] = '-';
			val++;
			continue;
		case ('\t'):
			/* FALLTHROUGH */
		case (ASCII_NBRSP):
			buf[pos++] = ' ';
			val++;
			/* FALLTHROUGH */
		case (ASCII_BREAK):
			continue;
		default:
			break;
		}
		if ('\\' != *val)
			break;

		/* Read past the slash. */

		val++;

		/*
		 * Parse the escape sequence and see if it's a
		 * predefined character or special character.
		 */

		esc = mandoc_escape
			((const char **)&val, &seq, &len);
		if (ESCAPE_ERROR == esc)
			break;
		if (ESCAPE_SPECIAL != esc)
			continue;

		/*
		 * Render the special character
		 * as either UTF-8 or ASCII.
		 */

		if (write_utf8) {
			if (0 == (u = mchars_spec2cp(mc, seq, len)))
				continue;
			cpp = utfbuf;
			if (0 == (sz = utf8(u, utfbuf)))
				continue;
			sz = strlen(cpp);
		} else {
			cpp = mchars_spec2str(mc, seq, len, &sz);
			if (NULL == cpp)
				continue;
			if (ASCII_NBRSP == *cpp) {
				cpp = " ";
				sz = 1;
			}
		}

		/* Copy the rendered glyph into the stream. */

		bsz += sz;
		buf = mandoc_realloc(buf, bsz);
		memcpy(&buf[pos], cpp, sz);
		pos += sz;
	}

	buf[pos] = '\0';
	key->rendered = buf;
}

/*
 * Flush the current page's terms (and their bits) into the database.
 * Wrap the entire set of additions in a transaction to make sqlite be a
 * little faster.
 * Also, handle escape sequences at the last possible moment.
 */
static void
dbadd(const struct mpage *mpage, struct mchars *mc)
{
	struct mlink	*mlink;
	struct str	*key;
	int64_t		 recno;
	size_t		 i;
	unsigned int	 slot;

	if (verb)
		say(mpage->mlinks->file, "Adding to database");

	if (nodb)
		return;

	i = 1;
	SQL_BIND_INT(stmts[STMT_INSERT_PAGE], i, FORM_SRC == mpage->form);
	SQL_STEP(stmts[STMT_INSERT_PAGE]);
	recno = sqlite3_last_insert_rowid(db);
	sqlite3_reset(stmts[STMT_INSERT_PAGE]);

	for (mlink = mpage->mlinks; mlink; mlink = mlink->next) {
		i = 1;
		SQL_BIND_TEXT(stmts[STMT_INSERT_LINK], i, mlink->dsec);
		SQL_BIND_TEXT(stmts[STMT_INSERT_LINK], i, mlink->arch);
		SQL_BIND_TEXT(stmts[STMT_INSERT_LINK], i, mlink->name);
		SQL_BIND_INT64(stmts[STMT_INSERT_LINK], i, recno);
		SQL_STEP(stmts[STMT_INSERT_LINK]);
		sqlite3_reset(stmts[STMT_INSERT_LINK]);
	}

	for (key = ohash_first(&strings, &slot); NULL != key;
	     key = ohash_next(&strings, &slot)) {
		assert(key->mpage == mpage);
		if (NULL == key->rendered)
			render_key(mc, key);
		i = 1;
		SQL_BIND_INT64(stmts[STMT_INSERT_KEY], i, key->mask);
		SQL_BIND_TEXT(stmts[STMT_INSERT_KEY], i, key->rendered);
		SQL_BIND_INT64(stmts[STMT_INSERT_KEY], i, recno);
		SQL_STEP(stmts[STMT_INSERT_KEY]);
		sqlite3_reset(stmts[STMT_INSERT_KEY]);
		if (key->rendered != key->key)
			free(key->rendered);
		free(key);
	}
}

static void
dbprune(void)
{
	struct mpage	*mpage;
	struct mlink	*mlink;
	size_t		 i;
	unsigned int	 slot;

	if (0 == nodb)
		SQL_EXEC("BEGIN TRANSACTION");

	for (mpage = ohash_first(&mpages, &slot); NULL != mpage;
	     mpage = ohash_next(&mpages, &slot)) {
		mlink = mpage->mlinks;
		if (verb)
			say(mlink->file, "Deleting from database");
		if (nodb)
			continue;
		for ( ; NULL != mlink; mlink = mlink->next) {
			i = 1;
			SQL_BIND_TEXT(stmts[STMT_DELETE_PAGE],
			    i, mlink->dsec);
			SQL_BIND_TEXT(stmts[STMT_DELETE_PAGE],
			    i, mlink->arch);
			SQL_BIND_TEXT(stmts[STMT_DELETE_PAGE],
			    i, mlink->name);
			SQL_STEP(stmts[STMT_DELETE_PAGE]);
			sqlite3_reset(stmts[STMT_DELETE_PAGE]);
		}
	}

	if (0 == nodb)
		SQL_EXEC("END TRANSACTION");
}

/*
 * Close an existing database and its prepared statements.
 * If "real" is not set, rename the temporary file into the real one.
 */
static void
dbclose(int real)
{
	size_t		 i;
	int		 status;
	pid_t		 child;

	if (nodb)
		return;

	for (i = 0; i < STMT__MAX; i++) {
		sqlite3_finalize(stmts[i]);
		stmts[i] = NULL;
	}

	sqlite3_close(db);
	db = NULL;

	if (real)
		return;

	if ('\0' == *tempfilename) {
		if (-1 == rename(MANDOC_DB "~", MANDOC_DB)) {
			exitcode = (int)MANDOCLEVEL_SYSERR;
			say(MANDOC_DB, "%s", strerror(errno));
		}
		return;
	}

	switch (child = fork()) {
	case (-1):
		exitcode = (int)MANDOCLEVEL_SYSERR;
		say("fork cmp", "%s", strerror(errno));
		return;
	case (0):
		execlp("cmp", "cmp", "-s",
		    tempfilename, MANDOC_DB, NULL);
		say("exec cmp", "%s", strerror(errno));
		exit(0);
	default:
		break;
	}
	if (-1 == waitpid(child, &status, 0)) {
		exitcode = (int)MANDOCLEVEL_SYSERR;
		say("wait cmp", "%s", strerror(errno));
	} else if (WIFSIGNALED(status)) {
		exitcode = (int)MANDOCLEVEL_SYSERR;
		say("cmp", "Died from a signal");
	} else if (WEXITSTATUS(status)) {
		exitcode = (int)MANDOCLEVEL_SYSERR;
		say(MANDOC_DB,
		    "Data changed, but cannot replace database");
	}

	*strrchr(tempfilename, '/') = '\0';
	switch (child = fork()) {
	case (-1):
		exitcode = (int)MANDOCLEVEL_SYSERR;
		say("fork rm", "%s", strerror(errno));
		return;
	case (0):
		execlp("rm", "rm", "-rf", tempfilename, NULL);
		say("exec rm", "%s", strerror(errno));
		exit((int)MANDOCLEVEL_SYSERR);
	default:
		break;
	}
	if (-1 == waitpid(child, &status, 0)) {
		exitcode = (int)MANDOCLEVEL_SYSERR;
		say("wait rm", "%s", strerror(errno));
	} else if (WIFSIGNALED(status) || WEXITSTATUS(status)) {
		exitcode = (int)MANDOCLEVEL_SYSERR;
		say(tempfilename,
		    "Cannot remove temporary directory");
	}
}

/*
 * This is straightforward stuff.
 * Open a database connection to a "temporary" database, then open a set
 * of prepared statements we'll use over and over again.
 * If "real" is set, we use the existing database; if not, we truncate a
 * temporary one.
 * Must be matched by dbclose().
 */
static int
dbopen(int real)
{
	const char	*sql;
	int		 rc, ofl;

	if (nodb) 
		return(1);

	*tempfilename = '\0';
	ofl = SQLITE_OPEN_READWRITE;

	if (real) {
		rc = sqlite3_open_v2(MANDOC_DB, &db, ofl, NULL);
		if (SQLITE_OK != rc) {
			exitcode = (int)MANDOCLEVEL_SYSERR;
			say(MANDOC_DB, "%s", sqlite3_errmsg(db));
			return(0);
		}
		goto prepare_statements;
	}

	ofl |= SQLITE_OPEN_CREATE | SQLITE_OPEN_EXCLUSIVE;

	remove(MANDOC_DB "~");
	rc = sqlite3_open_v2(MANDOC_DB "~", &db, ofl, NULL);
	if (SQLITE_OK == rc) 
		goto create_tables;
	if (quick) {
		exitcode = (int)MANDOCLEVEL_SYSERR;
		say(MANDOC_DB "~", "%s", sqlite3_errmsg(db));
		return(0);
	}

	if (strlcpy(tempfilename, "/tmp/mandocdb.XXXXXX",
	    sizeof(tempfilename)) >= sizeof(tempfilename)) {
		exitcode = (int)MANDOCLEVEL_SYSERR;
		say("/tmp/mandocdb.XXXXXX", "Filename too long");
		return(0);
	}
	if (NULL == mkdtemp(tempfilename)) {
		exitcode = (int)MANDOCLEVEL_SYSERR;
		say(tempfilename, "%s", strerror(errno));
		return(0);
	}
	if (strlcat(tempfilename, "/" MANDOC_DB,
	    sizeof(tempfilename)) >= sizeof(tempfilename)) {
		exitcode = (int)MANDOCLEVEL_SYSERR;
		say(tempfilename, "Filename too long");
		return(0);
	}
	rc = sqlite3_open_v2(tempfilename, &db, ofl, NULL);
	if (SQLITE_OK != rc) {
		exitcode = (int)MANDOCLEVEL_SYSERR;
		say(tempfilename, "%s", sqlite3_errmsg(db));
		return(0);
	}

create_tables:
	sql = "CREATE TABLE \"mpages\" (\n"
	      " \"form\" INTEGER NOT NULL,\n"
	      " \"id\" INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL\n"
	      ");\n"
	      "\n"
	      "CREATE TABLE \"mlinks\" (\n"
	      " \"sec\" TEXT NOT NULL,\n"
	      " \"arch\" TEXT NOT NULL,\n"
	      " \"name\" TEXT NOT NULL,\n"
	      " \"pageid\" INTEGER NOT NULL REFERENCES mpages(id) "
		"ON DELETE CASCADE\n"
	      ");\n"
	      "\n"
	      "CREATE TABLE \"keys\" (\n"
	      " \"bits\" INTEGER NOT NULL,\n"
	      " \"key\" TEXT NOT NULL,\n"
	      " \"pageid\" INTEGER NOT NULL REFERENCES mpages(id) "
		"ON DELETE CASCADE\n"
	      ");\n";

	if (SQLITE_OK != sqlite3_exec(db, sql, NULL, NULL, NULL)) {
		exitcode = (int)MANDOCLEVEL_SYSERR;
		say(MANDOC_DB, "%s", sqlite3_errmsg(db));
		return(0);
	}

prepare_statements:
	SQL_EXEC("PRAGMA foreign_keys = ON");
	sql = "DELETE FROM mpages WHERE id IN "
		"(SELECT pageid FROM mlinks WHERE "
		"sec=? AND arch=? AND name=?)";
	sqlite3_prepare_v2(db, sql, -1, &stmts[STMT_DELETE_PAGE], NULL);
	sql = "INSERT INTO mpages "
		"(form) VALUES (?)";
	sqlite3_prepare_v2(db, sql, -1, &stmts[STMT_INSERT_PAGE], NULL);
	sql = "INSERT INTO mlinks "
		"(sec,arch,name,pageid) VALUES (?,?,?,?)";
	sqlite3_prepare_v2(db, sql, -1, &stmts[STMT_INSERT_LINK], NULL);
	sql = "INSERT INTO keys "
		"(bits,key,pageid) VALUES (?,?,?)";
	sqlite3_prepare_v2(db, sql, -1, &stmts[STMT_INSERT_KEY], NULL);

	/*
	 * When opening a new database, we can turn off
	 * synchronous mode for much better performance.
	 */

	if (real)
		SQL_EXEC("PRAGMA synchronous = OFF");

	return(1);
}

static void *
hash_halloc(size_t sz, void *arg)
{

	return(mandoc_calloc(sz, 1));
}

static void *
hash_alloc(size_t sz, void *arg)
{

	return(mandoc_malloc(sz));
}

static void
hash_free(void *p, size_t sz, void *arg)
{

	free(p);
}

static int
set_basedir(const char *targetdir)
{
	static char	 startdir[PATH_MAX];
	static int	 fd;

	/*
	 * Remember where we started by keeping a fd open to the origin
	 * path component: throughout this utility, we chdir() a lot to
	 * handle relative paths, and by doing this, we can return to
	 * the starting point.
	 */
	if ('\0' == *startdir) {
		if (NULL == getcwd(startdir, PATH_MAX)) {
			exitcode = (int)MANDOCLEVEL_SYSERR;
			if (NULL != targetdir)
				say(".", NULL);
			return(0);
		}
		if (-1 == (fd = open(startdir, O_RDONLY, 0))) {
			exitcode = (int)MANDOCLEVEL_SYSERR;
			say(startdir, NULL);
			return(0);
		}
		if (NULL == targetdir)
			targetdir = startdir;
	} else {
		if (-1 == fd)
			return(0);
		if (-1 == fchdir(fd)) {
			close(fd);
			basedir[0] = '\0';
			exitcode = (int)MANDOCLEVEL_SYSERR;
			say(startdir, NULL);
			return(0);
		}
		if (NULL == targetdir) {
			close(fd);
			return(1);
		}
	}
	if (NULL == realpath(targetdir, basedir)) {
		basedir[0] = '\0';
		exitcode = (int)MANDOCLEVEL_BADARG;
		say(targetdir, NULL);
		return(0);
	} else if (-1 == chdir(basedir)) {
		exitcode = (int)MANDOCLEVEL_BADARG;
		say("", NULL);
		return(0);
	}
	return(1);
}

static void
say(const char *file, const char *format, ...)
{
	va_list		 ap;

	if ('\0' != *basedir)
		fprintf(stderr, "%s", basedir);
	if ('\0' != *basedir && '\0' != *file)
		fputs("//", stderr);
	if ('\0' != *file)
		fprintf(stderr, "%s", file);
	fputs(": ", stderr);

	if (NULL == format) {
		perror(NULL);
		return;
	}

	va_start(ap, format);
	vfprintf(stderr, format, ap);
	va_end(ap);

	fputc('\n', stderr);
}
