/*	$OpenBSD: req.c,v 1.1 2004/08/03 04:58:45 jfb Exp $	*/
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
#include <sys/stat.h>

#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sysexits.h>
#ifdef CVS_ZLIB
#include <zlib.h>
#endif

#include "buf.h"
#include "cvs.h"
#include "log.h"
#include "file.h"
#include "proto.h"


extern int   verbosity;
extern int   cvs_compress;
extern char *cvs_rsh;
extern int   cvs_trace;
extern int   cvs_nolog;
extern int   cvs_readonly;


static int  cvs_req_root       (int, char *);
static int  cvs_req_directory  (int, char *);
static int  cvs_req_version    (int, char *);


struct cvs_reqhdlr {
	int (*hdlr)(int, char *);
} cvs_req_swtab[CVS_REQ_MAX + 1] = {
	{ NULL               },
	{ cvs_req_root       },
	{ NULL               },
	{ NULL               },
	{ cvs_req_directory  },
	{ NULL               },
	{ NULL               },
	{ NULL               },
	{ NULL               },
	{ NULL               },
	{ NULL               },	/* 10 */
	{ NULL               },
	{ NULL               },
	{ NULL               },
	{ NULL               },
	{ NULL               },
	{ NULL               },
	{ NULL               },
	{ NULL               },
	{ NULL               },
	{ NULL               },	/* 20 */
	{ NULL               },
	{ NULL               },
	{ NULL               },
	{ NULL               },
	{ NULL               },
	{ NULL               },
	{ NULL               },
	{ NULL               },
	{ NULL               },
	{ NULL               },	/* 30 */
	{ NULL               },
	{ NULL               },
	{ NULL               },
	{ NULL               },
	{ NULL               },
	{ NULL               },
	{ NULL               },
	{ NULL               },
	{ NULL               },
	{ NULL               },	/* 40 */
	{ NULL               },
	{ NULL               },
	{ NULL               },
	{ NULL               },
	{ NULL               },
	{ NULL               },
	{ NULL               },
	{ NULL               },
	{ NULL               },
	{ NULL               },	/* 50 */
	{ NULL               },
	{ NULL               },
	{ NULL               },
	{ NULL               },
	{ NULL               },
	{ NULL               },
	{ NULL               },
	{ NULL               },
	{ NULL               },
	{ NULL               },	/* 60 */
	{ NULL               },
	{ NULL               },
	{ NULL               },
	{ NULL               },
	{ NULL               },
	{ NULL               },
	{ NULL               },
	{ NULL               },
	{ cvs_req_version    },
};



/*
 * cvs_req_handle()
 *
 * Generic request handler dispatcher.  The handler expects the first line
 * of the command as single argument.
 * Returns the return value of the command on success, or -1 on failure.
 */

int
cvs_req_handle(char *line)
{
	char *cp, *cmd;
	struct cvs_req *req;

	cmd = line;

	cp = strchr(cmd, ' ');
	if (cp != NULL)
		*(cp++) = '\0';

	req = cvs_req_getbyname(cmd);
	if (req == NULL)
		return (-1);
	else if (cvs_req_swtab[req->req_id].hdlr == NULL) {
		cvs_log(LP_ERRNO, "handler for `%s' not implemented", cmd);
		return (-1);
	}

	return (*cvs_req_swtab[req->req_id].hdlr)(req->req_id, cp);
}



static int
cvs_req_root(int reqid, char *line)
{



	return (0);
}

static int
cvs_req_directory(int reqid, char *line)
{



	return (0);
}


static int
cvs_req_version(int reqid, char *line)
{
	cvs_printf("%s\n", CVS_VERSION);
	return (0);
}
