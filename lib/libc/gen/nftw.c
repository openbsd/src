/*	$OpenBSD: nftw.c,v 1.1 2003/07/21 20:17:53 millert Exp $	*/

/*
 * Copyright (c) 2003 Todd C. Miller <Todd.Miller@courtesan.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static const char rcsid[] = "$OpenBSD: nftw.c,v 1.1 2003/07/21 20:17:53 millert Exp $";
#endif /* LIBC_SCCS and not lint */

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fts.h>
#include <ftw.h>
#include <limits.h>

int
nftw(const char *path, int (*fn)(const char *, const struct stat *, int,
     struct FTW *), int nfds, int ftwflags)
{
	const char *paths[2];
	struct FTW ftw;
	FTSENT *cur;
	FTS *ftsp;
	int ftsflags, fnflag, error, postorder, sverrno;

	/* XXX - nfds is currently unused */
	if (nfds < 1 || nfds > OPEN_MAX) {
		errno = EINVAL;
		return (-1);
	}

	ftsflags = FTS_COMFOLLOW;
	if (!(ftwflags & FTW_CHDIR))
		ftsflags |= FTS_NOCHDIR;
	if (ftwflags & FTW_MOUNT)
		ftsflags |= FTS_XDEV;
	if (ftwflags & FTW_PHYS)
		ftsflags |= FTS_PHYSICAL;
	postorder = (ftwflags & FTW_DEPTH) != 0;
	paths[0] = path;
	paths[1] = NULL;
	ftsp = fts_open((char * const *)paths, ftsflags, NULL);
	if (ftsp == NULL)
		return (-1);
	error = 0;
	while ((cur = fts_read(ftsp)) != NULL) {
		switch (cur->fts_info) {
		case FTS_D:
			if (postorder)
				continue;
			fnflag = FTW_D;
			break;
		case FTS_DNR:
			fnflag = FTW_DNR;
			break;
		case FTS_DP:
			if (!postorder)
				continue;
			fnflag = FTW_DP;
			break;
		case FTS_F:
		case FTS_DEFAULT:
			fnflag = FTW_F;
			break;
		case FTS_NS:
		case FTS_NSOK:
			fnflag = FTW_NS;
			break;
		case FTS_SL:
			fnflag = FTW_SL;
			break;
		case FTS_SLNONE:
			fnflag = FTW_SLN;
			break;
		case FTS_DC:
			errno = ELOOP;
			/* FALLTHROUGH */
		default:
			error = -1;
			goto done;
		}
		ftw.base = cur->fts_pathlen - cur->fts_namelen;
		ftw.level = cur->fts_level;
		error = fn(cur->fts_path, cur->fts_statp, fnflag, &ftw);
		if (error != 0)
			break;
	}
done:
	sverrno = errno;
	(void) fts_close(ftsp);
	errno = sverrno;
	return (error);
}
