/*	$OpenBSD: proto.c,v 1.13 2004/07/30 01:49:24 jfb Exp $	*/
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
/*
 * CVS client/server protocol
 * ==========================
 *
 * The following code implements the CVS client/server protocol, which is
 * documented at the following URL:
 *	http://www.loria.fr/~molli/cvs/doc/cvsclient_toc.html
 *
 * The protocol is split up into two parts; the first part is the client side
 * of things and is composed of all the response handlers, which are all named
 * with a prefix of "cvs_resp_".  The second part is the set of request
 * handlers used by the server.  These handlers process the request and
 * generate the appropriate response to send back.  The prefix for request
 * handlers is "cvs_req_".
 *
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
#include "proto.h"


#define CVS_MTSTK_MAXDEPTH   16

/* request flags */
#define CVS_REQF_RESP    0x01




extern int   verbosity;
extern int   cvs_compress;
extern char *cvs_rsh;
extern int   cvs_trace;
extern int   cvs_nolog;
extern int   cvs_readonly;



static int  cvs_resp_validreq  (struct cvsroot *, int, char *);
static int  cvs_resp_cksum     (struct cvsroot *, int, char *);
static int  cvs_resp_modtime   (struct cvsroot *, int, char *);
static int  cvs_resp_m         (struct cvsroot *, int, char *);
static int  cvs_resp_ok        (struct cvsroot *, int, char *);
static int  cvs_resp_error     (struct cvsroot *, int, char *);
static int  cvs_resp_statdir   (struct cvsroot *, int, char *);
static int  cvs_resp_sticky    (struct cvsroot *, int, char *);
static int  cvs_resp_newentry  (struct cvsroot *, int, char *);
static int  cvs_resp_updated   (struct cvsroot *, int, char *);
static int  cvs_resp_removed   (struct cvsroot *, int, char *);
static int  cvs_resp_mode      (struct cvsroot *, int, char *);
static int  cvs_resp_modxpand  (struct cvsroot *, int, char *);

static int  cvs_initlog   (void);

static const char *cvs_months[] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};



struct cvs_req {
	int      req_id;
	char     req_str[32];
	u_int    req_flags;
	int     (*req_hdlr)(int, char *);
} cvs_requests[] = {
	{ CVS_REQ_DIRECTORY,     "Directory",         0,  NULL },
	{ CVS_REQ_MAXDOTDOT,     "Max-dotdot",        0,  NULL },
	{ CVS_REQ_STATICDIR,     "Static-directory",  0,  NULL },
	{ CVS_REQ_STICKY,        "Sticky",            0,  NULL },
	{ CVS_REQ_ENTRY,         "Entry",             0,  NULL },
	{ CVS_REQ_ENTRYEXTRA,    "EntryExtra",        0,  NULL },
	{ CVS_REQ_CHECKINTIME,   "Checkin-time",      0,  NULL },
	{ CVS_REQ_MODIFIED,      "Modified",          0,  NULL },
	{ CVS_REQ_ISMODIFIED,    "Is-modified",       0,  NULL },
	{ CVS_REQ_UNCHANGED,     "Unchanged",         0,  NULL },
	{ CVS_REQ_USEUNCHANGED,  "UseUnchanged",      0,  NULL },
	{ CVS_REQ_NOTIFY,        "Notify",            0,  NULL },
	{ CVS_REQ_NOTIFYUSER,    "NotifyUser",        0,  NULL },
	{ CVS_REQ_QUESTIONABLE,  "Questionable",      0,  NULL },
	{ CVS_REQ_CASE,          "Case",              0,  NULL },
	{ CVS_REQ_UTF8,          "Utf8",              0,  NULL },
	{ CVS_REQ_ARGUMENT,      "Argument",          0,  NULL },
	{ CVS_REQ_ARGUMENTX,     "Argumentx",         0,  NULL },
	{ CVS_REQ_GLOBALOPT,     "Global_option",     0,  NULL },
	{ CVS_REQ_GZIPSTREAM,    "Gzip-stream",       0,  NULL },
	{ CVS_REQ_READCVSRC2,    "read-cvsrc2",       0,  NULL },
	{ CVS_REQ_READWRAP,      "read-cvswrappers",  0,  NULL },
	{ CVS_REQ_READIGNORE,    "read-cvsignore",    0,  NULL },
	{ CVS_REQ_ERRIFREADER,   "Error-If-Reader",   0,  NULL },
	{ CVS_REQ_VALIDRCSOPT,   "Valid-RcsOptions",  0,  NULL },
	{ CVS_REQ_SET,           "Set",               0,  NULL },
	{ CVS_REQ_XPANDMOD,      "expand-modules",    CVS_REQF_RESP,  NULL },
	{ CVS_REQ_LOG,           "log",               0,  NULL },
	{ CVS_REQ_CO,            "co",                CVS_REQF_RESP,  NULL },
	{ CVS_REQ_EXPORT,        "export",            CVS_REQF_RESP,  NULL },
	{ CVS_REQ_RANNOTATE,     "rannotate",         0,  NULL },
	{ CVS_REQ_RDIFF,         "rdiff",             0,  NULL },
	{ CVS_REQ_RLOG,          "rlog",              0,  NULL },
	{ CVS_REQ_RTAG,          "rtag",              CVS_REQF_RESP,  NULL },
	{ CVS_REQ_INIT,          "init",              CVS_REQF_RESP,  NULL },
	{ CVS_REQ_STATUS,        "status",            CVS_REQF_RESP,  NULL },
	{ CVS_REQ_UPDATE,        "update",            CVS_REQF_RESP,  NULL },
	{ CVS_REQ_HISTORY,       "history",           0,  NULL },
	{ CVS_REQ_IMPORT,        "import",            CVS_REQF_RESP,  NULL },
	{ CVS_REQ_ADD,           "add",               CVS_REQF_RESP,  NULL },
	{ CVS_REQ_REMOVE,        "remove",            CVS_REQF_RESP,  NULL },
	{ CVS_REQ_RELEASE,       "release",           CVS_REQF_RESP,  NULL },
	{ CVS_REQ_ROOT,          "Root",              0,  NULL },
	{ CVS_REQ_VALIDRESP,     "Valid-responses",   0,  NULL },
	{ CVS_REQ_VALIDREQ,      "valid-requests",    CVS_REQF_RESP,  NULL },
	{ CVS_REQ_VERSION,       "version",           CVS_REQF_RESP,  NULL },
	{ CVS_REQ_NOOP,          "noop",              CVS_REQF_RESP,  NULL },
	{ CVS_REQ_DIFF,          "diff",              CVS_REQF_RESP,  NULL },
};


struct cvs_resp {
	u_int  resp_id;
	char   resp_str[32];
	int  (*resp_hdlr)(struct cvsroot *, int, char *);
} cvs_responses[] = {
	{ CVS_RESP_OK,         "ok",                     cvs_resp_ok       },
	{ CVS_RESP_ERROR,      "error",                  cvs_resp_error    },
	{ CVS_RESP_VALIDREQ,   "Valid-requests",         cvs_resp_validreq },
	{ CVS_RESP_M,          "M",                      cvs_resp_m        },
	{ CVS_RESP_MBINARY,    "Mbinary",                cvs_resp_m        },
	{ CVS_RESP_MT,         "MT",                     cvs_resp_m        },
	{ CVS_RESP_E,          "E",                      cvs_resp_m        },
	{ CVS_RESP_F,          "F",                      cvs_resp_m        },
	{ CVS_RESP_CREATED,    "Created",                cvs_resp_updated  },
	{ CVS_RESP_UPDATED,    "Updated",                cvs_resp_updated  },
	{ CVS_RESP_UPDEXIST,   "Update-existing",        cvs_resp_updated  },
	{ CVS_RESP_REMOVED,    "Removed",                cvs_resp_removed  },
	{ CVS_RESP_MERGED,     "Merged",                 NULL              },
	{ CVS_RESP_CKSUM,      "Checksum",               cvs_resp_cksum    },
	{ CVS_RESP_CLRSTATDIR, "Clear-static-directory", cvs_resp_statdir  },
	{ CVS_RESP_SETSTATDIR, "Set-static-directory",   cvs_resp_statdir  },
	{ CVS_RESP_NEWENTRY,   "New-entry",              cvs_resp_newentry },
	{ CVS_RESP_CHECKEDIN,  "Checked-in",             cvs_resp_newentry },
	{ CVS_RESP_MODE,       "Mode",                   cvs_resp_mode     },
	{ CVS_RESP_MODTIME,    "Mod-time",               cvs_resp_modtime  },
	{ CVS_RESP_MODXPAND,   "Module-expansion",       cvs_resp_modxpand },
	{ CVS_RESP_SETSTICKY,  "Set-sticky",             cvs_resp_sticky   },
	{ CVS_RESP_CLRSTICKY,  "Clear-sticky",           cvs_resp_sticky   },
};


static char *cvs_mt_stack[CVS_MTSTK_MAXDEPTH];
static u_int cvs_mtstk_depth = 0;

static time_t cvs_modtime = 0;


#define CVS_NBREQ   (sizeof(cvs_requests)/sizeof(cvs_requests[0]))
#define CVS_NBRESP  (sizeof(cvs_responses)/sizeof(cvs_responses[0]))

/* mask of requets supported by server */
static u_char  cvs_server_validreq[CVS_REQ_MAX + 1];

/*
 * Local and remote directory used by the `Directory' request.
 */
char  cvs_ldir[MAXPATHLEN];
char  cvs_rdir[MAXPATHLEN];

char *cvs_fcksum = NULL;

mode_t  cvs_lastmode = 0;

static char  cvs_proto_buf[4096];

/*
 * Output files for protocol logging when the CVS_CLIENT_LOG enviroment
 * variable is set.
 */
static int   cvs_server_logon = 0;
static FILE *cvs_server_inlog = NULL;
static FILE *cvs_server_outlog = NULL;


/*
 * cvs_connect()
 *
 * Open a client connection to the cvs server whose address is given in
 * the <root> variable.  The method used to connect depends on the
 * setting of the CVS_RSH variable.
 * Returns 0 on success, or -1 on failure.
 */

int
cvs_connect(struct cvsroot *root)
{
	int argc, infd[2], outfd[2];
	pid_t pid;
	char *argv[16], *cvs_server_cmd, *vresp;

	if (pipe(infd) == -1) {
		cvs_log(LP_ERRNO,
		    "failed to create input pipe for client connection");
		return (-1);
	}

	if (pipe(outfd) == -1) {
		cvs_log(LP_ERRNO,
		    "failed to create output pipe for client connection");
		(void)close(infd[0]);
		(void)close(infd[1]);
		return (-1);
	}

	pid = fork();
	if (pid == -1) {
		cvs_log(LP_ERRNO, "failed to fork for cvs server connection");
		return (-1);
	}
	if (pid == 0) {
		if ((dup2(infd[0], STDIN_FILENO) == -1) ||
		    (dup2(outfd[1], STDOUT_FILENO) == -1)) {
			cvs_log(LP_ERRNO,
			    "failed to setup standard streams for cvs server");
			return (-1);
		}
		(void)close(infd[1]);
		(void)close(outfd[0]);

		argc = 0;
		argv[argc++] = cvs_rsh;

		if (root->cr_user != NULL) {
			argv[argc++] = "-l";
			argv[argc++] = root->cr_user;
		}


		cvs_server_cmd = getenv("CVS_SERVER");
		if (cvs_server_cmd == NULL)
			cvs_server_cmd = "cvs";

		argv[argc++] = root->cr_host;
		argv[argc++] = cvs_server_cmd;
		argv[argc++] = "server";
		argv[argc] = NULL;

		execvp(argv[0], argv);
		cvs_log(LP_ERRNO, "failed to exec");
		exit(EX_OSERR);
	}

	/* we are the parent */
	(void)close(infd[0]);
	(void)close(outfd[1]);

	root->cr_srvin = fdopen(infd[1], "w");
	if (root->cr_srvin == NULL) {
		cvs_log(LP_ERRNO, "failed to create pipe stream");
		return (-1);
	}

	root->cr_srvout = fdopen(outfd[0], "r");
	if (root->cr_srvout == NULL) {
		cvs_log(LP_ERRNO, "failed to create pipe stream");
		return (-1);
	}

	/* make the streams line-buffered */
	(void)setvbuf(root->cr_srvin, NULL, _IOLBF, 0);
	(void)setvbuf(root->cr_srvout, NULL, _IOLBF, 0);

	cvs_initlog();

	/*
	 * Send the server the list of valid responses, then ask for valid
	 * requests.
	 */

	vresp = cvs_resp_getvalid();
	if (vresp == NULL) {
		cvs_log(LP_ERR, "can't generate list of valid responses");
		return (-1);
	}

	if (cvs_sendreq(root, CVS_REQ_VALIDRESP, vresp) < 0) {
	}
	free(vresp);

	if (cvs_sendreq(root, CVS_REQ_VALIDREQ, NULL) < 0) {
		cvs_log(LP_ERR, "failed to get valid requests from server");
		return (-1);
	}

	/* now share our global options with the server */
	if (verbosity == 1)
		cvs_sendreq(root, CVS_REQ_GLOBALOPT, "-q");
	else if (verbosity == 0)
		cvs_sendreq(root, CVS_REQ_GLOBALOPT, "-Q");

	if (cvs_nolog)
		cvs_sendreq(root, CVS_REQ_GLOBALOPT, "-l");
	if (cvs_readonly)
		cvs_sendreq(root, CVS_REQ_GLOBALOPT, "-r");
	if (cvs_trace)
		cvs_sendreq(root, CVS_REQ_GLOBALOPT, "-t");

	/* now send the CVSROOT to the server */
	if (cvs_sendreq(root, CVS_REQ_ROOT, root->cr_dir) < 0)
		return (-1);

	/* not sure why, but we have to send this */
	if (cvs_sendreq(root, CVS_REQ_USEUNCHANGED, NULL) < 0)
		return (-1);

#ifdef CVS_ZLIB
	/* if compression was requested, initialize it */
#endif

	cvs_log(LP_DEBUG, "connected to %s", root->cr_host);

	return (0);
}


/*
 * cvs_disconnect()
 *
 * Disconnect from the cvs server.
 */

void
cvs_disconnect(struct cvsroot *root)
{
	cvs_log(LP_DEBUG, "closing connection to %s", root->cr_host);
	if (root->cr_srvin != NULL) {
		(void)fclose(root->cr_srvin);
		root->cr_srvin = NULL;
	}
	if (root->cr_srvout != NULL) {
		(void)fclose(root->cr_srvout);
		root->cr_srvout = NULL;
	}
}


/*
 * cvs_req_getbyid()
 *
 */

struct cvs_req*
cvs_req_getbyid(int reqid)
{
	u_int i;

	for (i = 0; i < CVS_NBREQ; i++)
		if (cvs_requests[i].req_id == reqid)
			return &(cvs_requests[i]);
	return (NULL);
}


/*
 * cvs_req_getbyname()
 */

struct cvs_req*
cvs_req_getbyname(const char *rname)
{
	u_int i;

	for (i = 0; i < CVS_NBREQ; i++)
		if (strcmp(cvs_requests[i].req_str, rname) == 0)
			return &(cvs_requests[i]);

	return (NULL);
}


/*
 * cvs_req_getvalid()
 *
 * Build a space-separated list of all the requests that this protocol
 * implementation supports.
 */

char*
cvs_req_getvalid(void)
{
	u_int i;
	size_t len;
	char *vrstr;
	BUF *buf;

	buf = cvs_buf_alloc(512, BUF_AUTOEXT);
	if (buf == NULL)
		return (NULL);

	cvs_buf_set(buf, cvs_requests[0].req_str,
	    strlen(cvs_requests[0].req_str), 0);

	for (i = 1; i < CVS_NBREQ; i++) {
		if ((cvs_buf_putc(buf, ' ') < 0) ||
		    (cvs_buf_append(buf, cvs_requests[i].req_str,
		    strlen(cvs_requests[i].req_str)) < 0)) {
			cvs_buf_free(buf);
			return (NULL);
		}
	}

	/* NUL-terminate */
	if (cvs_buf_putc(buf, '\0') < 0) {
		cvs_buf_free(buf);
		return (NULL);
	}

	len = cvs_buf_size(buf);
	vrstr = (char *)malloc(len);

	cvs_buf_copy(buf, 0, vrstr, len);

	cvs_buf_free(buf);

	return (vrstr);
}


/*
 * cvs_req_handle()
 *
 * Generic request handler dispatcher.
 */

int
cvs_req_handle(char *line)
{
	return (0);
}


/*
 * cvs_resp_getbyid()
 *
 */

struct cvs_resp*
cvs_resp_getbyid(int respid)
{
	u_int i;

	for (i = 0; i < CVS_NBREQ; i++)
		if (cvs_responses[i].resp_id == (u_int)respid)
			return &(cvs_responses[i]);
	return (NULL);
}


/*
 * cvs_resp_getbyname()
 */

struct cvs_resp*
cvs_resp_getbyname(const char *rname)
{
	u_int i;

	for (i = 0; i < CVS_NBREQ; i++)
		if (strcmp(cvs_responses[i].resp_str, rname) == 0)
			return &(cvs_responses[i]);

	return (NULL);
}


/*
 * cvs_resp_getvalid()
 *
 * Build a space-separated list of all the responses that this protocol
 * implementation supports.
 */

char*
cvs_resp_getvalid(void)
{
	u_int i;
	size_t len;
	char *vrstr;
	BUF *buf;

	buf = cvs_buf_alloc(512, BUF_AUTOEXT);
	if (buf == NULL)
		return (NULL);

	cvs_buf_set(buf, cvs_responses[0].resp_str,
	    strlen(cvs_responses[0].resp_str), 0);

	for (i = 1; i < CVS_NBRESP; i++) {
		if ((cvs_buf_putc(buf, ' ') < 0) ||
		    (cvs_buf_append(buf, cvs_responses[i].resp_str,
		    strlen(cvs_responses[i].resp_str)) < 0)) {
			cvs_buf_free(buf);
			return (NULL);
		}
	}

	/* NUL-terminate */
	if (cvs_buf_putc(buf, '\0') < 0) {
		cvs_buf_free(buf);
		return (NULL);
	}

	len = cvs_buf_size(buf);
	vrstr = (char *)malloc(len);

	cvs_buf_copy(buf, 0, vrstr, len);
	cvs_buf_free(buf);

	return (vrstr);
}


/*
 * cvs_resp_handle()
 *
 * Generic response handler dispatcher.  The handler expects the first line
 * of the command as single argument.
 * Returns the return value of the command on success, or -1 on failure.
 */

int
cvs_resp_handle(struct cvsroot *root, char *line)
{
	u_int i;
	char *cp, *cmd;

	cmd = line;

	cp = strchr(cmd, ' ');
	if (cp != NULL)
		*(cp++) = '\0';

	for (i = 0; i < CVS_NBRESP; i++) {
		if (strcmp(cvs_responses[i].resp_str, cmd) == 0)
			return (*cvs_responses[i].resp_hdlr)
			    (root, cvs_responses[i].resp_id, cp);
	}

	/* unhandled */
	return (-1);
}


/*
 * cvs_resp_validreq()
 *
 * Handler for the `Valid-requests' response.  The list of valid requests is
 * split on spaces and each request's entry in the valid request array is set
 * to 1 to indicate the validity.
 * Returns 0 on success, or -1 on failure.
 */

static int
cvs_resp_validreq(struct cvsroot *root, int type, char *line)
{
	char *sp, *ep;
	struct cvs_req *req;

	/* parse the requests */
	sp = line;
	do {
		ep = strchr(sp, ' ');
		if (ep != NULL)
			*ep = '\0';

		req = cvs_req_getbyname(sp);
		if (req != NULL)
			cvs_server_validreq[req->req_id] = 1;

		if (ep != NULL)
			sp = ep + 1;
	} while (ep != NULL);

	return (0);
}


/*
 * cvs_resp_m()
 *
 * Handler for the `M', 'MT', `F' and `E' responses.
 */

static int
cvs_resp_m(struct cvsroot *root, int type, char *line)
{
	char *cp;
	FILE *stream;

	stream = NULL;

	switch (type) {
	case CVS_RESP_F:
		fflush(stderr);
		return (0);
	case CVS_RESP_M:
		stream = stdout;
		break;
	case CVS_RESP_E:
		stream = stderr;
		break;
	case CVS_RESP_MT:
		if (*line == '+') {
			if (cvs_mtstk_depth == CVS_MTSTK_MAXDEPTH) {
				cvs_log(LP_ERR,
				    "MT scope stack has reached max depth");
				return (-1);
			}
			cvs_mt_stack[cvs_mtstk_depth] = strdup(line + 1);
			if (cvs_mt_stack[cvs_mtstk_depth] == NULL)
				return (-1);
			cvs_mtstk_depth++;
		}
		else if (*line == '-') {
			if (cvs_mtstk_depth == 0) {
				cvs_log(LP_ERR, "MT scope stack underflow");
				return (-1);
			}
			else if (strcmp(line + 1,
			    cvs_mt_stack[cvs_mtstk_depth - 1]) != 0) {
				cvs_log(LP_ERR, "mismatch in MT scope stack");
				return (-1);
			}
			free(cvs_mt_stack[cvs_mtstk_depth--]);
		}
		else {
			if (strcmp(line, "newline") == 0)
				putc('\n', stdout);
			else if (strncmp(line, "fname ", 6) == 0)
				printf("%s", line + 6);
			else {
				/* assume text */
				cp = strchr(line, ' ');
				if (cp != NULL)
					printf("%s", cp + 1);
			}
		}

		return (0);
	case CVS_RESP_MBINARY:
		cvs_log(LP_WARN, "Mbinary not supported in client yet");
		break;
	}

	fputs(line, stream);
	fputc('\n', stream);

	return (0);
}


/*
 * cvs_resp_ok()
 *
 * Handler for the `ok' response.  This handler's job is to 
 */

static int
cvs_resp_ok(struct cvsroot *root, int type, char *line)
{
	return (1);
}


/*
 * cvs_resp_error()
 *
 * Handler for the `error' response.  This handler's job is to 
 */

static int
cvs_resp_error(struct cvsroot *root, int type, char *line)
{
	return (1);
}


/*
 * cvs_resp_statdir()
 *
 * Handler for the `Clear-static-directory' and `Set-static-directory'
 * responses.
 */

static int
cvs_resp_statdir(struct cvsroot *root, int type, char *line)
{
	int fd;
	char rpath[MAXPATHLEN], statpath[MAXPATHLEN];

	cvs_getln(root, rpath, sizeof(rpath));

	snprintf(statpath, sizeof(statpath), "%s/%s", line,
	    CVS_PATH_STATICENTRIES);

	if ((type == CVS_RESP_CLRSTATDIR) &&
	    (unlink(statpath) == -1) && (errno != ENOENT)) {
		cvs_log(LP_ERRNO, "failed to unlink %s file",
		    CVS_PATH_STATICENTRIES);
		return (-1);
	}
	else if (type == CVS_RESP_SETSTATDIR) {
		fd = open(statpath, O_CREAT|O_TRUNC|O_WRONLY, 0400);
		if (fd == -1) {
			cvs_log(LP_ERRNO, "failed to create %s file",
			    CVS_PATH_STATICENTRIES);
			return (-1);
		}
		(void)close(fd);

	}

	return (0);
}

/*
 * cvs_resp_sticky()
 *
 * Handler for the `Clear-sticky' and `Set-sticky' responses.
 */

static int
cvs_resp_sticky(struct cvsroot *root, int type, char *line)
{
	size_t len;
	char rpath[MAXPATHLEN];
	struct stat st;
	CVSFILE *cf;

	/* remove trailing slash */
	len = strlen(line);
	if ((len > 0) && (line[len - 1] == '/'))
		line[--len] = '\0';

	/* get the remote path */
	cvs_getln(root, rpath, sizeof(rpath));

	/* if the directory doesn't exist, create it */
	if (stat(line, &st) == -1) {
		/* attempt to create it */
		if (errno != ENOENT) {
			cvs_log(LP_ERRNO, "failed to stat %s", line);
		}
		else {
			cf = cvs_file_create(line, DT_DIR, 0755);
			if (cf == NULL)
				return (-1);
			cf->cf_ddat->cd_repo = strdup(line);
			cf->cf_ddat->cd_root = cvs_root;
			cvs_mkadmin(cf, 0755);

			cf->cf_ddat->cd_root = NULL;
			cvs_file_free(cf);
		}
	}

	if (type == CVS_RESP_CLRSTICKY) {
	}
	else if (type == CVS_RESP_SETSTICKY) {
	}

	return (0);
}


/*
 * cvs_resp_newentry()
 *
 * Handler for the `New-entry' response and `Checked-in' responses.
 */

static int
cvs_resp_newentry(struct cvsroot *root, int type, char *line)
{
	char entbuf[128], path[MAXPATHLEN];
	CVSENTRIES *entfile;

	snprintf(path, sizeof(path), "%s/" CVS_PATH_ENTRIES, line);

	/* get the remote path */
	cvs_getln(root, entbuf, sizeof(entbuf));

	/* get the new Entries line */
	if (cvs_getln(root, entbuf, sizeof(entbuf)) < 0)
		return (-1);

	entfile = cvs_ent_open(path, O_WRONLY);
	if (entfile == NULL)
		return (-1);
	cvs_ent_addln(entfile, entbuf);
	cvs_ent_close(entfile);

	return (0);
}


/*
 * cvs_resp_cksum()
 *
 * Handler for the `Checksum' response.  We store the checksum received for
 * the next file in a dynamically-allocated buffer pointed to by <cvs_fcksum>.
 * Upon next file reception, the handler checks to see if there is a stored
 * checksum.
 * The file handler must make sure that the checksums match and free the
 * checksum buffer once it's done to indicate there is no further checksum.
 */

static int
cvs_resp_cksum(struct cvsroot *root, int type, char *line)
{
	if (cvs_fcksum != NULL) {
		cvs_log(LP_WARN, "unused checksum");
		free(cvs_fcksum);
	}

	cvs_fcksum = strdup(line);
	if (cvs_fcksum == NULL) {
		cvs_log(LP_ERRNO, "failed to copy checksum string");
		return (-1);
	}

	return (0);
}


/*
 * cvs_resp_modtime()
 *
 * Handler for the `Mod-time' file update modifying response.  The timestamp
 * given is used to set the last modification time on the next file that
 * will be received.
 */

static int
cvs_resp_modtime(struct cvsroot *root, int type, char *line)
{
	int i;
	long off;
	char sign, mon[8], gmt[8], hr[4], min[4], *ep;
	struct tm cvs_tm;

	memset(&cvs_tm, 0, sizeof(cvs_tm));
	sscanf(line, "%d %3s %d %2d:%2d:%2d %5s", &cvs_tm.tm_mday, mon,
	    &cvs_tm.tm_year, &cvs_tm.tm_hour, &cvs_tm.tm_min,
	    &cvs_tm.tm_sec, gmt);
	cvs_tm.tm_year -= 1900;
	cvs_tm.tm_isdst = -1;

	if (*gmt == '-') {
		sscanf(gmt, "%c%2s%2s", &sign, hr, min);
		cvs_tm.tm_gmtoff = strtol(hr, &ep, 10);
		if ((cvs_tm.tm_gmtoff == LONG_MIN) ||
		    (cvs_tm.tm_gmtoff == LONG_MAX) ||
		    (*ep != '\0')) {
			cvs_log(LP_ERR,
			    "parse error in GMT hours specification `%s'", hr);
			cvs_tm.tm_gmtoff = 0;
		}
		else {
			/* get seconds */
			cvs_tm.tm_gmtoff *= 3600;

			/* add the minutes */
			off = strtol(min, &ep, 10);
			if ((cvs_tm.tm_gmtoff == LONG_MIN) ||
			    (cvs_tm.tm_gmtoff == LONG_MAX) ||
			    (*ep != '\0')) {
				cvs_log(LP_ERR,
				    "parse error in GMT minutes "
				    "specification `%s'", min);
			}
			else
				cvs_tm.tm_gmtoff += off * 60;
		}
	}
	if (sign == '-')
		cvs_tm.tm_gmtoff = -cvs_tm.tm_gmtoff;

	for (i = 0; i < (int)(sizeof(cvs_months)/sizeof(cvs_months[0])); i++) {
		if (strcmp(cvs_months[i], mon) == 0) {
			cvs_tm.tm_mon = i;
			break;
		}
	}

	cvs_modtime = mktime(&cvs_tm);
	return (0);
}


/*
 * cvs_resp_updated()
 *
 * Handler for the `Updated' and `Created' responses.
 */

static int
cvs_resp_updated(struct cvsroot *root, int type, char *line)
{
	size_t len;
	char tbuf[32], path[MAXPATHLEN], cksum_buf[CVS_CKSUM_LEN];
	CVSENTRIES *ef;
	struct cvs_ent *ep;

	ep = NULL;

	if (type == CVS_RESP_CREATED) {
		/* read the remote path of the file */
		cvs_getln(root, path, sizeof(path));

		/* read the new entry */
		cvs_getln(root, path, sizeof(path));
		ep = cvs_ent_parse(path);
		if (ep == NULL)
			return (-1);

		/* set the timestamp as the last one received from Mod-time */
		ep->ce_timestamp = ctime_r(&cvs_modtime, tbuf);
		len = strlen(tbuf);
		if ((len > 0) && (tbuf[len - 1] == '\n'))
			tbuf[--len] = '\0';

		ef = cvs_ent_open(line, O_WRONLY);
		if (ef == NULL)
			return (-1);

		cvs_ent_add(ef, ep);
		cvs_ent_close(ef);
	}
	else if (type == CVS_RESP_UPDEXIST) {
	}
	else if (type == CVS_RESP_UPDATED) {
	}

	snprintf(path, sizeof(path), "%s%s", line, ep->ce_name);
	if (cvs_recvfile(root, path) < 0) {
		return (-1);
	}

	/* now see if there is a checksum */
	if (cvs_fcksum != NULL) {
		if (cvs_cksum(line, cksum_buf, sizeof(cksum_buf)) < 0) {
		}

		if (strcmp(cksum_buf, cvs_fcksum) != 0) {
			cvs_log(LP_ERR, "checksum error on received file");
			(void)unlink(line);
		}

		free(cvs_fcksum);
		cvs_fcksum = NULL;
	}

	return (0);
}


/*
 * cvs_resp_removed()
 *
 * Handler for the `Updated' response.
 */

static int
cvs_resp_removed(struct cvsroot *root, int type, char *line)
{
	return (0);
}


/*
 * cvs_resp_mode()
 *
 * Handler for the `Mode' response.
 */

static int
cvs_resp_mode(struct cvsroot *root, int type, char *line)
{
	if (cvs_strtomode(line, &cvs_lastmode) < 0) {
		return (-1);
	}
	return (0);
}


/*
 * cvs_resp_modxpand()
 *
 * Handler for the `Module-expansion' response.
 */

static int
cvs_resp_modxpand(struct cvsroot *root, int type, char *line)
{
	return (0);
}


/*
 * cvs_sendfile()
 *
 * Send the mode and size of a file followed by the file's contents.
 * Returns 0 on success, or -1 on failure.
 */

int
cvs_sendfile(struct cvsroot *root, const char *path)
{
	int fd;
	ssize_t ret;
	char buf[4096];
	struct stat st;

	if (stat(path, &st) == -1) {
		cvs_log(LP_ERRNO, "failed to stat `%s'", path);
		return (-1);
	}

	fd = open(path, O_RDONLY, 0);
	if (fd == -1) {
		return (-1);
	}

	if (cvs_modetostr(st.st_mode, buf, sizeof(buf)) < 0)
		return (-1);

	cvs_sendln(root, buf);
	snprintf(buf, sizeof(buf), "%lld\n", st.st_size);
	cvs_sendln(root, buf);

	while ((ret = read(fd, buf, sizeof(buf))) != 0) {
		if (ret == -1) {
			cvs_log(LP_ERRNO, "failed to read file `%s'", path);
			return (-1);
		}

		cvs_sendraw(root, buf, (size_t)ret);

	}

	(void)close(fd);

	return (0);
}


/*
 * cvs_recvfile()
 *
 * Receive the mode and size of a file followed the file's contents and
 * create or update the file whose path is <path> with the received
 * information.
 */

int
cvs_recvfile(struct cvsroot *root, const char *path)
{
	int fd;
	mode_t mode;
	size_t len;
	ssize_t ret;
	off_t fsz, cnt;
	char buf[4096], *ep;

	if ((cvs_getln(root, buf, sizeof(buf)) < 0) ||
	    (cvs_strtomode(buf, &mode) < 0)) {
		return (-1);
	}

	cvs_getln(root, buf, sizeof(buf));

	fsz = (off_t)strtol(buf, &ep, 10);
	if (*ep != '\0') {
		cvs_log(LP_ERR, "parse error in file size transmission");
		return (-1);
	}

	fd = open(path, O_WRONLY|O_CREAT, mode);
	if (fd == -1) {
		cvs_log(LP_ERRNO, "failed to open `%s'", path);
		return (-1);
	}

	cnt = 0;
	do {
		len = MIN(sizeof(buf), (size_t)(fsz - cnt));
		if (len == 0)
			break;
		ret = cvs_recvraw(root, buf, len);
		if (ret == -1) {
			(void)close(fd);
			(void)unlink(path);
			return (-1);
		}

		if (write(fd, buf, (size_t)ret) == -1) {
			cvs_log(LP_ERRNO,
			    "failed to write contents to file `%s'", path);
			(void)close(fd);
			(void)unlink(path);
			return (-1);
		}

		cnt += (off_t)ret;
	} while (cnt < fsz);

	(void)close(fd);

	return (0);
}


/*
 * cvs_sendreq()
 *
 * Send a request to the server of type <rid>, with optional arguments
 * contained in <arg>, which should not be terminated by a newline.
 * Returns 0 on success, or -1 on failure.
 */

int
cvs_sendreq(struct cvsroot *root, u_int rid, const char *arg)
{
	int ret;
	struct cvs_req *req;

	if (root->cr_srvin == NULL) {
		cvs_log(LP_ERR, "cannot send request: Not connected");
		return (-1);
	}

	req = cvs_req_getbyid(rid);
	if (req == NULL) {
		cvs_log(LP_ERR, "unsupported request type %u", rid);
		return (-1);
	}

	snprintf(cvs_proto_buf, sizeof(cvs_proto_buf), "%s%s%s\n",
	    req->req_str, (arg == NULL) ? "" : " ", (arg == NULL) ? "" : arg);

	if (cvs_server_inlog != NULL)
		fputs(cvs_proto_buf, cvs_server_inlog);

	ret = fputs(cvs_proto_buf, root->cr_srvin);
	if (ret == EOF) {
		cvs_log(LP_ERRNO, "failed to send request to server");
		return (-1);
	}

	if (req->req_flags & CVS_REQF_RESP)
		ret = cvs_getresp(root);

	return (ret);
}


/*
 * cvs_getresp()
 *
 * Get a response from the server.  This call will actually read and handle
 * responses from the server until one of the response handlers returns
 * non-zero (either an error occured or the end of the response was reached).
 * Returns the number of handled commands on success, or -1 on failure.
 */

int
cvs_getresp(struct cvsroot *root)
{
	int nbcmd, ret;
	size_t len;

	nbcmd = 0;

	do {
		/* wait for incoming data */
		if (fgets(cvs_proto_buf, sizeof(cvs_proto_buf),
		    root->cr_srvout) == NULL) {
			if (feof(root->cr_srvout))
				return (0);
			cvs_log(LP_ERRNO,
			    "failed to read response from server");
			return (-1);
		}

		if (cvs_server_outlog != NULL)
			fputs(cvs_proto_buf, cvs_server_outlog);

		if ((len = strlen(cvs_proto_buf)) != 0) {
			if (cvs_proto_buf[len - 1] != '\n') {
				/* truncated line */
			}
			else
				cvs_proto_buf[--len] = '\0';
		}

		ret = cvs_resp_handle(root, cvs_proto_buf);
		nbcmd++;
	} while (ret == 0);

	if (ret > 0)
		ret = nbcmd;
	return (ret);
}


/*
 * cvs_getln()
 *
 * Get a line from the server's output and store it in <lbuf>.  The terminating
 * newline character is stripped from the result.
 */

int
cvs_getln(struct cvsroot *root, char *lbuf, size_t len)
{
	size_t rlen;

	if (fgets(lbuf, len, root->cr_srvout) == NULL) {
		if (ferror(root->cr_srvout)) {
			cvs_log(LP_ERRNO, "failed to read line from server");
			return (-1);
		}

		if (feof(root->cr_srvout))
			*lbuf = '\0';
	}

	if (cvs_server_outlog != NULL)
		fputs(lbuf, cvs_server_outlog);

	rlen = strlen(lbuf);
	if ((rlen > 0) && (lbuf[rlen - 1] == '\n'))
		lbuf[--rlen] = '\0';

	return (0);
}

#ifdef notyet
/*
 * cvs_sendresp()
 *
 * Send a response to the client of type <rid>, with optional arguments
 * contained in <arg>, which should not be terminated by a newline.
 * Returns 0 on success, or -1 on failure.
 */

int
cvs_sendresp(u_int rid, const char *arg)
{
	int ret;
	size_t len;
	const char *resp;

	resp = cvs_resp_getbyid(rid);
	if (resp == NULL) {
		cvs_log(LP_ERR, "unsupported response type %u", rid);
		return (-1);
	}

	snprintf(cvs_proto_buf, sizeof(cvs_proto_buf), "%s %s\n", resp,
	    (arg == NULL) ? "" : arg);

	ret = fputs(resp, stdout);
	if (ret == EOF) {
		cvs_log(LP_ERRNO, "failed to send response to client");
	}
	else {
		if (arg != NULL)
			ret = fprintf(stdout, " %s", arg);
		putc('\n', stdout);
	}
	return (0);
}


/*
 * cvs_getreq()
 *
 * Get a request from the client.
 */

int
cvs_getreq(void)
{
	int nbcmd;

	nbcmd = 0;

	do {
		/* wait for incoming data */
		if (fgets(cvs_proto_buf, sizeof(cvs_proto_buf),
		    stdin) == NULL) {
			if (feof(stdin))
				return (0);
			cvs_log(LP_ERRNO,
			    "failed to read request from client");
			return (-1);
		}

		if ((len = strlen(cvs_proto_buf)) != 0) {
			if (cvs_proto_buf[len - 1] != '\n') {
				/* truncated line */
			}
			else
				cvs_proto_buf[--len] = '\0';
		}

		ret = cvs_resp_handle(cvs_proto_buf);
	} while (ret == 0);
}
#endif


/*
 * cvs_sendln()
 *
 * Send a single line <line> string to the server.  The line is sent as is,
 * without any modifications.
 * Returns 0 on success, or -1 on failure.
 */

int
cvs_sendln(struct cvsroot *root, const char *line)
{
	int nl;
	size_t len;

	nl = 0;
	len = strlen(line);

	if ((len > 0) && (line[len - 1] != '\n'))
		nl = 1;

	if (cvs_server_inlog != NULL) {
		fputs(line, cvs_server_inlog);
		if (nl)
			fputc('\n', cvs_server_inlog);
	}
	fputs(line, root->cr_srvin);
	if (nl)
		fputc('\n', root->cr_srvin);

	return (0);
}


/*
 * cvs_sendraw()
 *
 * Send the first <len> bytes from the buffer <src> to the server.
 */

int
cvs_sendraw(struct cvsroot *root, const void *src, size_t len)
{
	if (cvs_server_inlog != NULL)
		fwrite(src, sizeof(char), len, cvs_server_inlog);
	if (fwrite(src, sizeof(char), len, root->cr_srvin) < len) {
		return (-1);
	}

	return (0);
}


/*
 * cvs_recvraw()
 *
 * Receive the first <len> bytes from the buffer <src> to the server.
 */

ssize_t
cvs_recvraw(struct cvsroot *root, void *dst, size_t len)
{
	size_t ret;

	ret = fread(dst, sizeof(char), len, root->cr_srvout);
	if (ret == 0)
		return (-1);
	if (cvs_server_outlog != NULL)
		fwrite(dst, sizeof(char), len, cvs_server_outlog);
	return (ssize_t)ret;
}


/*
 * cvs_senddir()
 *
 * Send a `Directory' request along with the 2 paths that follow it.
 */

int
cvs_senddir(struct cvsroot *root, CVSFILE *dir) 
{
	char buf[MAXPATHLEN];

	if (dir->cf_ddat->cd_repo == NULL)
		strlcpy(buf, root->cr_dir, sizeof(buf));
	else
		snprintf(buf, sizeof(buf), "%s/%s", root->cr_dir,
		    dir->cf_ddat->cd_repo);

	if ((cvs_sendreq(root, CVS_REQ_DIRECTORY, dir->cf_path) < 0) ||
	    (cvs_sendln(root, buf) < 0))
		return (-1);

	return (0);
}


/*
 * cvs_sendarg()
 *
 * Send the argument <arg> to the server.  The argument <append> is used to
 * determine if the argument should be simply appended to the last argument
 * sent or if it should be created as a new argument (0).
 */

int
cvs_sendarg(struct cvsroot *root, const char *arg, int append)
{
	return cvs_sendreq(root, ((append == 0) ?
	    CVS_REQ_ARGUMENT : CVS_REQ_ARGUMENTX), arg);
}


/*
 * cvs_sendentry()
 *
 * Send an `Entry' request to the server along with the mandatory fields from
 * the CVS entry <ent> (which are the name and revision).
 */

int
cvs_sendentry(struct cvsroot *root, const struct cvs_ent *ent)
{
	char ebuf[128], numbuf[64];

	snprintf(ebuf, sizeof(ebuf), "/%s/%s///", ent->ce_name,
	    rcsnum_tostr(ent->ce_rev, numbuf, sizeof(numbuf)));

	return cvs_sendreq(root, CVS_REQ_ENTRY, ebuf);
}


/*
 * cvs_initlog()
 *
 * Initialize protocol logging if the CVS_CLIENT_LOG environment variable is
 * set.  In this case, the variable's value is used as a path to which the
 * appropriate suffix is added (".in" for server input and ".out" for server
 * output.
 * Returns 0 on success, or -1 on failure.
 */

static int
cvs_initlog(void)
{
	char *env, fpath[MAXPATHLEN];

	/* avoid doing it more than once */
	if (cvs_server_logon)
		return (0);

	env = getenv("CVS_CLIENT_LOG");
	if (env == NULL)
		return (0);

	strlcpy(fpath, env, sizeof(fpath));
	strlcat(fpath, ".in", sizeof(fpath));
	cvs_server_inlog = fopen(fpath, "w");
	if (cvs_server_inlog == NULL) {
		cvs_log(LP_ERRNO, "failed to open server input log `%s'",
		    fpath);
		return (-1);
	}

	strlcpy(fpath, env, sizeof(fpath));
	strlcat(fpath, ".out", sizeof(fpath));
	cvs_server_outlog = fopen(fpath, "w");
	if (cvs_server_outlog == NULL) {
		cvs_log(LP_ERRNO, "failed to open server output log `%s'",
		    fpath);
		return (-1);
	}

	/* make the streams line-buffered */
	setvbuf(cvs_server_inlog, NULL, _IOLBF, 0);
	setvbuf(cvs_server_outlog, NULL, _IOLBF, 0);

	cvs_server_logon = 1;

	return (0);
}
