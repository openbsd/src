/*	$OpenBSD: proto.c,v 1.90 2006/02/08 19:24:19 joris Exp $	*/
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
 *	http://www.iam.unibe.ch/~til/documentation/cvs/cvsclient_toc.html
 *	http://www.elegosoft.com/cvs/cvsclient_toc.html
 *
 * The protocol is split up into two parts; the first part is the client side
 * of things and is composed of all the response handlers, which are all named
 * with a prefix of "cvs_resp_".  The second part is the set of request
 * handlers used by the server.  These handlers process the request and
 * generate the appropriate response to send back.  The prefix for request
 * handlers is "cvs_req_".
 *
 */

#include "includes.h"

#include "cvs.h"
#include "log.h"
#include "proto.h"


/* request flags */
#define CVS_REQF_RESP	0x01


static int	cvs_initlog(void);

struct cvs_req cvs_requests[] = {
	{ CVS_REQ_DIRECTORY,     "Directory",         0             },
	{ CVS_REQ_MAXDOTDOT,     "Max-dotdot",        0             },
	{ CVS_REQ_STATICDIR,     "Static-directory",  0             },
	{ CVS_REQ_STICKY,        "Sticky",            0             },
	{ CVS_REQ_ENTRY,         "Entry",             0             },
	{ CVS_REQ_ENTRYEXTRA,    "EntryExtra",        0             },
	{ CVS_REQ_CHECKINTIME,   "Checkin-time",      0             },
	{ CVS_REQ_MODIFIED,      "Modified",          0             },
	{ CVS_REQ_ISMODIFIED,    "Is-modified",       0             },
	{ CVS_REQ_UNCHANGED,     "Unchanged",         0             },
	{ CVS_REQ_USEUNCHANGED,  "UseUnchanged",      0             },
	{ CVS_REQ_NOTIFY,        "Notify",            0             },
	{ CVS_REQ_NOTIFYUSER,    "NotifyUser",        0             },
	{ CVS_REQ_QUESTIONABLE,  "Questionable",      0             },
	{ CVS_REQ_CASE,          "Case",              0             },
	{ CVS_REQ_UTF8,          "Utf8",              0             },
	{ CVS_REQ_ARGUMENT,      "Argument",          0             },
	{ CVS_REQ_ARGUMENTX,     "Argumentx",         0             },
	{ CVS_REQ_GLOBALOPT,     "Global_option",     0             },
	{ CVS_REQ_GZIPSTREAM,    "Gzip-stream",       0             },
	{ CVS_REQ_READCVSRC2,    "read-cvsrc2",       0             },
	{ CVS_REQ_READWRAP,      "read-cvswrappers",  0             },
	{ CVS_REQ_READIGNORE,    "read-cvsignore",    0             },
	{ CVS_REQ_ERRIFREADER,   "Error-If-Reader",   0             },
	{ CVS_REQ_VALIDRCSOPT,   "Valid-RcsOptions",  0             },
	{ CVS_REQ_SET,           "Set",               0             },
	{ CVS_REQ_XPANDMOD,      "expand-modules",    CVS_REQF_RESP },
	{ CVS_REQ_LOG,           "log",               CVS_REQF_RESP },
	{ CVS_REQ_CO,            "co",                CVS_REQF_RESP },
	{ CVS_REQ_EXPORT,        "export",            CVS_REQF_RESP },
	{ CVS_REQ_ANNOTATE,      "annotate",          CVS_REQF_RESP },
	{ CVS_REQ_RDIFF,         "rdiff",             CVS_REQF_RESP },
	{ CVS_REQ_RTAG,          "rtag",              CVS_REQF_RESP },
	{ CVS_REQ_INIT,          "init",              CVS_REQF_RESP },
	{ CVS_REQ_STATUS,        "status",            CVS_REQF_RESP },
	{ CVS_REQ_UPDATE,        "update",            CVS_REQF_RESP },
	{ CVS_REQ_HISTORY,       "history",           CVS_REQF_RESP },
	{ CVS_REQ_IMPORT,        "import",            CVS_REQF_RESP },
	{ CVS_REQ_ADD,           "add",               CVS_REQF_RESP },
	{ CVS_REQ_REMOVE,        "remove",            CVS_REQF_RESP },
	{ CVS_REQ_RELEASE,       "release",           CVS_REQF_RESP },
	{ CVS_REQ_ROOT,          "Root",              0             },
	{ CVS_REQ_VALIDRESP,     "Valid-responses",   0             },
	{ CVS_REQ_VALIDREQ,      "valid-requests",    CVS_REQF_RESP },
	{ CVS_REQ_VERSION,       "version",           CVS_REQF_RESP },
	{ CVS_REQ_NOOP,          "noop",              CVS_REQF_RESP },
	{ CVS_REQ_DIFF,          "diff",              CVS_REQF_RESP },
	{ CVS_REQ_CI,            "ci",                CVS_REQF_RESP },
	{ CVS_REQ_TAG,           "tag",               CVS_REQF_RESP },
	{ CVS_REQ_ADMIN,         "admin",             CVS_REQF_RESP },
	{ CVS_REQ_WATCHERS,      "watchers",          CVS_REQF_RESP },
	{ CVS_REQ_WATCH_ON,      "watch-on",          CVS_REQF_RESP },
	{ CVS_REQ_WATCH_OFF,     "watch-off",         CVS_REQF_RESP },
	{ CVS_REQ_WATCH_ADD,     "watch-add",         CVS_REQF_RESP },
	{ CVS_REQ_WATCH_REMOVE,  "watch-remove",      CVS_REQF_RESP },
	{ CVS_REQ_EDITORS,       "editors",           CVS_REQF_RESP },
};

struct cvs_resp cvs_responses[] = {
	{ CVS_RESP_OK,         "ok"                     },
	{ CVS_RESP_ERROR,      "error"                  },
	{ CVS_RESP_VALIDREQ,   "Valid-requests"         },
	{ CVS_RESP_M,          "M"                      },
	{ CVS_RESP_MBINARY,    "Mbinary"                },
	{ CVS_RESP_MT,         "MT"                     },
	{ CVS_RESP_E,          "E"                      },
	{ CVS_RESP_F,          "F"                      },
	{ CVS_RESP_CREATED,    "Created"                },
	{ CVS_RESP_UPDATED,    "Updated"                },
	{ CVS_RESP_UPDEXIST,   "Update-existing"        },
	{ CVS_RESP_MERGED,     "Merged"                 },
	{ CVS_RESP_REMOVED,    "Removed"                },
	{ CVS_RESP_RMENTRY,    "Remove-entry"           },
	{ CVS_RESP_CKSUM,      "Checksum"               },
	{ CVS_RESP_CLRSTATDIR, "Clear-static-directory" },
	{ CVS_RESP_SETSTATDIR, "Set-static-directory"   },
	{ CVS_RESP_NEWENTRY,   "New-entry"              },
	{ CVS_RESP_CHECKEDIN,  "Checked-in"             },
	{ CVS_RESP_MODE,       "Mode"                   },
	{ CVS_RESP_MODTIME,    "Mod-time"               },
	{ CVS_RESP_MODXPAND,   "Module-expansion"       },
	{ CVS_RESP_SETSTICKY,  "Set-sticky"             },
	{ CVS_RESP_CLRSTICKY,  "Clear-sticky"           },
	{ CVS_RESP_RCSDIFF,    "Rcs-diff"               },
	{ CVS_RESP_TEMPLATE,   "Template"               },
	{ CVS_RESP_COPYFILE,   "Copy-file"              },
};

#define CVS_NBREQ	(sizeof(cvs_requests)/sizeof(cvs_requests[0]))
#define CVS_NBRESP	(sizeof(cvs_responses)/sizeof(cvs_responses[0]))

/* hack to receive the remote version without outputting it */
u_int cvs_version_sent = 0;

static char  cvs_proto_buf[4096];

/*
 * Output files for protocol logging when the CVS_CLIENT_LOG environment
 * variable is set.
 */
static int   cvs_server_logon = 0;
static FILE *cvs_server_inlog = NULL;
static FILE *cvs_server_outlog = NULL;

static pid_t cvs_subproc_pid;

/* last directory sent with cvs_senddir() */
static char cvs_lastdir[MAXPATHLEN] = "";


/*
 * cvs_connect()
 *
 * Open a client connection to the cvs server whose address is given in
 * the <root> variable.  The method used to connect depends on the
 * setting of the CVS_RSH variable.
 * Once the connection has been established, we first send the list of
 * responses we support and request the list of supported requests from the
 * server.  Then, a version request is sent and various global flags are sent.
 */
void
cvs_connect(struct cvsroot *root)
{
	size_t len;
	int argc, infd[2], outfd[2], errfd[2];
	char *argv[16], *cvs_server_cmd, tmsg[1024], *vresp;

	if (root->cr_method == CVS_METHOD_PSERVER)
		fatal("no pserver support due to security issues");
	else if ((root->cr_method == CVS_METHOD_KSERVER) ||
	    (root->cr_method == CVS_METHOD_GSERVER) ||
	    (root->cr_method == CVS_METHOD_EXT) ||
	    (root->cr_method == CVS_METHOD_FORK))
		fatal("connection method not supported yet");

	if (root->cr_flags & CVS_ROOT_CONNECTED) {
		cvs_log(LP_NOTICE, "already connected to CVSROOT");
		return;
	}

	if (pipe(infd) == -1)
		fatal("failed to create input pipe for client connection");

	if (pipe(outfd) == -1)
		fatal("failed to create output pipe for client connection");

	if (pipe(errfd) == -1)
		fatal("failed to create error pipe for client connection");

	cvs_subproc_pid = fork();
	if (cvs_subproc_pid == -1) {
		fatal("failed to fork for cvs server connection");
	} else if (cvs_subproc_pid == 0) {
		if ((dup2(infd[0], STDIN_FILENO) == -1) ||
		    (dup2(outfd[1], STDOUT_FILENO) == -1))
			fatal("failed to setup standard streams "
			    "for cvs server");

		(void)close(infd[1]);
		(void)close(outfd[0]);
		(void)close(errfd[0]);

		argc = 0;
		argv[argc++] = cvs_rsh;

		if (root->cr_user != NULL) {
			argv[argc++] = "-l";
			argv[argc++] = root->cr_user;
		}

		cvs_server_cmd = getenv("CVS_SERVER");
		if (cvs_server_cmd == NULL)
			cvs_server_cmd = CVS_SERVER_DEFAULT;

		argv[argc++] = root->cr_host;
		argv[argc++] = cvs_server_cmd;
		argv[argc++] = "server";
		argv[argc] = NULL;

		if (cvs_trace == 1) {
			tmsg[0] = '\0';
			for (argc = 0; argv[argc] != NULL; argc++) {
				len = strlcat(tmsg, argv[argc], sizeof(tmsg));
				if (len >= sizeof(tmsg))
					fatal("truncation in cvs_connect");

				len = strlcat(tmsg, " ", sizeof(tmsg));
				if (len >= sizeof(tmsg))
					fatal("truncation in cvs_connect");
			}
		}

		cvs_log(LP_TRACE, "Starting server: %s", tmsg);

		execvp(argv[0], argv);
		fatal("failed to execute cvs server");
	}

	/* we are the parent */
	(void)close(infd[0]);
	(void)close(outfd[1]);
	(void)close(errfd[1]);

	root->cr_srvin = fdopen(infd[1], "w");
	if (root->cr_srvin == NULL)
		fatal("failed to create pipe stream");

	root->cr_srvout = fdopen(outfd[0], "r");
	if (root->cr_srvout == NULL)
		fatal("failed to create pipe stream");

	/* make the streams line-buffered */
	(void)setvbuf(root->cr_srvin, NULL, _IOLBF, (size_t)0);
	(void)setvbuf(root->cr_srvout, NULL, _IOLBF, (size_t)0);

	cvs_initlog();

	/*
	 * Send the server the list of valid responses, then ask for valid
	 * requests.
	 */

	vresp = cvs_resp_getvalid();
	cvs_sendreq(root, CVS_REQ_VALIDRESP, vresp);
	xfree(vresp);

	cvs_sendreq(root, CVS_REQ_VALIDREQ, NULL);

	/* send the CVSROOT to the server */
	cvs_sendreq(root, CVS_REQ_ROOT, root->cr_dir);

	/* don't fail if this request doesn't work */
	cvs_sendreq(root, CVS_REQ_VERSION, NULL);

	/* now share our global options with the server */
	if (verbosity <= 1)
		cvs_sendreq(root, CVS_REQ_GLOBALOPT, "-q");
	if (verbosity == 0)
		cvs_sendreq(root, CVS_REQ_GLOBALOPT, "-Q");

	if (cvs_noexec == 1)
		cvs_sendreq(root, CVS_REQ_GLOBALOPT, "-n");

	if (cvs_nolog == 1)
		cvs_sendreq(root, CVS_REQ_GLOBALOPT, "-l");

	if (cvs_readonly == 1)
		cvs_sendreq(root, CVS_REQ_GLOBALOPT, "-r");

	if (cvs_trace == 1)
		cvs_sendreq(root, CVS_REQ_GLOBALOPT, "-t");

	/* not sure why, but we have to send this */
	cvs_sendreq(root, CVS_REQ_USEUNCHANGED, NULL);

	cvs_log(LP_DEBUG, "connected to %s", root->cr_host);

	root->cr_flags |= CVS_ROOT_CONNECTED;
}


/*
 * cvs_disconnect()
 *
 * Disconnect from the cvs server.
 */
void
cvs_disconnect(struct cvsroot *root)
{
	if (!(root->cr_flags & CVS_ROOT_CONNECTED))
		return;

	cvs_log(LP_DEBUG, "closing connection to %s", root->cr_host);

	if (root->cr_srvin != NULL) {
		(void)fclose(root->cr_srvin);
		root->cr_srvin = NULL;
	}
	if (root->cr_srvout != NULL) {
		(void)fclose(root->cr_srvout);
		root->cr_srvout = NULL;
	}

	root->cr_flags &= ~CVS_ROOT_CONNECTED;
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
char *
cvs_req_getvalid(void)
{
	u_int i;
	size_t len;
	char *vrstr;
	BUF *buf;

	buf = cvs_buf_alloc((size_t)512, BUF_AUTOEXT);

	cvs_buf_set(buf, cvs_requests[0].req_str,
	    strlen(cvs_requests[0].req_str), (size_t)0);

	for (i = 1; i < CVS_NBREQ; i++) {
		cvs_buf_putc(buf, ' ');
		cvs_buf_append(buf, cvs_requests[i].req_str,
		    strlen(cvs_requests[i].req_str));
	}

	/* NUL-terminate */
	cvs_buf_putc(buf, '\0');

	len = cvs_buf_len(buf);
	vrstr = (char *)xmalloc(len);
	cvs_buf_copy(buf, (size_t)0, vrstr, len);
	cvs_buf_free(buf);

	return (vrstr);
}


/*
 * cvs_resp_getbyid()
 *
 */
struct cvs_resp*
cvs_resp_getbyid(int respid)
{
	u_int i;

	for (i = 0; i < CVS_NBRESP; i++)
		if (cvs_responses[i].resp_id == (u_int)respid)
			return &(cvs_responses[i]);

	return (NULL);
}


/*
 * cvs_resp_getbyname()
 */
struct cvs_resp *
cvs_resp_getbyname(const char *rname)
{
	u_int i;

	for (i = 0; i < CVS_NBRESP; i++)
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
char *
cvs_resp_getvalid(void)
{
	u_int i;
	size_t len;
	char *vrstr;
	BUF *buf;

	buf = cvs_buf_alloc((size_t)512, BUF_AUTOEXT);

	cvs_buf_set(buf, cvs_responses[0].resp_str,
	    strlen(cvs_responses[0].resp_str), (size_t)0);

	for (i = 1; i < CVS_NBRESP; i++) {
		cvs_buf_putc(buf, ' ');
		cvs_buf_append(buf, cvs_responses[i].resp_str,
		    strlen(cvs_responses[i].resp_str));
	}

	/* NUL-terminate */
	cvs_buf_putc(buf, '\0');

	len = cvs_buf_len(buf);
	vrstr = (char *)xmalloc(len);
	cvs_buf_copy(buf, (size_t)0, vrstr, len);
	cvs_buf_free(buf);

	return (vrstr);
}


/*
 * cvs_sendfile()
 *
 * Send the mode and size of a file followed by the file's contents.
 * Returns 0 on success, or -1 on failure.
 */
void
cvs_sendfile(struct cvsroot *root, const char *path)
{
	int fd, l;
	ssize_t ret;
	char buf[4096];
	struct stat st;

	cvs_log(LP_TRACE, "Sending file `%s' to server", basename(path));

	if (stat(path, &st) == -1) {
		fatal("cvs_sendfile(): stat failed on '%s': %s",
		    path, strerror(errno));
	}

	cvs_modetostr(st.st_mode, buf, sizeof(buf));

	fd = open(path, O_RDONLY, 0);
	if (fd == -1) {
		fatal("cvs_sendfile(): failed to open '%s': %s",
		    path, strerror(errno));
	}

	cvs_sendln(root, buf);

	l = snprintf(buf, sizeof(buf), "%lld\n", st.st_size);
	if (l == -1 || l >= (int)sizeof(buf))
		fatal("overflow in cvs_sendfile");

	cvs_sendln(root, buf);

	while ((ret = read(fd, buf, sizeof(buf))) != 0) {
		if (ret == -1) {
			(void)close(fd);
			fatal("cvs_sendfile: read error on '%s'", path);
		}

		cvs_sendraw(root, buf, (size_t)ret);
	}

	(void)close(fd);
}


/*
 * cvs_recvfile()
 *
 * Receive the mode and size of a file followed the file's contents and
 * create or update the file whose path is <path> with the received
 * information.
 */
BUF*
cvs_recvfile(struct cvsroot *root, mode_t *mode)
{
	size_t len;
	ssize_t ret;
	off_t fsz, cnt;
	char buf[4096], *ep;
	BUF *fbuf;

	fbuf = cvs_buf_alloc(sizeof(buf), BUF_AUTOEXT);

	cvs_getln(root, buf, sizeof(buf));
	cvs_strtomode(buf, mode);

	cvs_getln(root, buf, sizeof(buf));

	fsz = (off_t)strtol(buf, &ep, 10);
	if (*ep != '\0')
		fatal("parse error in file size transmission");

	cnt = 0;
	do {
		len = MIN(sizeof(buf), (size_t)(fsz - cnt));
		if (len == 0)
			break;
		ret = cvs_recvraw(root, buf, len);
		cvs_buf_append(fbuf, buf, (size_t)ret);
		cnt += (off_t)ret;
	} while (cnt < fsz);

	return (fbuf);
}

/*
 * cvs_sendreq()
 *
 * Send a request to the server of type <rid>, with optional arguments
 * contained in <arg>, which should not be terminated by a newline.
 * Returns 0 on success, or -1 on failure.
 */
void
cvs_sendreq(struct cvsroot *root, u_int rid, const char *arg)
{
	int ret;
	struct cvs_req *req;

	if (root->cr_srvin == NULL)
		fatal("cannot send request %u: Not connected", rid);

	req = cvs_req_getbyid(rid);
	if (req == NULL)
		fatal("unsupported request type %u", rid);

	/* is this request supported by the server? */
	if (!CVS_GETVR(root, req->req_id)) {
		if (rid == CVS_REQ_VERSION) {
			cvs_sendreq(root, CVS_REQ_NOOP, arg);
		} else {
			cvs_log(LP_WARN,
			    "remote end does not support request `%s'",
			    req->req_str);
		}

		return;
	}

	if (strlcpy(cvs_proto_buf, req->req_str, sizeof(cvs_proto_buf)) >=
	    sizeof(cvs_proto_buf) ||
	    strlcat(cvs_proto_buf, (arg == NULL) ? "" : " ",
	    sizeof(cvs_proto_buf)) >= sizeof(cvs_proto_buf) ||
	    strlcat(cvs_proto_buf, (arg == NULL) ? "" : arg,
	    sizeof(cvs_proto_buf)) >= sizeof(cvs_proto_buf) ||
	    strlcat(cvs_proto_buf, "\n",
	    sizeof(cvs_proto_buf)) >= sizeof(cvs_proto_buf))
		fatal("cvs_sendreq: overflow when creating proto buffer");

	if (cvs_server_inlog != NULL)
		fputs(cvs_proto_buf, cvs_server_inlog);

	ret = fputs(cvs_proto_buf, root->cr_srvin);
	if (ret == EOF)
		fatal("failed to send request to server");

	if (rid == CVS_REQ_VERSION)
		cvs_version_sent = 1;

	if (req->req_flags & CVS_REQF_RESP)
		cvs_getresp(root);
}


/*
 * cvs_getresp()
 *
 * Get a response from the server.  This call will actually read and handle
 * responses from the server until one of the response handlers returns
 * non-zero (either an error occurred or the end of the response was reached).
 * Returns the number of handled commands on success, or -1 on failure.
 */
void
cvs_getresp(struct cvsroot *root)
{
	int ret;
	size_t len;

	do {
		/* wait for incoming data */
		if (fgets(cvs_proto_buf, (int)sizeof(cvs_proto_buf),
		    root->cr_srvout) == NULL) {
			if (feof(root->cr_srvout))
				return;
			fatal("failed to read response from server");
		}

		if (cvs_server_outlog != NULL)
			fputs(cvs_proto_buf, cvs_server_outlog);

		if ((len = strlen(cvs_proto_buf)) != 0) {
			/* if len - 1 != '\n' the line is truncated */
			if (cvs_proto_buf[len - 1] == '\n')
				cvs_proto_buf[--len] = '\0';
		}

		ret = cvs_resp_handle(root, cvs_proto_buf);
	} while (ret == 0);
}


/*
 * cvs_getln()
 *
 * Get a line from the remote end and store it in <lbuf>.  The terminating
 * newline character is stripped from the result.
 */
void
cvs_getln(struct cvsroot *root, char *lbuf, size_t len)
{
	size_t rlen;
	FILE *in;

	if (cvs_cmdop == CVS_OP_SERVER)
		in = stdin;
	else
		in = root->cr_srvout;

	if (fgets(lbuf, (int)len, in) == NULL) {
		if (ferror(in)) {
			fatal("cvs_getln: error reading server: %s",
			    strerror(errno));
		}

		if (feof(in))
			*lbuf = '\0';
	}

	if (cvs_server_outlog != NULL)
		fputs(lbuf, cvs_server_outlog);

	rlen = strlen(lbuf);
	if ((rlen > 0) && (lbuf[rlen - 1] == '\n'))
		lbuf[--rlen] = '\0';
}


/*
 * cvs_sendresp()
 *
 * Send a response of type <rid> to the client, with optional arguments
 * contained in <arg>, which should not be terminated by a newline.
 */
void
cvs_sendresp(u_int rid, const char *arg)
{
	int ret;
	struct cvs_resp *resp;

	if ((resp = cvs_resp_getbyid(rid)) == NULL)
		fatal("unsupported response type %u", rid);

	ret = fputs(resp->resp_str, stdout);
	if (ret == EOF) {
		cvs_log(LP_ERRNO, "failed to send response to client");
	} else {
		if (arg != NULL) {
			putc(' ', stdout);
			fputs(arg, stdout);
		}
		putc('\n', stdout);
	}
}


/*
 * cvs_sendln()
 *
 * Send a single line <line> string to the remote end.  The line is sent as is,
 * without any modifications.
 */
void
cvs_sendln(struct cvsroot *root, const char *line)
{
	int nl;
	size_t len;
	FILE *out;

	if (cvs_cmdop == CVS_OP_SERVER)
		out = stdout;
	else
		out = root->cr_srvin;

	nl = 0;
	len = strlen(line);

	if ((len > 0) && (line[len - 1] != '\n'))
		nl = 1;

	if (cvs_server_inlog != NULL) {
		fputs(line, cvs_server_inlog);
		if (nl)
			putc('\n', cvs_server_inlog);
	}
	fputs(line, out);
	if (nl)
		putc('\n', out);
}


/*
 * cvs_sendraw()
 *
 * Send the first <len> bytes from the buffer <src> to the server.
 */
void
cvs_sendraw(struct cvsroot *root, const void *src, size_t len)
{
	FILE *out;

	if (cvs_cmdop == CVS_OP_SERVER)
		out = stdout;
	else
		out = root->cr_srvin;

	if (cvs_server_inlog != NULL)
		fwrite(src, sizeof(char), len, cvs_server_inlog);
	if (fwrite(src, sizeof(char), len, out) < len)
		fatal("failed to send data");
}


/*
 * cvs_recvraw()
 *
 * Receive the first <len> bytes from the buffer <src> to the server.
 */
size_t
cvs_recvraw(struct cvsroot *root, void *dst, size_t len)
{
	size_t ret;
	FILE *in;

	if (cvs_cmdop == CVS_OP_SERVER)
		in = stdin;
	else
		in = root->cr_srvout;

	ret = fread(dst, sizeof(char), len, in);
	if (ret == 0) {
		if (ferror(in)) {
			fatal("cvs_recvraw: error reading from server: %s",
			    strerror(errno));
		}
	} else {
		if (cvs_server_outlog != NULL)
			fwrite(dst, sizeof(char), len, cvs_server_outlog);
	}

	return (ret);
}


/*
 * cvs_senddir()
 *
 * Send a `Directory' request along with the 2 paths that follow it.  If
 * the directory info to be sent is the same as the last info sent, the
 * call does nothing and simply returns without an error.
 */
void
cvs_senddir(struct cvsroot *root, CVSFILE *dir)
{
	size_t len;
	char lbuf[MAXPATHLEN], rbuf[MAXPATHLEN];

	if (dir->cf_type != DT_DIR)
		fatal("cvs_senddir(): cf_type != DT_DIR: %d", dir->cf_type);

	cvs_file_getpath(dir, lbuf, sizeof(lbuf));
	if (strcmp(lbuf, cvs_lastdir) == 0 && cvs_cmdop != CVS_OP_CHECKOUT)
		return;

	if (dir->cf_repo == NULL) {
		len = strlcpy(rbuf, root->cr_dir, sizeof(rbuf));
		if (len >= sizeof(rbuf))
			fatal("cvs_senddir: path truncation");
	} else {
		len = cvs_path_cat(root->cr_dir, dir->cf_repo, rbuf,
		    sizeof(rbuf));
		if (len >= sizeof(rbuf))
			fatal("cvs_senddir: path truncation");
	}

	cvs_sendreq(root, CVS_REQ_DIRECTORY, lbuf);
	cvs_sendln(root, rbuf);

	len = strlcpy(cvs_lastdir, lbuf, sizeof(cvs_lastdir));
	if (len >= sizeof(lbuf))
		fatal("path truncation in cvs_senddir");
}


/*
 * cvs_sendarg()
 *
 * Send the argument <arg> to the server.  The argument <append> is used to
 * determine if the argument should be simply appended to the last argument
 * sent or if it should be created as a new argument (0).
 */
void
cvs_sendarg(struct cvsroot *root, const char *arg, int append)
{
	cvs_sendreq(root, ((append == 0) ? CVS_REQ_ARGUMENT :
	    CVS_REQ_ARGUMENTX), arg);
}


/*
 * cvs_sendentry()
 *
 * Send an `Entry' request to the server along with the mandatory fields from
 * the CVS entry <ent> (which are the name and revision).
 */
void
cvs_sendentry(struct cvsroot *root, const CVSFILE *file)
{
	char ebuf[CVS_ENT_MAXLINELEN], numbuf[64];

	if (file->cf_type != DT_REG)
		fatal("cvs_sendentry: cf_type != DT_REG: %d", file->cf_type);

	/* don't send Entry for unknown files */
	if (file->cf_cvstat == CVS_FST_UNKNOWN)
		return;

	if (strlcpy(ebuf, "/", sizeof(ebuf)) >= sizeof(ebuf) ||
	    strlcat(ebuf, file->cf_name, sizeof(ebuf)) >= sizeof(ebuf) ||
	    strlcat(ebuf, "/", sizeof(ebuf)) >= sizeof(ebuf) ||
	    strlcat(ebuf, (file->cf_cvstat == CVS_FST_REMOVED) ? "-" : "",
	    sizeof(ebuf)) >= sizeof(ebuf) ||
	    strlcat(ebuf, rcsnum_tostr(file->cf_lrev, numbuf, sizeof(numbuf)),
	    sizeof(ebuf)) >= sizeof(ebuf) ||
	    strlcat(ebuf, "///", sizeof(ebuf)) >= sizeof(ebuf))
		fatal("cvs_sendentry: overflow when creating entry buffer");

	cvs_sendreq(root, CVS_REQ_ENTRY, ebuf);
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
	int l;
	u_int i;
	char *env, *envdup, buf[MAXPATHLEN], fpath[MAXPATHLEN];
	char rpath[MAXPATHLEN], *s;
	struct stat st;
	time_t now;
	struct passwd *pwd;

	/* avoid doing it more than once */
	if (cvs_server_logon)
		return (0);

	env = getenv("CVS_CLIENT_LOG");
	if (env == NULL)
		return (0);

	envdup = xstrdup(env);
	if ((s = strchr(envdup, '%')) != NULL)
		*s = '\0';

	strlcpy(buf, env, sizeof(buf));
	strlcpy(rpath, envdup, sizeof(rpath));
	xfree(envdup);

	s = buf;
	while ((s = strchr(s, '%')) != NULL) {
		*s++;
		switch (*s) {
		case 'c':
			strlcpy(fpath, cvs_command, sizeof(fpath));
			break;
		case 'd':
			time(&now);
			strlcpy(fpath, ctime(&now), sizeof(fpath));
			break;
		case 'p':
			snprintf(fpath, sizeof(fpath), "%d", getpid());
			break;
		case 'u':
			if ((pwd = getpwuid(getuid())) != NULL)
				strlcpy(fpath, pwd->pw_name, sizeof(fpath));
			else
				fpath[0] = '\0';
			endpwent();
			break;
		default:
			fpath[0] = '\0';
			break;
		}

		if (fpath[0] != '\0') {
			strlcat(rpath, "-", sizeof(rpath));
			strlcat(rpath, fpath, sizeof(rpath));
		}
	}

	for (i = 0; i < UINT_MAX; i++) {
		l = snprintf(fpath, sizeof(fpath), "%s-%d.in", rpath, i);
		if (l == -1 || l >= (int)sizeof(fpath)) {
			errno = ENAMETOOLONG;
			cvs_log(LP_ERRNO, "%s", fpath);
			return (-1);
		}

		if (stat(fpath, &st) != -1)
			continue;

		if (errno != ENOENT)
			return (-1);

		break;
	}

	cvs_server_inlog = fopen(fpath, "w");
	if (cvs_server_inlog == NULL) {
		cvs_log(LP_ERRNO, "failed to open server input log `%s'",
		    fpath);
		return (-1);
	}

	for (i = 0; i < UINT_MAX; i++) {
		l = snprintf(fpath, sizeof(fpath), "%s-%d.out", rpath, i);
		if (l == -1 || l >= (int)sizeof(fpath)) {
			errno = ENAMETOOLONG;
			cvs_log(LP_ERRNO, "%s", fpath);
			return (-1);
		}

		if (stat(fpath, &st) != -1)
			continue;

		if (errno != ENOENT)
			return (-1);

		break;
	}

	cvs_server_outlog = fopen(fpath, "w");
	if (cvs_server_outlog == NULL) {
		cvs_log(LP_ERRNO, "failed to open server output log `%s'",
		    fpath);
		return (-1);
	}

	/* make the streams line-buffered */
	setvbuf(cvs_server_inlog, NULL, _IOLBF, (size_t)0);
	setvbuf(cvs_server_outlog, NULL, _IOLBF, (size_t)0);

	cvs_server_logon = 1;

	return (0);
}
