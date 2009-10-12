#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
#ifndef NO_PPPORT_H
#   define NEED_my_strlcpy
#   define NEED_my_strlcat
#   include "ppport.h"
#endif

#ifdef I_UNISTD
#   include <unistd.h>
#endif

/* The realpath() implementation from OpenBSD 3.9 to 4.2 (realpath.c 1.13)
 * Renamed here to bsd_realpath() to avoid library conflicts.
 */

/* See
 * http://www.xray.mpe.mpg.de/mailing-lists/perl5-porters/2004-11/msg00979.html
 * for the details of why the BSD license is compatible with the
 * AL/GPL standard perl license.
 */

/*
 * Copyright (c) 2003 Constantin S. Svintsoff <kostik@iclub.nsu.ru>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The names of the authors may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* OpenBSD system #includes removed since the Perl ones should do. --jhi */

#ifndef MAXSYMLINKS
#define MAXSYMLINKS 8
#endif

/*
 * char *realpath(const char *path, char resolved[MAXPATHLEN]);
 *
 * Find the real name of path, by removing all ".", ".." and symlink
 * components.  Returns (resolved) on success, or (NULL) on failure,
 * in which case the path which caused trouble is left in (resolved).
 */
static
char *
bsd_realpath(const char *path, char resolved[MAXPATHLEN])
{
#ifdef VMS
       dTHX;
       return Perl_rmsexpand(aTHX_ (char*)path, resolved, NULL, 0);
#else
	char *p, *q, *s;
	size_t left_len, resolved_len;
	unsigned symlinks;
	int serrno;
	char left[MAXPATHLEN], next_token[MAXPATHLEN], symlink[MAXPATHLEN];

	serrno = errno;
	symlinks = 0;
	if (path[0] == '/') {
		resolved[0] = '/';
		resolved[1] = '\0';
		if (path[1] == '\0')
			return (resolved);
		resolved_len = 1;
		left_len = my_strlcpy(left, path + 1, sizeof(left));
	} else {
		if (getcwd(resolved, MAXPATHLEN) == NULL) {
			my_strlcpy(resolved, ".", MAXPATHLEN);
		return (NULL);
	}
		resolved_len = strlen(resolved);
		left_len = my_strlcpy(left, path, sizeof(left));
	}
	if (left_len >= sizeof(left) || resolved_len >= MAXPATHLEN) {
		errno = ENAMETOOLONG;
		return (NULL);
	}

	/*
	 * Iterate over path components in `left'.
	 */
	while (left_len != 0) {
		/*
		 * Extract the next path component and adjust `left'
		 * and its length.
		 */
		p = strchr(left, '/');
		s = p ? p : left + left_len;
		if (s - left >= sizeof(next_token)) {
			errno = ENAMETOOLONG;
			return (NULL);
			}
		memcpy(next_token, left, s - left);
		next_token[s - left] = '\0';
		left_len -= s - left;
		if (p != NULL)
			memmove(left, s + 1, left_len + 1);
		if (resolved[resolved_len - 1] != '/') {
			if (resolved_len + 1 >= MAXPATHLEN) {
				errno = ENAMETOOLONG;
				return (NULL);
		}
			resolved[resolved_len++] = '/';
			resolved[resolved_len] = '\0';
	}
		if (next_token[0] == '\0')
			continue;
		else if (strcmp(next_token, ".") == 0)
			continue;
		else if (strcmp(next_token, "..") == 0) {
			/*
			 * Strip the last path component except when we have
			 * single "/"
			 */
			if (resolved_len > 1) {
				resolved[resolved_len - 1] = '\0';
				q = strrchr(resolved, '/') + 1;
				*q = '\0';
				resolved_len = q - resolved;
			}
			continue;
    }

	/*
		 * Append the next path component and lstat() it. If
		 * lstat() fails we still can return successfully if
		 * there are no more path components left.
	 */
		resolved_len = my_strlcat(resolved, next_token, MAXPATHLEN);
		if (resolved_len >= MAXPATHLEN) {
			errno = ENAMETOOLONG;
			return (NULL);
		}
	#if defined(HAS_LSTAT) && defined(HAS_READLINK) && defined(HAS_SYMLINK)
		{
			struct stat sb;
			if (lstat(resolved, &sb) != 0) {
				if (errno == ENOENT && p == NULL) {
					errno = serrno;
					return (resolved);
				}
				return (NULL);
			}
			if (S_ISLNK(sb.st_mode)) {
				int slen;
				
				if (symlinks++ > MAXSYMLINKS) {
					errno = ELOOP;
					return (NULL);
				}
				slen = readlink(resolved, symlink, sizeof(symlink) - 1);
				if (slen < 0)
					return (NULL);
				symlink[slen] = '\0';
				if (symlink[0] == '/') {
					resolved[1] = 0;
					resolved_len = 1;
				} else if (resolved_len > 1) {
					/* Strip the last path component. */
					resolved[resolved_len - 1] = '\0';
					q = strrchr(resolved, '/') + 1;
					*q = '\0';
					resolved_len = q - resolved;
				}

	/*
				 * If there are any path components left, then
				 * append them to symlink. The result is placed
				 * in `left'.
	 */
				if (p != NULL) {
					if (symlink[slen - 1] != '/') {
						if (slen + 1 >= sizeof(symlink)) {
			errno = ENAMETOOLONG;
							return (NULL);
		}
						symlink[slen] = '/';
						symlink[slen + 1] = 0;
	}
					left_len = my_strlcat(symlink, left, sizeof(left));
					if (left_len >= sizeof(left)) {
						errno = ENAMETOOLONG;
						return (NULL);
	}
	}
				left_len = my_strlcpy(left, symlink, sizeof(left));
			}
		}
	#endif
	}

	/*
	 * Remove trailing slash except when the resolved pathname
	 * is a single "/".
	 */
	if (resolved_len > 1 && resolved[resolved_len - 1] == '/')
		resolved[resolved_len - 1] = '\0';
	return (resolved);
#endif
}

#ifndef SV_CWD_RETURN_UNDEF
#define SV_CWD_RETURN_UNDEF \
sv_setsv(sv, &PL_sv_undef); \
return FALSE
#endif

#ifndef OPpENTERSUB_HASTARG
#define OPpENTERSUB_HASTARG     32      /* Called from OP tree. */
#endif

#ifndef dXSTARG
#define dXSTARG SV * targ = ((PL_op->op_private & OPpENTERSUB_HASTARG) \
                             ? PAD_SV(PL_op->op_targ) : sv_newmortal())
#endif

#ifndef XSprePUSH
#define XSprePUSH (sp = PL_stack_base + ax - 1)
#endif

#ifndef SV_CWD_ISDOT
#define SV_CWD_ISDOT(dp) \
    (dp->d_name[0] == '.' && (dp->d_name[1] == '\0' || \
        (dp->d_name[1] == '.' && dp->d_name[2] == '\0')))
#endif

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
  {
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
  }
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
getcwd(...)
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
PROTOTYPE: DISABLE
PPCODE:
{
    dXSTARG;
    char *path;
    char buf[MAXPATHLEN];

    path = pathsv ? SvPV_nolen(pathsv) : (char *)".";

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

#if defined(WIN32) && !defined(UNDER_CE)

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

    New(0,dir,MAXPATHLEN,char);
    if (_getdcwd(drive, dir, MAXPATHLEN)) {
        sv_setpvn(TARG, dir, strlen(dir));
        SvPOK_only(TARG);
    }
    else
        sv_setsv(TARG, &PL_sv_undef);

    Safefree(dir);

    XSprePUSH; PUSHTARG;
#ifndef INCOMPLETE_TAINTS
    SvTAINTED_on(TARG);
#endif
}

#endif
