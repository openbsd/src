/*	$OpenBSD: fuser.c,v 1.7 2018/09/16 02:43:11 millert Exp $	*/

/*
 * Copyright (c) 2009 Todd C. Miller <Todd.Miller@courtesan.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Copyright (c) 2002 Peter Werner <peterw@ifost.org.au>
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

#include <sys/queue.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/ucred.h>
#define _KERNEL /* for DTYPE_VNODE */
#include <sys/file.h>
#undef _KERNEL

#include <err.h>
#include <fcntl.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "fstat.h"

/*
 * Returns 1 if the file watched (fa) is equivalent
 * to a file held by a process (kf), else 0.
 */
static int
match(struct filearg *fa, struct kinfo_file *kf)
{
	if (fa->dev == kf->va_fsid) {
		if (cflg)
			return (1);
		if (fa->ino == kf->va_fileid)
			return (1);
	}
	return (0);
}

/*
 * Examine kinfo_file struct and record the details if they
 * match a watched file.
 */
void
fuser_check(struct kinfo_file *kf)
{
	struct filearg *fa;
	struct fuser *fu;

	if (kf->f_type != DTYPE_VNODE)
		return;

	SLIST_FOREACH(fa, &fileargs, next) {
		if (!match(fa, kf))
			continue;

		/*
		 * This assumes that kinfo_files2 returns all files
		 * associated with a process in a contiguous block.
		 */
		if (TAILQ_EMPTY(&fa->fusers) || kf->p_pid !=
		    (fu = TAILQ_LAST(&fa->fusers, fuserhead))->pid) {
			fu = malloc(sizeof(*fu));
			if (fu == NULL)
				err(1, NULL);
			fu->pid = kf->p_pid;
			fu->uid = kf->p_uid;
			fu->flags = 0;
			TAILQ_INSERT_TAIL(&fa->fusers, fu, tq);
		}
		switch (kf->fd_fd) {
		case KERN_FILE_CDIR:
			fu->flags |= F_CWD;
			break;
		case KERN_FILE_RDIR:
			fu->flags |= F_ROOT;
			break;
		case KERN_FILE_TEXT:
			fu->flags |= F_TEXT;
			break;
		case KERN_FILE_TRACE:
			/* ignore */
			break;
		default:
			fu->flags |= F_OPEN;
			break;
		}
	}
}

/*
 * Print out the specfics for a given file/filesystem
 */
static void
printfu(struct fuser *fu)
{
	const char *name;

	printf("%d", fu->pid);
	fflush(stdout);

	if (fu->flags & F_CWD)
		fprintf(stderr, "c");

	if (fu->flags & F_ROOT)
		fprintf(stderr, "r");

	if (fu->flags & F_TEXT)
		fprintf(stderr, "t");

	if (uflg) {
		name = user_from_uid(fu->uid, 1);
		if (name != NULL)
			fprintf(stderr, "(%s)", name);
		else
			fprintf(stderr, "(%u)", fu->uid);
	}

	putchar(' ');
}

/*
 * For each file, print matching process info and optionally send a signal.
 */
void
fuser_run(void)
{
	struct filearg *fa;
	struct fuser *fu;
	pid_t mypid = getpid();

	SLIST_FOREACH(fa, &fileargs, next) {
		fprintf(stderr, "%s: ", fa->name);
		TAILQ_FOREACH(fu, &fa->fusers, tq) {
			printfu(fu);
			if (sflg && fu->pid != mypid) {
				kill(fu->pid, signo);
			}
		}
		fflush(stdout);
		fprintf(stderr, "\n");
	}
}
