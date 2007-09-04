/*	$OpenBSD: logmsg.c,v 1.43 2007/09/04 11:47:14 tobias Exp $	*/
/*
 * Copyright (c) 2007 Joris Vink <joris@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include "cvs.h"

#define CVS_LOGMSG_PREFIX		"CVS:"
#define CVS_LOGMSG_LINE		\
"----------------------------------------------------------------------"

int	cvs_logmsg_edit(const char *);

char *
cvs_logmsg_read(const char *path)
{
	int fd;
	BUF *bp;
	FILE *fp;
	size_t len;
	struct stat st;
	char *buf, *lbuf;

	if ((fd = open(path, O_RDONLY)) == -1)
		fatal("cvs_logmsg_read: open %s", strerror(errno));

	if (fstat(fd, &st) == -1)
		fatal("cvs_logmsg_read: fstat %s", strerror(errno));

	if (!S_ISREG(st.st_mode))
		fatal("cvs_logmsg_read: file is not a regular file");

	if ((fp = fdopen(fd, "r")) == NULL)
		fatal("cvs_logmsg_read: fdopen %s", strerror(errno));

	lbuf = NULL;
	bp = cvs_buf_alloc(st.st_size, BUF_AUTOEXT);
	while ((buf = fgetln(fp, &len))) {
		if (buf[len - 1] == '\n') {
			buf[len - 1] = '\0';
		} else {
			lbuf = xmalloc(len + 1);
			memcpy(lbuf, buf, len);
			lbuf[len] = '\0';
			buf = lbuf;
		}

		len = strlen(buf);

		if (!strncmp(buf, CVS_LOGMSG_PREFIX,
		    strlen(CVS_LOGMSG_PREFIX)))
			continue;

		cvs_buf_append(bp, buf, len);
		cvs_buf_putc(bp, '\n');
	}

	if (lbuf != NULL)
		xfree(lbuf);

	(void)fclose(fp);

	cvs_buf_putc(bp, '\0');
	return (cvs_buf_release(bp));
}

char *
cvs_logmsg_create(struct cvs_flisthead *added, struct cvs_flisthead *removed,
	struct cvs_flisthead *modified)
{
	FILE *fp;
	int c, fd, saved_errno;
	struct cvs_filelist *cf;
	struct stat st1, st2;
	char *fpath, *logmsg;

	(void)xasprintf(&fpath, "%s/cvsXXXXXXXXXX", cvs_tmpdir);

	if ((fd = mkstemp(fpath)) == NULL)
		fatal("cvs_logmsg_create: mkstemp %s", strerror(errno));

	cvs_worklist_add(fpath, &temp_files);

	if ((fp = fdopen(fd, "w")) == NULL) {
		saved_errno = errno;
		(void)unlink(fpath);
		fatal("cvs_logmsg_create: fdopen %s", strerror(saved_errno));
	}

	fprintf(fp, "\n%s %s\n%s Enter Log.  Lines beginning with `%s' are "
	    "removed automatically\n%s\n", CVS_LOGMSG_PREFIX, CVS_LOGMSG_LINE,
	    CVS_LOGMSG_PREFIX, CVS_LOGMSG_PREFIX, CVS_LOGMSG_PREFIX);

	if (added != NULL && !TAILQ_EMPTY(added)) {
		fprintf(fp, "%s Added Files:", CVS_LOGMSG_PREFIX);
		TAILQ_FOREACH(cf, added, flist)
			fprintf(fp, "\n%s\t%s",
			    CVS_LOGMSG_PREFIX, cf->file_path);
		fputs("\n", fp);
	}

	if (removed != NULL && !TAILQ_EMPTY(removed)) {
		fprintf(fp, "%s Removed Files:", CVS_LOGMSG_PREFIX);
		TAILQ_FOREACH(cf, removed, flist)
			fprintf(fp, "\n%s\t%s",
			    CVS_LOGMSG_PREFIX, cf->file_path);
		fputs("\n", fp);
	}

	if (modified != NULL && !TAILQ_EMPTY(modified)) {
		fprintf(fp, "%s Modified Files:", CVS_LOGMSG_PREFIX);
		TAILQ_FOREACH(cf, modified, flist)
			fprintf(fp, "\n%s\t%s",
			    CVS_LOGMSG_PREFIX, cf->file_path);
		fputs("\n", fp);
	}

	fprintf(fp, "%s %s\n", CVS_LOGMSG_PREFIX, CVS_LOGMSG_LINE);
	(void)fflush(fp);

	if (fstat(fd, &st1) == -1) {
		saved_errno = errno;
		(void)unlink(fpath);
		fatal("cvs_logmsg_create: fstat %s", strerror(saved_errno));
	}

	logmsg = NULL;

	for (;;) {
		if (cvs_logmsg_edit(fpath) == -1)
			break;

		if (fstat(fd, &st2) == -1) {
			saved_errno = errno;
			(void)unlink(fpath);
			fatal("cvs_logmsg_create: fstat %s",
			    strerror(saved_errno));
		}

		if (st1.st_mtime != st2.st_mtime) {
			logmsg = cvs_logmsg_read(fpath);
			break;
		}

		printf("\nLog message unchanged or not specified\n"
		    "a)bort, c)ontinue, e)dit\nAction: (continue) ");
		(void)fflush(stdout);

		c = getc(stdin);
		if (c == EOF || c == 'a') {
			fatal("Aborted by user");
		} else if (c == '\n' || c == 'c') {
			logmsg = xstrdup("");
			break;
		} else if (c == 'e') {
			continue;
		} else {
			cvs_log(LP_ERR, "invalid input");
			continue;
		}
	}

	(void)fclose(fp);
	(void)unlink(fpath);
	xfree(fpath);

	return (logmsg);
}

/*
 * Execute an editor on the specified pathname, which is interpreted
 * from the shell.  This means flags may be included.
 *
 * Returns -1 on error, or the exit value on success.
 */
int
cvs_logmsg_edit(const char *pathname)
{
	char *argp[] = {"sh", "-c", NULL, NULL}, *p;
	sig_t sighup, sigint, sigquit;
	pid_t pid;
	int saved_errno, st;

	(void)xasprintf(&p, "%s %s", cvs_editor, pathname);
	argp[2] = p;

	sighup = signal(SIGHUP, SIG_IGN);
	sigint = signal(SIGINT, SIG_IGN);
	sigquit = signal(SIGQUIT, SIG_IGN);
	if ((pid = fork()) == -1)
		goto fail;
	if (pid == 0) {
		execv(_PATH_BSHELL, argp);
		_exit(127);
	}
	while (waitpid(pid, &st, 0) == -1)
		if (errno != EINTR)
			goto fail;
	xfree(p);
	(void)signal(SIGHUP, sighup);
	(void)signal(SIGINT, sigint);
	(void)signal(SIGQUIT, sigquit);
	if (!WIFEXITED(st)) {
		errno = EINTR;
		return (-1);
	}
	return (WEXITSTATUS(st));

 fail:
	saved_errno = errno;
	(void)signal(SIGHUP, sighup);
	(void)signal(SIGINT, sigint);
	(void)signal(SIGQUIT, sigquit);
	xfree(p);
	errno = saved_errno;
	return (-1);
}
