/*	$OpenBSD: client.c,v 1.2 2004/07/26 16:01:22 jfb Exp $	*/
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

#include "cvs.h"
#include "log.h"



extern int   verbosity;
extern int   cvs_compress;
extern char *cvs_rsh;
extern int   cvs_trace;
extern int   cvs_nolog;
extern int   cvs_readonly;

extern struct cvsroot *cvs_root;




static int  cvs_client_sendinfo (void);
static int  cvs_client_initlog  (void);



static int    cvs_server_infd = -1;
static int    cvs_server_outfd = -1;
static FILE  *cvs_server_in;
static FILE  *cvs_server_out;

/* protocol log files, if the CVS_CLIENT_LOG environment variable is used */
static FILE  *cvs_server_inlog;
static FILE  *cvs_server_outlog;

static char   cvs_client_buf[4096];



/*
 * cvs_client_connect()
 *
 * Open a client connection to the cvs server whose address is given in
 * the global <cvs_root> variable.  The method used to connect depends on the
 * setting of the CVS_RSH variable.
 */

int
cvs_client_connect(void)
{
	int argc, infd[2], outfd[2];
	pid_t pid;
	char *argv[16], *cvs_server_cmd;

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

		if (cvs_root->cr_user != NULL) {
			argv[argc++] = "-l";
			argv[argc++] = cvs_root->cr_user;
		}


		cvs_server_cmd = getenv("CVS_SERVER");
		if (cvs_server_cmd == NULL)
			cvs_server_cmd = "cvs";

		argv[argc++] = cvs_root->cr_host;
		argv[argc++] = cvs_server_cmd;
		argv[argc++] = "server";
		argv[argc] = NULL;

		execvp(argv[0], argv);
		cvs_log(LP_ERRNO, "failed to exec");
		exit(EX_OSERR);
	}

	/* we are the parent */
	cvs_server_infd = infd[1];
	cvs_server_outfd = outfd[0];

	cvs_server_in = fdopen(cvs_server_infd, "w");
	if (cvs_server_in == NULL) {
		cvs_log(LP_ERRNO, "failed to create pipe stream");
		return (-1);
	}

	cvs_server_out = fdopen(cvs_server_outfd, "r");
	if (cvs_server_out == NULL) {
		cvs_log(LP_ERRNO, "failed to create pipe stream");
		return (-1);
	}

	/* make the streams line-buffered */
	setvbuf(cvs_server_in, NULL, _IOLBF, 0);
	setvbuf(cvs_server_out, NULL, _IOLBF, 0);

	(void)close(infd[0]);
	(void)close(outfd[1]);

	cvs_client_initlog();

	cvs_client_sendinfo();

#ifdef CVS_ZLIB
	/* if compression was requested, initialize it */
#endif

	return (0);
}


/*
 * cvs_client_disconnect()
 *
 * Disconnect from the cvs server.
 */

void
cvs_client_disconnect(void)
{
	cvs_log(LP_DEBUG, "closing client connection");
	(void)fclose(cvs_server_in);
	(void)fclose(cvs_server_out);
	cvs_server_in = NULL;
	cvs_server_out = NULL;
	cvs_server_infd = -1;
	cvs_server_outfd = -1;

	if (cvs_server_inlog != NULL)
		fclose(cvs_server_inlog);
	if (cvs_server_outlog != NULL)
		fclose(cvs_server_outlog);
}


/*
 * cvs_client_sendreq()
 *
 * Send a request to the server of type <rid>, with optional arguments
 * contained in <arg>, which should not be terminated by a newline.
 * The <resp> argument is 0 if no response is expected, or any other value if
 * a response is expected.
 * Returns 0 on success, or -1 on failure.
 */

int
cvs_client_sendreq(u_int rid, const char *arg, int resp)
{
	int ret;
	size_t len;
	char *rbp;
	const char *reqp;

	if (cvs_server_in == NULL) {
		cvs_log(LP_ERR, "cannot send request: Not connected");
		return (-1);
	}

	reqp = cvs_req_getbyid(rid);
	if (reqp == NULL) {
		cvs_log(LP_ERR, "unsupported request type %u", rid);
		return (-1);
	}

	snprintf(cvs_client_buf, sizeof(cvs_client_buf), "%s %s\n", reqp,
	    (arg == NULL) ? "" : arg);

	rbp = cvs_client_buf;

	if (cvs_server_inlog != NULL)
		fputs(cvs_client_buf, cvs_server_inlog);

	ret = fputs(cvs_client_buf, cvs_server_in);
	if (ret == EOF) {
		cvs_log(LP_ERRNO, "failed to send request to server");
		return (-1);
	}

	if (resp) {
		do {
			/* wait for incoming data */
			if (fgets(cvs_client_buf, sizeof(cvs_client_buf),
			    cvs_server_out) == NULL) {
				if (feof(cvs_server_out))
					return (0);
				cvs_log(LP_ERRNO,
				    "failed to read response from server");
				return (-1);
			}

			if (cvs_server_outlog != NULL)
				fputs(cvs_client_buf, cvs_server_outlog);

			if ((len = strlen(cvs_client_buf)) != 0) {
				if (cvs_client_buf[len - 1] != '\n') {
					/* truncated line */
				}
				else
					cvs_client_buf[--len] = '\0';
			}

			ret = cvs_resp_handle(cvs_client_buf);
		} while (ret == 0);
	}

	return (0);
}


/*
 * cvs_client_sendln()
 *
 * Send a single line <line> string to the server.  The line is sent as is,
 * without any modifications.
 * Returns 0 on success, or -1 on failure.
 */

int
cvs_client_sendln(const char *line)
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
	fputs(line, cvs_server_in);
	if (nl)
		fputc('\n', cvs_server_in);

	return (0);
}


/*
 * cvs_client_sendraw()
 *
 * Send the first <len> bytes from the buffer <src> to the server.
 */

int
cvs_client_sendraw(const void *src, size_t len)
{
	if (cvs_server_inlog != NULL)
		fwrite(src, sizeof(char), len, cvs_server_inlog);
	if (fwrite(src, sizeof(char), len, cvs_server_in) < len) {
		return (-1);
	}

	return (0);
}


/*
 * cvs_client_recvraw()
 *
 * Receive the first <len> bytes from the buffer <src> to the server.
 */

ssize_t
cvs_client_recvraw(void *dst, size_t len)
{
	size_t ret;

	ret = fread(dst, sizeof(char), len, cvs_server_out);
	if (ret == 0)
		return (-1);
	if (cvs_server_outlog != NULL)
		fwrite(dst, sizeof(char), len, cvs_server_outlog);
	return (ssize_t)ret;
}


/*
 * cvs_client_getln()
 *
 * Get a line from the server's output and store it in <lbuf>.  The terminating
 * newline character is stripped from the result.
 */

int
cvs_client_getln(char *lbuf, size_t len)
{
	size_t rlen;

	if (fgets(lbuf, len, cvs_server_out) == NULL) {
		if (ferror(cvs_server_out)) {
			cvs_log(LP_ERRNO, "failed to read line from server");
			return (-1);
		}

		if (feof(cvs_server_out))
			*lbuf = '\0';
	}

	if (cvs_server_outlog != NULL)
		fputs(lbuf, cvs_server_outlog);

	rlen = strlen(lbuf);
	if ((rlen > 0) && (lbuf[rlen - 1] == '\n'))
		lbuf[--rlen] = '\0';

	return (0);
}


/*
 * cvs_client_sendinfo()
 *
 * Initialize the connection status by first requesting the list of
 * supported requests from the server.  Then, we send the CVSROOT variable
 * with the `Root' request.
 * Returns 0 on success, or -1 on failure.
 */

static int
cvs_client_sendinfo(void)
{
	char *vresp;
	/*
	 * First, send the server the list of valid responses, then ask
	 * for valid requests
	 */

	vresp = cvs_resp_getvalid();
	if (vresp == NULL) {
		cvs_log(LP_ERR, "can't generate list of valid responses");
		return (-1);
	}

	if (cvs_client_sendreq(CVS_REQ_VALIDRESP, vresp, 0) < 0) {
	}
	free(vresp);

	if (cvs_client_sendreq(CVS_REQ_VALIDREQ, NULL, 1) < 0) {
		cvs_log(LP_ERR, "failed to get valid requests from server");
		return (-1);
	}

	/* now share our global options with the server */
	if (verbosity == 1)
		cvs_client_sendreq(CVS_REQ_GLOBALOPT, "-q", 0);
	else if (verbosity == 0)
		cvs_client_sendreq(CVS_REQ_GLOBALOPT, "-Q", 0);

	if (cvs_nolog)
		cvs_client_sendreq(CVS_REQ_GLOBALOPT, "-l", 0);
	if (cvs_readonly)
		cvs_client_sendreq(CVS_REQ_GLOBALOPT, "-r", 0);
	if (cvs_trace)
		cvs_client_sendreq(CVS_REQ_GLOBALOPT, "-t", 0);

	/* now send the CVSROOT to the server */
	if (cvs_client_sendreq(CVS_REQ_ROOT, cvs_root->cr_dir, 0) < 0)
		return (-1);

	/* not sure why, but we have to send this */
	if (cvs_client_sendreq(CVS_REQ_USEUNCHANGED, NULL, 0) < 0)
		return (-1);

	return (0);
}


/*
 * cvs_client_senddir()
 *
 * Send a `Directory' request along with the 2 paths that follow it.
 */

int
cvs_client_senddir(const char *dir)
{
	char repo[MAXPATHLEN], buf[MAXPATHLEN];

	if (cvs_readrepo(dir, repo, sizeof(repo)) < 0) {
		repo[0] = '\0';
		strlcpy(buf, cvs_root->cr_dir, sizeof(buf));
	}
	else {
		snprintf(buf, sizeof(buf), "%s/%s", cvs_root->cr_dir, repo);
	}

	if ((cvs_client_sendreq(CVS_REQ_DIRECTORY, dir, 0) < 0) ||
	    (cvs_client_sendln(buf) < 0))
		return (-1);

	return (0);
}


/*
 * cvs_client_sendarg()
 *
 * Send the argument <arg> to the server.  The argument <append> is used to
 * determine if the argument should be simply appended to the last argument
 * sent or if it should be created as a new argument (0).
 */

int
cvs_client_sendarg(const char *arg, int append)
{
	return cvs_client_sendreq(((append == 0) ?
	    CVS_REQ_ARGUMENT : CVS_REQ_ARGUMENTX), arg, 0);
}


/*
 * cvs_client_sendentry()
 *
 * Send an `Entry' request to the server along with the mandatory fields from
 * the CVS entry <ent> (which are the name and revision).
 */

int
cvs_client_sendentry(const struct cvs_ent *ent)
{
	char ebuf[128], numbuf[64];

	snprintf(ebuf, sizeof(ebuf), "/%s/%s///", ent->ce_name,
	    rcsnum_tostr(ent->ce_rev, numbuf, sizeof(numbuf)));

	return cvs_client_sendreq(CVS_REQ_ENTRY, ebuf, 0);
}


/*
 * cvs_client_initlog()
 *
 * Initialize protocol logging if the CVS_CLIENT_LOG environment variable is
 * set.  In this case, the variable's value is used as a path to which the
 * appropriate suffix is added (".in" for server input and ".out" for server
 * output.
 * Returns 0 on success, or -1 on failure.
 */

static int
cvs_client_initlog(void)
{
	char *env, fpath[MAXPATHLEN];

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

	return (0);
}


/*
 * cvs_client_sendfile()
 *
 */

int
cvs_client_sendfile(const char *path)
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

	cvs_client_sendln(buf);
	snprintf(buf, sizeof(buf), "%lld\n", st.st_size);
	cvs_client_sendln(buf);

	while ((ret = read(fd, buf, sizeof(buf))) != 0) {
		if (ret == -1) {
			cvs_log(LP_ERRNO, "failed to read file `%s'", path);
			return (-1);
		}

		cvs_client_sendraw(buf, (size_t)ret);

	}

	(void)close(fd);

	return (0);
}
