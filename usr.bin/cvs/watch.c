/*	$OpenBSD: watch.c,v 1.3 2005/06/15 09:17:14 xsa Exp $	*/
/*
 * Copyright (c) 2005 Xavier Santolaria <xsa@openbsd.org>
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

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cvs.h"
#include "log.h"
#include "proto.h"

static int cvs_watch_init   (struct cvs_cmd *, int, char **, int *);
static int cvs_watch_remote (CVSFILE *, void*);
static int cvs_watch_local  (CVSFILE *, void*);

static int cvs_watchers_remote (CVSFILE *, void*);
static int cvs_watchers_local  (CVSFILE *, void*);


struct cvs_cmd cvs_cmd_watch = {
	CVS_OP_WATCH, CVS_REQ_NOOP, "watch",
	{},
	"Set watches",
	"on | off | add | remove [-lR] [-a action] [file ...]",
	"a:lR",
	NULL,
	CF_SORT | CF_RECURSE,
	cvs_watch_init,
	NULL,
	cvs_watch_remote,
	cvs_watch_local,
	NULL,
	NULL,
	0
};

struct cvs_cmd cvs_cmd_watchers = {
        CVS_OP_WATCHERS, CVS_REQ_WATCHERS, "watchers",
        {},
        "See who is watching a file",
        "[-lR] [file ...]",
        "lR",
        NULL,
        CF_SORT | CF_RECURSE,
        cvs_watch_init,
        NULL,
        cvs_watchers_remote,
        cvs_watchers_local,
        NULL,
        NULL,
        0
};



static int
cvs_watch_init(struct cvs_cmd *cmd, int argc, char **argv, int *arg)
{
	int ch;
	
	while ((ch = getopt(argc, argv, cmd->cmd_opts)) != -1) {
		switch (ch) {
		case 'a':
			/*
			 * The `watchers' command does not have the
			 * -a option. Check which command has been issued.
			 */
			if (cvs_cmdop != CVS_OP_WATCH)
				return (CVS_EX_USAGE);
			break;
		case 'l':
			cmd->file_flags &= ~CF_RECURSE;
			break;
		case 'R':
			cmd->file_flags |= CF_RECURSE;
			break;
		default:
			return (CVS_EX_USAGE);
		}
	}

	*arg = optind;
	return (CVS_EX_OK);
}


/*
 * cvs_watch_remote()
 *
 */
static int
cvs_watch_remote(CVSFILE *file, void *arg)
{
	return (CVS_EX_OK);
}


/*
 * cvs_watch_local()
 *
 */
static int
cvs_watch_local(CVSFILE *file, void *arg)
{
	return (CVS_EX_OK);
}


/*
 * cvs_watchers_remote()
 *
 */
static int
cvs_watchers_remote(CVSFILE *file, void *arg)
{
	return (CVS_EX_OK);
}
 
 
/*
 * cvs_watchers_local()
 *
 */
static int
cvs_watchers_local(CVSFILE *file, void *arg)
{
	return (CVS_EX_OK);
}
