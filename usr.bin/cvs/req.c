/*	$OpenBSD: req.c,v 1.10 2005/01/13 05:39:07 jfb Exp $	*/
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


static int  cvs_req_set        (int, char *);
static int  cvs_req_root       (int, char *);
static int  cvs_req_validreq   (int, char *);
static int  cvs_req_validresp  (int, char *);
static int  cvs_req_directory  (int, char *);
static int  cvs_req_case       (int, char *);
static int  cvs_req_argument   (int, char *);
static int  cvs_req_globalopt  (int, char *);
static int  cvs_req_gzipstream (int, char *);
static int  cvs_req_version    (int, char *);


struct cvs_reqhdlr {
	int (*hdlr)(int, char *);
} cvs_req_swtab[CVS_REQ_MAX + 1] = {
	{ NULL               },
	{ cvs_req_root       },
	{ cvs_req_validreq   },
	{ cvs_req_validresp  },
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
	{ cvs_req_case       },
	{ NULL               },
	{ cvs_req_argument   },	/* 20 */
	{ cvs_req_argument   },
	{ cvs_req_globalopt  },
	{ cvs_req_gzipstream },
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
	{ cvs_req_set        },
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
 * Argument array built by `Argument' and `Argumentx' requests.
 */

static char *cvs_req_args[CVS_PROTO_MAXARG];
static int   cvs_req_nargs = 0;


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
		cvs_log(LP_ERR, "handler for `%s' not implemented", cmd);
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
cvs_req_set(int reqid, char *line)
{
	char *cp;

	cp = strchr(line, '=');
	if (cp == NULL) {
		cvs_log(LP_ERR, "error in Set request "
		    "(no = in variable assignment)");
		return (-1);
	}

	if (cvs_var_set(line, cp) < 0)
		return (-1);
	return (0);
}


static int
cvs_req_validreq(int reqid, char *line)
{
	char *vreq;

	vreq = cvs_req_getvalid();
	if (vreq == NULL)
		return (-1);

	cvs_sendresp(CVS_RESP_VALIDREQ, vreq);

	return (0);
}

static int
cvs_req_validresp(int reqid, char *line)
{
	char *sp, *ep;
	struct cvs_resp *resp;

	sp = line;
	do {
		ep = strchr(sp, ' ');
		if (ep != NULL)
			*(ep++) = '\0';

		resp = cvs_resp_getbyname(sp);
		if (resp != NULL)
			;

		if (ep != NULL)
			sp = ep + 1;
	} while (ep != NULL);

	return (0);
}

static int
cvs_req_directory(int reqid, char *line)
{

	return (0);
}

/*
 * cvs_req_case()
 *
 * Handler for the `Case' requests, which toggles case sensitivity ON or OFF
 */
static int
cvs_req_case(int reqid, char *line)
{
	cvs_nocase = 1;
	return (0);
}


static int
cvs_req_argument(int reqid, char *line)
{
	char *nap;

	if (cvs_req_nargs == CVS_PROTO_MAXARG) {
		cvs_log(LP_ERR, "too many arguments");
		return (-1);
	}

	if (reqid == CVS_REQ_ARGUMENT) {
		cvs_req_args[cvs_req_nargs] = strdup(line);
		if (cvs_req_args[cvs_req_nargs] == NULL) {
			cvs_log(LP_ERRNO, "failed to copy argument");
			return (-1);
		}
		cvs_req_nargs++;
	} else if (reqid == CVS_REQ_ARGUMENTX) {
		if (cvs_req_nargs == 0)
			cvs_log(LP_WARN, "no argument to append to");
		else {
			asprintf(&nap, "%s%s", cvs_req_args[cvs_req_nargs - 1],
			    line);
			if (nap == NULL) {
				cvs_log(LP_ERRNO,
				    "failed to append to argument");
				return (-1);
			}
			free(cvs_req_args[cvs_req_nargs - 1]);
			cvs_req_args[cvs_req_nargs - 1] = nap;
		}
	}

	return (0);
}


static int
cvs_req_globalopt(int reqid, char *line)
{
	if ((*line != '-') || (*(line + 2) != '\0')) {
		cvs_log(LP_ERR,
		    "invalid `Global_option' request format");
		return (-1);
	}

	switch (*(line + 1)) {
	case 'l':
		cvs_nolog = 1;
		break;
	case 'n':
		break;
	case 'Q':
		verbosity = 0;
		break;
	case 'q':
		if (verbosity > 1)
			verbosity = 1;
		break;
	case 'r':
		cvs_readonly = 1;
		break;
	case 't':
		cvs_trace = 1;
		break;
	default:
		cvs_log(LP_ERR, "unknown global option `%s'", line);
		return (-1);
	}

	return (0);
}


/*
 * cvs_req_gzipstream()
 *
 * Handler for the `Gzip-stream' request, which enables compression at the
 * level given along with the request.  After this request has been processed,
 * all further connection data should be compressed.
 */

static int
cvs_req_gzipstream(int reqid, char *line)
{
	char *ep;
	long val;

	val = strtol(line, &ep, 10);
	if ((line[0] == '\0') || (*ep != '\0')) {
		cvs_log(LP_ERR, "invalid Gzip-stream level `%s'", line);
		return (-1);
	} else if ((errno == ERANGE) && ((val < 0) || (val > 9))) {
		cvs_log(LP_ERR, "Gzip-stream level %ld out of range", val);
		return (-1);
	}

	cvs_compress = (int)val;

	return (0);
}


static int
cvs_req_version(int reqid, char *line)
{
	cvs_printf("%s\n", CVS_VERSION);
	return (0);
}
