/*	$OpenBSD: resp.c,v 1.63 2005/12/03 01:02:09 joris Exp $	*/
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
#include <sys/time.h>

#include <dirent.h>
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


#define CVS_MTSTK_MAXDEPTH	16


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
static int  cvs_resp_rcsdiff   (struct cvsroot *, int, char *);
static int  cvs_resp_template  (struct cvsroot *, int, char *);
static int  cvs_resp_copyfile  (struct cvsroot *, int, char *);
static int  cvs_resp_createdir (char *);

struct cvs_resphdlr {
	int	(*hdlr)(struct cvsroot *, int, char *);
} cvs_resp_swtab[CVS_RESP_MAX + 1] = {
	{ NULL              },
	{ cvs_resp_ok       },
	{ cvs_resp_error    },
	{ cvs_resp_validreq },
	{ cvs_resp_newentry },
	{ cvs_resp_newentry },
	{ cvs_resp_cksum    },
	{ cvs_resp_copyfile },
	{ cvs_resp_updated  },
	{ cvs_resp_updated  },
	{ cvs_resp_updated  },	/* 10 */
	{ cvs_resp_updated  },
	{ cvs_resp_updated  },
	{ cvs_resp_rcsdiff  },
	{ cvs_resp_mode     },
	{ cvs_resp_modtime  },
	{ cvs_resp_removed  },
	{ cvs_resp_removed  },
	{ cvs_resp_statdir  },
	{ cvs_resp_statdir  },
	{ cvs_resp_sticky   },	/* 20 */
	{ cvs_resp_sticky   },
	{ cvs_resp_template },
	{ NULL              },
	{ NULL              },
	{ NULL              },
	{ cvs_resp_modxpand },
	{ NULL              },
	{ cvs_resp_m        },
	{ cvs_resp_m        },
	{ cvs_resp_m        },	/* 30 */
	{ cvs_resp_m        },
	{ cvs_resp_m        },
};

/*
 * Instead of opening and closing the Entry file all the time,
 * which caused a huge CPU load and slowed down everything,
 * we keep the Entry file for the directory we are working in
 * open until we encounter a new directory.
 */
static char cvs_resp_lastdir[MAXPATHLEN] = "";
static CVSENTRIES *cvs_resp_lastent = NULL;
static int resp_check_dir(struct cvsroot *, const char *);

/*
 * The MT command uses scoping to tag the data.  Whenever we encouter a '+',
 * we push the name of the tag on the stack, and we pop it when we encounter
 * a '-' with the same name.
 */

static char *cvs_mt_stack[CVS_MTSTK_MAXDEPTH];
static u_int cvs_mtstk_depth = 0;

static time_t cvs_modtime = CVS_DATE_DMSEC;


/* last checksum received */
char *cvs_fcksum = NULL;

mode_t  cvs_lastmode = 0;

/* hack to receive the remote version without outputting it */
extern u_int cvs_version_sent;


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
	int ret;
	char *cp, *cmd;
	struct cvs_resp *resp;

	cmd = line;

	cp = strchr(cmd, ' ');
	if (cp != NULL)
		*(cp++) = '\0';

	resp = cvs_resp_getbyname(cmd);
	if (resp == NULL) {
		return (-1);
	} else if (cvs_resp_swtab[resp->resp_id].hdlr == NULL) {
		cvs_log(LP_ERR, "handler for `%s' not implemented", cmd);
		return (-1);
	}

	ret = (*cvs_resp_swtab[resp->resp_id].hdlr)(root, resp->resp_id, cp);

	if (ret == -1)
		cvs_log(LP_ERR, "error in handling of `%s' response", cmd);

	return (ret);
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
			CVS_SETVR(root, req->req_id);

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
		if (cvs_version_sent) {
			/*
			 * Instead of outputting the line, we save it as the
			 * remote server's version string.
			 */
			cvs_version_sent = 0;
			root->cr_version = strdup(line);
			return (0);
		}
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
		} else if (*line == '-') {
			if (cvs_mtstk_depth == 0) {
				cvs_log(LP_ERR, "MT scope stack underflow");
				return (-1);
			} else if (strcmp(line + 1,
			    cvs_mt_stack[cvs_mtstk_depth - 1]) != 0) {
				cvs_log(LP_ERR, "mismatch in MT scope stack");
				return (-1);
			}
			free(cvs_mt_stack[cvs_mtstk_depth--]);
		} else {
			if (strcmp(line, "newline") == 0)
				putc('\n', stdout);
			else if (strncmp(line, "fname ", (size_t)6) == 0)
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
	/*
	 * If we still have an Entry file open, close it now.
	 */
	if (cvs_resp_lastent != NULL)
		cvs_ent_close(cvs_resp_lastent);

	return (1);
}


/*
 * cvs_resp_error()
 *
 * Handler for the `error' response.  This handler's job is to
 * show the error message given by the server.
 */
static int
cvs_resp_error(struct cvsroot *root, int type, char *line)
{
	if (line == NULL)
		return (1);

	/* XXX - GNU cvs sends an empty error message
	 * at the end of the diff command, even for successfull
	 * diff.
	 */
	if ((strlen(line) == 1) && (*line == ' '))
		return (1);

	fprintf(stderr, "%s\n", line);
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
	int fd, len;
	char rpath[MAXPATHLEN], statpath[MAXPATHLEN];

	/* remote directory line */
	if (cvs_getln(root, rpath, sizeof(rpath)) < 0)
		return (-1);

	STRIP_SLASH(line);

	/*
	 * Create the directory if it does not exist.
	 */
	if (cvs_resp_createdir(line) < 0)
		return (-1);

	len = snprintf(statpath, sizeof(statpath), "%s/%s", line,
	    CVS_PATH_STATICENTRIES);
	if (len == -1 || len >= (int)sizeof(statpath)) {
		cvs_log(LP_ERR,
		    "path overflow for Entries.static specification");
		return (-1);
	}

	if (cvs_noexec == 0) {
		if ((type == CVS_RESP_CLRSTATDIR) &&
		    (cvs_unlink(statpath) == -1)) {
			return (-1);
		} else if (type == CVS_RESP_SETSTATDIR) {
			fd = open(statpath, O_CREAT|O_TRUNC|O_WRONLY, 0644);
			if (fd == -1) {
				cvs_log(LP_ERRNO,
				    "failed to set static directory on %s",
				    line);
				return (-1);
			}
			(void)close(fd);

		}
	}

	return (0);
}

/*
 * cvs_resp_sticky()
 *
 * Handler for the `Clear-sticky' and `Set-sticky' responses.  If the
 * specified directory doesn't exist, we create it.
 */
static int
cvs_resp_sticky(struct cvsroot *root, int type, char *line)
{
	char buf[MAXPATHLEN];

	/* get the remote path */
	if (cvs_getln(root, buf, sizeof(buf)) < 0)
		return (-1);

	STRIP_SLASH(line);

	if (cvs_resp_createdir(line) < 0)
		return (-1);

	return (0);
}

/*
 * Shared code for cvs_resp[static, sticky]
 *
 * Looks if the directory requested exists, if it doesn't it will
 * create it plus all administrative files as well.
 */
static int
cvs_resp_createdir(char *line)
{
	int l;
	CVSFILE *base, *cf;
	CVSENTRIES *entf;
	struct stat st;
	struct cvs_ent *ent;
	char *file, subdir[MAXPATHLEN], buf[CVS_ENT_MAXLINELEN];

	entf = NULL;
	cf = NULL;

	/*
	 * we do not want to handle the '.' case,
	 * so return early.
	 */
	if (!strcmp(line, "."))
		return (0);

	cvs_splitpath(line, subdir, sizeof(subdir), &file);
	base = cvs_file_loadinfo(subdir, CF_NOFILES, NULL, NULL, 1);
	if (base == NULL)
		return (-1);

	/*
	 * If <line> doesn't exist, we create it.
	 */
	if (stat(line, &st) == -1) {
		if (errno != ENOENT) {
			cvs_log(LP_ERRNO, "failed to stat `%s'", line);
			return (-1);
		}

		cf = cvs_file_create(base, line, DT_DIR, 0755);
	} else {
		cf = cvs_file_loadinfo(line, CF_NOFILES, NULL, NULL, 1);
	}

	if (cf == NULL) {
		cvs_file_free(base);
		return (-1);
	}

	/*
	 * If the Entries file for the parent is already
	 * open, operate on that, instead of reopening it
	 * and invalidating the opened list.
	 */
	if (!strcmp(subdir, cvs_resp_lastdir))
		entf = cvs_resp_lastent;
	else
		entf = cvs_ent_open(subdir, O_WRONLY);

	/*
	 * see if the entry is still present. If not, we add it again.
	 */
	if (entf != NULL) {
		if ((ent = cvs_ent_get(entf, cf->cf_name)) == NULL) {
			l = snprintf(buf, sizeof(buf), "D/%s////", cf->cf_name);
			if (l == -1 || l >= (int)sizeof(buf)) {
				cvs_file_free(cf);
				cvs_file_free(base);
				return (-1);
			}

			ent = cvs_ent_parse(buf);
			if (ent == NULL)
				cvs_log(LP_ERR,
				    "failed to create directory entry");
			else
				cvs_ent_add(entf, ent);
		}

		if (strcmp(subdir, cvs_resp_lastdir))
			cvs_ent_close(entf);
	}

	cvs_file_free(cf);
	cvs_file_free(base);
	return (0);
}


/*
 * cvs_resp_newentry()
 *
 * Handler for the `New-entry' response and `Checked-in' responses.
 * In the case of `New-entry', we expect the entry line
 */
static int
cvs_resp_newentry(struct cvsroot *root, int type, char *line)
{
	char entbuf[CVS_ENT_MAXLINELEN];
	struct cvs_ent *ent;

	/* get the remote path */
	if (cvs_getln(root, entbuf, sizeof(entbuf)) < 0)
		return (-1);

	/* get the new Entries line */
	if (cvs_getln(root, entbuf, sizeof(entbuf)) < 0)
		return (-1);

	if (resp_check_dir(root, line) < 0)
		return (-1);

	if (type == CVS_RESP_NEWENTRY) {
		cvs_ent_addln(cvs_resp_lastent, entbuf);
	} else if (type == CVS_RESP_CHECKEDIN) {
		ent = cvs_ent_parse(entbuf);
		if (ent == NULL) {
			cvs_log(LP_ERR, "failed to parse entry");
			return (-1);
		}

		/* timestamp it to now */
		ent->ce_mtime = time(&(ent->ce_mtime));

		/* replace the current entry with the one we just received */
		(void)cvs_ent_remove(cvs_resp_lastent, ent->ce_name, 0);

		cvs_ent_add(cvs_resp_lastent, ent);
	}

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
 * cvs_resp_copyfile()
 *
 * Handler for the `Copy-file' response, which is used to copy the contents
 * of a file to another file for which the name is provided.  The CVS protocol
 * documentation states that this response is only used prior to a `Merged'
 * response to create a backup of the file.
 */
static int
cvs_resp_copyfile(struct cvsroot *root, int type, char *line)
{
	int len;
	char path[MAXPATHLEN], newpath[MAXPATHLEN], newname[MAXNAMLEN], *file;

	/* read the remote path of the file to copy and its new name */
	if ((cvs_getln(root, path, sizeof(path)) < 0) ||
	    (cvs_getln(root, newname, sizeof(newname)) < 0))
		return (-1);

	if ((file = basename(path)) == NULL) {
		cvs_log(LP_ERR, "no base file name in Copy-file path");
		return (-1);
	}

	len = snprintf(path, sizeof(path), "%s%s", line, file);
	if (len == -1 || len >= (int)sizeof(path)) {
		cvs_log(LP_ERR, "source path overflow in Copy-file response");
		return (-1);
	}
	len = snprintf(newpath, sizeof(newpath), "%s%s", line, newname);
	if (len == -1 || len >= (int)sizeof(path)) {
		cvs_log(LP_ERR,
		    "destination path overflow in Copy-file response");
		return (-1);
	}

	if (rename(path, newpath) == -1) {
		cvs_log(LP_ERRNO, "failed to rename %s to %s", path, newpath);
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
	cvs_modtime = cvs_date_parse(line);
	return (0);
}


/*
 * cvs_resp_updated()
 *
 * Handler for the `Updated', `Update-existing', `Created', `Merged' and
 * `Patched' responses, which all have a very similar format.
 */
static int
cvs_resp_updated(struct cvsroot *root, int type, char *line)
{
	int ret;
	mode_t fmode;
	char path[MAXPATHLEN], cksum_buf[CVS_CKSUM_LEN];
	BUF *fbuf;
	struct cvs_ent *ent;
	struct timeval tv[2];

	STRIP_SLASH(line);

	/* read the remote path of the file */
	if (cvs_getln(root, path, sizeof(path)) < 0)
		return (-1);

	/* read the new entry */
	if (cvs_getln(root, path, sizeof(path)) < 0)
		return (-1);

	if ((ent = cvs_ent_parse(path)) == NULL)
		return (-1);
	ret = snprintf(path, sizeof(path), "%s/%s", line, ent->ce_name);
	if (ret == -1 || ret >= (int)sizeof(path)) {
		cvs_log(LP_ERR, "Entries path overflow in response");
		return (-1);
	}
	ret = 0;

	/*
	 * Please be sure the directory does exist.
	 */
	if (cvs_resp_createdir(line) < 0)
		return (-1);

	if (resp_check_dir(root, line) < 0)
		return (-1);

	if (cvs_modtime != CVS_DATE_DMSEC) {
		ent->ce_mtime = cvs_modtime;
	} else
		ent->ce_mtime = time(&(ent->ce_mtime));

	if ((type == CVS_RESP_UPDEXIST) || (type == CVS_RESP_UPDATED) ||
	    (type == CVS_RESP_MERGED) || (type == CVS_RESP_CREATED)) {
		if ((cvs_ent_remove(cvs_resp_lastent, ent->ce_name, 0) < 0) &&
		    (type != CVS_RESP_CREATED)) {
			cvs_log(LP_WARN, "failed to remove entry for '%s`",
			    ent->ce_name);
		}
	}

	if (cvs_ent_add(cvs_resp_lastent, ent) < 0) {
		cvs_ent_free(ent);
		return (-1);
	}

	if ((fbuf = cvs_recvfile(root, &fmode)) == NULL)
		return (-1);
	if (cvs_buf_write(fbuf, path, fmode) < 0) {
		cvs_buf_free(fbuf);
		return (-1);
	}
	cvs_buf_free(fbuf);

	if (cvs_modtime != CVS_DATE_DMSEC) {
		tv[0].tv_sec = (long)cvs_modtime;
		tv[0].tv_usec = 0;
		tv[1].tv_sec = (long)cvs_modtime;
		tv[1].tv_usec = 0;
		if (utimes(path, tv) == -1)
			cvs_log(LP_ERRNO, "failed to set file timestamps");
	}

	/* invalidate last received timestamp */
	cvs_modtime = CVS_DATE_DMSEC;

	/* now see if there is a checksum */
	if (cvs_fcksum != NULL) {
		if (cvs_cksum(path, cksum_buf, sizeof(cksum_buf)) < 0)
			ret = -1;
		else if (strcmp(cksum_buf, cvs_fcksum) != 0) {
			cvs_log(LP_ERR, "checksum error on received file");
			(void)unlink(line);
			ret = -1;
		}

		free(cvs_fcksum);
		cvs_fcksum = NULL;
	}

	return (ret);
}


/*
 * cvs_resp_removed()
 *
 * Handler for the `Removed' and `Remove-entry' responses.  The `Removed'
 * response is received when both a file and its entry need to be removed from
 * the local copy.  The `Remove-entry' is received in cases where the file is
 * already gone but there is still an entry to remove in the Entries file.
 */
static int
cvs_resp_removed(struct cvsroot *root, int type, char *line)
{
	int l;
	char buf[MAXPATHLEN], base[MAXPATHLEN], fpath[MAXPATHLEN], *file;

	if (cvs_getln(root, buf, sizeof(buf)) < 0)
		return (-1);

	cvs_splitpath(buf, base, sizeof(base), &file);
	l = snprintf(fpath, sizeof(fpath), "%s/%s", line, file);
	if (l == -1 || l >= (int)sizeof(fpath)) {
		errno = ENAMETOOLONG;
		cvs_log(LP_ERRNO, "%s", fpath);
		return (-1);
	}

	if (resp_check_dir(root, line) < 0)
		return (-1);

	(void)cvs_ent_remove(cvs_resp_lastent, file, 0);
	if ((type == CVS_RESP_REMOVED) && ((unlink(fpath) == -1) &&
	    errno != ENOENT)) {
		cvs_log(LP_ERRNO, "failed to unlink `%s'", file);
		return (-1);
	}

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
		cvs_log(LP_ERR, "error handling Mode response");
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
 * cvs_resp_rcsdiff()
 *
 * Handler for the `Rcs-diff' response.
 */
static int
cvs_resp_rcsdiff(struct cvsroot *root, int type, char *line)
{
	int len;
	char file[MAXPATHLEN];
	char buf[CVS_ENT_MAXLINELEN], cksum_buf[CVS_CKSUM_LEN];
	char *fname, *orig, *patch;
	mode_t fmode;
	BUF *res, *fcont, *patchbuf;
	CVSENTRIES *entf;
	struct cvs_ent *ent;

	/* get remote path and build local path of file to be patched */
	if (cvs_getln(root, buf, sizeof(buf)) < 0)
		return (-1);

	fname = strrchr(buf, '/');
	if (fname == NULL)
		fname = buf;
	len = snprintf(file, sizeof(file), "%s%s", line, fname);
	if (len == -1 || len >= (int)sizeof(file)) {
		cvs_log(LP_ERR, "path overflow in Rcs-diff response");
		return (-1);
	}

	/* get updated entry fields */
	if (cvs_getln(root, buf, sizeof(buf)) < 0)
		return (-1);

	ent = cvs_ent_parse(buf);
	if (ent == NULL)
		return (-1);

	patchbuf = cvs_recvfile(root, &fmode);
	fcont = cvs_buf_load(file, BUF_AUTOEXT);
	if (fcont == NULL)
		return (-1);

	cvs_buf_putc(patchbuf, '\0');
	cvs_buf_putc(fcont, '\0');
	orig = cvs_buf_release(fcont);
	patch = cvs_buf_release(patchbuf);

	res = cvs_patchfile(orig, patch, rcs_patch_lines);
	if (res == NULL)
		return (-1);

	cvs_buf_write(res, file, fmode);

	/* now see if there is a checksum */
	if (cvs_fcksum != NULL) {
		if (cvs_cksum(file, cksum_buf, sizeof(cksum_buf)) < 0) {
		}

		if (strcmp(cksum_buf, cvs_fcksum) != 0) {
			cvs_log(LP_ERR, "checksum error on received file");
			(void)unlink(file);
		}

		free(cvs_fcksum);
		cvs_fcksum = NULL;
	}

	/* update revision in entries */
	entf = cvs_ent_open(line, O_WRONLY);
	if (entf == NULL)
		return (-1);

	cvs_ent_close(entf);

	return (0);
}


/*
 * cvs_resp_template()
 *
 * Handler for the `Template' response.
 */
static int
cvs_resp_template(struct cvsroot *root, int type, char *line)
{
	mode_t mode;
	BUF *tmpl;

	tmpl = cvs_recvfile(root, &mode);
	if (tmpl == NULL)
		return (-1);

	return (0);
}

/*
 * Check if <dir> is the same as the last
 * received directory, if it's not, switch Entry files.
 */
static int
resp_check_dir(struct cvsroot *root, const char *dir)
{
	int l;
	size_t len;
	char cvspath[MAXPATHLEN], repo[MAXPATHLEN];
	struct stat st;

	/*
	 * Make sure the CVS directory exists.
	 */
	l = snprintf(cvspath, sizeof(cvspath), "%s/%s", dir, CVS_PATH_CVSDIR);
	if (l == -1 || l >= (int)sizeof(cvspath))
		return (-1);

	if (stat(cvspath, &st) == -1) {
		if (errno != ENOENT)
			return (-1);
		if  (cvs_repo_base != NULL) {
			l = snprintf(repo, sizeof(repo), "%s/%s", cvs_repo_base,
			    dir);
			if (l == -1 || l >= (int)sizeof(repo))
				return (-1);
		} else {
			strlcpy(repo, dir, sizeof(repo));
		}

		if (cvs_mkadmin(dir, root->cr_str, repo, NULL, NULL, 0) < 0)
			return (-1);
	}

	if (strcmp(dir, cvs_resp_lastdir)) {
		if (cvs_resp_lastent != NULL)
			cvs_ent_close(cvs_resp_lastent);
		cvs_resp_lastent = cvs_ent_open(dir, O_WRONLY);
		if (cvs_resp_lastent == NULL)
			return (-1);

		len = strlcpy(cvs_resp_lastdir, dir, sizeof(cvs_resp_lastdir));
		if (len >= sizeof(cvs_resp_lastdir)) {
			errno = ENAMETOOLONG;
			cvs_log(LP_ERRNO, "%s", cvs_resp_lastdir);
			return (-1);
		}
	} else {
		/* make sure the old one is still open */
		if (cvs_resp_lastent == NULL) {
			cvs_resp_lastent = cvs_ent_open(cvs_resp_lastdir,
			    O_WRONLY);
			if (cvs_resp_lastent == NULL)
				return (-1);
		}
	}

	return (0);
}
