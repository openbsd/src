/*
 * Copyright (c) 1983, 1993
 *	The Regents of the University of California.  All rights reserved.
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
static char rcsid[] = "$OpenBSD: opendir.c,v 1.10 2004/05/18 02:05:52 jfb Exp $";
#endif /* LIBC_SCCS and not lint */

#include <sys/param.h>
#include <sys/mount.h>
#include <sys/stat.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int direntcmp(const void *, const void *);

/*
 * Comparison function for sorting dirent structures that never returns 0;
 * this causes qsort() to emulate a stable sort.
 */
static int
direntcmp(const void *d1, const void *d2)
{
	int i;

	i = strcmp((*(struct dirent **)d1)->d_name,
	    (*(struct dirent **)d2)->d_name);
	return (i != 0 ? i : (char *)d2 - (char *)d1);
}

/*
 * Open a directory.
 */
DIR *
opendir(const char *name)
{

	return (__opendir2(name, DTF_HIDEW|DTF_NODUP));
}

DIR *
__opendir2(const char *name, int flags)
{
	DIR *dirp;
	int fd;
	struct stat sb;
	int pagesz;
	int incr;
	int unionstack;

	if ((fd = open(name, O_RDONLY | O_NONBLOCK)) == -1)
		return (NULL);
	if (fstat(fd, &sb) || !S_ISDIR(sb.st_mode)) {
		errno = ENOTDIR;
		close(fd);
		return (NULL);
	}
	if (fcntl(fd, F_SETFD, FD_CLOEXEC) == -1 ||
	    (dirp = (DIR *)malloc(sizeof(DIR))) == NULL) {
		close(fd);
		return (NULL);
	}

	/*
	 * If the machine's page size is an exact multiple of DIRBLKSIZ,
	 * use a buffer that is cluster boundary aligned.
	 * Hopefully this can be a big win someday by allowing page trades
	 * to user space to be done by getdirentries()
	 */
	if (((pagesz = getpagesize()) % DIRBLKSIZ) == 0)
		incr = pagesz;
	else
		incr = DIRBLKSIZ;

	/*
	 * Determine whether this directory is the top of a union stack.
	 */
	if (flags & DTF_NODUP) {
		struct statfs sfb;

		if (fstatfs(fd, &sfb) < 0) {
			free(dirp);
			close(fd);
			return (NULL);
		}
		unionstack = strncmp(sfb.f_fstypename, MOUNT_UNION, MFSNAMELEN) == 0 ||
			     (sfb.f_flags & MNT_UNION);
	} else {
		unionstack = 0;
	}

	if (unionstack) {
		int len = 0;
		int space = 0;
		char *buf = 0;
		char *ddptr = 0;
		char *ddeptr;
		int n;
		struct dirent **dpv;

		/*
		 * The strategy here is to read all the directory
		 * entries into a buffer, sort the buffer, and
		 * remove duplicate entries by setting the inode
		 * number to zero.
		 */

		do {
			/*
			 * Always make at least DIRBLKSIZ bytes
			 * available to getdirentries
			 */
			if (space < DIRBLKSIZ) {
				char *nbuf;

				space += incr;
				len += incr;
				nbuf = realloc(buf, len);
				if (nbuf == NULL) {
					if (buf)
						free(buf);
					free(dirp);
					close(fd);
					return (NULL);
				}
				buf = nbuf;
				ddptr = buf + (len - space);
			}

			n = getdirentries(fd, ddptr, space, &dirp->dd_seek);
			if (n > 0) {
				ddptr += n;
				space -= n;
			}
		} while (n > 0);

		ddeptr = ddptr;
		flags |= __DTF_READALL;

		/*
		 * Re-open the directory.
		 * This has the effect of rewinding back to the
		 * top of the union stack and is needed by
		 * programs which plan to fchdir to a descriptor
		 * which has also been read -- see fts.c.
		 */
		if (flags & DTF_REWIND) {
			(void) close(fd);
			if ((fd = open(name, O_RDONLY)) == -1) {
				free(buf);
				free(dirp);
				return (NULL);
			}
		}

		/*
		 * There is now a buffer full of (possibly) duplicate
		 * names.
		 */
		dirp->dd_buf = buf;

		/*
		 * Go round this loop twice...
		 *
		 * Scan through the buffer, counting entries.
		 * On the second pass, save pointers to each one.
		 * Then sort the pointers and remove duplicate names.
		 */
		for (dpv = 0;;) {
			for (n = 0, ddptr = buf; ddptr < ddeptr;) {
				struct dirent *dp;

				dp = (struct dirent *) ddptr;
				if ((long)dp & 03)
					break;
				if ((dp->d_reclen <= 0) ||
				    (dp->d_reclen > (ddeptr + 1 - ddptr)))
					break;
				ddptr += dp->d_reclen;
				if (dp->d_fileno) {
					if (dpv)
						dpv[n] = dp;
					n++;
				}
			}

			if (dpv) {
				struct dirent *xp;

				/*
				 * This sort must be stable.
				 */
				qsort(dpv, n, sizeof(*dpv), direntcmp);

				dpv[n] = NULL;
				xp = NULL;

				/*
				 * Scan through the buffer in sort order,
				 * zapping the inode number of any
				 * duplicate names.
				 */
				for (n = 0; dpv[n]; n++) {
					struct dirent *dp = dpv[n];

					if ((xp == NULL) ||
					    strcmp(dp->d_name, xp->d_name))
						xp = dp;
					else
						dp->d_fileno = 0;
					if (dp->d_type == DT_WHT &&
					    (flags & DTF_HIDEW))
						dp->d_fileno = 0;
				}

				free(dpv);
				break;
			} else {
				if (n+1 > SIZE_T_MAX / sizeof(struct dirent *))
					break;
				dpv = malloc((n+1) * sizeof(struct dirent *));
				if (dpv == NULL)
					break;
			}
		}

		dirp->dd_len = len;
		dirp->dd_size = ddptr - dirp->dd_buf;
	} else {
		dirp->dd_len = incr;
		dirp->dd_buf = malloc(dirp->dd_len);
		if (dirp->dd_buf == NULL) {
			free(dirp);
			close (fd);
			return (NULL);
		}
		dirp->dd_seek = 0;
		flags &= ~DTF_REWIND;
	}

	dirp->dd_loc = 0;
	dirp->dd_fd = fd;
	dirp->dd_flags = flags;

	/*
	 * Set up seek point for rewinddir.
	 */
	dirp->dd_rewind = telldir(dirp);

	return (dirp);
}
