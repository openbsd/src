/*	$OpenBSD: fsmagic.c,v 1.7 2003/03/11 21:26:26 ian Exp $	*/

/*
 * fsmagic - magic based on filesystem info - directory, special files, etc.
 *
 * Copyright (c) Ian F. Darwin 1986-1995.
 * Software written by Ian F. Darwin and others;
 * maintained 1995-present by Christos Zoulas and others.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by Ian F. Darwin and others.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *  
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <err.h>
#ifndef major
# if defined(__SVR4) || defined(_SVR4_SOURCE)
#  include <sys/mkdev.h>
# endif
#endif
#ifndef	major			/* if `major' not defined in types.h, */
#include <sys/sysmacros.h>	/* try this one. */
#endif
#ifndef	major	/* still not defined? give up, manual intervention needed */
		/* If cc tries to compile this, read and act on it. */
		/* On most systems cpp will discard it automatically */
		Congratulations, you have found a portability bug.
		Please grep /usr/include/sys and edit the above #include 
		to point at the file that defines the "major" macro.
#endif	/*major*/

#include "file.h"

#ifndef	lint
static char *moduleid = "$OpenBSD: fsmagic.c,v 1.7 2003/03/11 21:26:26 ian Exp $";
#endif	/* lint */

int
fsmagic(fn, sb)
const char *fn;
struct stat *sb;
{
	int ret = 0;

	/*
	 * Fstat is cheaper but fails for files you don't have read perms on.
	 * On 4.2BSD and similar systems, use lstat() to identify symlinks.
	 */
#ifdef	S_IFLNK
	if (!lflag)
		ret = lstat(fn, sb);
	else
#endif
	ret = stat(fn, sb);	/* don't merge into if; see "ret =" above */

	if (ret) {
		ckfprintf(stdout,
			/* Yes, I do mean stdout. */
			/* No \n, caller will provide. */
			"can't stat `%s' (%s).", fn, strerror(errno));
		return 1;
	}

	if (sb->st_mode & S_ISUID) ckfputs("setuid ", stdout);
	if (sb->st_mode & S_ISGID) ckfputs("setgid ", stdout);
	if (sb->st_mode & S_ISVTX) ckfputs("sticky ", stdout);
	
	switch (sb->st_mode & S_IFMT) {
	case S_IFDIR:
		ckfputs("directory", stdout);
		return 1;
	case S_IFCHR:
		(void) printf("character special (%ld/%ld)",
			(long) major(sb->st_rdev), (long) minor(sb->st_rdev));
		return 1;
	case S_IFBLK:
		(void) printf("block special (%ld/%ld)",
			(long) major(sb->st_rdev), (long) minor(sb->st_rdev));
		return 1;
	/* TODO add code to handle V7 MUX and Blit MUX files */
#ifdef	S_IFIFO
	case S_IFIFO:
		ckfputs("fifo (named pipe)", stdout);
		return 1;
#endif
#ifdef	S_IFLNK
	case S_IFLNK:
		{
			char buf[BUFSIZ+4];
			int nch;
			struct stat tstatbuf;

			if ((nch = readlink(fn, buf, BUFSIZ-1)) <= 0) {
				ckfprintf(stdout, "unreadable symlink (%s).", 
				      strerror(errno));
				return 1;
			}
			buf[nch] = '\0';	/* readlink(2) forgets this */

			/* If broken symlink, say so and quit early. */
			if (*buf == '/') {
			    if (stat(buf, &tstatbuf) < 0) {
				ckfprintf(stdout,
					"broken symbolic link to %s", buf);
				return 1;
			    }
			}
			else {
			    char *tmp;
			    char buf2[BUFSIZ+BUFSIZ+4];

			    if ((tmp = strrchr(fn,  '/')) == NULL) {
				tmp = buf; /* in current directory anyway */
			    } else if (strlen(fn) + strlen(buf) > sizeof(buf2)-1) {
				ckfprintf(stdout, "name too long %s", fn);
				return 1;
			    } else {
				strcpy (buf2, fn);  /* ok; take directory part */
				buf2[tmp-fn+1] = '\0';
				strcat (buf2, buf); /* ok; plus (relative) symlink */
				tmp = buf2;
			    }
			    if (stat(tmp, &tstatbuf) < 0) {
				ckfprintf(stdout,
					"broken symbolic link to %s", buf);
				return 1;
			    }
                        }

			/* Otherwise, handle it. */
			if (lflag) {
				process(buf, strlen(buf));
				return 1;
			} else { /* just print what it points to */
				ckfputs("symbolic link to ", stdout);
				ckfputs(buf, stdout);
			}
		}
		return 1;
#endif
#ifdef	S_IFSOCK
#ifndef __COHERENT__
	case S_IFSOCK:
		ckfputs("socket", stdout);
		return 1;
#endif
#endif
	case S_IFREG:
		break;
	default:
		errx(1, "invalid mode 0%o", sb->st_mode);
		/*NOTREACHED*/
	}

	/*
	 * regular file, check next possibility
	 */
	if (sb->st_size == 0) {
		ckfputs("empty", stdout);
		return 1;
	}
	return 0;
}

