/*	$OpenBSD: req.c,v 1.38 2005/12/24 19:07:52 xsa Exp $	*/
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
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "buf.h"
#include "cvs.h"
#include "log.h"
#include "proto.h"


extern char *cvs_rootstr;
extern int   cvs_compress;
extern char *cvs_rsh;
extern int   cvs_trace;
extern int   cvs_nolog;
extern int   cvs_readonly;


static int	cvs_req_set(int, char *);
static int	cvs_req_noop(int, char *);
static int	cvs_req_root(int, char *);
static int	cvs_req_validreq(int, char *);
static int	cvs_req_validresp(int, char *);
static int	cvs_req_expandmod(int, char *);
static int	cvs_req_directory(int, char *);
static int	cvs_req_useunchanged(int, char *);
static int	cvs_req_case(int, char *);
static int	cvs_req_argument(int, char *);
static int	cvs_req_globalopt(int, char *);
static int	cvs_req_gzipstream(int, char *);
static int	cvs_req_entry(int, char *);
static int	cvs_req_filestate(int, char *);

static int	cvs_req_command(int, char *);


struct cvs_reqhdlr {
	int	(*hdlr)(int, char *);
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
	{ cvs_req_command       },
	{ cvs_req_command       },	/* 50 */
	{ cvs_req_command       },
	{ cvs_req_command       },
	{ cvs_req_command       },
	{ cvs_req_command       },
	{ cvs_req_command       },
	{ cvs_req_command       },
	{ cvs_req_command       },
	{ cvs_req_command       },
	{ cvs_req_command       },
	{ cvs_req_command       },	/* 60 */
	{ NULL                  },
	{ cvs_req_command       },
	{ cvs_req_command       },
	{ cvs_req_noop          },
	{ cvs_req_command       },
	{ cvs_req_command       },
	{ cvs_req_command       },
	{ cvs_req_command       },
	{ cvs_req_command       },
};



/*
 * Argument array built by `Argument' and `Argumentx' requests.
 */
static char *cvs_req_args[CVS_PROTO_MAXARG];

/* start at 1, because 0 will be the command name */
static int  cvs_req_nargs = 1;

static char *cvs_req_modulename;
static char *cvs_req_rootpath;
static char *cvs_req_currentdir;
extern char cvs_server_tmpdir[MAXPATHLEN];
static CVSENTRIES *cvs_req_entf;

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

	if ((req = cvs_req_getbyname(cmd)) == NULL)
		fatal("cvs_req_handle: cvs_req_getbyname failed");
	else if (cvs_req_swtab[req->req_id].hdlr == NULL)
		fatal("handler for `%s' not implemented", cmd);

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

	cvs_req_rootpath = xstrdup(line);
	cvs_rootstr = cvs_req_rootpath;

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
	int pwd;
	size_t dirlen;
	char rdir[MAXPATHLEN];
	char *repo, *s, *p;

	pwd = (!strcmp(line, "."));

	if (cvs_getln(NULL, rdir, sizeof(rdir)) < 0)
		return (-1);

	STRIP_SLASH(rdir);

	if (cvs_req_currentdir != NULL)
		xfree(cvs_req_currentdir);

	cvs_req_currentdir = xstrdup(rdir);

	dirlen = strlen(cvs_req_currentdir);

	/*
	 * Lets make sure we always start at the correct
	 * directory.
	 */
	cvs_chdir(cvs_server_tmpdir, 1);

	/*
	 * Set repository path.
	 */
	if (strlen(cvs_req_rootpath) < dirlen) {
		s = cvs_req_currentdir + strlen(cvs_req_rootpath) + 1;
		if (s >= (cvs_req_currentdir + dirlen))
			fatal("you're bad, go away");
	} else
		s = cvs_req_currentdir;

	repo = xstrdup(s);

	/*
	 * Skip back "foo/bar" part, so we can feed the repo
	 * as a startpoint for cvs_create_dir().
	 */
	if (!pwd) {
		s = repo + strlen(repo) - strlen(line) - 1;
		if (*s != '/')
			fatal("cvs_req_directory: malformed directory");

		*s = '\0';
	}

	/*
	 * Obtain the modulename, we only need to do this at
	 * the very first time we get a Directory request.
	 */
	if (cvs_req_modulename == NULL) {
		if ((p = strchr(repo, '/')) != NULL)
			*p = '\0';

		cvs_req_modulename = xstrdup(repo);

		if (p != NULL)
			*p = '/';

		/*
		 * Now, create the admin files in the top-level
		 * directory for the temp repo.
		 */
		if (cvs_mkadmin(cvs_server_tmpdir, cvs_rootstr, repo,
		    NULL, NULL, 0) < 0) {
			cvs_log(LP_ERR, "failed to create admin files");
			xfree(repo);
			return (-1);
		}
	}

	/*
	 * create the directory plus the administrative files.
	 */
	if (cvs_create_dir(line, 1, cvs_rootstr, repo) < 0) {
		xfree(repo);
		return (-1);
	}

	/*
	 * cvs_create_dir() has already put us in the correct directory
	 * so now open it's Entry file for incoming files.
	 */
	if (cvs_req_entf != NULL)
		cvs_ent_close(cvs_req_entf);
	cvs_req_entf = cvs_ent_open(".", O_RDWR);
	if (cvs_req_entf == NULL)
		fatal("failed to open Entry file for %s", line);

	xfree(repo);
	return (0);
}

static int
cvs_req_entry(int reqid, char *line)
{
	struct cvs_ent *ent;

	/* parse received entry */
	if ((ent = cvs_ent_parse(line)) == NULL)
		return (-1);

	/* add it to the entry file and done */
	if (cvs_ent_add(cvs_req_entf, ent) < 0)
		fatal("cvs_req_entry: cvs_ent_add: `%s'", ent->ce_name);

	/* XXX */
	cvs_ent_write(cvs_req_entf);

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
	int ret;
	mode_t fmode;
	BUF *fdata;
	struct cvs_ent *ent;

	ret = 0;
	switch (reqid) {
	case CVS_REQ_MODIFIED:
		fdata = cvs_recvfile(NULL, &fmode);
		if (fdata == NULL)
			return (-1);

		/* write the file */
		cvs_buf_write(fdata, line, fmode);
		cvs_buf_free(fdata);
		break;
	case CVS_REQ_ISMODIFIED:
		break;
	case CVS_REQ_UNCHANGED:
		ent = cvs_ent_get(cvs_req_entf, line);
		if (ent == NULL) {
			cvs_log(LP_ERR,
			    "received Unchanged request "
			    "for a non-existing file");
			ret = -1;
		} else {
			ent->ce_status = CVS_ENT_UPTODATE;
		}
		break;
	case CVS_REQ_QUESTIONABLE:
		cvs_printf("? %s\n", line);
		break;
	default:
		cvs_log(LP_ERR, "wrong request id type");
		ret = -1;
		break;
	}

	/* XXX */
	cvs_req_entf->cef_flags &= ~CVS_ENTF_SYNC;
	cvs_ent_write(cvs_req_entf);

	return (ret);
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

	lp = xstrdup(line);
	if ((cp = strchr(lp, '=')) == NULL)
		fatal("error in Set request (no = in variable assignment)");

	*(cp++) = '\0';

	if (cvs_var_set(lp, cp) < 0) {
		xfree(lp);
		return (-1);
	}

	xfree(lp);

	return (0);
}


static int
cvs_req_argument(int reqid, char *line)
{
	char *nap;

	if (cvs_req_nargs == CVS_PROTO_MAXARG)
		fatal("too many arguments");

	if (reqid == CVS_REQ_ARGUMENT) {
		cvs_req_args[cvs_req_nargs] = xstrdup(line);
		cvs_req_nargs++;
	} else if (reqid == CVS_REQ_ARGUMENTX) {
		if (cvs_req_nargs == 0)
			cvs_log(LP_WARN, "no argument to append to");
		else {
			asprintf(&nap, "%s%s", cvs_req_args[cvs_req_nargs - 1],
			    line);
			if (nap == NULL)
				fatal("cvs_req_argument: asprintf failed");

			xfree(cvs_req_args[cvs_req_nargs - 1]);
			cvs_req_args[cvs_req_nargs - 1] = nap;
		}
	}

	return (0);
}


static int
cvs_req_globalopt(int reqid, char *line)
{
	if ((*line != '-') || (*(line + 2) != '\0'))
		fatal("invalid `Global_option' request format");

	switch (*(line + 1)) {
	case 'l':
		cvs_nolog = 1;
		break;
	case 'n':
		cvs_noexec = 1;
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
	if ((line[0] == '\0') || (*ep != '\0'))
		fatal("invalid Gzip-stream level `%s'", line);
	else if ((errno == ERANGE) && ((val < 0) || (val > 9)))
		fatal("Gzip-stream level %ld out of range", val);

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

	/* close the Entry file if it's still open */
	if (cvs_req_entf != NULL)
		cvs_ent_close(cvs_req_entf);

	/* fill in the command name */
	cvs_req_args[0] = cmdp->cmd_name;

	/* switch to the correct directory */
	if (cmdp->cmd_op != CVS_OP_VERSION)
		cvs_chdir(cvs_server_tmpdir, 1);

	ret = cvs_startcmd(cmdp, cvs_req_nargs, cvs_req_args);

	if (ret == 0)
		ret = cvs_sendresp(CVS_RESP_OK, NULL);

	return (ret);
}
