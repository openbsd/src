#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#ifdef I_UNISTD
#   include <unistd.h>
#endif

/* The realpath() implementation from OpenBSD 2.9 (realpath.c 1.4)
 * Renamed here to bsd_realpath() to avoid library conflicts.
 * --jhi 2000-06-20 */

/*
 * Copyright (c) 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Jan-Simon Pendry.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char *rcsid = "$OpenBSD: realpath.c,v 1.4 1998/05/18 09:55:19 deraadt Exp $";
#endif /* LIBC_SCCS and not lint */

/* OpenBSD system #includes removed since the Perl ones should do. --jhi */

#ifndef MAXSYMLINKS
#define MAXSYMLINKS 8
#endif

/*
 * char *realpath(const char *path, char resolved_path[MAXPATHLEN]);
 *
 * Find the real name of path, by removing all ".", ".." and symlink
 * components.  Returns (resolved) on success, or (NULL) on failure,
 * in which case the path which caused trouble is left in (resolved).
 */
static
char *
bsd_realpath(path, resolved)
	const char *path;
	char *resolved;
{
#ifdef VMS
       dTHX;
       return Perl_rmsexpand(aTHX_ (char*)path, resolved, NULL, 0);
#else
	int rootd, serrno;
	char *p, *q, wbuf[MAXPATHLEN];
	int symlinks = 0;

	/* Save the starting point. */
#ifdef HAS_FCHDIR
	int fd;

	if ((fd = open(".", O_RDONLY)) < 0) {
		(void)strcpy(resolved, ".");
		return (NULL);
	}
#else
	char wd[MAXPATHLEN];

	if (getcwd(wd, MAXPATHLEN - 1) == NULL) {
		(void)strcpy(resolved, ".");
		return (NULL);
	}
#endif

	/*
	 * Find the dirname and basename from the path to be resolved.
	 * Change directory to the dirname component.
	 * lstat the basename part.
	 *     if it is a symlink, read in the value and loop.
	 *     if it is a directory, then change to that directory.
	 * get the current directory name and append the basename.
	 */
	(void)strncpy(resolved, path, MAXPATHLEN - 1);
	resolved[MAXPATHLEN - 1] = '\0';
loop:
	q = strrchr(resolved, '/');
	if (q != NULL) {
		p = q + 1;
		if (q == resolved)
			q = "/";
		else {
			do {
				--q;
			} while (q > resolved && *q == '/');
			q[1] = '\0';
			q = resolved;
		}
		if (chdir(q) < 0)
			goto err1;
	} else
		p = resolved;

#if defined(HAS_LSTAT) && defined(HAS_READLINK) && defined(HAS_SYMLINK)
    {
	struct stat sb;
	/* Deal with the last component. */
	if (lstat(p, &sb) == 0) {
		if (S_ISLNK(sb.st_mode)) {
			int n;
			if (++symlinks > MAXSYMLINKS) {
				errno = ELOOP;
				goto err1;
			}
			n = readlink(p, resolved, MAXPATHLEN-1);
			if (n < 0)
				goto err1;
			resolved[n] = '\0';
			goto loop;
		}
		if (S_ISDIR(sb.st_mode)) {
			if (chdir(p) < 0)
				goto err1;
			p = "";
		}
	}
    }
#endif

	/*
	 * Save the last component name and get the full pathname of
	 * the current directory.
	 */
	(void)strcpy(wbuf, p);
	if (getcwd(resolved, MAXPATHLEN) == 0)
		goto err1;

	/*
	 * Join the two strings together, ensuring that the right thing
	 * happens if the last component is empty, or the dirname is root.
	 */
	if (resolved[0] == '/' && resolved[1] == '\0')
		rootd = 1;
	else
		rootd = 0;

	if (*wbuf) {
		if (strlen(resolved) + strlen(wbuf) + (1 - rootd) + 1 > MAXPATHLEN) {
			errno = ENAMETOOLONG;
			goto err1;
		}
		if (rootd == 0)
			(void)strcat(resolved, "/");
		(void)strcat(resolved, wbuf);
	}

	/* Go back to where we came from. */
#ifdef HAS_FCHDIR
	if (fchdir(fd) < 0) {
		serrno = errno;
		goto err2;
	}
#else
	if (chdir(wd) < 0) {
		serrno = errno;
		goto err2;
	}
#endif

	/* It's okay if the close fails, what's an fd more or less? */
#ifdef HAS_FCHDIR
	(void)close(fd);
#endif
	return (resolved);

err1:	serrno = errno;
#ifdef HAS_FCHDIR
	(void)fchdir(fd);
#else
	(void)chdir(wd);
#endif

err2:
#ifdef HAS_FCHDIR
	(void)close(fd);
#endif
	errno = serrno;
	return (NULL);
#endif
}

#ifndef getcwd_sv
/* Taken from perl 5.8's util.c */
#define getcwd_sv(a) Perl_getcwd_sv(aTHX_ a)
int Perl_getcwd_sv(pTHX_ register SV *sv)
{
#ifndef PERL_MICRO

#ifndef INCOMPLETE_TAINTS
    SvTAINTED_on(sv);
#endif

#ifdef HAS_GETCWD
    {
	char buf[MAXPATHLEN];

	/* Some getcwd()s automatically allocate a buffer of the given
	 * size from the heap if they are given a NULL buffer pointer.
	 * The problem is that this behaviour is not portable. */
	if (getcwd(buf, sizeof(buf) - 1)) {
	    STRLEN len = strlen(buf);
	    sv_setpvn(sv, buf, len);
	    return TRUE;
	}
	else {
	    sv_setsv(sv, &PL_sv_undef);
	    return FALSE;
	}
    }

#else

    Stat_t statbuf;
    int orig_cdev, orig_cino, cdev, cino, odev, oino, tdev, tino;
    int namelen, pathlen=0;
    DIR *dir;
    Direntry_t *dp;

    (void)SvUPGRADE(sv, SVt_PV);

    if (PerlLIO_lstat(".", &statbuf) < 0) {
	SV_CWD_RETURN_UNDEF;
    }

    orig_cdev = statbuf.st_dev;
    orig_cino = statbuf.st_ino;
    cdev = orig_cdev;
    cino = orig_cino;

    for (;;) {
	odev = cdev;
	oino = cino;

	if (PerlDir_chdir("..") < 0) {
	    SV_CWD_RETURN_UNDEF;
	}
	if (PerlLIO_stat(".", &statbuf) < 0) {
	    SV_CWD_RETURN_UNDEF;
	}

	cdev = statbuf.st_dev;
	cino = statbuf.st_ino;

	if (odev == cdev && oino == cino) {
	    break;
	}
	if (!(dir = PerlDir_open("."))) {
	    SV_CWD_RETURN_UNDEF;
	}

	while ((dp = PerlDir_read(dir)) != NULL) {
#ifdef DIRNAMLEN
	    namelen = dp->d_namlen;
#else
	    namelen = strlen(dp->d_name);
#endif
	    /* skip . and .. */
	    if (SV_CWD_ISDOT(dp)) {
		continue;
	    }

	    if (PerlLIO_lstat(dp->d_name, &statbuf) < 0) {
		SV_CWD_RETURN_UNDEF;
	    }

	    tdev = statbuf.st_dev;
	    tino = statbuf.st_ino;
	    if (tino == oino && tdev == odev) {
		break;
	    }
	}

	if (!dp) {
	    SV_CWD_RETURN_UNDEF;
	}

	if (pathlen + namelen + 1 >= MAXPATHLEN) {
	    SV_CWD_RETURN_UNDEF;
	}

	SvGROW(sv, pathlen + namelen + 1);

	if (pathlen) {
	    /* shift down */
	    Move(SvPVX(sv), SvPVX(sv) + namelen + 1, pathlen, char);
	}

	/* prepend current directory to the front */
	*SvPVX(sv) = '/';
	Move(dp->d_name, SvPVX(sv)+1, namelen, char);
	pathlen += (namelen + 1);

#ifdef VOID_CLOSEDIR
	PerlDir_close(dir);
#else
	if (PerlDir_close(dir) < 0) {
	    SV_CWD_RETURN_UNDEF;
	}
#endif
    }

    if (pathlen) {
	SvCUR_set(sv, pathlen);
	*SvEND(sv) = '\0';
	SvPOK_only(sv);

	if (PerlDir_chdir(SvPVX(sv)) < 0) {
	    SV_CWD_RETURN_UNDEF;
	}
    }
    if (PerlLIO_stat(".", &statbuf) < 0) {
	SV_CWD_RETURN_UNDEF;
    }

    cdev = statbuf.st_dev;
    cino = statbuf.st_ino;

    if (cdev != orig_cdev || cino != orig_cino) {
	Perl_croak(aTHX_ "Unstable directory path, "
		   "current directory changed unexpectedly");
    }

    return TRUE;
#endif

#else
    return FALSE;
#endif
}

#endif


MODULE = Cwd		PACKAGE = Cwd

PROTOTYPES: ENABLE

void
fastcwd()
PROTOTYPE: DISABLE
PPCODE:
{
    dXSTARG;
    getcwd_sv(TARG);
    XSprePUSH; PUSHTARG;
#ifndef INCOMPLETE_TAINTS
    SvTAINTED_on(TARG);
#endif
}

void
abs_path(pathsv=Nullsv)
    SV *pathsv
PPCODE:
{
    dXSTARG;
    char *path;
    char buf[MAXPATHLEN];

    path = pathsv ? SvPV_nolen(pathsv) : ".";

    if (bsd_realpath(path, buf)) {
        sv_setpvn(TARG, buf, strlen(buf));
        SvPOK_only(TARG);
	SvTAINTED_on(TARG);
    }
    else
        sv_setsv(TARG, &PL_sv_undef);

    XSprePUSH; PUSHTARG;
#ifndef INCOMPLETE_TAINTS
    SvTAINTED_on(TARG);
#endif
}

#ifdef WIN32

void
getdcwd(...)
PPCODE:
{
    dXSTARG;
    int drive;
    char *dir;

    /* Drive 0 is the current drive, 1 is A:, 2 is B:, 3 is C: and so on. */
    if ( items == 0 ||
        (items == 1 && (!SvOK(ST(0)) || (SvPOK(ST(0)) && !SvCUR(ST(0))))))
        drive = 0;
    else if (items == 1 && SvPOK(ST(0)) && SvCUR(ST(0)) &&
             isALPHA(SvPVX(ST(0))[0]))
        drive = toUPPER(SvPVX(ST(0))[0]) - 'A' + 1;
    else
        croak("Usage: getdcwd(DRIVE)");

    /* Pass a NULL pointer as the second argument to have space allocated. */
    if (dir = _getdcwd(drive, NULL, MAXPATHLEN)) {
        sv_setpvn(TARG, dir, strlen(dir));
        free(dir);
        SvPOK_only(TARG);
    }
    else
        sv_setsv(TARG, &PL_sv_undef);

    XSprePUSH; PUSHTARG;
#ifndef INCOMPLETE_TAINTS
    SvTAINTED_on(TARG);
#endif
}

#endif
