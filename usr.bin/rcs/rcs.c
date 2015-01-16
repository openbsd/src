/*	$OpenBSD: rcs.c,v 1.82 2015/01/16 06:40:11 deraadt Exp $	*/
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

#include <sys/param.h>	/* MAXBSIZE */
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
#include "rcsparse.h"
#include "rcsprog.h"
#include "rcsutil.h"
#include "xmalloc.h"

#define MINIMUM(a, b)	(((a) < (b)) ? (a) : (b))

/* invalid characters in RCS states */
static const char rcs_state_invch[] = RCS_STATE_INVALCHAR;

/* invalid characters in RCS symbol names */
static const char rcs_sym_invch[] = RCS_SYM_INVALCHAR;

struct rcs_kw rcs_expkw[] =  {
	{ "Author",	RCS_KW_AUTHOR   },
	{ "Date",	RCS_KW_DATE     },
	{ "Locker",	RCS_KW_LOCKER   },
	{ "Header",	RCS_KW_HEADER   },
	{ "Id",		RCS_KW_ID       },
	{ "OpenBSD",	RCS_KW_ID       },
	{ "Log",	RCS_KW_LOG      },
	{ "Name",	RCS_KW_NAME     },
	{ "RCSfile",	RCS_KW_RCSFILE  },
	{ "Revision",	RCS_KW_REVISION },
	{ "Source",	RCS_KW_SOURCE   },
	{ "State",	RCS_KW_STATE    },
};

int rcs_errno = RCS_ERR_NOERR;
char *timezone_flag = NULL;

int		rcs_patch_lines(struct rcs_lines *, struct rcs_lines *);
static int	rcs_movefile(char *, char *, mode_t, u_int);

static void	rcs_freedelta(struct rcs_delta *);
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
	if (fd == -1)
		rfp->rf_file = NULL;
	else if ((rfp->rf_file = fdopen(fd, "r")) == NULL)
		err(1, "rcs_open: fdopen: `%s'", path);

	TAILQ_INIT(&(rfp->rf_delta));
	TAILQ_INIT(&(rfp->rf_access));
	TAILQ_INIT(&(rfp->rf_symbols));
	TAILQ_INIT(&(rfp->rf_locks));

	if (!(rfp->rf_flags & RCS_CREATE)) {
		if (rcsparse_init(rfp))
			errx(1, "could not parse admin data");

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

	if (rfp->rf_file != NULL)
		fclose(rfp->rf_file);
	if (rfp->rf_path != NULL)
		xfree(rfp->rf_path);
	if (rfp->rf_comment != NULL)
		xfree(rfp->rf_comment);
	if (rfp->rf_expand != NULL)
		xfree(rfp->rf_expand);
	if (rfp->rf_desc != NULL)
		xfree(rfp->rf_desc);
	if (rfp->rf_pdata != NULL)
		rcsparse_free(rfp);
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
	char numbuf[RCS_REV_BUFSZ], *fn;
	struct rcs_access *ap;
	struct rcs_sym *symp;
	struct rcs_branch *brp;
	struct rcs_delta *rdp;
	struct rcs_lock *lkp;
	size_t len;
	int fd;

	fn = NULL;

	if (rfp->rf_flags & RCS_SYNCED)
		return;

	/* Write operations need the whole file parsed */
	if (rcsparse_deltatexts(rfp, NULL))
		errx(1, "problem parsing deltatexts");

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

	worklist_add(fn, &temp_files);

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
		if (RCSNUM_ISBRANCH(symp->rs_num))
			rcsnum_addmagic(symp->rs_num);
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
			fprintf(fp, "\n\t%s", rcsnum_tostr(brp->rb_num, numbuf,
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
			if (len == 0 || rdp->rd_log[len-1] != '\n')
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
		(void)fclose(src);
		return (-1);
	}
	if (fchmod(fileno(dst), perm)) {
		warn("%s", to);
		(void)unlink(to);
		(void)fclose(src);
		(void)fclose(dst);
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

	(void)unlink(from);

out:
	(void)fclose(src);
	(void)fclose(dst);
	xfree(buf);

	return (0);
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
	const unsigned char *cp;

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
				dlp = TAILQ_PREV(dlp, tqh, l_list);
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
					    tqh);
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
 * buf_free() once the caller is done using it.
 */
BUF *
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

	/* No matter what, we'll need everything parsed up until the description
           so go for it. */
	if (rcsparse_deltas(rfp, NULL))
		return (NULL);

	rdp = rcs_findrev(rfp, rfp->rf_head);
	if (rdp == NULL) {
		warnx("failed to get RCS HEAD revision");
		return (NULL);
	}

	if (rdp->rd_tlen == 0)
		if (rcsparse_deltatexts(rfp, rfp->rf_head))
			return (NULL);

	len = rdp->rd_tlen;
	if (len == 0) {
		rbuf = buf_alloc(1);
		buf_empty(rbuf);
		return (rbuf);
	}

	rbuf = buf_alloc(len);
	buf_append(rbuf, rdp->rd_text, len);

	isbranch = 0;
	brev = NULL;

	/*
	 * If a branch was passed, get the latest revision on it.
	 */
	if (RCSNUM_ISBRANCH(rev)) {
		brev = rev;
		rdp = rcs_findrev(rfp, rev);
		if (rdp == NULL) {
			buf_free(rbuf);
			return (NULL);
		}

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
				numlen = MINIMUM(brev->rn_len,
				    rb->rb_num->rn_len - 1);
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
			buf_free(rbuf);
			return (NULL);
		}

		plen = rdp->rd_tlen;
		dlen = buf_len(rbuf);
		patch = rdp->rd_text;
		data = buf_release(rbuf);
		/* check if we have parsed this rev's deltatext */
		if (rdp->rd_tlen == 0)
			if (rcsparse_deltatexts(rfp, rdp->rd_num))
				return (NULL);

		rbuf = rcs_patchfile(data, dlen, patch, plen, rcs_patch_lines);
		xfree(data);

		if (rbuf == NULL)
			break;
	} while (rcsnum_cmp(crev, rev, 0) != 0);

	return (rbuf);
}

void
rcs_delta_stats(struct rcs_delta *rdp, int *ladded, int *lremoved)
{
	struct rcs_lines *plines;
	struct rcs_line *lp;
	int added, i, nbln, removed;
	char op, *ep;
	u_char tmp;

	added = removed = 0;

	plines = rcs_splitlines(rdp->rd_text, rdp->rd_tlen);
	lp = TAILQ_FIRST(&(plines->l_lines));

	/* skip first bogus line */
	for (lp = TAILQ_NEXT(lp, l_list); lp != NULL;
		lp = TAILQ_NEXT(lp, l_list)) {
			if (lp->l_len < 2)
				errx(1,
				    "line too short, RCS patch seems broken");
			op = *(lp->l_line);
			/* NUL-terminate line buffer for strtol() safety. */
			tmp = lp->l_line[lp->l_len - 1];
			lp->l_line[lp->l_len - 1] = '\0';
			(void)strtol((lp->l_line + 1), &ep, 10);
			ep++;
			nbln = (int)strtol(ep, &ep, 10);
			/* Restore the last byte of the buffer */
			lp->l_line[lp->l_len - 1] = tmp;
			if (nbln < 0)
				errx(1, "invalid line number specification "
				    "in RCS patch");

			if (op == 'a') {
				added += nbln;
				for (i = 0; i < nbln; i++) {
					lp = TAILQ_NEXT(lp, l_list);
					if (lp == NULL)
						errx(1, "truncated RCS patch");
				}
			} else if (op == 'd')
				removed += nbln;
			else
				errx(1, "unknown RCS patch operation '%c'", op);
	}

	rcs_freelines(plines);

	*ladded = added;
	*lremoved = removed;
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
 * If <author> is NULL, set the author for this revision to the current user.
 * Returns 0 on success, or -1 on failure.
 */
int
rcs_rev_add(RCSFILE *rf, RCSNUM *rev, const char *msg, time_t date,
    const char *author)
{
	time_t now;
	struct passwd *pw;
	struct rcs_delta *ordp, *rdp;

	if (rev == RCS_HEAD_REV) {
		if (rf->rf_flags & RCS_CREATE) {
			if ((rev = rcsnum_parse(RCS_HEAD_INIT)) == NULL)
				return (-1);
			rf->rf_head = rev;
		} else {
			rev = rcsnum_inc(rf->rf_head);
		}
	} else {
		if ((rdp = rcs_findrev(rf, rev)) != NULL) {
			rcs_errno = RCS_ERR_DUPENT;
			return (-1);
		}
	}

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

	if (!author && !(author = getlogin())) {
		if (!(pw = getpwuid(getuid())))
			errx(1, "getpwuid failed");
		author = pw->pw_name;
	}
	rdp->rd_author = xstrdup(author);
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
	nextrdp = (struct rcs_delta *)TAILQ_PREV(rdp, tqh, rd_list);

	newdeltatext = prevbuf = nextbuf = NULL;

	if (prevrdp != NULL) {
		if ((prevbuf = rcs_getrev(rf, prevrdp->rd_num)) == NULL)
			errx(1, "error getting revision");
	}

	if (prevrdp != NULL && nextrdp != NULL) {
		if ((nextbuf = rcs_getrev(rf, nextrdp->rd_num)) == NULL)
			errx(1, "error getting revision");

		newdiff = buf_alloc(64);

		/* calculate new diff */
		(void)xasprintf(&path_tmp1, "%s/diff1.XXXXXXXXXX", rcs_tmpdir);
		buf_write_stmp(nextbuf, path_tmp1);
		buf_free(nextbuf);

		(void)xasprintf(&path_tmp2, "%s/diff2.XXXXXXXXXX", rcs_tmpdir);
		buf_write_stmp(prevbuf, path_tmp2);
		buf_free(prevbuf);

		diff_format = D_RCSDIFF;
		if (diffreg(path_tmp1, path_tmp2, newdiff, D_FORCEASCII) == D_ERROR)
			errx(1, "diffreg failed");

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
		if (rcsparse_deltas(rfp, rev))
			return (NULL);
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
	if (file->rf_expand == NULL)
		return (RCS_KWEXP_DEFAULT);

	return (rcs_kflag_get(file->rf_expand));
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

	if (flags == NULL || !(len = strlen(flags)))
		return (RCS_KWEXP_ERR);

	fl = 0;
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
			fl |= RCS_KWEXP_NONE;
		} else	/* unknown letter */
			fl |= RCS_KWEXP_ERR;
	}

	return (fl);
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
rcs_expand_keywords(char *rcsfile_in, struct rcs_delta *rdp, BUF *bp, int mode)
{
	BUF *newbuf;
	u_char *c, *kw, *fin;
	char buf[256], *tmpf, resolved[PATH_MAX], *rcsfile;
	u_char *line, *line2;
	u_int i, j;
	int kwtype;
	int found;
	struct tm tb;

	tb = rdp->rd_date;
	if (timezone_flag != NULL)
		rcs_set_tz(timezone_flag, rdp, &tb);

	if (realpath(rcsfile_in, resolved) == NULL)
		rcsfile = rcsfile_in;
	else
		rcsfile = resolved;

	newbuf = buf_alloc(buf_len(bp));

	/*
	 * Keyword formats:
	 * $Keyword$
	 * $Keyword: value$
	 */
	c = buf_get(bp);
	fin = c + buf_len(bp);
	/* Copying to newbuf is deferred until the first keyword. */
	found = 0;

	while (c < fin) {
		kw = memchr(c, '$', fin - c);
		if (kw == NULL)
			break;
		++kw;
		if (found) {
			/* Copy everything up to and including the $. */
			buf_append(newbuf, c, kw - c);
		}
		c = kw;
		/* c points after the $ now. */
		if (c == fin)
			break;
		if (!isalpha(*c)) /* all valid keywords start with a letter */
			continue;

		for (i = 0; i < RCS_NKWORDS; ++i) {
			size_t kwlen;

			kwlen = strlen(rcs_expkw[i].kw_str);
			/*
			 * kwlen must be less than clen since clen includes
			 * either a terminating `$' or a `:'.
			 */
			if (c + kwlen < fin &&
			    memcmp(c , rcs_expkw[i].kw_str, kwlen) == 0 &&
			    (c[kwlen] == '$' || c[kwlen] == ':')) {
				c += kwlen;
				break;
			}
		}
		if (i == RCS_NKWORDS)
			continue;
		kwtype = rcs_expkw[i].kw_type;

		/*
		 * If the next character is ':' we need to look for an '$'
		 * before the end of the line to be sure it is in fact a
		 * keyword.
		 */
		if (*c == ':') {
			for (; c < fin; ++c) {
				if (*c == '$' || *c == '\n')
					break;
			}

			if (*c != '$') {
				if (found)
					buf_append(newbuf, kw, c - kw);
				continue;
			}
		}
		++c;

		if (!found) {
			found = 1;
			/* Copy everything up to and including the $. */
			buf_append(newbuf, buf_get(bp), kw - buf_get(bp));
		}

		if (mode & RCS_KWEXP_NAME) {
			buf_puts(newbuf, rcs_expkw[i].kw_str);
			if (mode & RCS_KWEXP_VAL)
				buf_puts(newbuf, ": ");
		}

		/* Order matters because of RCS_KW_ID and RCS_KW_HEADER. */
		if (mode & RCS_KWEXP_VAL) {
			if (kwtype & (RCS_KW_RCSFILE|RCS_KW_LOG)) {
				if ((kwtype & RCS_KW_FULLPATH) ||
				    (tmpf = strrchr(rcsfile, '/')) == NULL)
					buf_puts(newbuf, rcsfile);
				else
					buf_puts(newbuf, tmpf + 1);
				buf_putc(newbuf, ' ');
			}

			if (kwtype & RCS_KW_REVISION) {
				rcsnum_tostr(rdp->rd_num, buf, sizeof(buf));
				buf_puts(newbuf, buf);
				buf_putc(newbuf, ' ');
			}

			if (kwtype & RCS_KW_DATE) {
				strftime(buf, sizeof(buf),
				    "%Y/%m/%d %H:%M:%S ", &tb);
				buf_puts(newbuf, buf);
			}

			if (kwtype & RCS_KW_AUTHOR) {
				buf_puts(newbuf, rdp->rd_author);
				buf_putc(newbuf, ' ');
			}

			if (kwtype & RCS_KW_STATE) {
				buf_puts(newbuf, rdp->rd_state);
				buf_putc(newbuf, ' ');
			}

			/* Order does not matter anymore below. */
			if (kwtype & RCS_KW_SOURCE) {
				buf_puts(newbuf, rcsfile);
				buf_putc(newbuf, ' ');
			}

			if (kwtype & RCS_KW_NAME)
				buf_putc(newbuf, ' ');

			if ((kwtype & RCS_KW_LOCKER)) {
				if (rdp->rd_locker) {
					buf_puts(newbuf, rdp->rd_locker);
					buf_putc(newbuf, ' ');
				}
			}
		}

		/* End the expansion. */
		if (mode & RCS_KWEXP_NAME)
			buf_putc(newbuf, '$');

		if (kwtype & RCS_KW_LOG) {
			line = memrchr(buf_get(bp), '\n', kw - buf_get(bp) - 1);
			if (line == NULL)
				line = buf_get(bp);
			else
				++line;
			line2 = kw - 1;
			while (line2 > line && line2[-1] == ' ')
				--line2;

			buf_putc(newbuf, '\n');
			buf_append(newbuf, line, kw - 1 - line);
			buf_puts(newbuf, "Revision ");
			rcsnum_tostr(rdp->rd_num, buf, sizeof(buf));
			buf_puts(newbuf, buf);
			buf_puts(newbuf, "  ");
			strftime(buf, sizeof(buf), "%Y/%m/%d %H:%M:%S", &tb);
			buf_puts(newbuf, buf);

			buf_puts(newbuf, "  ");
			buf_puts(newbuf, rdp->rd_author);
			buf_putc(newbuf, '\n');

			for (i = 0; rdp->rd_log[i]; i += j) {
				j = strcspn(rdp->rd_log + i, "\n");
				if (j == 0)
					buf_append(newbuf, line, line2 - line);
				else
					buf_append(newbuf, line, kw - 1 - line);
				if (rdp->rd_log[i + j])
					++j;
				buf_append(newbuf, rdp->rd_log + i, j);
			}
			buf_append(newbuf, line, line2 - line);
			for (j = 0; c + j < fin; ++j) {
				if (c[j] != ' ')
					break;
			}
			if (c + j == fin || c[j] == '\n')
				c += j;
		}
	}

	if (found) {
		buf_append(newbuf, c, fin - c);
		buf_free(bp);
		return (newbuf);
	} else {
		buf_free(newbuf);
		return (bp);
	}
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
	if (rcsparse_deltatexts(rfp, NULL))
		return (-1);

	if ((rdp = rcs_findrev(rfp, rev)) == NULL)
		return (-1);

	if (rdp->rd_text != NULL)
		xfree(rdp->rd_text);

	len = buf_len(bp);
	dtext = buf_release(bp);
	bp = NULL;

	if (len != 0) {
		rdp->rd_text = xmalloc(len);
		rdp->rd_tlen = len;
		(void)memcpy(rdp->rd_text, dtext, len);
	} else {
		rdp->rd_text = NULL;
		rdp->rd_tlen = 0;
	}

	if (dtext != NULL)
		xfree(dtext);

	return (0);
}

/*
 * rcs_rev_setlog()
 *
 * Sets the log message of revision <rev> to <logtext>.
 */
int
rcs_rev_setlog(RCSFILE *rfp, RCSNUM *rev, const char *logtext)
{
	struct rcs_delta *rdp;

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
	int ret;
	const unsigned char *cp;

	ret = 0;
	cp = state;
	if (!isalpha(*cp++))
		return (-1);

	for (; *cp != '\0'; cp++)
		if (!isgraph(*cp) || (strchr(rcs_state_invch, *cp) != NULL)) {
			ret = -1;
			break;
		}

	return (ret);
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
	expmode = rcs_kwexp_get(rf);

	if (!(expmode & RCS_KWEXP_NONE)) {
		if ((rdp = rcs_findrev(rf, rev)) == NULL)
			errx(1, "could not fetch revision");
		return (rcs_expand_keywords(rf->rf_path, rdp, bp, expmode));
	}
	return (bp);
}
