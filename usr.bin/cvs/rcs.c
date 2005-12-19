/*	$OpenBSD: rcs.c,v 1.112 2005/12/19 17:43:01 xsa Exp $	*/
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

#include <sys/param.h>
#include <sys/stat.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cvs.h"
#include "log.h"
#include "rcs.h"
#include "diff.h"

#define RCS_BUFSIZE	16384
#define RCS_BUFEXTSIZE	8192


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

#define NB_COMTYPES	(sizeof(rcs_comments)/sizeof(rcs_comments[0]))

#ifdef notyet
static struct rcs_kfl {
	char	rk_char;
	int	rk_val;
} rcs_kflags[] = {
	{ 'k',   RCS_KWEXP_NAME },
	{ 'v',   RCS_KWEXP_VAL  },
	{ 'l',   RCS_KWEXP_LKR  },
	{ 'o',   RCS_KWEXP_OLD  },
	{ 'b',   RCS_KWEXP_NONE },
};
#endif

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

/*
 * Keyword expansion table
 */
#define RCS_KW_AUTHOR		0x1000
#define RCS_KW_DATE		0x2000
#define RCS_KW_LOG		0x4000
#define RCS_KW_NAME		0x8000
#define RCS_KW_RCSFILE		0x0100
#define RCS_KW_REVISION		0x0200
#define RCS_KW_SOURCE		0x0400
#define RCS_KW_STATE		0x0800
#define RCS_KW_FULLPATH		0x0010

#define RCS_KW_ID \
	(RCS_KW_RCSFILE | RCS_KW_REVISION | RCS_KW_DATE \
	| RCS_KW_AUTHOR | RCS_KW_STATE)

#define RCS_KW_HEADER	(RCS_KW_ID | RCS_KW_FULLPATH)

static struct rcs_kw {
	char	kw_str[16];
	int	kw_type;
} rcs_expkw[] = {
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

#define RCS_NKWORDS	(sizeof(rcs_expkw)/sizeof(rcs_expkw[0]))

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


static int	rcs_write(RCSFILE *);
static int	rcs_parse(RCSFILE *);
static int	rcs_parse_admin(RCSFILE *);
static int	rcs_parse_delta(RCSFILE *);
static int	rcs_parse_deltatext(RCSFILE *);

static int	rcs_parse_access(RCSFILE *);
static int	rcs_parse_symbols(RCSFILE *);
static int	rcs_parse_locks(RCSFILE *);
static int	rcs_parse_branches(RCSFILE *, struct rcs_delta *);
static void	rcs_freedelta(struct rcs_delta *);
static void	rcs_freepdata(struct rcs_pdata *);
static int	rcs_gettok(RCSFILE *);
static int	rcs_pushtok(RCSFILE *, const char *, int);
static int	rcs_growbuf(RCSFILE *);
static int	rcs_strprint(const u_char *, size_t, FILE *);

static int	rcs_expand_keywords(char *, struct rcs_delta *, char *, char *,
		    size_t, int);

/*
 * rcs_open()
 *
 * Open a file containing RCS-formatted information.  The file's path is
 * given in <path>, and the opening flags are given in <flags>, which is either
 * RCS_READ, RCS_WRITE, or RCS_RDWR.  If the open requests write access and
 * the file does not exist, the RCS_CREATE flag must also be given, in which
 * case it will be created with the mode specified in a third argument of
 * type mode_t.  If the file exists and RCS_CREATE is passed, the open will
 * fail.
 * Returns a handle to the opened file on success, or NULL on failure.
 */
RCSFILE *
rcs_open(const char *path, int flags, ...)
{
	int ret;
	mode_t fmode;
	RCSFILE *rfp;
	struct stat st;
	va_list vap;
	struct rcs_delta *rdp;
	struct rcs_lock *lkr;

	fmode = 0;
	flags &= 0xffff;	/* ditch any internal flags */

	if (((ret = stat(path, &st)) == -1) && (errno == ENOENT)) {
		if (flags & RCS_CREATE) {
			va_start(vap, flags);
			fmode = va_arg(vap, mode_t);
			va_end(vap);
		} else {
			/* XXX, make this command dependant? */
#if 0
			cvs_log(LP_ERR, "RCS file `%s' does not exist", path);
#endif
			return (NULL);
		}
	} else if ((ret == 0) && (flags & RCS_CREATE)) {
		cvs_log(LP_ERR, "RCS file `%s' exists", path);
		return (NULL);
	}

	rfp = (RCSFILE *)xmalloc(sizeof(*rfp));
	memset(rfp, 0, sizeof(*rfp));

	rfp->rf_path = xstrdup(path);
	rfp->rf_ref = 1;
	rfp->rf_flags = flags | RCS_SLOCK;
	rfp->rf_mode = fmode;

	TAILQ_INIT(&(rfp->rf_delta));
	TAILQ_INIT(&(rfp->rf_access));
	TAILQ_INIT(&(rfp->rf_symbols));
	TAILQ_INIT(&(rfp->rf_locks));

	if (rfp->rf_flags & RCS_CREATE) {
	} else if (rcs_parse(rfp) < 0) {
		rcs_close(rfp);
		return (NULL);
	}

	/* fill in rd_locker */
	TAILQ_FOREACH(lkr, &(rfp->rf_locks), rl_list) {
		if ((rdp = rcs_findrev(rfp, lkr->rl_num)) == NULL) {
			rcs_close(rfp);
			return (NULL);
		}

		rdp->rd_locker = xstrdup(lkr->rl_name);
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

	if (rfp->rf_ref > 1) {
		rfp->rf_ref--;
		return;
	}

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
	xfree(rfp);
}

/*
 * rcs_write()
 *
 * Write the contents of the RCS file handle <rfp> to disk in the file whose
 * path is in <rf_path>.
 * Returns 0 on success, or -1 on failure.
 */
static int
rcs_write(RCSFILE *rfp)
{
	FILE *fp;
	char buf[1024], numbuf[64], fn[19] = "";
	void *bp;
	struct rcs_access *ap;
	struct rcs_sym *symp;
	struct rcs_branch *brp;
	struct rcs_delta *rdp;
	struct rcs_lock *lkp;
	ssize_t nread;
	size_t len;
	int fd, from_fd, to_fd;

	from_fd = to_fd = fd = -1;

	if (rfp->rf_flags & RCS_SYNCED)
		return (0);

	strlcpy(fn, "/tmp/rcs.XXXXXXXXXX", sizeof(fn));
	if ((fd = mkstemp(fn)) == -1 ||
	    (fp = fdopen(fd, "w+")) == NULL) {
		if (fd != -1) {
			unlink(fn);
			close(fd);
		}
		rcs_errno = RCS_ERR_ERRNO;
		cvs_log(LP_ERRNO, "failed to open temp RCS output file `%s'",
		    fn);
		return (-1);
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
		snprintf(buf, sizeof(buf), "%s:%s", symp->rs_name, numbuf);
		fprintf(fp, "\n\t%s", buf);
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

	if (rfp->rf_comment != NULL) {
		fputs("comment\t@", fp);
		rcs_strprint((const u_char *)rfp->rf_comment,
		    strlen(rfp->rf_comment), fp);
		fputs("@;\n", fp);
	}

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
	if (rfp->rf_desc != NULL) {
		len = strlen(rfp->rf_desc);
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
		if (rdp->rd_text != NULL) {
			rcs_strprint(rdp->rd_text, rdp->rd_tlen, fp);

			if (rdp->rd_tlen != 0) {
				if (rdp->rd_text[rdp->rd_tlen-1] != '\n')
					fputc('\n', fp);
			}
		}
		fputs("@\n", fp);
	}
	fclose(fp);

	/*
	 * We try to use rename() to atomically put the new file in place.
	 * If that fails, we try a copy.
	 */
	if (rename(fn, rfp->rf_path) == -1) {
		if (errno == EXDEV) {
			/* rename() not supported so we have to copy. */
			if ((chmod(rfp->rf_path, S_IWUSR) == -1)
			    && !(rfp->rf_flags & RCS_CREATE)) {
				cvs_log(LP_ERRNO, "failed to chmod `%s'",
				    rfp->rf_path);
				return (-1);
			}

			if ((from_fd = open(fn, O_RDONLY)) == -1) {
				cvs_log(LP_ERRNO, "failed to open `%s'",
				    rfp->rf_path);
				return (-1);
			}

			if ((to_fd = open(rfp->rf_path,
			    O_WRONLY|O_TRUNC|O_CREAT)) == -1) {
				cvs_log(LP_ERRNO, "failed to open `%s'", fn);
				close(from_fd);
				return (-1);
			}

			bp = xmalloc(MAXBSIZE);
			while ((nread = read(from_fd, bp, MAXBSIZE)) > 0) {
				if (write(to_fd, bp, nread) != nread)
					goto err;
			}

			if (nread < 0) {
err:				if (unlink(rfp->rf_path) == -1)
					cvs_log(LP_ERRNO,
					    "failed to unlink `%s'",
					    rfp->rf_path);
				close(from_fd);
				close(to_fd);
				xfree(bp);
				return (-1);
			}

			close(from_fd);
			close(to_fd);
			xfree(bp);

			if (unlink(fn) == -1) {
				cvs_log(LP_ERRNO,
				    "failed to unlink `%s'", fn);
				return (-1);
			}
		} else {
			cvs_log(LP_ERRNO,
			    "failed to access temp RCS output file");
			return (-1);
		}
	}

	if ((chmod(rfp->rf_path, S_IRUSR|S_IRGRP|S_IROTH) == -1)) {
		cvs_log(LP_ERRNO, "failed to chmod `%s'",
		    rfp->rf_path);
		return (-1);
	}

	rfp->rf_flags |= RCS_SYNCED;

	return (0);
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
rcs_head_set(RCSFILE *file, const RCSNUM *rev)
{
	struct rcs_delta *rd;

	if ((rd = rcs_findrev(file, rev)) == NULL)
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

	ap = (struct rcs_access *)xmalloc(sizeof(*ap));
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

	symp = (struct rcs_sym *)xmalloc(sizeof(*symp));
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
	else {
		cvs_log(LP_ERRNO, "invalid lock mode %d", mode);
		return (-1);
	}

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
		if ((strcmp(lkp->rl_name, user) == 0) &&
		    (rcsnum_cmp(rev, lkp->rl_num, 0) == 0)) {
			rcs_errno = RCS_ERR_DUPENT;
			return (-1);
		}
	}

	lkp = (struct rcs_lock *)xmalloc(sizeof(*lkp));
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
		if ((strcmp(lkp->rl_name, user) == 0) &&
		    (rcsnum_cmp(lkp->rl_num, rev, 0) == 0))
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
 * Returns 0 on success, or -1 on failure.
 */
int
rcs_desc_set(RCSFILE *file, const char *desc)
{
	char *tmp;

	tmp = xstrdup(desc);
	if (file->rf_desc != NULL)
		xfree(file->rf_desc);
	file->rf_desc = tmp;
	file->rf_flags &= ~RCS_SYNCED;

	return (0);
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
 * Returns 0 on success, or -1 on failure.
 */
int
rcs_comment_set(RCSFILE *file, const char *comment)
{
	char *tmp;

	tmp = xstrdup(comment);
	if (file->rf_comment != NULL)
		xfree(file->rf_comment);
	file->rf_comment = tmp;
	file->rf_flags &= ~RCS_SYNCED;

	return (0);
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
rcs_patch_lines(struct cvs_lines *dlines, struct cvs_lines *plines)
{
	char op, *ep;
	struct cvs_line *lp, *dlp, *ndlp;
	int i, lineno, nbln;

	dlp = TAILQ_FIRST(&(dlines->l_lines));
	lp = TAILQ_FIRST(&(plines->l_lines));

	/* skip first bogus line */
	for (lp = TAILQ_NEXT(lp, l_list); lp != NULL;
	    lp = TAILQ_NEXT(lp, l_list)) {
		op = *(lp->l_line);
		lineno = (int)strtol((lp->l_line + 1), &ep, 10);
		if ((lineno > dlines->l_nblines) || (lineno < 0) ||
		    (*ep != ' ')) {
			cvs_log(LP_ERR,
			    "invalid line specification in RCS patch");
			return (-1);
		}
		ep++;
		nbln = (int)strtol(ep, &ep, 10);
		if ((nbln < 0) || (*ep != '\0')) {
			cvs_log(LP_ERR,
			    "invalid line number specification in RCS patch");
			return (-1);
		}

		/* find the appropriate line */
		for (;;) {
			if (dlp == NULL)
				break;
			if (dlp->l_lineno == lineno)
				break;
			if (dlp->l_lineno > lineno) {
				dlp = TAILQ_PREV(dlp, cvs_tqh, l_list);
			} else if (dlp->l_lineno < lineno) {
				ndlp = TAILQ_NEXT(dlp, l_list);
				if (ndlp->l_lineno > lineno)
					break;
				dlp = ndlp;
			}
		}
		if (dlp == NULL) {
			cvs_log(LP_ERR,
			    "can't find referenced line in RCS patch");
			return (-1);
		}

		if (op == 'd') {
			for (i = 0; (i < nbln) && (dlp != NULL); i++) {
				ndlp = TAILQ_NEXT(dlp, l_list);
				TAILQ_REMOVE(&(dlines->l_lines), dlp, l_list);
				dlp = ndlp;
			}
		} else if (op == 'a') {
			for (i = 0; i < nbln; i++) {
				ndlp = lp;
				lp = TAILQ_NEXT(lp, l_list);
				if (lp == NULL) {
					cvs_log(LP_ERR, "truncated RCS patch");
					return (-1);
				}
				TAILQ_REMOVE(&(plines->l_lines), lp, l_list);
				TAILQ_INSERT_AFTER(&(dlines->l_lines), dlp,
				    lp, l_list);
				dlp = lp;

				/* we don't want lookup to block on those */
				lp->l_lineno = lineno;

				lp = ndlp;
			}
		} else {
			cvs_log(LP_ERR, "unknown RCS patch operation `%c'", op);
			return (-1);
		}

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
 * cvs_buf_free() once the caller is done using it.
 */
BUF*
rcs_getrev(RCSFILE *rfp, RCSNUM *frev)
{
	int expmode, res;
	size_t len;
	void *bp;
	RCSNUM *crev, *rev;
	BUF *rbuf, *dbuf = NULL;
	struct rcs_delta *rdp = NULL;
	struct cvs_lines *lines;
	struct cvs_line *lp;
	char out[1024];				/* XXX */

	if (rfp->rf_head == NULL)
		return (NULL);

	if (frev == RCS_HEAD_REV)
		rev = rfp->rf_head;
	else
		rev = frev;

	res = rcsnum_cmp(rfp->rf_head, rev, 0);
	if (res == 1) {
		rcs_errno = RCS_ERR_NOENT;
		return (NULL);
	}

	rdp = rcs_findrev(rfp, rfp->rf_head);
	if (rdp == NULL) {
		cvs_log(LP_ERR, "failed to get RCS HEAD revision");
		return (NULL);
	}

	len = rdp->rd_tlen;
	if ((rbuf = cvs_buf_alloc(len, BUF_AUTOEXT)) == NULL)
		return (NULL);

	cvs_buf_append(rbuf, rdp->rd_text, len);

	if (res != 0) {
		/* Apply patches backwards to get the right version.
		 * This will need some rework to support sub branches.
		 */
		do {
			crev = rdp->rd_next;
			rdp = rcs_findrev(rfp, crev);
			if (rdp == NULL) {
				cvs_buf_free(rbuf);
				return (NULL);
			}

			if (cvs_buf_putc(rbuf, '\0') < 0) {
				cvs_buf_free(rbuf);
				return (NULL);
			}
			bp = cvs_buf_release(rbuf);
			rbuf = cvs_patchfile((char *)bp, (char *)rdp->rd_text,
			    rcs_patch_lines);
			xfree(bp);
			if (rbuf == NULL)
				break;
		} while (rcsnum_cmp(crev, rev, 0) != 0);
	}

	/*
	 * Do keyword expansion if required.
	 */
	if (rfp->rf_expand != NULL)
		expmode = rcs_kwexp_get(rfp);
	else
		expmode = RCS_KWEXP_DEFAULT;

	if ((rbuf != NULL) && !(expmode & RCS_KWEXP_NONE)) {
		if ((dbuf = cvs_buf_alloc(len, BUF_AUTOEXT)) == NULL)
			return (rbuf);
		if ((rdp = rcs_findrev(rfp, rev)) == NULL)
			return (rbuf);

		if (cvs_buf_putc(rbuf, '\0') < 0) {
			cvs_buf_free(dbuf);
			return (rbuf);
		}

		bp = cvs_buf_release(rbuf);
		if ((lines = cvs_splitlines((char *)bp)) != NULL) {
			res = 0;
			TAILQ_FOREACH(lp, &lines->l_lines, l_list) {
				if (res++ == 0)
					continue;
				rcs_expand_keywords(rfp->rf_path, rdp,
				    lp->l_line, out, sizeof(out), expmode);
				cvs_buf_fappend(dbuf, "%s\n", out);
			}
			cvs_freelines(lines);
		}
		xfree(bp);
	}

	return (dbuf);
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
	RCSNUM *old;

	old = rcsnum_alloc();
	if (rev == RCS_HEAD_REV) {
		if (rf->rf_flags & RCS_CREATE) {
			if ((rev = rcsnum_parse(RCS_HEAD_INIT)) == NULL)
				return (-1);
			rf->rf_head = rcsnum_alloc();
			rcsnum_cpy(rev, rf->rf_head, 0);
		} else {
			rcsnum_cpy(rf->rf_head, old, 0);
			rev = rcsnum_inc(rf->rf_head);
		}
	} else {
		if ((rdp = rcs_findrev(rf, rev)) != NULL) {
			rcs_errno = RCS_ERR_DUPENT;
			rcsnum_free(old);
			return (-1);
		}

		if (!(rf->rf_flags & RCS_CREATE)) {
			ordp = NULL;
			rcsnum_cpy(rev, old, 0);
			while (ordp == NULL) {
				old = rcsnum_dec(old);
				ordp = rcs_findrev(rf, old);
			}
		}
	}

	if ((pw = getpwuid(getuid())) == NULL)
		fatal("getpwuid failed");

	rdp = (struct rcs_delta *)xmalloc(sizeof(*rdp));
	memset(rdp, 0, sizeof(*rdp));

	TAILQ_INIT(&(rdp->rd_branches));
	TAILQ_INIT(&(rdp->rd_snodes));

	rdp->rd_num = rcsnum_alloc();
	rcsnum_cpy(rev, rdp->rd_num, 0);

	rdp->rd_next = rcsnum_alloc();

	if (!(rf->rf_flags & RCS_CREATE))
		rcsnum_cpy(old, rdp->rd_next, 0);

	rcsnum_free(old);

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
	int ret;
	struct rcs_delta *rdp;

	ret = 0;
	if (rev == RCS_HEAD_REV)
		rev = rf->rf_head;

	/* do we actually have that revision? */
	if ((rdp = rcs_findrev(rf, rev)) == NULL) {
		rcs_errno = RCS_ERR_NOENT;
		ret = -1;
	} else {
		/* XXX assumes it's not a sub node */
		TAILQ_REMOVE(&(rf->rf_delta), rdp, rd_list);
		rf->rf_ndelta--;
		rf->rf_flags &= ~RCS_SYNCED;
	}

	return (ret);

}

/*
 * rcs_findrev()
 *
 * Find a specific revision's delta entry in the tree of the RCS file <rfp>.
 * The revision number is given in <rev>.
 * Returns a pointer to the delta on success, or NULL on failure.
 */
struct rcs_delta *
rcs_findrev(RCSFILE *rfp, const RCSNUM *rev)
{
	u_int cmplen;
	struct rcs_delta *rdp;
	struct rcs_dlist *hp;
	int found;

	cmplen = 2;
	hp = &(rfp->rf_delta);

	do {
		found = 0;
		TAILQ_FOREACH(rdp, hp, rd_list) {
			if (rcsnum_cmp(rdp->rd_num, rev, cmplen) == 0) {
				if (cmplen == rev->rn_len)
					return (rdp);

				hp = &(rdp->rd_snodes);
				cmplen += 2;
				found = 1;
				break;
			}
		}
	} while (found && cmplen < rev->rn_len);

	return (NULL);
}

/*
 * rcs_kwexp_set()
 *
 * Set the keyword expansion mode to use on the RCS file <file> to <mode>.
 * Returns 0 on success, or -1 on failure.
 */
int
rcs_kwexp_set(RCSFILE *file, int mode)
{
	int i;
	char *tmp, buf[8] = "";

	if (RCS_KWEXP_INVAL(mode))
		return (-1);

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
		free(file->rf_expand);
	file->rf_expand = tmp;
	/* not synced anymore */
	file->rf_flags &= ~RCS_SYNCED;

	return (0);
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

	if ((code < 0) || ((code >= (int)RCS_NERR) && (code != RCS_ERR_ERRNO)))
		esp = NULL;
	else if (code == RCS_ERR_ERRNO)
		esp = strerror(errno);
	else
		esp = rcs_errstrs[code];
	return (esp);
}

void
rcs_kflag_usage(void)
{
	fprintf(stderr, "Valid expansion modes include:\n"
	    "\t-kkv\tGenerate keywords using the default form.\n"
	    "\t-kkvl\tLike -kkv, except locker's name inserted.\n"
	    "\t-kk\tGenerate only keyword names in keyword strings.\n"
	    "\t-kv\tGenerate only keyword values in keyword strings.\n"
	    "\t-ko\tGenerate old keyword string "
	    "(no changes from checked in file).\n"
	    "\t-kb\tGenerate binary file unmodified (merges not allowed).\n");
}

/*
 * rcs_parse()
 *
 * Parse the contents of file <path>, which are in the RCS format.
 * Returns 0 on success, or -1 on failure.
 */
static int
rcs_parse(RCSFILE *rfp)
{
	int ret;
	struct rcs_pdata *pdp;

	if (rfp->rf_flags & RCS_PARSED)
		return (0);

	pdp = (struct rcs_pdata *)xmalloc(sizeof(*pdp));
	memset(pdp, 0, sizeof(*pdp));

	pdp->rp_lines = 0;
	pdp->rp_pttype = RCS_TOK_ERR;

	pdp->rp_file = fopen(rfp->rf_path, "r");
	if (pdp->rp_file == NULL) {
		rcs_errno = RCS_ERR_ERRNO;
		cvs_log(LP_ERRNO, "failed to open RCS file `%s'", rfp->rf_path);
		rcs_freepdata(pdp);
		return (-1);
	}

	pdp->rp_buf = (char *)xmalloc((size_t)RCS_BUFSIZE);
	pdp->rp_blen = RCS_BUFSIZE;
	pdp->rp_bufend = pdp->rp_buf + pdp->rp_blen - 1;

	/* ditch the strict lock */
	rfp->rf_flags &= ~RCS_SLOCK;
	rfp->rf_pdata = pdp;

	if ((ret = rcs_parse_admin(rfp)) < 0) {
		rcs_freepdata(pdp);
		return (-1);
	} else if (ret == RCS_TOK_NUM) {
		for (;;) {
			ret = rcs_parse_delta(rfp);
			if (ret == 0)
				break;
			else if (ret == -1) {
				rcs_freepdata(pdp);
				return (-1);
			}
		}
	}

	ret = rcs_gettok(rfp);
	if (ret != RCS_TOK_DESC) {
		rcs_errno = RCS_ERR_PARSE;
		cvs_log(LP_ERR, "token `%s' found where RCS desc expected",
		    RCS_TOKSTR(rfp));
		rcs_freepdata(pdp);
		return (-1);
	}

	ret = rcs_gettok(rfp);
	if (ret != RCS_TOK_STRING) {
		rcs_errno = RCS_ERR_PARSE;
		cvs_log(LP_ERR, "token `%s' found where RCS desc expected",
		    RCS_TOKSTR(rfp));
		rcs_freepdata(pdp);
		return (-1);
	}

	rfp->rf_desc = xstrdup(RCS_TOKSTR(rfp));
	for (;;) {
		ret = rcs_parse_deltatext(rfp);
		if (ret == 0)
			break;
		else if (ret == -1) {
			rcs_freepdata(pdp);
			return (-1);
		}
	}

	rcs_freepdata(pdp);

	rfp->rf_pdata = NULL;
	rfp->rf_flags |= RCS_PARSED | RCS_SYNCED;

	return (0);
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
			cvs_log(LP_ERR, "parse error in RCS admin section");
			return (-1);
		} else if ((tok == RCS_TOK_NUM) || (tok == RCS_TOK_DESC)) {
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
			cvs_log(LP_ERR, "duplicate RCS key");
			return (-1);
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
				cvs_log(LP_ERR,
				    "invalid value type for RCS key `%s'",
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
					return (-1);
			} else if (tok == RCS_TOK_COMMENT) {
				rfp->rf_comment = xstrdup(RCS_TOKSTR(rfp));
			} else if (tok == RCS_TOK_EXPAND) {
				rfp->rf_expand = xstrdup(RCS_TOKSTR(rfp));
			}

			/* now get the expected semi-colon */
			ntok = rcs_gettok(rfp);
			if (ntok != RCS_TOK_SCOLON) {
				rcs_errno = RCS_ERR_PARSE;
				cvs_log(LP_ERR,
				    "missing semi-colon after RCS `%s' key",
				    rk->rk_str);
				return (-1);
			}
			break;
		case RCS_TOK_ACCESS:
			if (rcs_parse_access(rfp) < 0)
				return (-1);
			break;
		case RCS_TOK_SYMBOLS:
			if (rcs_parse_symbols(rfp) < 0)
				return (-1);
			break;
		case RCS_TOK_LOCKS:
			if (rcs_parse_locks(rfp) < 0)
				return (-1);
			break;
		default:
			rcs_errno = RCS_ERR_PARSE;
			cvs_log(LP_ERR,
			    "unexpected token `%s' in RCS admin section",
			    RCS_TOKSTR(rfp));
			return (-1);
		}
	}

	return (0);
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

	rdp = (struct rcs_delta *)xmalloc(sizeof(*rdp));
	memset(rdp, 0, sizeof(*rdp));

	rdp->rd_num = rcsnum_alloc();
	rdp->rd_next = rcsnum_alloc();

	TAILQ_INIT(&(rdp->rd_branches));
	TAILQ_INIT(&(rdp->rd_snodes));

	tok = rcs_gettok(rfp);
	if (tok != RCS_TOK_NUM) {
		rcs_errno = RCS_ERR_PARSE;
		cvs_log(LP_ERR, "unexpected token `%s' at start of delta",
		    RCS_TOKSTR(rfp));
		rcs_freedelta(rdp);
		return (-1);
	}
	rcsnum_aton(RCS_TOKSTR(rfp), NULL, rdp->rd_num);

	hmask = 0;
	ret = 0;
	tokstr = NULL;

	for (;;) {
		tok = rcs_gettok(rfp);
		if (tok == RCS_TOK_ERR) {
			rcs_errno = RCS_ERR_PARSE;
			cvs_log(LP_ERR, "parse error in RCS delta section");
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
			cvs_log(LP_ERR, "duplicate RCS key");
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
					cvs_log(LP_ERR, "missing mandatory "
					    "value to RCS key `%s'",
					    rk->rk_str);
					rcs_freedelta(rdp);
					return (-1);
				}
			}

			if (ntok != rk->rk_val) {
				rcs_errno = RCS_ERR_PARSE;
				cvs_log(LP_ERR,
				    "invalid value type for RCS key `%s'",
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
				cvs_log(LP_ERR,
				    "missing semi-colon after RCS `%s' key",
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
					cvs_log(LP_ERR,
					    "RCS date specification has %s "
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
			cvs_log(LP_ERR,
			    "unexpected token `%s' in RCS delta",
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
		cvs_log(LP_ERR,
		    "unexpected token `%s' at start of RCS delta text",
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
		cvs_log(LP_ERR, "RCS delta text `%s' has no matching delta",
		    RCS_TOKSTR(rfp));
		return (-1);
	}

	tok = rcs_gettok(rfp);
	if (tok != RCS_TOK_LOG) {
		rcs_errno = RCS_ERR_PARSE;
		cvs_log(LP_ERR, "unexpected token `%s' where RCS log expected",
		    RCS_TOKSTR(rfp));
		return (-1);
	}

	tok = rcs_gettok(rfp);
	if (tok != RCS_TOK_STRING) {
		rcs_errno = RCS_ERR_PARSE;
		cvs_log(LP_ERR, "unexpected token `%s' where RCS log expected",
		    RCS_TOKSTR(rfp));
		return (-1);
	}
	rdp->rd_log = xstrdup(RCS_TOKSTR(rfp));
	tok = rcs_gettok(rfp);
	if (tok != RCS_TOK_TEXT) {
		rcs_errno = RCS_ERR_PARSE;
		cvs_log(LP_ERR, "unexpected token `%s' where RCS text expected",
		    RCS_TOKSTR(rfp));
		return (-1);
	}

	tok = rcs_gettok(rfp);
	if (tok != RCS_TOK_STRING) {
		rcs_errno = RCS_ERR_PARSE;
		cvs_log(LP_ERR, "unexpected token `%s' where RCS text expected",
		    RCS_TOKSTR(rfp));
		return (-1);
	}

	rdp->rd_text = (u_char *)xmalloc(RCS_TOKLEN(rfp) + 1);
	strlcpy(rdp->rd_text, RCS_TOKSTR(rfp), (RCS_TOKLEN(rfp) + 1));
	rdp->rd_tlen = RCS_TOKLEN(rfp);

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
			cvs_log(LP_ERR, "unexpected token `%s' in access list",
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
			cvs_log(LP_ERR, "unexpected token `%s' in symbol list",
			    RCS_TOKSTR(rfp));
			return (-1);
		}

		symp = (struct rcs_sym *)xmalloc(sizeof(*symp));
		symp->rs_name = xstrdup(RCS_TOKSTR(rfp));
		symp->rs_num = rcsnum_alloc();

		type = rcs_gettok(rfp);
		if (type != RCS_TOK_COLON) {
			rcs_errno = RCS_ERR_PARSE;
			cvs_log(LP_ERR, "unexpected token `%s' in symbol list",
			    RCS_TOKSTR(rfp));
			rcsnum_free(symp->rs_num);
			xfree(symp->rs_name);
			xfree(symp);
			return (-1);
		}

		type = rcs_gettok(rfp);
		if (type != RCS_TOK_NUM) {
			rcs_errno = RCS_ERR_PARSE;
			cvs_log(LP_ERR, "unexpected token `%s' in symbol list",
			    RCS_TOKSTR(rfp));
			rcsnum_free(symp->rs_num);
			xfree(symp->rs_name);
			xfree(symp);
			return (-1);
		}

		if (rcsnum_aton(RCS_TOKSTR(rfp), NULL, symp->rs_num) < 0) {
			cvs_log(LP_ERR, "failed to parse RCS NUM `%s'",
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
			cvs_log(LP_ERR, "unexpected token `%s' in lock list",
			    RCS_TOKSTR(rfp));
			return (-1);
		}

		lkp = (struct rcs_lock *)xmalloc(sizeof(*lkp));
		lkp->rl_name = xstrdup(RCS_TOKSTR(rfp));
		lkp->rl_num = rcsnum_alloc();

		type = rcs_gettok(rfp);
		if (type != RCS_TOK_COLON) {
			rcs_errno = RCS_ERR_PARSE;
			cvs_log(LP_ERR, "unexpected token `%s' in symbol list",
			    RCS_TOKSTR(rfp));
			rcsnum_free(lkp->rl_num);
			xfree(lkp->rl_name);
			xfree(lkp);
			return (-1);
		}

		type = rcs_gettok(rfp);
		if (type != RCS_TOK_NUM) {
			rcs_errno = RCS_ERR_PARSE;
			cvs_log(LP_ERR, "unexpected token `%s' in symbol list",
			    RCS_TOKSTR(rfp));
			rcsnum_free(lkp->rl_num);
			xfree(lkp->rl_name);
			xfree(lkp);
			return (-1);
		}

		if (rcsnum_aton(RCS_TOKSTR(rfp), NULL, lkp->rl_num) < 0) {
			cvs_log(LP_ERR, "failed to parse RCS NUM `%s'",
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
			cvs_log(LP_ERR,
			    "missing semi-colon after `strict' keyword");
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
			cvs_log(LP_ERR,
			    "unexpected token `%s' in list of branches",
			    RCS_TOKSTR(rfp));
			return (-1);
		}

		brp = (struct rcs_branch *)xmalloc(sizeof(*brp));
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
	struct rcs_delta *crdp;

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

	while ((crdp = TAILQ_FIRST(&(rdp->rd_snodes))) != NULL) {
		TAILQ_REMOVE(&(rdp->rd_snodes), crdp, rd_list);
		rcs_freedelta(crdp);
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
		strlcpy(pdp->rp_buf, pdp->rp_ptok, pdp->rp_blen);
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
			if (!isalnum(ch) && ch != '_' && ch != '-') {
				ungetc(ch, pdp->rp_file);
				break;
			}
			*(bp++) = ch;
			pdp->rp_tlen++;
			if (bp == pdp->rp_bufend - 1) {
				len = bp - pdp->rp_buf;
				if (rcs_growbuf(rfp) < 0) {
					type = RCS_TOK_ERR;
					break;
				}
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
			if (ch == '@') {
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
				if (rcs_growbuf(rfp) < 0) {
					type = RCS_TOK_ERR;
					break;
				}
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
	strlcpy(pdp->rp_ptok, tok, sizeof(pdp->rp_ptok));
	return (0);
}


/*
 * rcs_growbuf()
 *
 * Attempt to grow the internal parse buffer for the RCS file <rf> by
 * RCS_BUFEXTSIZE.
 * In case of failure, the original buffer is left unmodified.
 * Returns 0 on success, or -1 on failure.
 */
static int
rcs_growbuf(RCSFILE *rf)
{
	void *tmp;
	struct rcs_pdata *pdp = (struct rcs_pdata *)rf->rf_pdata;

	tmp = realloc(pdp->rp_buf, pdp->rp_blen + RCS_BUFEXTSIZE);
	if (tmp == NULL) {
		rcs_errno = RCS_ERR_ERRNO;
		cvs_log(LP_ERRNO, "failed to grow RCS parse buffer");
		return (-1);
	}

	pdp->rp_buf = (char *)tmp;
	pdp->rp_blen += RCS_BUFEXTSIZE;
	pdp->rp_bufend = pdp->rp_buf + pdp->rp_blen - 1;

	return (0);
}

/*
 * rcs_strprint()
 *
 * Output an RCS string <str> of size <slen> to the stream <stream>.  Any
 * '@' characters are escaped.  Otherwise, the string can contain arbitrary
 * binary data.
 */
static int
rcs_strprint(const u_char *str, size_t slen, FILE *stream)
{
	const u_char *ap, *ep, *sp;
	size_t ret;

	if (slen == 0)
		return (0);

	ep = str + slen - 1;

	for (sp = str; sp <= ep;)  {
		ap = memchr(sp, '@', ep - sp);
		if (ap == NULL)
			ap = ep;
		ret = fwrite(sp, sizeof(u_char), ap - sp + 1, stream);

		if (*ap == '@')
			putc('@', stream);
		sp = ap + 1;
	}

	return (0);
}

/*
 * rcs_expand_keywords()
 *
 * Expand any RCS keywords in <line> into <out>
 */
static int
rcs_expand_keywords(char *rcsfile, struct rcs_delta *rdp, char *line, char *out,
    size_t len, int mode)
{
	int kwtype;
	u_int i, j, found;
	char *c, *kwstr, *start;
	char expbuf[128], buf[128];

	kwtype = 0;
	kwstr = NULL;
	i = 0;

	/*
	 * Keyword formats:
	 * $Keyword$
	 * $Keyword: value$
	 */
	memset(out, '\0', len);
	for (c = line; *c != '\0' && i < len; *c++) {
		out[i++] = *c;
		if (*c == '$') {
			/* remember start of this possible keyword */
			start = c;

			/* first following character has to be alphanumeric */
			*c++;
			if (!isalpha(*c)) {
				c = start;
				continue;
			}

			/* look for any matching keywords */
			found = 0;
			for (j = 0; j < RCS_NKWORDS; j++) {
				if (!strncmp(c, rcs_expkw[j].kw_str,
				    strlen(rcs_expkw[j].kw_str))) {
					found = 1;
					kwstr = rcs_expkw[j].kw_str;
					kwtype = rcs_expkw[j].kw_type;
					break;
				}
			}

			/* unknown keyword, continue looking */
			if (found == 0) {
				c = start;
				continue;
			}

			/* next character has to be ':' or '$' */
			c += strlen(kwstr);
			if (*c != ':' && *c != '$') {
				c = start;
				continue;
			}

			/*
			 * if the next character was ':' we need to look for
			 * an '$' before the end of the line to be sure it is
			 * in fact a keyword.
			 */
			if (*c == ':') {
				while (*c++) {
					if (*c == '$' || *c == '\n')
						break;
				}

				if (*c != '$') {
					c = start;
					continue;
				}
			}

			/* start constructing the expansion */
			expbuf[0] = '\0';

			if (mode & RCS_KWEXP_NAME) {
				strlcat(expbuf, "$", sizeof(expbuf));
				strlcat(expbuf, kwstr, sizeof(expbuf));
				if (mode & RCS_KWEXP_VAL)
					strlcat(expbuf, ": ", sizeof(expbuf));
			}

			/*
			 * order matters because of RCS_KW_ID and
			 * RCS_KW_HEADER here
			 */
			if (mode & RCS_KWEXP_VAL) {
				if (kwtype & RCS_KW_RCSFILE) {
					if (!(kwtype & RCS_KW_FULLPATH))
						strlcat(expbuf,
						    basename(rcsfile),
						    sizeof(expbuf));
					else
						strlcat(expbuf, rcsfile,
						    sizeof(expbuf));
					strlcat(expbuf, " ", sizeof(expbuf));
				}

				if (kwtype & RCS_KW_REVISION) {
					rcsnum_tostr(rdp->rd_num, buf,
					    sizeof(buf));
					strlcat(buf, " ", sizeof(buf));
					strlcat(expbuf, buf, sizeof(expbuf));
				}

				if (kwtype & RCS_KW_DATE) {
					strftime(buf, sizeof(buf),
					    "%Y/%m/%d %H:%M:%S ",
					    &rdp->rd_date);
					strlcat(expbuf, buf, sizeof(expbuf));
				}

				if (kwtype & RCS_KW_AUTHOR) {
					strlcat(expbuf, rdp->rd_author,
					    sizeof(expbuf));
					strlcat(expbuf, " ", sizeof(expbuf));
				}

				if (kwtype & RCS_KW_STATE) {
					strlcat(expbuf, rdp->rd_state,
					    sizeof(expbuf));
					strlcat(expbuf, " ", sizeof(expbuf));
				}

				/* order does not matter anymore below */
				if (kwtype & RCS_KW_LOG)
					strlcat(expbuf, " ", sizeof(expbuf));

				if (kwtype & RCS_KW_SOURCE) {
					strlcat(expbuf, rcsfile,
					    sizeof(expbuf));
					strlcat(expbuf, " ", sizeof(expbuf));
				}

				if (kwtype & RCS_KW_NAME)
					strlcat(expbuf, " ", sizeof(expbuf));
			}

			/* end the expansion */
			if (mode & RCS_KWEXP_NAME)
				strlcat(expbuf, "$", sizeof(expbuf));

			out[--i] = '\0';
			strlcat(out, expbuf, len);
			i += strlen(expbuf);
		}
	}

	return (0);
}

/*
 * rcs_deltatext_set()
 *
 * Set deltatext for <rev> in RCS file <rfp> to <dtext>
 * Returns -1 on error, 0 on success.
 */
int
rcs_deltatext_set(RCSFILE *rfp, RCSNUM *rev, const char *dtext)
{
	size_t len;
	struct rcs_delta *rdp;

	if ((rdp = rcs_findrev(rfp, rev)) == NULL)
		return (-1);

	if (rdp->rd_text != NULL)
		xfree(rdp->rd_text);

	len = strlen(dtext);
	if (len != 0) {
		rdp->rd_text = (u_char *)xmalloc(len);
		rdp->rd_tlen = len - 1;
		strlcpy(rdp->rd_text, dtext, len);
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


#if !defined(RCSPROG)

static char *month_tab[] = {
	"Jan",
	"Feb",
	"Mar",
	"Apr",
	"May",
	"Jun",
	"Jul",
	"Aug",
	"Sep",
	"Oct",
	"Nov",
	"Dec"
};

/*
 * Checkout a certain revision <rev> of RCS file <rf> to either standard
 * output when running in server mode, or to <fpath> when running in local mode.
 *
 * If type is CHECKOUT_REV_MERGED we have an extra argument, which
 * is the buffer containing the merged file.
 *
 * If type is CHECKOUT_REV_REMOVED, the file has been removed and we
 * need to do the same thing.
 */
int
cvs_checkout_rev(RCSFILE *rf, RCSNUM *rev, CVSFILE *cf, char *fpath,
    int local, int type, ...)
{
	BUF *bp;
	int l, ret, fsize;
	char timebuf[32], entry[MAXPATHLEN], copyfile[MAXPATHLEN];
	char *content, *repo, buf[MAXPATHLEN], modestr[16];
	struct cvsroot *root;
	struct cvs_ent *ent;
	va_list ap;
	time_t rcstime;
	struct timeval tv[2];
	struct tm *tp;
	RCSNUM *oldrev;

	bp = NULL;
	ret = -1;
	content = NULL;
	oldrev = NULL;

	if ((type != CHECKOUT_REV_MERGED) && (type != CHECKOUT_REV_REMOVED)) {
		/* fetch the contents of the revision */
		if ((bp = rcs_getrev(rf, rev)) == NULL) {
			cvs_log(LP_ERR, "revision '%s' not found in file '%s'",
			    rcsnum_tostr(rev, buf, sizeof(buf)), fpath);
			goto out;
		}
	} else if (type != CHECKOUT_REV_REMOVED) {
		va_start(ap, type);
		bp = va_arg(ap, BUF *);
		va_end(ap);
	}

	if (type == CHECKOUT_REV_CREATED)
		rcstime = rcs_rev_getdate(rf, rev);
	else if (type == CHECKOUT_REV_MERGED ||
	    type == CHECKOUT_REV_UPDATED) {
		time(&rcstime);
		if ((rcstime = cvs_hack_time(rcstime, 1)) < 0)
			goto out;
	}

	if (type == CHECKOUT_REV_CREATED ||
	    type == CHECKOUT_REV_UPDATED) {
		ctime_r(&rcstime, timebuf);
		l = strlen(timebuf);
		if ((l > 0) && (timebuf[l - 1] == '\n'))
			timebuf[--l] = '\0';

		l = snprintf(entry, sizeof(entry), "/%s/%s/%s/%s/", cf->cf_name,
		    rcsnum_tostr(rev, buf, sizeof(buf)),
		    (local == 1) ? timebuf : "",
		    (type == CHECKOUT_REV_MERGED) ? "+=" : "");
		if (l == -1 || l >= (int)sizeof(buf))
			goto out;
	}

	if (type == CHECKOUT_REV_MERGED) {
		oldrev = rcsnum_alloc();
		rcsnum_cpy(rev, oldrev, 0);

		if (rcsnum_dec(oldrev) == NULL)
			goto out;

		l = snprintf(copyfile, sizeof(copyfile), ".#%s.%s",
		    cf->cf_name, rcsnum_tostr(oldrev, buf, sizeof(buf)));
		if (l == -1 || l >= (int)sizeof(copyfile))
			goto out;
	}

	root = CVS_DIR_ROOT(cf);
	repo = CVS_DIR_REPO(cf);

	/*
	 * In local mode, just copy the entire contents to fpath.
	 * In server mode, we need to send it to the client together with
	 * some responses.
	 */
	if (local) {
		l = 0;
		if (cf->cf_entry == NULL) {
			l = 1;
			cf->cf_entry = cvs_ent_open(cf->cf_dir, O_RDWR);
			if (cf->cf_entry == NULL) {
				cvs_log(LP_ERR,
				    "failed to open Entry file '%s'", cf->cf_dir);
				goto out;
			}
		}

		cvs_ent_remove(cf->cf_entry, cf->cf_name, 1);
		if (type != CHECKOUT_REV_REMOVED) {
			cvs_ent_addln(cf->cf_entry, entry);
			ent = cvs_ent_get(cf->cf_entry, cf->cf_name);
			ent->processed = 1;
		}

		if (l == 1)
			cvs_ent_close(cf->cf_entry);

		switch (type) {
		case CHECKOUT_REV_REMOVED:
			if (cvs_unlink(fpath) < 0)
				goto out;
			break;
		case CHECKOUT_REV_MERGED:
			/* XXX move the old file when merging */
		case CHECKOUT_REV_UPDATED:
		case CHECKOUT_REV_CREATED:
			if (cvs_buf_write(bp, fpath, cf->cf_mode) < 0) {
				cvs_log(LP_ERR, "failed to update file '%s'",
				    fpath);
				goto out;
			}

			/*
			 * correct the time first
			 */
			if ((rcstime = cvs_hack_time(rcstime, 0)) == 0)
				goto out;

			tv[0].tv_sec = rcstime;
			tv[0].tv_usec = 0;
			tv[1] = tv[0];
			if (utimes(fpath, tv) == -1)
				cvs_log(LP_ERRNO, "failed to set timestamps");
			break;
		}
	} else {
		/* sanity */
		if (cf->cf_type != DT_REG) {
			cvs_log(LP_ERR, "cvs_checkout_rev: none DT_REG file");
			goto out;
		}

		/*
		 * if we are removing a file, we don't need this stuff.
		 */
		if (type != CHECKOUT_REV_REMOVED) {
			if ((rcstime = cvs_hack_time(rcstime, 0)) == 0)
				goto out;

			tp = gmtime(&rcstime);
			l = snprintf(timebuf, sizeof(timebuf),
			    "%02d %s %d %02d:%02d:%02d -0000",
			    tp->tm_mday, month_tab[tp->tm_mon],
			    tp->tm_year + 1900, tp->tm_hour,
			    tp->tm_min, tp->tm_sec);
			if (l == -1 || l >= (int)sizeof(timebuf))
				goto out;

			fsize = cvs_buf_len(bp);
			cvs_modetostr(cf->cf_mode, modestr, sizeof(modestr));
			if (cvs_buf_putc(bp, '\0') < 0)
				goto out;
			content = cvs_buf_release(bp);
			bp = NULL;
		}

		if (type == CHECKOUT_REV_MERGED) {
			printf("Copy-file %s/\n", (cf->cf_dir != NULL) ?
			    cf->cf_dir : ".");
			printf("%s/%s/%s\n", root->cr_dir, repo, cf->cf_name);
			printf("%s\n", copyfile);
		}

		switch (type) {
		case CHECKOUT_REV_MERGED:
			printf("Merged");
			break;
		case CHECKOUT_REV_REMOVED:
			printf("Removed");
			break;
		case CHECKOUT_REV_CREATED:
			printf("Mod-time %s\n", timebuf);
			printf("Created");
			break;
		default:
			cvs_log(LP_ERR, "cvs_checkout_rev: bad type %d",
			    type);
			goto out;
		}

		printf(" %s/\n", (cf->cf_dir != NULL) ? cf->cf_dir : ".");
		printf("%s/%s\n", repo, cf->cf_name);

		if (type != CHECKOUT_REV_REMOVED) {
			printf("%s\n", entry);
			printf("%s\n%d\n%s", modestr, fsize, content);
		}
	}

	ret = 0;

out:
	if (oldrev != NULL)
		rcsnum_free(oldrev);
	if (bp != NULL)
		cvs_buf_free(bp);
	if (content != NULL)
		xfree(content);

	return (ret);
}

#endif
