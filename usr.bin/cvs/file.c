/*	$OpenBSD: file.c,v 1.1 2004/07/14 03:33:09 jfb Exp $	*/
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

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/stat.h>

#include <pwd.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fnmatch.h>

#include "cvs.h"
#include "log.h"


#define CVS_IGN_STATIC    0x01     /* pattern is static, no need to glob */



#define CVS_CHAR_ISMETA(c)  ((c == '*') || (c == '?') || (c == '['))



/* ignore pattern */
struct cvs_ignpat {
	char  ip_pat[MAXNAMLEN];
	int   ip_flags;
	TAILQ_ENTRY (cvs_ignpat) ip_list;
};


/*
 * Standard patterns to ignore.
 */

static const char *cvs_ign_std[] = {
	".",
	"..",
	"*.o",
	"*.so",
	"*.bak",
	"*.orig",
	"*.rej",
	"*.exe",
	"*.depend",
	"CVS",
	"core",
#ifdef OLD_SMELLY_CRUFT
	"RCSLOG",
	"tags",
	"TAGS",
	"RCS",
	"SCCS",
	"#*",
	".#*",
	",*",
#endif
};


TAILQ_HEAD(, cvs_ignpat)  cvs_ign_pats;


/*
 * cvs_file_init()
 *
 */

int
cvs_file_init(void)
{
	int i;
	size_t len;
	char path[MAXPATHLEN], buf[MAXNAMLEN];
	FILE *ifp;
	struct passwd *pwd;

	TAILQ_INIT(&cvs_ign_pats);

	/* standard patterns to ignore */
	for (i = 0; i < sizeof(cvs_ign_std)/sizeof(char *); i++)
		cvs_file_ignore(cvs_ign_std[i]); 

	/* read the cvsignore file in the user's home directory, if any */
	pwd = getpwuid(getuid());
	if (pwd != NULL) {
		snprintf(path, sizeof(path), "%s/.cvsignore", pwd->pw_dir);
		ifp = fopen(path, "r");
		if (ifp == NULL) {
			if (errno != ENOENT)
				cvs_log(LP_ERRNO, "failed to open `%s'", path);
		}
		else {
			while (fgets(buf, sizeof(buf), ifp) != NULL) {
				len = strlen(buf);
				if (len == 0)
					continue;
				if (buf[len - 1] != '\n') {
					cvs_log(LP_ERR, "line too long in `%s'",
					    path);
				}
				buf[--len] = '\0';
				cvs_file_ignore(buf);
			}
			(void)fclose(ifp);
		}
	}

	return (0);
}


/*
 * cvs_file_ignore()
 *
 * Add the pattern <pat> to the list of patterns for files to ignore.
 * Returns 0 on success, or -1 on failure.
 */

int
cvs_file_ignore(const char *pat)
{
	char *cp;
	struct cvs_ignpat *ip;

	ip = (struct cvs_ignpat *)malloc(sizeof(*ip));
	if (ip == NULL) {
		cvs_log(LP_ERR, "failed to allocate space for ignore pattern");
		return (-1);
	}

	strlcpy(ip->ip_pat, pat, sizeof(ip->ip_pat));

	/* check if we will need globbing for that pattern */
	ip->ip_flags = CVS_IGN_STATIC;
	for (cp = ip->ip_pat; *cp != '\0'; cp++) {
		if (CVS_CHAR_ISMETA(*cp)) {
			ip->ip_flags &= ~CVS_IGN_STATIC;
			break;
		}
	}

	TAILQ_INSERT_TAIL(&cvs_ign_pats, ip, ip_list);

	return (0);
}


/*
 * cvs_file_isignored()
 *
 * Returns 1 if the filename <file> is matched by one of the ignore
 * patterns, or 0 otherwise.
 */

int
cvs_file_isignored(const char *file)
{
	struct cvs_ignpat *ip;

	TAILQ_FOREACH(ip, &cvs_ign_pats, ip_list) {
		if (ip->ip_flags & CVS_IGN_STATIC) {
			if (strcmp(file, ip->ip_pat) == 0)
				return (1);
		}
		else if (fnmatch(ip->ip_pat, file, FNM_PERIOD) == 0)
			return (1);
	}

	return (0);
}


/*
 * cvs_file_getv()
 *
 * Get a vector of all the files found in the directory <dir> and not
 * matching any of the ignore patterns.  The number of files found is
 * returned in <nfiles>.
 * Returns a pointer to a dynamically-allocated string vector on success,
 * or NULL on failure.
 */

char**
cvs_file_getv(const char *dir, int *nfiles)
{
	int nf, ret, fd;
	long base;
	void *dp, *ep, *tmp;
	char fbuf[1024], **fvec;
	struct dirent *ent;

	*nfiles = 0;
	fvec = NULL;

	fd = open(dir, O_RDONLY);
	if (fd == -1) {
		cvs_log(LP_ERRNO, "failed to open `%s'", dir);
		return (NULL);
	}
	ret = getdirentries(fd, fbuf, sizeof(fbuf), &base);
	if (ret == -1) {
		cvs_log(LP_ERRNO, "failed to get directory entries");
		(void)close(fd);
		return (NULL);
	}

	dp = fbuf;
	ep = fbuf + (size_t)ret;
	while (dp < ep) {
		ent = (struct dirent *)dp;
		dp += ent->d_reclen;

		if (cvs_file_isignored(ent->d_name))
			continue;

		tmp = realloc(fvec, (*nfiles + 1) * sizeof(char *));
		if (tmp == NULL) {
			cvs_log(LP_ERRNO, "failed to reallocate file vector");
			(void)close(fd);
			free(fvec);
			return (NULL);
		}
		fvec[++(*nfiles)] = strdup(ent->d_name);

		*nfiles++;
	}

	(void)close(fd);

	return (fvec);
}


/*
 * cvs_file_freev()
 *
 * Free a file vector obtained with cvs_file_getv().
 */

void
cvs_file_freev(char **fvec, int nfiles)
{
	int i;

	for (i = 0; i < nfiles; i++)
		free(fvec[i]);
	free(fvec);
}
