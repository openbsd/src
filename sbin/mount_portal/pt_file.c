/*	$OpenBSD: pt_file.c,v 1.6 1998/08/07 01:31:46 csapuntz Exp $	*/
/*	$NetBSD: pt_file.c,v 1.7 1995/06/06 19:54:30 mycroft Exp $	*/

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 * All rights reserved.
 *
 * This code is derived from software donated to Berkeley by
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *
 *	from: Id: pt_file.c,v 1.1 1992/05/25 21:43:09 jsp Exp
 *	@(#)pt_file.c	8.3 (Berkeley) 7/3/94
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/syslog.h>

#include "portald.h"

int
portal_file(pcr, key, v, so, fdp)
	struct portal_cred *pcr;
	char *key;
	char **v;
	int so;
	int *fdp;
{
	int fd;
	char pbuf[MAXPATHLEN];
	int error;

	pbuf[0] = '/';
	(void)strncpy(pbuf+1, key + (v[1] ? strlen(v[1]) : 0), sizeof pbuf-2);
	pbuf[sizeof pbuf-1] = '\0';

#ifdef DEBUG
	(void)printf("path = %s, uid = %d, gid = %d\n", pbuf, pcr->pcr_uid,
	    pcr->pcr_gid);
#endif

	if (setegid(pcr->pcr_gid) < 0 ||
	    setgroups(pcr->pcr_ngroups, pcr->pcr_groups) < 0)
		return (errno);

	if (seteuid(pcr->pcr_uid) < 0)
		return (errno);


	error = 0;

	fd = open(pbuf, O_RDWR|O_CREAT, 0666);
	if (fd < 0) {
	        if (errno == EISDIR) {
			errno = 0;
			fd = open(pbuf, O_RDONLY);
		}
		if (fd < 0)
			error = errno;
	}

	if (seteuid((uid_t) 0) < 0) {	/* XXX - should reset gidset too */
		error = errno;
		syslog(LOG_ERR, "setcred: %m");
		if (fd >= 0) {
			(void)close(fd);
			fd = -1;
		}
	}

	if (error == 0)
		*fdp = fd;

#ifdef DEBUG
	(void)fprintf(stderr, "pt_file returns *fdp = %d, error = %d\n",
	    *fdp, error);
#endif

	return (error);
}
