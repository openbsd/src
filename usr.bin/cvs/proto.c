/*	$OpenBSD: proto.c,v 1.10 2004/07/29 18:34:55 jfb Exp $	*/
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


#define CVS_MTSTK_MAXDEPTH   16



extern int   verbosity;
extern int   cvs_compress;
extern char *cvs_rsh;
extern int   cvs_trace;
extern int   cvs_nolog;
extern int   cvs_readonly;



static int  cvs_resp_validreq  (int, char *);
static int  cvs_resp_cksum     (int, char *);
static int  cvs_resp_modtime   (int, char *);
static int  cvs_resp_m         (int, char *);
static int  cvs_resp_ok        (int, char *);
static int  cvs_resp_error     (int, char *);
static int  cvs_resp_statdir   (int, char *);
static int  cvs_resp_sticky    (int, char *);
static int  cvs_resp_newentry  (int, char *);
static int  cvs_resp_updated   (int, char *);
static int  cvs_resp_removed   (int, char *);
static int  cvs_resp_mode      (int, char *);
static int  cvs_resp_modxpand  (int, char *);


static const char *cvs_months[] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};



struct cvs_req {
	int      req_id;
	char     req_str[32];
	u_int    req_flags;
	int    (*req_hdlr)(int, char *);
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
	{ CVS_REQ_XPANDMOD,      "expand-modules",    0,  NULL },
	{ CVS_REQ_LOG,           "log",               0,  NULL },
	{ CVS_REQ_CO,            "co",                0,  NULL },
	{ CVS_REQ_EXPORT,        "export",            0,  NULL },
	{ CVS_REQ_RANNOTATE,     "rannotate",         0,  NULL },
	{ CVS_REQ_RDIFF,         "rdiff",             0,  NULL },
	{ CVS_REQ_RLOG,          "rlog",              0,  NULL },
	{ CVS_REQ_RTAG,          "rtag",              0,  NULL },
	{ CVS_REQ_INIT,          "init",              0,  NULL },
	{ CVS_REQ_UPDATE,        "update",            0,  NULL },
	{ CVS_REQ_HISTORY,       "history",           0,  NULL },
	{ CVS_REQ_IMPORT,        "import",            0,  NULL },
	{ CVS_REQ_ADD,           "add",               0,  NULL },
	{ CVS_REQ_REMOVE,        "remove",            0,  NULL },
	{ CVS_REQ_RELEASE,       "release",           0,  NULL },
	{ CVS_REQ_ROOT,          "Root",              0,  NULL },
	{ CVS_REQ_VALIDRESP,     "Valid-responses",   0,  NULL },
	{ CVS_REQ_VALIDREQ,      "valid-requests",    0,  NULL },
	{ CVS_REQ_VERSION,       "version",           0,  NULL },
	{ CVS_REQ_NOOP,          "noop",              0,  NULL },
	{ CVS_REQ_DIFF,          "diff",              0,  NULL },
};


struct cvs_resp {
	u_int  resp_id;
	char   resp_str[32];
	int  (*resp_hdlr)(int, char *);
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


/*
 * cvs_req_getbyid()
 *
 */

const char*
cvs_req_getbyid(int reqid)
{
	u_int i;

	for (i = 0; i < CVS_NBREQ; i++)
		if (cvs_requests[i].req_id == reqid)
			return (cvs_requests[i].req_str);
	return (NULL);
}


/*
 * cvs_req_getbyname()
 */

int
cvs_req_getbyname(const char *rname)
{
	u_int i;

	for (i = 0; i < CVS_NBREQ; i++)
		if (strcmp(cvs_requests[i].req_str, rname) == 0)
			return (cvs_requests[i].req_id);

	return (-1);
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

const char*
cvs_resp_getbyid(int respid)
{
	u_int i;

	for (i = 0; i < CVS_NBREQ; i++)
		if (cvs_responses[i].resp_id == respid)
			return (cvs_responses[i].resp_str);
	return (NULL);
}


/*
 * cvs_resp_getbyname()
 */

int
cvs_resp_getbyname(const char *rname)
{
	u_int i;

	for (i = 0; i < CVS_NBREQ; i++)
		if (strcmp(cvs_responses[i].resp_str, rname) == 0)
			return (cvs_responses[i].resp_id);

	return (-1);
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
cvs_resp_handle(char *line)
{
	u_int i;
	size_t len;
	char *cp, *cmd;

	cmd = line;

	cp = strchr(cmd, ' ');
	if (cp != NULL)
		*(cp++) = '\0';

	for (i = 0; i < CVS_NBRESP; i++) {
		if (strcmp(cvs_responses[i].resp_str, cmd) == 0)
			return (*cvs_responses[i].resp_hdlr)
			    (cvs_responses[i].resp_id, cp);
	}

	/* unhandled */
	return (-1);
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
	/* first things first, get list of valid requests from server */
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
 * cvs_resp_validreq()
 *
 * Handler for the `Valid-requests' response.  The list of valid requests is
 * split on spaces and each request's entry in the valid request array is set
 * to 1 to indicate the validity.
 * Returns 0 on success, or -1 on failure.
 */

static int
cvs_resp_validreq(int type, char *line)
{
	int i;
	char *sp, *ep;

	/* parse the requests */
	sp = line;
	do {
		ep = strchr(sp, ' ');
		if (ep != NULL)
			*ep = '\0';

		i = cvs_req_getbyname(sp);
		if (i != -1)
			cvs_server_validreq[i] = 1;

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
cvs_resp_m(int type, char *line)
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
cvs_resp_ok(int type, char *line)
{
	return (1);
}


/*
 * cvs_resp_error()
 *
 * Handler for the `error' response.  This handler's job is to 
 */

static int
cvs_resp_error(int type, char *line)
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
cvs_resp_statdir(int type, char *line)
{
	int fd;
	char rpath[MAXPATHLEN], statpath[MAXPATHLEN];

	cvs_client_getln(rpath, sizeof(rpath));

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
cvs_resp_sticky(int type, char *line)
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
	cvs_client_getln(rpath, sizeof(rpath));

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
cvs_resp_newentry(int type, char *line)
{
	char entbuf[128], path[MAXPATHLEN];
	CVSENTRIES *entfile;

	snprintf(path, sizeof(path), "%s/" CVS_PATH_ENTRIES, line);

	/* get the remote path */
	cvs_client_getln(entbuf, sizeof(entbuf));

	/* get the new Entries line */
	if (cvs_client_getln(entbuf, sizeof(entbuf)) < 0)
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
cvs_resp_cksum(int type, char *line)
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
cvs_resp_modtime(int type, char *line)
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
cvs_resp_updated(int type, char *line)
{
	size_t len;
	char tbuf[32], path[MAXPATHLEN], cksum_buf[CVS_CKSUM_LEN];
	CVSENTRIES *ef;
	struct cvs_ent *ep;

	if (type == CVS_RESP_CREATED) {
		/* read the remote path of the file */
		cvs_client_getln(path, sizeof(path));

		/* read the new entry */
		cvs_client_getln(path, sizeof(path));
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
	if (cvs_recvfile(path) < 0) {
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
cvs_resp_removed(int type, char *line)
{
	return (0);
}


/*
 * cvs_resp_mode()
 *
 * Handler for the `Mode' response.
 */

static int
cvs_resp_mode(int type, char *line)
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
cvs_resp_modxpand(int type, char *line)
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
cvs_sendfile(const char *path)
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


/*
 * cvs_recvfile()
 *
 * Receive the mode and size of a file followed the file's contents and
 * create or update the file whose path is <path> with the received
 * information.
 */

int
cvs_recvfile(const char *path)
{
	int fd;
	mode_t mode;
	size_t len;
	ssize_t ret;
	off_t fsz, cnt;
	char buf[4096], *ep;

	if ((cvs_client_getln(buf, sizeof(buf)) < 0) ||
	    (cvs_strtomode(buf, &mode) < 0)) {
		return (-1);
	}

	cvs_client_getln(buf, sizeof(buf));

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
		ret = cvs_client_recvraw(buf, len);
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
