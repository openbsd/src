/*	$OpenBSD: rcs.c,v 1.37 2007/05/17 03:33:24 ray Exp $	*/
/*
 * Copyright (c) 2004 Jean-Francois Brousseau <jfb@openbsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL  DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/stat.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <libgen.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "diff.h"
#include "rcs.h"
#include "rcsprog.h"
#include "rcsutil.h"
#include "xmalloc.h"

#define RCS_BUFSIZE	16384
#define RCS_BUFEXTSIZE	8192
#define RCS_KWEXP_SIZE  1024

/* RCS token types */
#define RCS_TOK_ERR	-1
#define RCS_TOK_EOF	0
#define RCS_TOK_NUM	1
#define RCS_TOK_ID	2
#define RCS_TOK_STRING	3
#define RCS_TOK_SCOLON	4
#define RCS_TOK_COLON	5

#define RCS_TOK_HEAD		8
#define RCS_TOK_BRANCH		9
#define RCS_TOK_ACCESS		10
#define RCS_TOK_SYMBOLS		11
#define RCS_TOK_LOCKS		12
#define RCS_TOK_COMMENT		13
#define RCS_TOK_EXPAND		14
#define RCS_TOK_DATE		15
#define RCS_TOK_AUTHOR		16
#define RCS_TOK_STATE		17
#define RCS_TOK_NEXT		18
#define RCS_TOK_BRANCHES	19
#define RCS_TOK_DESC		20
#define RCS_TOK_LOG		21
#define RCS_TOK_TEXT		22
#define RCS_TOK_STRICT		23

#define RCS_ISKEY(t)	(((t) >= RCS_TOK_HEAD) && ((t) <= RCS_TOK_BRANCHES))

#define RCS_NOSCOL	0x01	/* no terminating semi-colon */
#define RCS_VOPT	0x02	/* value is optional */

/* opaque parse data */
struct rcs_pdata {
	u_int	rp_lines;

	char	*rp_buf;
	size_t	 rp_blen;
	char	*rp_bufend;
	size_t	 rp_tlen;

	/* pushback token buffer */
	char	rp_ptok[128];
	int	rp_pttype;	/* token type, RCS_TOK_ERR if no token */

	FILE	*rp_file;
};

#define RCS_TOKSTR(rfp)	((struct rcs_pdata *)rfp->rf_pdata)->rp_buf
#define RCS_TOKLEN(rfp)	((struct rcs_pdata *)rfp->rf_pdata)->rp_tlen

/* invalid characters in RCS symbol names */
static const char rcs_sym_invch[] = RCS_SYM_INVALCHAR;

/* comment leaders, depending on the file's suffix */
static const struct rcs_comment {
	const char	*rc_suffix;
	const char	*rc_cstr;
} rcs_comments[] = {
	{ "1",    ".\\\" " },
	{ "2",    ".\\\" " },
	{ "3",    ".\\\" " },
	{ "4",    ".\\\" " },
	{ "5",    ".\\\" " },
	{ "6",    ".\\\" " },
	{ "7",    ".\\\" " },
	{ "8",    ".\\\" " },
	{ "9",    ".\\\" " },
	{ "a",    "-- "    },	/* Ada		 */
	{ "ada",  "-- "    },
	{ "adb",  "-- "    },
	{ "asm",  ";; "    },	/* assembler (MS-DOS) */
	{ "ads",  "-- "    },	/* Ada */
	{ "bat",  ":: "    },	/* batch (MS-DOS) */
	{ "body", "-- "    },	/* Ada */
	{ "c",    " * "    },	/* C */
	{ "c++",  "// "    },	/* C++ */
	{ "cc",   "// "    },
	{ "cpp",  "// "    },
	{ "cxx",  "// "    },
	{ "m",    "// "    },	/* Objective-C */
	{ "cl",   ";;; "   },	/* Common Lisp	 */
	{ "cmd",  ":: "    },	/* command (OS/2) */
	{ "cmf",  "c "     },	/* CM Fortran	 */
	{ "csh",  "# "     },	/* shell	 */
	{ "e",    "# "     },	/* efl		 */
	{ "epsf", "% "     },	/* encapsulated postscript */
	{ "epsi", "% "     },	/* encapsulated postscript */
	{ "el",   "; "     },	/* Emacs Lisp	 */
	{ "f",    "c "     },	/* Fortran	 */
	{ "for",  "c "     },
	{ "h",    " * "    },	/* C-header	 */
	{ "hh",   "// "    },	/* C++ header	 */
	{ "hpp",  "// "    },
	{ "hxx",  "// "    },
	{ "in",   "# "     },	/* for Makefile.in */
	{ "l",    " * "    },	/* lex */
	{ "mac",  ";; "    },	/* macro (DEC-10, MS-DOS, PDP-11, VMS, etc) */
	{ "mak",  "# "     },	/* makefile, e.g. Visual C++ */
	{ "me",   ".\\\" " },	/* me-macros	t/nroff	 */
	{ "ml",   "; "     },	/* mocklisp	 */
	{ "mm",   ".\\\" " },	/* mm-macros	t/nroff	 */
	{ "ms",   ".\\\" " },	/* ms-macros	t/nroff	 */
	{ "man",  ".\\\" " },	/* man-macros	t/nroff	 */
	{ "p",    " * "    },	/* pascal	 */
	{ "pas",  " * "    },
	{ "pl",   "# "     },	/* Perl	(conflict with Prolog) */
	{ "pm",   "# "     },	/* Perl	module */
	{ "ps",   "% "     },	/* postscript */
	{ "psw",  "% "     },	/* postscript wrap */
	{ "pswm", "% "     },	/* postscript wrap */
	{ "r",    "# "     },	/* ratfor	 */
	{ "rc",   " * "    },	/* Microsoft Windows resource file */
	{ "red",  "% "     },	/* psl/rlisp	 */
	{ "sh",   "# "     },	/* shell	 */
	{ "sl",   "% "     },	/* psl		 */
	{ "spec", "-- "    },	/* Ada		 */
	{ "tex",  "% "     },	/* tex		 */
	{ "y",    " * "    },	/* yacc		 */
	{ "ye",   " * "    },	/* yacc-efl	 */
	{ "yr",   " * "    },	/* yacc-ratfor	 */
};

struct rcs_kw rcs_expkw[] =  {
	{ "Author",	RCS_KW_AUTHOR   },
	{ "Date",	RCS_KW_DATE     },
	{ "Header",	RCS_KW_HEADER   },
	{ "Id",		RCS_KW_ID       },
	{ "Log",	RCS_KW_LOG      },
	{ "Name",	RCS_KW_NAME     },
	{ "RCSfile",	RCS_KW_RCSFILE  },
	{ "Revision",	RCS_KW_REVISION },
	{ "Source",	RCS_KW_SOURCE   },
	{ "State",	RCS_KW_STATE    },
};

#define NB_COMTYPES	(sizeof(rcs_comments)/sizeof(rcs_comments[0]))

static struct rcs_key {
	char	rk_str[16];
	int	rk_id;
	int	rk_val;
	int	rk_flags;
} rcs_keys[] = {
	{ "access",   RCS_TOK_ACCESS,   RCS_TOK_ID,     RCS_VOPT     },
	{ "author",   RCS_TOK_AUTHOR,   RCS_TOK_ID,     0            },
	{ "branch",   RCS_TOK_BRANCH,   RCS_TOK_NUM,    RCS_VOPT     },
	{ "branches", RCS_TOK_BRANCHES, RCS_TOK_NUM,    RCS_VOPT     },
	{ "comment",  RCS_TOK_COMMENT,  RCS_TOK_STRING, RCS_VOPT     },
	{ "date",     RCS_TOK_DATE,     RCS_TOK_NUM,    0            },
	{ "desc",     RCS_TOK_DESC,     RCS_TOK_STRING, RCS_NOSCOL   },
	{ "expand",   RCS_TOK_EXPAND,   RCS_TOK_STRING, RCS_VOPT     },
	{ "head",     RCS_TOK_HEAD,     RCS_TOK_NUM,    RCS_VOPT     },
	{ "locks",    RCS_TOK_LOCKS,    RCS_TOK_ID,     0            },
	{ "log",      RCS_TOK_LOG,      RCS_TOK_STRING, RCS_NOSCOL   },
	{ "next",     RCS_TOK_NEXT,     RCS_TOK_NUM,    RCS_VOPT     },
	{ "state",    RCS_TOK_STATE,    RCS_TOK_ID,     RCS_VOPT     },
	{ "strict",   RCS_TOK_STRICT,   0,              0,           },
	{ "symbols",  RCS_TOK_SYMBOLS,  0,              0            },
	{ "text",     RCS_TOK_TEXT,     RCS_TOK_STRING, RCS_NOSCOL   },
};

#define RCS_NKEYS	(sizeof(rcs_keys)/sizeof(rcs_keys[0]))

static const char *rcs_errstrs[] = {
	"No error",
	"No such entry",
	"Duplicate entry found",
	"Bad RCS number",
	"Invalid RCS symbol",
	"Parse error",
};

#define RCS_NERR   (sizeof(rcs_errstrs)/sizeof(rcs_errstrs[0]))

int rcs_errno = RCS_ERR_NOERR;
char *timezone_flag = NULL;

int		rcs_patch_lines(struct rcs_lines *, struct rcs_lines *);
static int	rcs_movefile(char *, char *, mode_t, u_int);
static void	rcs_parse_init(RCSFILE *);
static int	rcs_parse_admin(RCSFILE *);
static int	rcs_parse_delta(RCSFILE *);
static void	rcs_parse_deltas(RCSFILE *, RCSNUM *);
static int	rcs_parse_deltatext(RCSFILE *);
static void	rcs_parse_deltatexts(RCSFILE *, RCSNUM *);
static void	rcs_parse_desc(RCSFILE *, RCSNUM *);

static int	rcs_parse_access(RCSFILE *);
static int	rcs_parse_symbols(RCSFILE *);
static int	rcs_parse_locks(RCSFILE *);
static int	rcs_parse_branches(RCSFILE *, struct rcs_delta *);
static void	rcs_freedelta(struct rcs_delta *);
static void	rcs_freepdata(struct rcs_pdata *);
static int	rcs_gettok(RCSFILE *);
static int	rcs_pushtok(RCSFILE *, const char *, int);
static void	rcs_growbuf(RCSFILE *);
static void	rcs_strprint(const u_char *, size_t, FILE *);

static BUF	*rcs_expand_keywords(char *, struct rcs_delta *, BUF *, int);

RCSFILE *
rcs_open(const char *path, int fd, int flags, ...)
{
	int mode;
	mode_t fmode;
	RCSFILE *rfp;
	va_list vap;
	struct rcs_delta *rdp;
	struct rcs_lock *lkr;

	fmode = S_IRUSR|S_IRGRP|S_IROTH;
	flags &= 0xffff;	/* ditch any internal flags */

	if (flags & RCS_CREATE) {
		va_start(vap, flags);
		mode = va_arg(vap, int);
		va_end(vap);
		fmode = (mode_t)mode;
	}

	rfp = xcalloc(1, sizeof(*rfp));

	rfp->rf_path = xstrdup(path);
	rfp->rf_flags = flags | RCS_SLOCK | RCS_SYNCED;
	rfp->rf_mode = fmode;
	rfp->rf_fd = fd;

	TAILQ_INIT(&(rfp->rf_delta));
	TAILQ_INIT(&(rfp->rf_access));
	TAILQ_INIT(&(rfp->rf_symbols));
	TAILQ_INIT(&(rfp->rf_locks));

	if (!(rfp->rf_flags & RCS_CREATE)) {
		rcs_parse_init(rfp);

		/* fill in rd_locker */
		TAILQ_FOREACH(lkr, &(rfp->rf_locks), rl_list) {
			if ((rdp = rcs_findrev(rfp, lkr->rl_num)) == NULL) {
				rcs_close(rfp);
				return (NULL);
			}

			rdp->rd_locker = xstrdup(lkr->rl_name);
		}
	}

	return (rfp);
}

/*
 * rcs_close()
 *
 * Close an RCS file handle.
 */
void
rcs_close(RCSFILE *rfp)
{
	struct rcs_delta *rdp;
	struct rcs_access *rap;
	struct rcs_lock *rlp;
	struct rcs_sym *rsp;

	if ((rfp->rf_flags & RCS_WRITE) && !(rfp->rf_flags & RCS_SYNCED))
		rcs_write(rfp);

	while (!TAILQ_EMPTY(&(rfp->rf_delta))) {
		rdp = TAILQ_FIRST(&(rfp->rf_delta));
		TAILQ_REMOVE(&(rfp->rf_delta), rdp, rd_list);
		rcs_freedelta(rdp);
	}

	while (!TAILQ_EMPTY(&(rfp->rf_access))) {
		rap = TAILQ_FIRST(&(rfp->rf_access));
		TAILQ_REMOVE(&(rfp->rf_access), rap, ra_list);
		xfree(rap->ra_name);
		xfree(rap);
	}

	while (!TAILQ_EMPTY(&(rfp->rf_symbols))) {
		rsp = TAILQ_FIRST(&(rfp->rf_symbols));
		TAILQ_REMOVE(&(rfp->rf_symbols), rsp, rs_list);
		rcsnum_free(rsp->rs_num);
		xfree(rsp->rs_name);
		xfree(rsp);
	}

	while (!TAILQ_EMPTY(&(rfp->rf_locks))) {
		rlp = TAILQ_FIRST(&(rfp->rf_locks));
		TAILQ_REMOVE(&(rfp->rf_locks), rlp, rl_list);
		rcsnum_free(rlp->rl_num);
		xfree(rlp->rl_name);
		xfree(rlp);
	}

	if (rfp->rf_head != NULL)
		rcsnum_free(rfp->rf_head);
	if (rfp->rf_branch != NULL)
		rcsnum_free(rfp->rf_branch);

	if (rfp->rf_path != NULL)
		xfree(rfp->rf_path);
	if (rfp->rf_comment != NULL)
		xfree(rfp->rf_comment);
	if (rfp->rf_expand != NULL)
		xfree(rfp->rf_expand);
	if (rfp->rf_desc != NULL)
		xfree(rfp->rf_desc);
	if (rfp->rf_pdata != NULL)
		rcs_freepdata(rfp->rf_pdata);
	xfree(rfp);
}

/*
 * rcs_write()
 *
 * Write the contents of the RCS file handle <rfp> to disk in the file whose
 * path is in <rf_path>.
 */
void
rcs_write(RCSFILE *rfp)
{
	FILE *fp;
	char numbuf[64], *fn;
	struct rcs_access *ap;
	struct rcs_sym *symp;
	struct rcs_branch *brp;
	struct rcs_delta *rdp;
	struct rcs_lock *lkp;
	size_t len;
	int fd;

	fn = NULL;
	fd = -1;

	if (rfp->rf_flags & RCS_SYNCED)
		return;

	/* Write operations need the whole file parsed */
	rcs_parse_deltatexts(rfp, NULL);

	(void)xasprintf(&fn, "%s/rcs.XXXXXXXXXX", rcs_tmpdir);

	if ((fd = mkstemp(fn)) == -1)
		err(1, "%s", fn);

	if ((fp = fdopen(fd, "w+")) == NULL) {
		int saved_errno;

		saved_errno = errno;
		(void)unlink(fn);
		errno = saved_errno;
		err(1, "%s", fn);
	}

	if (rfp->rf_head != NULL)
		rcsnum_tostr(rfp->rf_head, numbuf, sizeof(numbuf));
	else
		numbuf[0] = '\0';

	fprintf(fp, "head\t%s;\n", numbuf);

	if (rfp->rf_branch != NULL) {
		rcsnum_tostr(rfp->rf_branch, numbuf, sizeof(numbuf));
		fprintf(fp, "branch\t%s;\n", numbuf);
	}

	fputs("access", fp);
	TAILQ_FOREACH(ap, &(rfp->rf_access), ra_list) {
		fprintf(fp, "\n\t%s", ap->ra_name);
	}
	fputs(";\n", fp);

	fprintf(fp, "symbols");
	TAILQ_FOREACH(symp, &(rfp->rf_symbols), rs_list) {
		rcsnum_tostr(symp->rs_num, numbuf, sizeof(numbuf));
		fprintf(fp, "\n\t%s:%s", symp->rs_name, numbuf);
	}
	fprintf(fp, ";\n");

	fprintf(fp, "locks");
	TAILQ_FOREACH(lkp, &(rfp->rf_locks), rl_list) {
		rcsnum_tostr(lkp->rl_num, numbuf, sizeof(numbuf));
		fprintf(fp, "\n\t%s:%s", lkp->rl_name, numbuf);
	}

	fprintf(fp, ";");

	if (rfp->rf_flags & RCS_SLOCK)
		fprintf(fp, " strict;");
	fputc('\n', fp);

	fputs("comment\t@", fp);
	if (rfp->rf_comment != NULL) {
		rcs_strprint((const u_char *)rfp->rf_comment,
		    strlen(rfp->rf_comment), fp);
		fputs("@;\n", fp);
	} else
		fputs("# @;\n", fp);

	if (rfp->rf_expand != NULL) {
		fputs("expand @", fp);
		rcs_strprint((const u_char *)rfp->rf_expand,
		    strlen(rfp->rf_expand), fp);
		fputs("@;\n", fp);
	}

	fputs("\n\n", fp);

	TAILQ_FOREACH(rdp, &(rfp->rf_delta), rd_list) {
		fprintf(fp, "%s\n", rcsnum_tostr(rdp->rd_num, numbuf,
		    sizeof(numbuf)));
		fprintf(fp, "date\t%d.%02d.%02d.%02d.%02d.%02d;",
		    rdp->rd_date.tm_year + 1900, rdp->rd_date.tm_mon + 1,
		    rdp->rd_date.tm_mday, rdp->rd_date.tm_hour,
		    rdp->rd_date.tm_min, rdp->rd_date.tm_sec);
		fprintf(fp, "\tauthor %s;\tstate %s;\n",
		    rdp->rd_author, rdp->rd_state);
		fputs("branches", fp);
		TAILQ_FOREACH(brp, &(rdp->rd_branches), rb_list) {
			fprintf(fp, " %s", rcsnum_tostr(brp->rb_num, numbuf,
			    sizeof(numbuf)));
		}
		fputs(";\n", fp);
		fprintf(fp, "next\t%s;\n\n", rcsnum_tostr(rdp->rd_next,
		    numbuf, sizeof(numbuf)));
	}

	fputs("\ndesc\n@", fp);
	if (rfp->rf_desc != NULL && (len = strlen(rfp->rf_desc)) > 0) {
		rcs_strprint((const u_char *)rfp->rf_desc, len, fp);
		if (rfp->rf_desc[len-1] != '\n')
			fputc('\n', fp);
	}
	fputs("@\n", fp);

	/* deltatexts */
	TAILQ_FOREACH(rdp, &(rfp->rf_delta), rd_list) {
		fprintf(fp, "\n\n%s\n", rcsnum_tostr(rdp->rd_num, numbuf,
		    sizeof(numbuf)));
		fputs("log\n@", fp);
		if (rdp->rd_log != NULL) {
			len = strlen(rdp->rd_log);
			rcs_strprint((const u_char *)rdp->rd_log, len, fp);
			if (rdp->rd_log[len-1] != '\n')
				fputc('\n', fp);
		}
		fputs("@\ntext\n@", fp);
		if (rdp->rd_text != NULL)
			rcs_strprint(rdp->rd_text, rdp->rd_tlen, fp);
		fputs("@\n", fp);
	}
	(void)fclose(fp);

	if (rcs_movefile(fn, rfp->rf_path, rfp->rf_mode, rfp->rf_flags) == -1) {
		(void)unlink(fn);
		errx(1, "rcs_movefile failed");
	}

	rfp->rf_flags |= RCS_SYNCED;

	if (fn != NULL)
		xfree(fn);
}

/*
 * rcs_movefile()
 *
 * Move a file using rename(2) if possible and copying if not.
 * Returns 0 on success, -1 on failure.
 */
static int
rcs_movefile(char *from, char *to, mode_t perm, u_int to_flags)
{
	FILE *src, *dst;
	size_t nread, nwritten;
	char *buf;
	int ret;

	ret = -1;

	if (rename(from, to) == 0) {
		if (chmod(to, perm) == -1) {
			warn("%s", to);
			return (-1);
		}
		return (0);
	} else if (errno != EXDEV) {
		warn("failed to access temp RCS output file");
		return (-1);
	}

	if ((chmod(to, S_IWUSR) == -1) && !(to_flags & RCS_CREATE)) {
		warnx("chmod(%s, 0%o) failed", to, S_IWUSR);
		return (-1);
	}

	/* different filesystem, have to copy the file */
	if ((src = fopen(from, "r")) == NULL) {
		warn("%s", from);
		return (-1);
	}
	if ((dst = fopen(to, "w")) == NULL) {
		warn("%s", to);
		return (-1);
	}
	if (fchmod(fileno(dst), perm)) {
		warn("%s", to);
		(void)unlink(to);
		return (-1);
	}

	buf = xmalloc(MAXBSIZE);
	while ((nread = fread(buf, sizeof(char), MAXBSIZE, src)) != 0) {
		if (ferror(src)) {
			warnx("failed to read `%s'", from);
			(void)unlink(to);
			goto out;
		}
		nwritten = fwrite(buf, sizeof(char), nread, dst);
		if (nwritten != nread) {
			warnx("failed to write `%s'", to);
			(void)unlink(to);
			goto out;
		}
	}

	ret = 0;

	(void)fclose(src);
	(void)fclose(dst);
	(void)unlink(from);

out:
	xfree(buf);

	return (ret);
}

/*
 * rcs_head_get()
 *
 * Retrieve the revision number of the head revision for the RCS file <file>.
 */
const RCSNUM *
rcs_head_get(RCSFILE *file)
{
	return (file->rf_head);
}

/*
 * rcs_head_set()
 *
 * Set the revision number of the head revision for the RCS file <file> to
 * <rev>, which must reference a valid revision within the file.
 */
int
rcs_head_set(RCSFILE *file, RCSNUM *rev)
{
	if (rcs_findrev(file, rev) == NULL)
		return (-1);

	if (file->rf_head == NULL)
		file->rf_head = rcsnum_alloc();

	rcsnum_cpy(rev, file->rf_head, 0);
	file->rf_flags &= ~RCS_SYNCED;
	return (0);
}


/*
 * rcs_branch_get()
 *
 * Retrieve the default branch number for the RCS file <file>.
 * Returns the number on success.  If NULL is returned, then there is no
 * default branch for this file.
 */
const RCSNUM *
rcs_branch_get(RCSFILE *file)
{
	return (file->rf_branch);
}

/*
 * rcs_branch_set()
 *
 * Set the default branch for the RCS file <file> to <bnum>.
 * Returns 0 on success, -1 on failure.
 */
int
rcs_branch_set(RCSFILE *file, const RCSNUM *bnum)
{
	if (file->rf_branch == NULL)
		file->rf_branch = rcsnum_alloc();

	rcsnum_cpy(bnum, file->rf_branch, 0);
	file->rf_flags &= ~RCS_SYNCED;
	return (0);
}

/*
 * rcs_access_add()
 *
 * Add the login name <login> to the access list for the RCS file <file>.
 * Returns 0 on success, or -1 on failure.
 */
int
rcs_access_add(RCSFILE *file, const char *login)
{
	struct rcs_access *ap;

	/* first look for duplication */
	TAILQ_FOREACH(ap, &(file->rf_access), ra_list) {
		if (strcmp(ap->ra_name, login) == 0) {
			rcs_errno = RCS_ERR_DUPENT;
			return (-1);
		}
	}

	ap = xmalloc(sizeof(*ap));
	ap->ra_name = xstrdup(login);
	TAILQ_INSERT_TAIL(&(file->rf_access), ap, ra_list);

	/* not synced anymore */
	file->rf_flags &= ~RCS_SYNCED;
	return (0);
}

/*
 * rcs_access_remove()
 *
 * Remove an entry with login name <login> from the access list of the RCS
 * file <file>.
 * Returns 0 on success, or -1 on failure.
 */
int
rcs_access_remove(RCSFILE *file, const char *login)
{
	struct rcs_access *ap;

	TAILQ_FOREACH(ap, &(file->rf_access), ra_list)
		if (strcmp(ap->ra_name, login) == 0)
			break;

	if (ap == NULL) {
		rcs_errno = RCS_ERR_NOENT;
		return (-1);
	}

	TAILQ_REMOVE(&(file->rf_access), ap, ra_list);
	xfree(ap->ra_name);
	xfree(ap);

	/* not synced anymore */
	file->rf_flags &= ~RCS_SYNCED;
	return (0);
}

/*
 * rcs_sym_add()
 *
 * Add a symbol to the list of symbols for the RCS file <rfp>.  The new symbol
 * is named <sym> and is bound to the RCS revision <snum>.
 * Returns 0 on success, or -1 on failure.
 */
int
rcs_sym_add(RCSFILE *rfp, const char *sym, RCSNUM *snum)
{
	struct rcs_sym *symp;

	if (!rcs_sym_check(sym)) {
		rcs_errno = RCS_ERR_BADSYM;
		return (-1);
	}

	/* first look for duplication */
	TAILQ_FOREACH(symp, &(rfp->rf_symbols), rs_list) {
		if (strcmp(symp->rs_name, sym) == 0) {
			rcs_errno = RCS_ERR_DUPENT;
			return (-1);
		}
	}

	symp = xmalloc(sizeof(*symp));
	symp->rs_name = xstrdup(sym);
	symp->rs_num = rcsnum_alloc();
	rcsnum_cpy(snum, symp->rs_num, 0);

	TAILQ_INSERT_HEAD(&(rfp->rf_symbols), symp, rs_list);

	/* not synced anymore */
	rfp->rf_flags &= ~RCS_SYNCED;
	return (0);
}

/*
 * rcs_sym_remove()
 *
 * Remove the symbol with name <sym> from the symbol list for the RCS file
 * <file>.  If no such symbol is found, the call fails and returns with an
 * error.
 * Returns 0 on success, or -1 on failure.
 */
int
rcs_sym_remove(RCSFILE *file, const char *sym)
{
	struct rcs_sym *symp;

	if (!rcs_sym_check(sym)) {
		rcs_errno = RCS_ERR_BADSYM;
		return (-1);
	}

	TAILQ_FOREACH(symp, &(file->rf_symbols), rs_list)
		if (strcmp(symp->rs_name, sym) == 0)
			break;

	if (symp == NULL) {
		rcs_errno = RCS_ERR_NOENT;
		return (-1);
	}

	TAILQ_REMOVE(&(file->rf_symbols), symp, rs_list);
	xfree(symp->rs_name);
	rcsnum_free(symp->rs_num);
	xfree(symp);

	/* not synced anymore */
	file->rf_flags &= ~RCS_SYNCED;
	return (0);
}

/*
 * rcs_sym_getrev()
 *
 * Retrieve the RCS revision number associated with the symbol <sym> for the
 * RCS file <file>.  The returned value is a dynamically-allocated copy and
 * should be freed by the caller once they are done with it.
 * Returns the RCSNUM on success, or NULL on failure.
 */
RCSNUM *
rcs_sym_getrev(RCSFILE *file, const char *sym)
{
	RCSNUM *num;
	struct rcs_sym *symp;

	if (!rcs_sym_check(sym)) {
		rcs_errno = RCS_ERR_BADSYM;
		return (NULL);
	}

	num = NULL;
	TAILQ_FOREACH(symp, &(file->rf_symbols), rs_list)
		if (strcmp(symp->rs_name, sym) == 0)
			break;

	if (symp == NULL) {
		rcs_errno = RCS_ERR_NOENT;
	} else {
		num = rcsnum_alloc();
		rcsnum_cpy(symp->rs_num, num, 0);
	}

	return (num);
}

/*
 * rcs_sym_check()
 *
 * Check the RCS symbol name <sym> for any unsupported characters.
 * Returns 1 if the tag is correct, 0 if it isn't valid.
 */
int
rcs_sym_check(const char *sym)
{
	int ret;
	const char *cp;

	ret = 1;
	cp = sym;
	if (!isalpha(*cp++))
		return (0);

	for (; *cp != '\0'; cp++)
		if (!isgraph(*cp) || (strchr(rcs_sym_invch, *cp) != NULL)) {
			ret = 0;
			break;
		}

	return (ret);
}

/*
 * rcs_lock_getmode()
 *
 * Retrieve the locking mode of the RCS file <file>.
 */
int
rcs_lock_getmode(RCSFILE *file)
{
	return (file->rf_flags & RCS_SLOCK) ? RCS_LOCK_STRICT : RCS_LOCK_LOOSE;
}

/*
 * rcs_lock_setmode()
 *
 * Set the locking mode of the RCS file <file> to <mode>, which must either
 * be RCS_LOCK_LOOSE or RCS_LOCK_STRICT.
 * Returns the previous mode on success, or -1 on failure.
 */
int
rcs_lock_setmode(RCSFILE *file, int mode)
{
	int pmode;
	pmode = rcs_lock_getmode(file);

	if (mode == RCS_LOCK_STRICT)
		file->rf_flags |= RCS_SLOCK;
	else if (mode == RCS_LOCK_LOOSE)
		file->rf_flags &= ~RCS_SLOCK;
	else
		errx(1, "rcs_lock_setmode: invalid mode `%d'", mode);

	file->rf_flags &= ~RCS_SYNCED;
	return (pmode);
}

/*
 * rcs_lock_add()
 *
 * Add an RCS lock for the user <user> on revision <rev>.
 * Returns 0 on success, or -1 on failure.
 */
int
rcs_lock_add(RCSFILE *file, const char *user, RCSNUM *rev)
{
	struct rcs_lock *lkp;

	/* first look for duplication */
	TAILQ_FOREACH(lkp, &(file->rf_locks), rl_list) {
		if (strcmp(lkp->rl_name, user) == 0 &&
		    rcsnum_cmp(rev, lkp->rl_num, 0) == 0) {
			rcs_errno = RCS_ERR_DUPENT;
			return (-1);
		}
	}

	lkp = xmalloc(sizeof(*lkp));
	lkp->rl_name = xstrdup(user);
	lkp->rl_num = rcsnum_alloc();
	rcsnum_cpy(rev, lkp->rl_num, 0);

	TAILQ_INSERT_TAIL(&(file->rf_locks), lkp, rl_list);

	/* not synced anymore */
	file->rf_flags &= ~RCS_SYNCED;
	return (0);
}


/*
 * rcs_lock_remove()
 *
 * Remove the RCS lock on revision <rev>.
 * Returns 0 on success, or -1 on failure.
 */
int
rcs_lock_remove(RCSFILE *file, const char *user, RCSNUM *rev)
{
	struct rcs_lock *lkp;

	TAILQ_FOREACH(lkp, &(file->rf_locks), rl_list) {
		if (strcmp(lkp->rl_name, user) == 0 &&
		    rcsnum_cmp(lkp->rl_num, rev, 0) == 0)
			break;
	}

	if (lkp == NULL) {
		rcs_errno = RCS_ERR_NOENT;
		return (-1);
	}

	TAILQ_REMOVE(&(file->rf_locks), lkp, rl_list);
	rcsnum_free(lkp->rl_num);
	xfree(lkp->rl_name);
	xfree(lkp);

	/* not synced anymore */
	file->rf_flags &= ~RCS_SYNCED;
	return (0);
}

/*
 * rcs_desc_get()
 *
 * Retrieve the description for the RCS file <file>.
 */
const char *
rcs_desc_get(RCSFILE *file)
{
	return (file->rf_desc);
}

/*
 * rcs_desc_set()
 *
 * Set the description for the RCS file <file>.
 */
void
rcs_desc_set(RCSFILE *file, const char *desc)
{
	char *tmp;

	tmp = xstrdup(desc);
	if (file->rf_desc != NULL)
		xfree(file->rf_desc);
	file->rf_desc = tmp;
	file->rf_flags &= ~RCS_SYNCED;
}

/*
 * rcs_comment_lookup()
 *
 * Lookup the assumed comment leader based on a file's suffix.
 * Returns a pointer to the string on success, or NULL on failure.
 */
const char *
rcs_comment_lookup(const char *filename)
{
	int i;
	const char *sp;

	if ((sp = strrchr(filename, '.')) == NULL) {
		rcs_errno = RCS_ERR_NOENT;
		return (NULL);
	}
	sp++;

	for (i = 0; i < (int)NB_COMTYPES; i++)
		if (strcmp(rcs_comments[i].rc_suffix, sp) == 0)
			return (rcs_comments[i].rc_cstr);
	return (NULL);
}

/*
 * rcs_comment_get()
 *
 * Retrieve the comment leader for the RCS file <file>.
 */
const char *
rcs_comment_get(RCSFILE *file)
{
	return (file->rf_comment);
}

/*
 * rcs_comment_set()
 *
 * Set the comment leader for the RCS file <file>.
 */
void
rcs_comment_set(RCSFILE *file, const char *comment)
{
	char *tmp;

	tmp = xstrdup(comment);
	if (file->rf_comment != NULL)
		xfree(file->rf_comment);
	file->rf_comment = tmp;
	file->rf_flags &= ~RCS_SYNCED;
}

/*
 * rcs_tag_resolve()
 *
 * Retrieve the revision number corresponding to the tag <tag> for the RCS
 * file <file>.
 */
RCSNUM *
rcs_tag_resolve(RCSFILE *file, const char *tag)
{
	RCSNUM *num;

	if ((num = rcsnum_parse(tag)) == NULL) {
		num = rcs_sym_getrev(file, tag);
	}

	return (num);
}

int
rcs_patch_lines(struct rcs_lines *dlines, struct rcs_lines *plines)
{
	char op, *ep;
	struct rcs_line *lp, *dlp, *ndlp;
	int i, lineno, nbln;
	u_char tmp;

	dlp = TAILQ_FIRST(&(dlines->l_lines));
	lp = TAILQ_FIRST(&(plines->l_lines));

	/* skip first bogus line */
	for (lp = TAILQ_NEXT(lp, l_list); lp != NULL;
	    lp = TAILQ_NEXT(lp, l_list)) {
		if (lp->l_len < 2)
			errx(1, "line too short, RCS patch seems broken");
		op = *(lp->l_line);
		/* NUL-terminate line buffer for strtol() safety. */
		tmp = lp->l_line[lp->l_len - 1];
		lp->l_line[lp->l_len - 1] = '\0';
		lineno = (int)strtol((lp->l_line + 1), &ep, 10);
		if (lineno > dlines->l_nblines || lineno < 0 ||
		    *ep != ' ')
			errx(1, "invalid line specification in RCS patch");
		ep++;
		nbln = (int)strtol(ep, &ep, 10);
		/* Restore the last byte of the buffer */
		lp->l_line[lp->l_len - 1] = tmp;
		if (nbln < 0)
			errx(1,
			    "invalid line number specification in RCS patch");

		/* find the appropriate line */
		for (;;) {
			if (dlp == NULL)
				break;
			if (dlp->l_lineno == lineno)
				break;
			if (dlp->l_lineno > lineno) {
				dlp = TAILQ_PREV(dlp, rcs_tqh, l_list);
			} else if (dlp->l_lineno < lineno) {
				if (((ndlp = TAILQ_NEXT(dlp, l_list)) == NULL) ||
				    ndlp->l_lineno > lineno)
					break;
				dlp = ndlp;
			}
		}
		if (dlp == NULL)
			errx(1, "can't find referenced line in RCS patch");

		if (op == 'd') {
			for (i = 0; (i < nbln) && (dlp != NULL); i++) {
				ndlp = TAILQ_NEXT(dlp, l_list);
				TAILQ_REMOVE(&(dlines->l_lines), dlp, l_list);
				xfree(dlp);
				dlp = ndlp;
				/* last line is gone - reset dlp */
				if (dlp == NULL) {
					ndlp = TAILQ_LAST(&(dlines->l_lines),
					    rcs_tqh);
					dlp = ndlp;
				}
			}
		} else if (op == 'a') {
			for (i = 0; i < nbln; i++) {
				ndlp = lp;
				lp = TAILQ_NEXT(lp, l_list);
				if (lp == NULL)
					errx(1, "truncated RCS patch");
				TAILQ_REMOVE(&(plines->l_lines), lp, l_list);
				TAILQ_INSERT_AFTER(&(dlines->l_lines), dlp,
				    lp, l_list);
				dlp = lp;

				/* we don't want lookup to block on those */
				lp->l_lineno = lineno;

				lp = ndlp;
			}
		} else
			errx(1, "unknown RCS patch operation `%c'", op);

		/* last line of the patch, done */
		if (lp->l_lineno == plines->l_nblines)
			break;
	}

	/* once we're done patching, rebuild the line numbers */
	lineno = 0;
	TAILQ_FOREACH(lp, &(dlines->l_lines), l_list)
		lp->l_lineno = lineno++;
	dlines->l_nblines = lineno - 1;

	return (0);
}

/*
 * rcs_getrev()
 *
 * Get the whole contents of revision <rev> from the RCSFILE <rfp>.  The
 * returned buffer is dynamically allocated and should be released using
 * rcs_buf_free() once the caller is done using it.
 */
BUF*
rcs_getrev(RCSFILE *rfp, RCSNUM *frev)
{
	u_int i, numlen;
	int isbranch, lookonbranch, found;
	size_t dlen, plen, len;
	RCSNUM *crev, *rev, *brev;
	BUF *rbuf;
	struct rcs_delta *rdp = NULL;
	struct rcs_branch *rb;
	u_char *data, *patch;

	if (rfp->rf_head == NULL)
		return (NULL);

	if (frev == RCS_HEAD_REV)
		rev = rfp->rf_head;
	else
		rev = frev;

	/* XXX rcsnum_cmp() */
	for (i = 0; i < rfp->rf_head->rn_len; i++) {
		if (rfp->rf_head->rn_id[i] < rev->rn_id[i]) {
			rcs_errno = RCS_ERR_NOENT;
			return (NULL);
		}
	}

	/* No matter what, we're going to need up the the description parsed */
	rcs_parse_desc(rfp, NULL);

	rdp = rcs_findrev(rfp, rfp->rf_head);
	if (rdp == NULL) {
		warnx("failed to get RCS HEAD revision");
		return (NULL);
	}

	if (rdp->rd_tlen == 0)
		rcs_parse_deltatexts(rfp, rfp->rf_head);

	len = rdp->rd_tlen;
	if (len == 0) {
		rbuf = rcs_buf_alloc(1, 0);
		rcs_buf_empty(rbuf);
		return (rbuf);
	}

	rbuf = rcs_buf_alloc(len, BUF_AUTOEXT);
	rcs_buf_append(rbuf, rdp->rd_text, len);

	isbranch = 0;
	brev = NULL;

	/*
	 * If a branch was passed, get the latest revision on it.
	 */
	if (RCSNUM_ISBRANCH(rev)) {
		brev = rev;
		rdp = rcs_findrev(rfp, rev);
		if (rdp == NULL)
			return (NULL);

		rev = rdp->rd_num;
	} else {
		if (RCSNUM_ISBRANCHREV(rev)) {
			brev = rcsnum_revtobr(rev);
			isbranch = 1;
		}
	}

	lookonbranch = 0;
	crev = NULL;

	/* Apply patches backwards to get the right version.
	 */
	do {
		found = 0;

		if (rcsnum_cmp(rfp->rf_head, rev, 0) == 0)
			break;

		if (isbranch == 1 && rdp->rd_num->rn_len < rev->rn_len &&
		    !TAILQ_EMPTY(&(rdp->rd_branches)))
			lookonbranch = 1;

		if (isbranch && lookonbranch == 1) {
			lookonbranch = 0;
			TAILQ_FOREACH(rb, &(rdp->rd_branches), rb_list) {
				/* XXX rcsnum_cmp() is totally broken for
				 * this purpose.
				 */
				numlen = MIN(brev->rn_len, rb->rb_num->rn_len);
				for (i = 0; i < numlen; i++) {
					if (rb->rb_num->rn_id[i] !=
					    brev->rn_id[i])
						break;
				}

				if (i == numlen) {
					crev = rb->rb_num;
					found = 1;
					break;
				}
			}
			if (found == 0)
				crev = rdp->rd_next;
		} else {
			crev = rdp->rd_next;
		}

		rdp = rcs_findrev(rfp, crev);
		if (rdp == NULL) {
			rcs_buf_free(rbuf);
			return (NULL);
		}

		plen = rdp->rd_tlen;
		dlen = rcs_buf_len(rbuf);
		patch = rdp->rd_text;
		data = rcs_buf_release(rbuf);
		/* check if we have parsed this rev's deltatext */
		if (rdp->rd_tlen == 0)
			rcs_parse_deltatexts(rfp, rdp->rd_num);

		rbuf = rcs_patchfile(data, dlen, patch, plen, rcs_patch_lines);

		if (rbuf == NULL)
			break;
	} while (rcsnum_cmp(crev, rev, 0) != 0);

	return (rbuf);
}

/*
 * rcs_rev_add()
 *
 * Add a revision to the RCS file <rf>.  The new revision's number can be
 * specified in <rev> (which can also be RCS_HEAD_REV, in which case the
 * new revision will have a number equal to the previous head revision plus
 * one).  The <msg> argument specifies the log message for that revision, and
 * <date> specifies the revision's date (a value of -1 is
 * equivalent to using the current time).
 * If <username> is NULL, set the author for this revision to the current user.
 * Otherwise, set it to <username>.
 * Returns 0 on success, or -1 on failure.
 */
int
rcs_rev_add(RCSFILE *rf, RCSNUM *rev, const char *msg, time_t date,
    const char *username)
{
	time_t now;
	struct passwd *pw;
	struct rcs_delta *ordp, *rdp;

	if (rev == RCS_HEAD_REV) {
		if (rf->rf_flags & RCS_CREATE) {
			if ((rev = rcsnum_parse(RCS_HEAD_INIT)) == NULL)
				return (-1);
			rf->rf_head = rcsnum_alloc();
			rcsnum_cpy(rev, rf->rf_head, 0);
		} else {
			rev = rcsnum_inc(rf->rf_head);
		}
	} else {
		if ((rdp = rcs_findrev(rf, rev)) != NULL) {
			rcs_errno = RCS_ERR_DUPENT;
			return (-1);
		}
	}

	if ((pw = getpwuid(getuid())) == NULL)
		errx(1, "getpwuid failed");

	rdp = xcalloc(1, sizeof(*rdp));

	TAILQ_INIT(&(rdp->rd_branches));

	rdp->rd_num = rcsnum_alloc();
	rcsnum_cpy(rev, rdp->rd_num, 0);

	rdp->rd_next = rcsnum_alloc();

	if (!(rf->rf_flags & RCS_CREATE)) {
		/* next should point to the previous HEAD */
		ordp = TAILQ_FIRST(&(rf->rf_delta));
		rcsnum_cpy(ordp->rd_num, rdp->rd_next, 0);
	}


	if (username == NULL)
		username = pw->pw_name;

	rdp->rd_author = xstrdup(username);
	rdp->rd_state = xstrdup(RCS_STATE_EXP);
	rdp->rd_log = xstrdup(msg);

	if (date != (time_t)(-1))
		now = date;
	else
		time(&now);
	gmtime_r(&now, &(rdp->rd_date));

	TAILQ_INSERT_HEAD(&(rf->rf_delta), rdp, rd_list);
	rf->rf_ndelta++;

	/* not synced anymore */
	rf->rf_flags &= ~RCS_SYNCED;

	return (0);
}

/*
 * rcs_rev_remove()
 *
 * Remove the revision whose number is <rev> from the RCS file <rf>.
 */
int
rcs_rev_remove(RCSFILE *rf, RCSNUM *rev)
{
	char *path_tmp1, *path_tmp2;
	struct rcs_delta *rdp, *prevrdp, *nextrdp;
	BUF *newdeltatext, *nextbuf, *prevbuf, *newdiff;

	nextrdp = prevrdp = NULL;
	path_tmp1 = path_tmp2 = NULL;

	if (rev == RCS_HEAD_REV)
		rev = rf->rf_head;

	/* do we actually have that revision? */
	if ((rdp = rcs_findrev(rf, rev)) == NULL) {
		rcs_errno = RCS_ERR_NOENT;
		return (-1);
	}

	/*
	 * This is confusing, the previous delta is next in the TAILQ list.
	 * the next delta is the previous one in the TAILQ list.
	 *
	 * When the HEAD revision got specified, nextrdp will be NULL.
	 * When the first revision got specified, prevrdp will be NULL.
	 */
	prevrdp = (struct rcs_delta *)TAILQ_NEXT(rdp, rd_list);
	nextrdp = (struct rcs_delta *)TAILQ_PREV(rdp, rcs_tqh, rd_list);

	newdeltatext = prevbuf = nextbuf = NULL;

	if (prevrdp != NULL) {
		if ((prevbuf = rcs_getrev(rf, prevrdp->rd_num)) == NULL)
			errx(1, "error getting revision");
	}

	if (prevrdp != NULL && nextrdp != NULL) {
		if ((nextbuf = rcs_getrev(rf, nextrdp->rd_num)) == NULL)
			errx(1, "error getting revision");

		newdiff = rcs_buf_alloc(64, BUF_AUTOEXT);

		/* calculate new diff */
		(void)xasprintf(&path_tmp1, "%s/diff1.XXXXXXXXXX", rcs_tmpdir);
		rcs_buf_write_stmp(nextbuf, path_tmp1);
		rcs_buf_free(nextbuf);

		(void)xasprintf(&path_tmp2, "%s/diff2.XXXXXXXXXX", rcs_tmpdir);
		rcs_buf_write_stmp(prevbuf, path_tmp2);
		rcs_buf_free(prevbuf);

		diff_format = D_RCSDIFF;
		if (rcs_diffreg(path_tmp1, path_tmp2, newdiff, 0) == D_ERROR)
			errx(1, "rcs_diffreg failed");

		newdeltatext = newdiff;
	} else if (nextrdp == NULL && prevrdp != NULL) {
		newdeltatext = prevbuf;
	}

	if (newdeltatext != NULL) {
		if (rcs_deltatext_set(rf, prevrdp->rd_num, newdeltatext) < 0)
			errx(1, "error setting new deltatext");
	}

	TAILQ_REMOVE(&(rf->rf_delta), rdp, rd_list);

	/* update pointers */
	if (prevrdp != NULL && nextrdp != NULL) {
		rcsnum_cpy(prevrdp->rd_num, nextrdp->rd_next, 0);
	} else if (prevrdp != NULL) {
		if (rcs_head_set(rf, prevrdp->rd_num) < 0)
			errx(1, "rcs_head_set failed");
	} else if (nextrdp != NULL) {
		rcsnum_free(nextrdp->rd_next);
		nextrdp->rd_next = rcsnum_alloc();
	} else {
		rcsnum_free(rf->rf_head);
		rf->rf_head = NULL;
	}

	rf->rf_ndelta--;
	rf->rf_flags &= ~RCS_SYNCED;

	rcs_freedelta(rdp);

	if (path_tmp1 != NULL)
		xfree(path_tmp1);
	if (path_tmp2 != NULL)
		xfree(path_tmp2);

	return (0);
}

/*
 * rcs_findrev()
 *
 * Find a specific revision's delta entry in the tree of the RCS file <rfp>.
 * The revision number is given in <rev>.
 *
 * If the given revision is a branch number, we translate it into the latest
 * revision on the branch.
 *
 * Returns a pointer to the delta on success, or NULL on failure.
 */
struct rcs_delta *
rcs_findrev(RCSFILE *rfp, RCSNUM *rev)
{
	u_int cmplen;
	struct rcs_delta *rdp;
	RCSNUM *brev, *frev;

	/*
	 * We need to do more parsing if the last revision in the linked list
	 * is greater than the requested revision.
	 */
	rdp = TAILQ_LAST(&(rfp->rf_delta), rcs_dlist);
	if (rdp == NULL ||
	    rcsnum_cmp(rdp->rd_num, rev, 0) == -1) {
		rcs_parse_deltas(rfp, rev);
	}

	/*
	 * Translate a branch into the latest revision on the branch itself.
	 */
	if (RCSNUM_ISBRANCH(rev)) {
		brev = rcsnum_brtorev(rev);
		frev = brev;
		for (;;) {
			rdp = rcs_findrev(rfp, frev);
			if (rdp == NULL)
				return (NULL);

			if (rdp->rd_next->rn_len == 0)
				break;

			frev = rdp->rd_next;
		}

		rcsnum_free(brev);
		return (rdp);
	}

	cmplen = rev->rn_len;

	TAILQ_FOREACH(rdp, &(rfp->rf_delta), rd_list) {
		if (rcsnum_cmp(rdp->rd_num, rev, cmplen) == 0)
			return (rdp);
	}

	return (NULL);
}

/*
 * rcs_kwexp_set()
 *
 * Set the keyword expansion mode to use on the RCS file <file> to <mode>.
 */
void
rcs_kwexp_set(RCSFILE *file, int mode)
{
	int i;
	char *tmp, buf[8] = "";

	if (RCS_KWEXP_INVAL(mode))
		return;

	i = 0;
	if (mode == RCS_KWEXP_NONE)
		buf[0] = 'b';
	else if (mode == RCS_KWEXP_OLD)
		buf[0] = 'o';
	else {
		if (mode & RCS_KWEXP_NAME)
			buf[i++] = 'k';
		if (mode & RCS_KWEXP_VAL)
			buf[i++] = 'v';
		if (mode & RCS_KWEXP_LKR)
			buf[i++] = 'l';
	}

	tmp = xstrdup(buf);
	if (file->rf_expand != NULL)
		xfree(file->rf_expand);
	file->rf_expand = tmp;
	/* not synced anymore */
	file->rf_flags &= ~RCS_SYNCED;
}

/*
 * rcs_kwexp_get()
 *
 * Retrieve the keyword expansion mode to be used for the RCS file <file>.
 */
int
rcs_kwexp_get(RCSFILE *file)
{
	return rcs_kflag_get(file->rf_expand);
}

/*
 * rcs_kflag_get()
 *
 * Get the keyword expansion mode from a set of character flags given in
 * <flags> and return the appropriate flag mask.  In case of an error, the
 * returned mask will have the RCS_KWEXP_ERR bit set to 1.
 */
int
rcs_kflag_get(const char *flags)
{
	int fl;
	size_t len;
	const char *fp;

	fl = 0;
	len = strlen(flags);

	for (fp = flags; *fp != '\0'; fp++) {
		if (*fp == 'k')
			fl |= RCS_KWEXP_NAME;
		else if (*fp == 'v')
			fl |= RCS_KWEXP_VAL;
		else if (*fp == 'l')
			fl |= RCS_KWEXP_LKR;
		else if (*fp == 'o') {
			if (len != 1)
				fl |= RCS_KWEXP_ERR;
			fl |= RCS_KWEXP_OLD;
		} else if (*fp == 'b') {
			if (len != 1)
				fl |= RCS_KWEXP_ERR;
		} else	/* unknown letter */
			fl |= RCS_KWEXP_ERR;
	}

	return (fl);
}

/*
 * rcs_errstr()
 *
 * Get the error string matching the RCS error code <code>.
 */
const char *
rcs_errstr(int code)
{
	const char *esp;

	if (code < 0 || (code >= (int)RCS_NERR && code != RCS_ERR_ERRNO))
		esp = NULL;
	else if (code == RCS_ERR_ERRNO)
		esp = strerror(errno);
	else
		esp = rcs_errstrs[code];
	return (esp);
}

/* rcs_parse_deltas()
 *
 * Parse deltas. If <rev> is not NULL, parse only as far as that
 * revision. If <rev> is NULL, parse all deltas.
 */
static void
rcs_parse_deltas(RCSFILE *rfp, RCSNUM *rev)
{
	int ret;
	struct rcs_delta *enddelta;

	if ((rfp->rf_flags & PARSED_DELTAS) || (rfp->rf_flags & RCS_CREATE))
		return;

	for (;;) {
		ret = rcs_parse_delta(rfp);
		if (rev != NULL) {
			enddelta = TAILQ_LAST(&(rfp->rf_delta), rcs_dlist);
			if (rcsnum_cmp(enddelta->rd_num, rev, 0) == 0)
				break;
		}
		if (ret == 0) {
			rfp->rf_flags |= PARSED_DELTAS;
			break;
		}
		else if (ret == -1)
			errx(1, "error parsing deltas");
	}
}

/* rcs_parse_deltatexts()
 *
 * Parse deltatexts. If <rev> is not NULL, parse only as far as that
 * revision. If <rev> is NULL, parse everything.
 */
static void
rcs_parse_deltatexts(RCSFILE *rfp, RCSNUM *rev)
{
	int ret;
	struct rcs_delta *rdp;

	if ((rfp->rf_flags & PARSED_DELTATEXTS) ||
	    (rfp->rf_flags & RCS_CREATE))
		return;

	if (!(rfp->rf_flags & PARSED_DESC))
		rcs_parse_desc(rfp, rev);
	for (;;) {
		if (rev != NULL) {
			rdp = rcs_findrev(rfp, rev);
			if (rdp->rd_text != NULL)
				break;
			else
				ret = rcs_parse_deltatext(rfp);
		} else
			ret = rcs_parse_deltatext(rfp);
		if (ret == 0) {
			rfp->rf_flags |= PARSED_DELTATEXTS;
			break;
		}
		else if (ret == -1)
			errx(1, "problem parsing deltatexts");
	}
}

/* rcs_parse_desc()
 *
 * Parse RCS description.
 */
static void
rcs_parse_desc(RCSFILE *rfp, RCSNUM *rev)
{
	int ret = 0;

	if ((rfp->rf_flags & PARSED_DESC) || (rfp->rf_flags & RCS_CREATE))
		return;
	if (!(rfp->rf_flags & PARSED_DELTAS))
		rcs_parse_deltas(rfp, rev);
	/* do parsing */
	ret = rcs_gettok(rfp);
	if (ret != RCS_TOK_DESC)
		errx(1, "token `%s' found where RCS desc expected",
		    RCS_TOKSTR(rfp));

	ret = rcs_gettok(rfp);
	if (ret != RCS_TOK_STRING)
		errx(1, "token `%s' found where RCS desc expected",
		    RCS_TOKSTR(rfp));

	rfp->rf_desc = xstrdup(RCS_TOKSTR(rfp));
	rfp->rf_flags |= PARSED_DESC;
}

/*
 * rcs_parse_init()
 *
 * Initial parsing of file <path>, which are in the RCS format.
 * Just does admin section.
 */
static void
rcs_parse_init(RCSFILE *rfp)
{
	struct rcs_pdata *pdp;

	if (rfp->rf_flags & RCS_PARSED)
		return;

	pdp = xcalloc(1, sizeof(*pdp));

	pdp->rp_lines = 0;
	pdp->rp_pttype = RCS_TOK_ERR;

	if ((pdp->rp_file = fdopen(rfp->rf_fd, "r")) == NULL)
		err(1, "fopen: `%s'", rfp->rf_path);

	pdp->rp_buf = xmalloc((size_t)RCS_BUFSIZE);
	pdp->rp_blen = RCS_BUFSIZE;
	pdp->rp_bufend = pdp->rp_buf + pdp->rp_blen - 1;

	/* ditch the strict lock */
	rfp->rf_flags &= ~RCS_SLOCK;
	rfp->rf_pdata = pdp;

	if (rcs_parse_admin(rfp) < 0) {
		rcs_freepdata(pdp);
		errx(1, "could not parse admin data");
	}

	if (rfp->rf_flags & RCS_PARSE_FULLY)
		rcs_parse_deltatexts(rfp, NULL);

	rfp->rf_flags |= RCS_SYNCED;
}

/*
 * rcs_parse_admin()
 *
 * Parse the administrative portion of an RCS file.
 * Returns the type of the first token found after the admin section on
 * success, or -1 on failure.
 */
static int
rcs_parse_admin(RCSFILE *rfp)
{
	u_int i;
	int tok, ntok, hmask;
	struct rcs_key *rk;

	/* hmask is a mask of the headers already encountered */
	hmask = 0;
	for (;;) {
		tok = rcs_gettok(rfp);
		if (tok == RCS_TOK_ERR) {
			rcs_errno = RCS_ERR_PARSE;
			warnx("parse error in RCS admin section");
			goto fail;
		} else if (tok == RCS_TOK_NUM || tok == RCS_TOK_DESC) {
			/*
			 * Assume this is the start of the first delta or
			 * that we are dealing with an empty RCS file and
			 * we just found the description.
			 */
			rcs_pushtok(rfp, RCS_TOKSTR(rfp), tok);
			return (tok);
		}

		rk = NULL;
		for (i = 0; i < RCS_NKEYS; i++)
			if (rcs_keys[i].rk_id == tok)
				rk = &(rcs_keys[i]);

		if (hmask & (1 << tok)) {
			rcs_errno = RCS_ERR_PARSE;
			warnx("duplicate RCS key");
			goto fail;
		}
		hmask |= (1 << tok);

		switch (tok) {
		case RCS_TOK_HEAD:
		case RCS_TOK_BRANCH:
		case RCS_TOK_COMMENT:
		case RCS_TOK_EXPAND:
			ntok = rcs_gettok(rfp);
			if (ntok == RCS_TOK_SCOLON)
				break;
			if (ntok != rk->rk_val) {
				rcs_errno = RCS_ERR_PARSE;
				warnx("invalid value type for RCS key `%s'",
				    rk->rk_str);
			}

			if (tok == RCS_TOK_HEAD) {
				if (rfp->rf_head == NULL)
					rfp->rf_head = rcsnum_alloc();
				rcsnum_aton(RCS_TOKSTR(rfp), NULL,
				    rfp->rf_head);
			} else if (tok == RCS_TOK_BRANCH) {
				if (rfp->rf_branch == NULL)
					rfp->rf_branch = rcsnum_alloc();
				if (rcsnum_aton(RCS_TOKSTR(rfp), NULL,
				    rfp->rf_branch) < 0)
					goto fail;
			} else if (tok == RCS_TOK_COMMENT) {
				rfp->rf_comment = xstrdup(RCS_TOKSTR(rfp));
			} else if (tok == RCS_TOK_EXPAND) {
				rfp->rf_expand = xstrdup(RCS_TOKSTR(rfp));
			}

			/* now get the expected semi-colon */
			ntok = rcs_gettok(rfp);
			if (ntok != RCS_TOK_SCOLON) {
				rcs_errno = RCS_ERR_PARSE;
				warnx("missing semi-colon after RCS `%s' key",
				    rk->rk_str);
				goto fail;
			}
			break;
		case RCS_TOK_ACCESS:
			if (rcs_parse_access(rfp) < 0)
				goto fail;
			break;
		case RCS_TOK_SYMBOLS:
			if (rcs_parse_symbols(rfp) < 0)
				goto fail;
			break;
		case RCS_TOK_LOCKS:
			if (rcs_parse_locks(rfp) < 0)
				goto fail;
			break;
		default:
			rcs_errno = RCS_ERR_PARSE;
			warnx("unexpected token `%s' in RCS admin section",
			    RCS_TOKSTR(rfp));
			goto fail;
		}
	}

fail:
	return (-1);
}

/*
 * rcs_parse_delta()
 *
 * Parse an RCS delta section and allocate the structure to store that delta's
 * information in the <rfp> delta list.
 * Returns 1 if the section was parsed OK, 0 if it is the last delta, and
 * -1 on error.
 */
static int
rcs_parse_delta(RCSFILE *rfp)
{
	int ret, tok, ntok, hmask;
	u_int i;
	char *tokstr;
	RCSNUM *datenum;
	struct rcs_delta *rdp;
	struct rcs_key *rk;

	tok = rcs_gettok(rfp);
	if (tok == RCS_TOK_DESC) {
		rcs_pushtok(rfp, RCS_TOKSTR(rfp), tok);
		return (0);
	} else if (tok != RCS_TOK_NUM) {
		rcs_errno = RCS_ERR_PARSE;
		warnx("unexpected token `%s' at start of delta",
		    RCS_TOKSTR(rfp));
		return (-1);
	}

	rdp = xcalloc(1, sizeof(*rdp));

	rdp->rd_num = rcsnum_alloc();
	rdp->rd_next = rcsnum_alloc();

	TAILQ_INIT(&(rdp->rd_branches));

	rcsnum_aton(RCS_TOKSTR(rfp), NULL, rdp->rd_num);

	hmask = 0;
	ret = 0;
	tokstr = NULL;

	for (;;) {
		tok = rcs_gettok(rfp);
		if (tok == RCS_TOK_ERR) {
			rcs_errno = RCS_ERR_PARSE;
			warnx("parse error in RCS delta section");
			rcs_freedelta(rdp);
			return (-1);
		} else if (tok == RCS_TOK_NUM || tok == RCS_TOK_DESC) {
			rcs_pushtok(rfp, RCS_TOKSTR(rfp), tok);
			ret = (tok == RCS_TOK_NUM ? 1 : 0);
			break;
		}

		rk = NULL;
		for (i = 0; i < RCS_NKEYS; i++)
			if (rcs_keys[i].rk_id == tok)
				rk = &(rcs_keys[i]);

		if (hmask & (1 << tok)) {
			rcs_errno = RCS_ERR_PARSE;
			warnx("duplicate RCS key");
			rcs_freedelta(rdp);
			return (-1);
		}
		hmask |= (1 << tok);

		switch (tok) {
		case RCS_TOK_DATE:
		case RCS_TOK_AUTHOR:
		case RCS_TOK_STATE:
		case RCS_TOK_NEXT:
			ntok = rcs_gettok(rfp);
			if (ntok == RCS_TOK_SCOLON) {
				if (rk->rk_flags & RCS_VOPT)
					break;
				else {
					rcs_errno = RCS_ERR_PARSE;
					warnx("missing mandatory "
					    "value to RCS key `%s'",
					    rk->rk_str);
					rcs_freedelta(rdp);
					return (-1);
				}
			}

			if (ntok != rk->rk_val) {
				rcs_errno = RCS_ERR_PARSE;
				warnx("invalid value type for RCS key `%s'",
				    rk->rk_str);
				rcs_freedelta(rdp);
				return (-1);
			}

			if (tokstr != NULL)
				xfree(tokstr);
			tokstr = xstrdup(RCS_TOKSTR(rfp));
			/* now get the expected semi-colon */
			ntok = rcs_gettok(rfp);
			if (ntok != RCS_TOK_SCOLON) {
				rcs_errno = RCS_ERR_PARSE;
				warnx("missing semi-colon after RCS `%s' key",
				    rk->rk_str);
				xfree(tokstr);
				rcs_freedelta(rdp);
				return (-1);
			}

			if (tok == RCS_TOK_DATE) {
				if ((datenum = rcsnum_parse(tokstr)) == NULL) {
					xfree(tokstr);
					rcs_freedelta(rdp);
					return (-1);
				}
				if (datenum->rn_len != 6) {
					rcs_errno = RCS_ERR_PARSE;
					warnx("RCS date specification has %s "
					    "fields",
					    (datenum->rn_len > 6) ? "too many" :
					    "missing");
					xfree(tokstr);
					rcs_freedelta(rdp);
					rcsnum_free(datenum);
					return (-1);
				}
				rdp->rd_date.tm_year = datenum->rn_id[0];
				if (rdp->rd_date.tm_year >= 1900)
					rdp->rd_date.tm_year -= 1900;
				rdp->rd_date.tm_mon = datenum->rn_id[1] - 1;
				rdp->rd_date.tm_mday = datenum->rn_id[2];
				rdp->rd_date.tm_hour = datenum->rn_id[3];
				rdp->rd_date.tm_min = datenum->rn_id[4];
				rdp->rd_date.tm_sec = datenum->rn_id[5];
				rcsnum_free(datenum);
			} else if (tok == RCS_TOK_AUTHOR) {
				rdp->rd_author = tokstr;
				tokstr = NULL;
			} else if (tok == RCS_TOK_STATE) {
				rdp->rd_state = tokstr;
				tokstr = NULL;
			} else if (tok == RCS_TOK_NEXT) {
				rcsnum_aton(tokstr, NULL, rdp->rd_next);
			}
			break;
		case RCS_TOK_BRANCHES:
			if (rcs_parse_branches(rfp, rdp) < 0) {
				rcs_freedelta(rdp);
				return (-1);
			}
			break;
		default:
			rcs_errno = RCS_ERR_PARSE;
			warnx("unexpected token `%s' in RCS delta",
			    RCS_TOKSTR(rfp));
			rcs_freedelta(rdp);
			return (-1);
		}
	}

	if (tokstr != NULL)
		xfree(tokstr);

	TAILQ_INSERT_TAIL(&(rfp->rf_delta), rdp, rd_list);
	rfp->rf_ndelta++;

	return (ret);
}

/*
 * rcs_parse_deltatext()
 *
 * Parse an RCS delta text section and fill in the log and text field of the
 * appropriate delta section.
 * Returns 1 if the section was parsed OK, 0 if it is the last delta, and
 * -1 on error.
 */
static int
rcs_parse_deltatext(RCSFILE *rfp)
{
	int tok;
	RCSNUM *tnum;
	struct rcs_delta *rdp;

	tok = rcs_gettok(rfp);
	if (tok == RCS_TOK_EOF)
		return (0);

	if (tok != RCS_TOK_NUM) {
		rcs_errno = RCS_ERR_PARSE;
		warnx("unexpected token `%s' at start of RCS delta text",
		    RCS_TOKSTR(rfp));
		return (-1);
	}

	tnum = rcsnum_alloc();
	rcsnum_aton(RCS_TOKSTR(rfp), NULL, tnum);

	TAILQ_FOREACH(rdp, &(rfp->rf_delta), rd_list) {
		if (rcsnum_cmp(tnum, rdp->rd_num, 0) == 0)
			break;
	}
	rcsnum_free(tnum);

	if (rdp == NULL) {
		warnx("RCS delta text `%s' has no matching delta",
		    RCS_TOKSTR(rfp));
		return (-1);
	}

	tok = rcs_gettok(rfp);
	if (tok != RCS_TOK_LOG) {
		rcs_errno = RCS_ERR_PARSE;
		warnx("unexpected token `%s' where RCS log expected",
		    RCS_TOKSTR(rfp));
		return (-1);
	}

	tok = rcs_gettok(rfp);
	if (tok != RCS_TOK_STRING) {
		rcs_errno = RCS_ERR_PARSE;
		warnx("unexpected token `%s' where RCS log expected",
		    RCS_TOKSTR(rfp));
		return (-1);
	}
	rdp->rd_log = xstrdup(RCS_TOKSTR(rfp));
	tok = rcs_gettok(rfp);
	if (tok != RCS_TOK_TEXT) {
		rcs_errno = RCS_ERR_PARSE;
		warnx("unexpected token `%s' where RCS text expected",
		    RCS_TOKSTR(rfp));
		return (-1);
	}

	tok = rcs_gettok(rfp);
	if (tok != RCS_TOK_STRING) {
		rcs_errno = RCS_ERR_PARSE;
		warnx("unexpected token `%s' where RCS text expected",
		    RCS_TOKSTR(rfp));
		return (-1);
	}

	if (RCS_TOKLEN(rfp) == 0) {
		rdp->rd_text = xmalloc(1);
		rdp->rd_text[0] = '\0';
		rdp->rd_tlen = 0;
	} else {
		rdp->rd_text = xmalloc(RCS_TOKLEN(rfp));
		memcpy(rdp->rd_text, RCS_TOKSTR(rfp), (RCS_TOKLEN(rfp)));
		rdp->rd_tlen = RCS_TOKLEN(rfp);
	}

	return (1);
}

/*
 * rcs_parse_access()
 *
 * Parse the access list given as value to the `access' keyword.
 * Returns 0 on success, or -1 on failure.
 */
static int
rcs_parse_access(RCSFILE *rfp)
{
	int type;

	while ((type = rcs_gettok(rfp)) != RCS_TOK_SCOLON) {
		if (type != RCS_TOK_ID) {
			rcs_errno = RCS_ERR_PARSE;
			warnx("unexpected token `%s' in access list",
			    RCS_TOKSTR(rfp));
			return (-1);
		}

		if (rcs_access_add(rfp, RCS_TOKSTR(rfp)) < 0)
			return (-1);
	}

	return (0);
}

/*
 * rcs_parse_symbols()
 *
 * Parse the symbol list given as value to the `symbols' keyword.
 * Returns 0 on success, or -1 on failure.
 */
static int
rcs_parse_symbols(RCSFILE *rfp)
{
	int type;
	struct rcs_sym *symp;

	for (;;) {
		type = rcs_gettok(rfp);
		if (type == RCS_TOK_SCOLON)
			break;

		if (type != RCS_TOK_ID) {
			rcs_errno = RCS_ERR_PARSE;
			warnx("unexpected token `%s' in symbol list",
			    RCS_TOKSTR(rfp));
			return (-1);
		}

		symp = xmalloc(sizeof(*symp));
		symp->rs_name = xstrdup(RCS_TOKSTR(rfp));
		symp->rs_num = rcsnum_alloc();

		type = rcs_gettok(rfp);
		if (type != RCS_TOK_COLON) {
			rcs_errno = RCS_ERR_PARSE;
			warnx("unexpected token `%s' in symbol list",
			    RCS_TOKSTR(rfp));
			rcsnum_free(symp->rs_num);
			xfree(symp->rs_name);
			xfree(symp);
			return (-1);
		}

		type = rcs_gettok(rfp);
		if (type != RCS_TOK_NUM) {
			rcs_errno = RCS_ERR_PARSE;
			warnx("unexpected token `%s' in symbol list",
			    RCS_TOKSTR(rfp));
			rcsnum_free(symp->rs_num);
			xfree(symp->rs_name);
			xfree(symp);
			return (-1);
		}

		if (rcsnum_aton(RCS_TOKSTR(rfp), NULL, symp->rs_num) < 0) {
			warnx("failed to parse RCS NUM `%s'",
			    RCS_TOKSTR(rfp));
			rcsnum_free(symp->rs_num);
			xfree(symp->rs_name);
			xfree(symp);
			return (-1);
		}

		TAILQ_INSERT_TAIL(&(rfp->rf_symbols), symp, rs_list);
	}

	return (0);
}

/*
 * rcs_parse_locks()
 *
 * Parse the lock list given as value to the `locks' keyword.
 * Returns 0 on success, or -1 on failure.
 */
static int
rcs_parse_locks(RCSFILE *rfp)
{
	int type;
	struct rcs_lock *lkp;

	for (;;) {
		type = rcs_gettok(rfp);
		if (type == RCS_TOK_SCOLON)
			break;

		if (type != RCS_TOK_ID) {
			rcs_errno = RCS_ERR_PARSE;
			warnx("unexpected token `%s' in lock list",
			    RCS_TOKSTR(rfp));
			return (-1);
		}

		lkp = xmalloc(sizeof(*lkp));
		lkp->rl_name = xstrdup(RCS_TOKSTR(rfp));
		lkp->rl_num = rcsnum_alloc();

		type = rcs_gettok(rfp);
		if (type != RCS_TOK_COLON) {
			rcs_errno = RCS_ERR_PARSE;
			warnx("unexpected token `%s' in symbol list",
			    RCS_TOKSTR(rfp));
			rcsnum_free(lkp->rl_num);
			xfree(lkp->rl_name);
			xfree(lkp);
			return (-1);
		}

		type = rcs_gettok(rfp);
		if (type != RCS_TOK_NUM) {
			rcs_errno = RCS_ERR_PARSE;
			warnx("unexpected token `%s' in symbol list",
			    RCS_TOKSTR(rfp));
			rcsnum_free(lkp->rl_num);
			xfree(lkp->rl_name);
			xfree(lkp);
			return (-1);
		}

		if (rcsnum_aton(RCS_TOKSTR(rfp), NULL, lkp->rl_num) < 0) {
			warnx("failed to parse RCS NUM `%s'",
			    RCS_TOKSTR(rfp));
			rcsnum_free(lkp->rl_num);
			xfree(lkp->rl_name);
			xfree(lkp);
			return (-1);
		}

		TAILQ_INSERT_HEAD(&(rfp->rf_locks), lkp, rl_list);
	}

	/* check if we have a `strict' */
	type = rcs_gettok(rfp);
	if (type != RCS_TOK_STRICT) {
		rcs_pushtok(rfp, RCS_TOKSTR(rfp), type);
	} else {
		rfp->rf_flags |= RCS_SLOCK;

		type = rcs_gettok(rfp);
		if (type != RCS_TOK_SCOLON) {
			rcs_errno = RCS_ERR_PARSE;
			warnx("missing semi-colon after `strict' keyword");
			return (-1);
		}
	}

	return (0);
}

/*
 * rcs_parse_branches()
 *
 * Parse the list of branches following a `branches' keyword in a delta.
 * Returns 0 on success, or -1 on failure.
 */
static int
rcs_parse_branches(RCSFILE *rfp, struct rcs_delta *rdp)
{
	int type;
	struct rcs_branch *brp;

	for (;;) {
		type = rcs_gettok(rfp);
		if (type == RCS_TOK_SCOLON)
			break;

		if (type != RCS_TOK_NUM) {
			rcs_errno = RCS_ERR_PARSE;
			warnx("unexpected token `%s' in list of branches",
			    RCS_TOKSTR(rfp));
			return (-1);
		}

		brp = xmalloc(sizeof(*brp));
		brp->rb_num = rcsnum_parse(RCS_TOKSTR(rfp));
		if (brp->rb_num == NULL) {
			xfree(brp);
			return (-1);
		}

		TAILQ_INSERT_TAIL(&(rdp->rd_branches), brp, rb_list);
	}

	return (0);
}

/*
 * rcs_freedelta()
 *
 * Free the contents of a delta structure.
 */
static void
rcs_freedelta(struct rcs_delta *rdp)
{
	struct rcs_branch *rb;

	if (rdp->rd_num != NULL)
		rcsnum_free(rdp->rd_num);
	if (rdp->rd_next != NULL)
		rcsnum_free(rdp->rd_next);

	if (rdp->rd_author != NULL)
		xfree(rdp->rd_author);
	if (rdp->rd_locker != NULL)
		xfree(rdp->rd_locker);
	if (rdp->rd_state != NULL)
		xfree(rdp->rd_state);
	if (rdp->rd_log != NULL)
		xfree(rdp->rd_log);
	if (rdp->rd_text != NULL)
		xfree(rdp->rd_text);

	while ((rb = TAILQ_FIRST(&(rdp->rd_branches))) != NULL) {
		TAILQ_REMOVE(&(rdp->rd_branches), rb, rb_list);
		rcsnum_free(rb->rb_num);
		xfree(rb);
	}

	xfree(rdp);
}

/*
 * rcs_freepdata()
 *
 * Free the contents of the parser data structure.
 */
static void
rcs_freepdata(struct rcs_pdata *pd)
{
	if (pd->rp_file != NULL)
		(void)fclose(pd->rp_file);
	if (pd->rp_buf != NULL)
		xfree(pd->rp_buf);
	xfree(pd);
}

/*
 * rcs_gettok()
 *
 * Get the next RCS token from the string <str>.
 */
static int
rcs_gettok(RCSFILE *rfp)
{
	u_int i;
	int ch, last, type;
	size_t len;
	char *bp;
	struct rcs_pdata *pdp = (struct rcs_pdata *)rfp->rf_pdata;

	type = RCS_TOK_ERR;
	bp = pdp->rp_buf;
	pdp->rp_tlen = 0;
	*bp = '\0';

	if (pdp->rp_pttype != RCS_TOK_ERR) {
		type = pdp->rp_pttype;
		if (strlcpy(pdp->rp_buf, pdp->rp_ptok, pdp->rp_blen) >=
		    pdp->rp_blen)
			errx(1, "rcs_gettok: strlcpy");
		pdp->rp_pttype = RCS_TOK_ERR;
		return (type);
	}

	/* skip leading whitespace */
	/* XXX we must skip backspace too for compatibility, should we? */
	do {
		ch = getc(pdp->rp_file);
		if (ch == '\n')
			pdp->rp_lines++;
	} while (isspace(ch));

	if (ch == EOF) {
		type = RCS_TOK_EOF;
	} else if (ch == ';') {
		type = RCS_TOK_SCOLON;
	} else if (ch == ':') {
		type = RCS_TOK_COLON;
	} else if (isalpha(ch)) {
		type = RCS_TOK_ID;
		*(bp++) = ch;
		for (;;) {
			ch = getc(pdp->rp_file);
			if (ch == EOF) {
				type = RCS_TOK_EOF;
				break;
			} else if (!isalnum(ch) && ch != '_' && ch != '-' &&
			    ch != '/') {
				ungetc(ch, pdp->rp_file);
				break;
			}
			*(bp++) = ch;
			pdp->rp_tlen++;
			if (bp == pdp->rp_bufend - 1) {
				len = bp - pdp->rp_buf;
				rcs_growbuf(rfp);
				bp = pdp->rp_buf + len;
			}
		}
		*bp = '\0';

		if (type != RCS_TOK_ERR) {
			for (i = 0; i < RCS_NKEYS; i++) {
				if (strcmp(rcs_keys[i].rk_str,
				    pdp->rp_buf) == 0) {
					type = rcs_keys[i].rk_id;
					break;
				}
			}
		}
	} else if (ch == '@') {
		/* we have a string */
		type = RCS_TOK_STRING;
		for (;;) {
			ch = getc(pdp->rp_file);
			if (ch == EOF) {
				type = RCS_TOK_EOF;
				break;
			} else if (ch == '@') {
				ch = getc(pdp->rp_file);
				if (ch != '@') {
					ungetc(ch, pdp->rp_file);
					break;
				}
			} else if (ch == '\n')
				pdp->rp_lines++;

			*(bp++) = ch;
			pdp->rp_tlen++;
			if (bp == pdp->rp_bufend - 1) {
				len = bp - pdp->rp_buf;
				rcs_growbuf(rfp);
				bp = pdp->rp_buf + len;
			}
		}

		*bp = '\0';
	} else if (isdigit(ch)) {
		*(bp++) = ch;
		last = ch;
		type = RCS_TOK_NUM;

		for (;;) {
			ch = getc(pdp->rp_file);
			if (ch == EOF) {
				type = RCS_TOK_EOF;
				break;
			}
			if (bp == pdp->rp_bufend)
				break;
			if (!isdigit(ch) && ch != '.') {
				ungetc(ch, pdp->rp_file);
				break;
			}

			if (last == '.' && ch == '.') {
				type = RCS_TOK_ERR;
				break;
			}
			last = ch;
			*(bp++) = ch;
			pdp->rp_tlen++;
		}
		*bp = '\0';
	}

	return (type);
}

/*
 * rcs_pushtok()
 *
 * Push a token back in the parser's token buffer.
 */
static int
rcs_pushtok(RCSFILE *rfp, const char *tok, int type)
{
	struct rcs_pdata *pdp = (struct rcs_pdata *)rfp->rf_pdata;

	if (pdp->rp_pttype != RCS_TOK_ERR)
		return (-1);

	pdp->rp_pttype = type;
	if (strlcpy(pdp->rp_ptok, tok, sizeof(pdp->rp_ptok)) >=
	    sizeof(pdp->rp_ptok))
		errx(1, "rcs_pushtok: strlcpy");
	return (0);
}


/*
 * rcs_growbuf()
 *
 * Attempt to grow the internal parse buffer for the RCS file <rf> by
 * RCS_BUFEXTSIZE.
 * In case of failure, the original buffer is left unmodified.
 */
static void
rcs_growbuf(RCSFILE *rf)
{
	void *tmp;
	struct rcs_pdata *pdp = (struct rcs_pdata *)rf->rf_pdata;

	tmp = xrealloc(pdp->rp_buf, 1, pdp->rp_blen + RCS_BUFEXTSIZE);
	pdp->rp_buf = tmp;
	pdp->rp_blen += RCS_BUFEXTSIZE;
	pdp->rp_bufend = pdp->rp_buf + pdp->rp_blen - 1;
}

/*
 * rcs_strprint()
 *
 * Output an RCS string <str> of size <slen> to the stream <stream>.  Any
 * '@' characters are escaped.  Otherwise, the string can contain arbitrary
 * binary data.
 */
static void
rcs_strprint(const u_char *str, size_t slen, FILE *stream)
{
	const u_char *ap, *ep, *sp;

	if (slen == 0)
		return;

	ep = str + slen - 1;

	for (sp = str; sp <= ep;)  {
		ap = memchr(sp, '@', ep - sp);
		if (ap == NULL)
			ap = ep;
		(void)fwrite(sp, sizeof(u_char), ap - sp + 1, stream);

		if (*ap == '@')
			putc('@', stream);
		sp = ap + 1;
	}
}

/*
 * rcs_expand_keywords()
 *
 * Return expansion any RCS keywords in <data>
 *
 * On error, return NULL.
 */
static BUF *
rcs_expand_keywords(char *rcsfile, struct rcs_delta *rdp, BUF *bp, int mode)
{
	BUF *newbuf;
	int kwtype;
	u_int j, found;
	u_char *c, *kwstr, *start, *end, *fin;
	char expbuf[256], buf[256];
	struct tm tb;
	char *fmt;
	size_t len;

	kwtype = 0;
	kwstr = NULL;

	/*
	 * -z support for RCS
	 */
	tb = rdp->rd_date;
	if (timezone_flag != NULL)
		rcs_set_tz(timezone_flag, rdp, &tb);

	len = rcs_buf_len(bp);

	c = rcs_buf_get(bp);
	found = 0;
	/* Final character in buffer. */
	fin = c + len - 1;

	/* If no keywords are found, return original buffer. */
	newbuf = bp;

	/*
	 * Keyword formats:
	 * $Keyword$
	 * $Keyword: value$
	 */
	for (; c < fin; c++) {
		if (*c == '$') {
			BUF *tmpbuf;
			size_t clen;

			/* remember start of this possible keyword */
			start = c;

			/* first following character has to be alphanumeric */
			c++;
			if (!isalpha(*c)) {
				c = start;
				continue;
			}

			/* Number of characters between c and fin, inclusive. */
			clen = fin - c + 1;

			/* look for any matching keywords */
			found = 0;
			for (j = 0; j < RCS_NKWORDS; j++) {
				size_t kwlen;

				kwlen = strlen(rcs_expkw[j].kw_str);
				/*
				 * kwlen must be less than clen since clen
				 * includes either a terminating `$' or a `:'.
				 */
				if (kwlen < clen &&
				    memcmp(c, rcs_expkw[j].kw_str, kwlen) == 0 &&
				    (c[kwlen] == '$' || c[kwlen] == ':')) {
					found = 1;
					kwstr = rcs_expkw[j].kw_str;
					kwtype = rcs_expkw[j].kw_type;
					c += kwlen;
					break;
				}
			}

			/* unknown keyword, continue looking */
			if (found == 0) {
				c = start;
				continue;
			}

			/*
			 * if the next character was ':' we need to look for
			 * an '$' before the end of the line to be sure it is
			 * in fact a keyword.
			 */
			if (*c == ':') {
				for (; c <= fin; ++c) {
					if (*c == '$' || *c == '\n')
						break;
				}

				if (*c != '$') {
					c = start;
					continue;
				}
			}
			end = c + 1;

			/* start constructing the expansion */
			expbuf[0] = '\0';

			if (mode & RCS_KWEXP_NAME) {
				char *tmp;

				(void)xasprintf(&tmp, "$%s%s", kwstr,
				    (mode & RCS_KWEXP_VAL) ? ": " : "");
				if (strlcat(expbuf, tmp, sizeof(expbuf)) >= sizeof(expbuf))
					errx(1, "rcs_expand_keywords: string truncated");
				xfree(tmp);
			}

			/*
			 * order matters because of RCS_KW_ID and
			 * RCS_KW_HEADER here
			 */
			if (mode & RCS_KWEXP_VAL) {
				if (kwtype & RCS_KW_RCSFILE) {
					char *tmp;

					(void)xasprintf(&tmp, "%s ",
					    (kwtype & RCS_KW_FULLPATH) ? rcsfile : basename(rcsfile));
					if (strlcat(expbuf, tmp, sizeof(expbuf)) >= sizeof(expbuf))
						errx(1, "rcs_expand_keywords: string truncated");
					xfree(tmp);
				}

				if (kwtype & RCS_KW_REVISION) {
					char *tmp;

					rcsnum_tostr(rdp->rd_num, buf, sizeof(buf));
					(void)xasprintf(&tmp, "%s ", buf);
					if (strlcat(expbuf, tmp, sizeof(expbuf)) >= sizeof(buf))
						errx(1, "rcs_expand_keywords: string truncated");
					xfree(tmp);
				}

				if (kwtype & RCS_KW_DATE) {
					if (timezone_flag != NULL)
						fmt = "%Y/%m/%d %H:%M:%S%z ";
					else
						fmt = "%Y/%m/%d %H:%M:%S ";

					strftime(buf, sizeof(buf), fmt, &tb);
					if (strlcat(expbuf, buf, sizeof(expbuf)) >= sizeof(expbuf))
						errx(1, "rcs_expand_keywords: string truncated");
				}

				if (kwtype & RCS_KW_AUTHOR) {
					char *tmp;

					(void)xasprintf(&tmp, "%s ", rdp->rd_author);
					if (strlcat(expbuf, tmp, sizeof(expbuf)) >= sizeof(expbuf))
						errx(1, "rcs_expand_keywords: string truncated");
					xfree(tmp);
				}

				if (kwtype & RCS_KW_STATE) {
					char *tmp;

					(void)xasprintf(&tmp, "%s ", rdp->rd_state);
					if (strlcat(expbuf, tmp, sizeof(expbuf)) >= sizeof(expbuf))
						errx(1, "rcs_expand_keywords: string truncated");
					xfree(tmp);
				}

				/* order does not matter anymore below */
				if (kwtype & RCS_KW_LOG)
					if (strlcat(expbuf, " ", sizeof(expbuf)) >= sizeof(expbuf))
						errx(1, "rcs_expand_keywords: string truncated");

				if (kwtype & RCS_KW_SOURCE) {
					char *tmp;

					(void)xasprintf(&tmp, "%s ", rcsfile);
					if (strlcat(expbuf, tmp, sizeof(expbuf)) >= sizeof(expbuf))
						errx(1, "rcs_expand_keywords: string truncated");
					xfree(tmp);
				}

				if (kwtype & RCS_KW_NAME)
					if (strlcat(expbuf, " ", sizeof(expbuf)) >= sizeof(expbuf))
						errx(1, "rcs_expand_keywords: string truncated");
			}

			/* end the expansion */
			if (mode & RCS_KWEXP_NAME)
				if (strlcat(expbuf, "$", sizeof(expbuf)) >= sizeof(expbuf))
					errx(1, "rcs_expand_keywords: string truncated");

			/* Concatenate everything together. */
			tmpbuf = rcs_buf_alloc(len + strlen(expbuf), BUF_AUTOEXT);
			/* Append everything before keyword. */
			rcs_buf_append(tmpbuf, rcs_buf_get(newbuf),
			    start - (unsigned char *)rcs_buf_get(newbuf));
			/* Append keyword. */
			rcs_buf_append(tmpbuf, expbuf, strlen(expbuf));
			/* Point c to end of keyword. */
			c = rcs_buf_get(tmpbuf) + rcs_buf_len(tmpbuf) - 1;
			/* Append everything after keyword. */
			rcs_buf_append(tmpbuf, end,
			    ((unsigned char *)rcs_buf_get(newbuf) + rcs_buf_len(newbuf)) - end);
			/* Point fin to end of data. */
			fin = rcs_buf_get(tmpbuf) + rcs_buf_len(tmpbuf) - 1;
			/* Recalculate new length. */
			len = rcs_buf_len(tmpbuf);

			/* tmpbuf is now ready, free old newbuf if allocated here. */
			if (newbuf != bp)
				rcs_buf_free(newbuf);
			newbuf = tmpbuf;
		}
	}

	return (newbuf);
}

/*
 * rcs_deltatext_set()
 *
 * Set deltatext for <rev> in RCS file <rfp> to <dtext>
 * Returns -1 on error, 0 on success.
 */
int
rcs_deltatext_set(RCSFILE *rfp, RCSNUM *rev, BUF *bp)
{
	size_t len;
	u_char *dtext;
	struct rcs_delta *rdp;

	/* Write operations require full parsing */
	rcs_parse_deltatexts(rfp, NULL);

	if ((rdp = rcs_findrev(rfp, rev)) == NULL)
		return (-1);

	if (rdp->rd_text != NULL)
		xfree(rdp->rd_text);

	len = rcs_buf_len(bp);
	dtext = rcs_buf_release(bp);
	bp = NULL;
	if (len != 0) {
		rdp->rd_text = xmalloc(len);
		rdp->rd_tlen = len;
		(void)memcpy(rdp->rd_text, dtext, len);
		xfree(dtext);
	} else {
		rdp->rd_text = NULL;
		rdp->rd_tlen = 0;
	}

	return (0);
}

/*
 * rcs_rev_setlog()
 *
 * Sets the log message of revision <rev> to <logtext>
 */
int
rcs_rev_setlog(RCSFILE *rfp, RCSNUM *rev, const char *logtext)
{
	struct rcs_delta *rdp;
	char buf[16];

	rcsnum_tostr(rev, buf, sizeof(buf));

	if ((rdp = rcs_findrev(rfp, rev)) == NULL)
		return (-1);

	if (rdp->rd_log != NULL)
		xfree(rdp->rd_log);

	rdp->rd_log = xstrdup(logtext);
	rfp->rf_flags &= ~RCS_SYNCED;
	return (0);
}
/*
 * rcs_rev_getdate()
 *
 * Get the date corresponding to a given revision.
 * Returns the date on success, -1 on failure.
 */
time_t
rcs_rev_getdate(RCSFILE *rfp, RCSNUM *rev)
{
	struct rcs_delta *rdp;

	if ((rdp = rcs_findrev(rfp, rev)) == NULL)
		return (-1);

	return (mktime(&rdp->rd_date));
}

/*
 * rcs_state_set()
 *
 * Sets the state of revision <rev> to <state>
 * NOTE: default state is 'Exp'. States may not contain spaces.
 *
 * Returns -1 on failure, 0 on success.
 */
int
rcs_state_set(RCSFILE *rfp, RCSNUM *rev, const char *state)
{
	struct rcs_delta *rdp;

	if ((rdp = rcs_findrev(rfp, rev)) == NULL)
		return (-1);

	if (rdp->rd_state != NULL)
		xfree(rdp->rd_state);

	rdp->rd_state = xstrdup(state);

	rfp->rf_flags &= ~RCS_SYNCED;

	return (0);
}

/*
 * rcs_state_check()
 *
 * Check if string <state> is valid.
 *
 * Returns 0 if the string is valid, -1 otherwise.
 */
int
rcs_state_check(const char *state)
{
	if (strchr(state, ' ') != NULL)
		return (-1);

	return (0);
}

/*
 * rcs_state_get()
 *
 * Get the state for a given revision of a specified RCSFILE.
 *
 * Returns NULL on failure.
 */
const char *
rcs_state_get(RCSFILE *rfp, RCSNUM *rev)
{
	struct rcs_delta *rdp;

	if ((rdp = rcs_findrev(rfp, rev)) == NULL)
		return (NULL);

	return (rdp->rd_state);
}

/*
 * rcs_kwexp_buf()
 *
 * Do keyword expansion on a buffer if necessary
 *
 */
BUF *
rcs_kwexp_buf(BUF *bp, RCSFILE *rf, RCSNUM *rev)
{
	struct rcs_delta *rdp;
	int expmode;

	/*
	 * Do keyword expansion if required.
	 */
	if (rf->rf_expand != NULL)
		expmode = rcs_kwexp_get(rf);
	else
		expmode = RCS_KWEXP_DEFAULT;

	if (!(expmode & RCS_KWEXP_NONE)) {
		if ((rdp = rcs_findrev(rf, rev)) == NULL)
			errx(1, "could not fetch revision");
		return (rcs_expand_keywords(rf->rf_path, rdp, bp, expmode));
	}
	return (bp);
}
