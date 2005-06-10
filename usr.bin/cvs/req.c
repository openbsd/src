/*	$OpenBSD: req.c,v 1.21 2005/06/10 21:14:47 joris Exp $	*/
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

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "buf.h"
#include "cvs.h"
#include "log.h"
#include "proto.h"


extern int   verbosity;
extern int   cvs_compress;
extern char *cvs_rsh;
extern int   cvs_trace;
extern int   cvs_nolog;
extern int   cvs_readonly;


static int  cvs_req_set          (int, char *);
static int  cvs_req_noop         (int, char *);
static int  cvs_req_root         (int, char *);
static int  cvs_req_validreq     (int, char *);
static int  cvs_req_validresp    (int, char *);
static int  cvs_req_expandmod    (int, char *);
static int  cvs_req_directory    (int, char *);
static int  cvs_req_useunchanged (int, char *);
static int  cvs_req_case         (int, char *);
static int  cvs_req_argument     (int, char *);
static int  cvs_req_globalopt    (int, char *);
static int  cvs_req_gzipstream   (int, char *);
static int  cvs_req_entry        (int, char *);
static int  cvs_req_filestate    (int, char *);

static int  cvs_req_command      (int, char *);


struct cvs_reqhdlr {
	int (*hdlr)(int, char *);
} cvs_req_swtab[CVS_REQ_MAX + 1] = {
	{ NULL                  },
	{ cvs_req_root          },
	{ cvs_req_validreq      },
	{ cvs_req_validresp     },
	{ cvs_req_directory     },
	{ NULL                  },
	{ NULL                  },
	{ NULL                  },
	{ cvs_req_entry         },
	{ NULL                  },
	{ NULL                  },	/* 10 */
	{ cvs_req_filestate     },
	{ cvs_req_filestate     },
	{ cvs_req_filestate     },
	{ cvs_req_useunchanged  },
	{ NULL                  },
	{ NULL                  },
	{ cvs_req_filestate     },
	{ cvs_req_case          },
	{ NULL                  },
	{ cvs_req_argument      },	/* 20 */
	{ cvs_req_argument      },
	{ cvs_req_globalopt     },
	{ cvs_req_gzipstream    },
	{ NULL                  },
	{ NULL                  },
	{ NULL                  },
	{ NULL                  },
	{ NULL                  },
	{ NULL                  },
	{ NULL                  },	/* 30 */
	{ NULL                  },
	{ NULL                  },
	{ NULL                  },
	{ cvs_req_set           },
	{ cvs_req_expandmod     },
	{ cvs_req_command       },
	{ NULL                  },
	{ NULL                  },
	{ NULL                  },
	{ NULL                  },	/* 40 */
	{ NULL                  },
	{ NULL                  },
	{ NULL                  },
	{ NULL                  },
	{ cvs_req_command       },
	{ cvs_req_command       },
	{ NULL                  },
	{ cvs_req_command       },
	{ NULL                  },
	{ NULL                  },	/* 50 */
	{ NULL                  },
	{ NULL                  },
	{ NULL                  },
	{ cvs_req_command       },
	{ cvs_req_command       },
	{ cvs_req_command       },
	{ NULL                  },
	{ NULL                  },
	{ NULL                  },
	{ cvs_req_command       },	/* 60 */
	{ NULL                  },
	{ cvs_req_command       },
	{ cvs_req_command       },
	{ cvs_req_noop          },
	{ NULL                  },
	{ NULL                  },
	{ NULL                  },
	{ NULL                  },
	{ cvs_req_command       },
};



/*
 * Argument array built by `Argument' and `Argumentx' requests.
 */

static char *cvs_req_rootpath;
static char *cvs_req_currentdir;
static char *cvs_req_repopath;
static char cvs_req_tmppath[MAXPATHLEN];

extern char cvs_server_tmpdir[MAXPATHLEN];
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

/*
 * cvs_req_noop()
 */
static int
cvs_req_noop(int reqid, char *line)
{
	int ret;

	ret = cvs_sendresp(CVS_RESP_OK, NULL);
	if (ret < 0)
		return (-1);
	return (0);
}


static int
cvs_req_root(int reqid, char *line)
{
	if (cvs_req_rootpath != NULL) {
		cvs_log(LP_ERR, "duplicate Root request received");
		cvs_printf("Protocol error: Duplicate Root request");
		return (-1);
	}

	cvs_req_rootpath = strdup(line);
	if (cvs_req_rootpath == NULL) {
		cvs_log(LP_ERRNO, "failed to copy Root path");
		return (-1);
	}

	return (0);
}


static int
cvs_req_validreq(int reqid, char *line)
{
	char *vreq;

	vreq = cvs_req_getvalid();
	if (vreq == NULL)
		return (-1);

	if ((cvs_sendresp(CVS_RESP_VALIDREQ, vreq) < 0) ||
	    (cvs_sendresp(CVS_RESP_OK, NULL) < 0))
		return (-1);

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
	int l;
	char rdir[MAXPATHLEN];

	if (cvs_getln(NULL, rdir, sizeof(rdir)) < 0)
		return (-1);

	if (cvs_req_currentdir != NULL)
		free(cvs_req_currentdir);

	cvs_req_currentdir = strdup(rdir);
	if (cvs_req_currentdir == NULL) {
		cvs_log(LP_ERR, "failed to duplicate directory");
		return (-1);
	}

	/* now obtain the path relative to the Root directory */
	cvs_req_repopath = cvs_req_currentdir + strlen(cvs_req_rootpath) + 1;

	/* create tmp path */
	l = snprintf(cvs_req_tmppath, sizeof(cvs_req_tmppath), "%s/%s",
	    cvs_server_tmpdir, cvs_req_repopath);
	if (l == -1 || l >= (int)sizeof(cvs_req_tmppath)) {
		errno = ENAMETOOLONG;
		cvs_log(LP_ERRNO, "%s", cvs_req_tmppath);
		return (-1);
	}

	if ((mkdir(cvs_req_tmppath, 0755) == -1) && (errno != EEXIST)) {
		cvs_log(LP_ERRNO, "failed to create temporary directory '%s'",
		    cvs_req_tmppath);
		return (-1);
	}

	/* create the CVS/ administrative files */
	/* XXX - TODO */

	return (0);
}

static int
cvs_req_entry(int reqid, char *line)
{
	struct cvs_ent *ent;
	CVSFILE *cf;

	if ((ent = cvs_ent_parse(line)) == NULL)
		return (-1);

	cf = cvs_file_create(NULL, ent->ce_name, DT_REG, 0644);

	return (0);
}

/*
 * cvs_req_filestate()
 *
 * Handler for the `Modified', `Is-Modified', `Unchanged' and `Questionable'
 * requests, which are all used to report the assumed state of a file from the
 * client.
 */
static int
cvs_req_filestate(int reqid, char *line)
{
	int l;
	mode_t fmode;
	BUF *fdata;
	char fpath[MAXPATHLEN];

	if (reqid == CVS_REQ_MODIFIED) {
		fdata = cvs_recvfile(NULL, &fmode);
		if (fdata == NULL)
			return (-1);

		/* create full temporary path */
		l = snprintf(fpath, sizeof(fpath), "%s/%s", cvs_req_tmppath,
		    line);
		if (l == -1 || l >= (int)sizeof(fpath)) {
			errno = ENAMETOOLONG;
			cvs_log(LP_ERRNO, "%s", fpath);
			cvs_buf_free(fdata);
			return (-1);
		}

		/* write the file */
		if (cvs_buf_write(fdata, fpath, fmode) < 0) {
			cvs_log(LP_ERR, "failed to create file %s", fpath);
			cvs_buf_free(fdata);
			return (-1);
		}

		cvs_buf_free(fdata);
	}

	return (0);
}

/*
 * cvs_req_expandmod()
 *
 */
static int
cvs_req_expandmod(int reqid, char *line)
{
	int ret;

	ret = cvs_sendresp(CVS_RESP_OK, NULL);
	if (ret < 0)
		return (-1);
	return (0);
}


/*
 * cvs_req_useunchanged()
 *
 * Handler for the `UseUnchanged' requests.  The protocol documentation
 * specifies that this request must be supported by the server and must be
 * sent by the client, though it gives no clue regarding its use.
 */
static int
cvs_req_useunchanged(int reqid, char *line)
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
cvs_req_set(int reqid, char *line)
{
	char *cp, *lp;

	if ((lp = strdup(line)) == NULL) {
		cvs_log(LP_ERRNO, "failed to copy Set argument");
		return (-1);
	}

	if ((cp = strchr(lp, '=')) == NULL) {
		cvs_log(LP_ERR, "error in Set request "
		    "(no = in variable assignment)");
		free(lp);
		return (-1);
	}
	*(cp++) = '\0';

	if (cvs_var_set(lp, cp) < 0) {
		free(lp);
		return (-1);
	}

	free(lp);

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


/*
 * cvs_req_command()
 *
 * Generic request handler for CVS command requests (i.e. diff, update, tag).
 */
static int
cvs_req_command(int reqid, char *line)
{
	int ret = 0;
	struct cvs_cmd *cmdp;

	cmdp = cvs_findcmdbyreq(reqid);
	if (cmdp == NULL) {
		cvs_sendresp(CVS_RESP_ERROR, NULL);
		return (-1);
	}

	ret = cvs_startcmd(cmdp, cvs_req_nargs, cvs_req_args);
	if (ret == 0)
		ret = cvs_sendresp(CVS_RESP_OK, NULL);

	return (ret);
}
