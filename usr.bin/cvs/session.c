/*	$OpenBSD: session.c,v 1.1 2004/11/09 20:50:27 krapht Exp $	*/
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
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/uio.h>

#include <err.h>
#include <pwd.h>
#include <grp.h>
#include <poll.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <sysexits.h>

#include "log.h"
#include "sock.h"
#include "cvs.h"
#include "cvsd.h"


/*
 * cvsd_sess_alloc()
 *
 * Allocate a new session.
 */

struct cvsd_sess*
cvsd_sess_alloc(int fd)
{
	gid_t gid;
	struct cvsd_sess *sp;

	sp = (struct cvsd_sess *)malloc(sizeof(*sp));
	if (sp == NULL) {
		cvs_log(LP_ERRNO, "failed to allocate session");
		return (NULL);
	}

	sp->cs_fd = fd;
	/* only local sessions are currently supported */
	sp->cs_type = CVSD_SESS_LOCAL;

	if (sp->cs_type == CVSD_SESS_LOCAL) {
		if (getpeereid(fd, &(sp->cs_uid), &gid) == -1) {
			cvs_log(LP_ERRNO, "failed to get remote effective ID");
			free(sp);
			return (NULL);
		}
	}

	cvs_log(LP_INFO, "session opened for user %u", sp->cs_uid);

	return (sp);
}


/*
 * cvsd_sess_free()
 *
 */

void
cvsd_sess_free(struct cvsd_sess *sessp)
{
	if (sessp != NULL) {
		free(sessp);
	}
}
