/*	$OpenBSD: entries.c,v 1.52 2005/12/03 15:02:55 joris Exp $	*/
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

#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cvs.h"
#include "log.h"


#define CVS_ENTRIES_NFIELDS	6
#define CVS_ENTRIES_DELIM	'/'


/*
 * cvs_ent_open()
 *
 * Open the CVS Entries file for the directory <dir>.
 * Returns a pointer to the CVSENTRIES file structure on success, or NULL
 * on failure.
 */
CVSENTRIES *
cvs_ent_open(const char *dir, int flags)
{
	size_t len;
	int exists, nodir;
	char bpath[MAXPATHLEN], *p;
	char cdpath[MAXPATHLEN], ebuf[CVS_ENT_MAXLINELEN], entpath[MAXPATHLEN];
	char mode[4];
	FILE *fp;
	struct stat st;
	struct cvs_ent *ent;
	CVSENTRIES *ep;

	exists = 0;
	nodir = 1;
	memset(mode, 0, sizeof(mode));

	/*
	 * Check if the CVS/ dir does exist. If it does,
	 * maybe the Entries file was deleted by accident,
	 * display error message. Else we might be doing a fresh
	 * update or checkout of a module.
	 */
	len = cvs_path_cat(dir, CVS_PATH_CVSDIR, cdpath, sizeof(cdpath));
	if (len >= sizeof(cdpath))
		return (NULL);

	if ((stat(cdpath, &st) == 0) && S_ISDIR(st.st_mode))
		nodir = 0;	/* the CVS/ directory does exist */

	len = cvs_path_cat(dir, CVS_PATH_BACKUPENTRIES, bpath, sizeof(bpath));
	if (len >= sizeof(entpath))
		return (NULL);

	len = cvs_path_cat(dir, CVS_PATH_ENTRIES, entpath, sizeof(entpath));
	if (len >= sizeof(entpath))
		return (NULL);

	switch (flags & O_ACCMODE) {
	case O_WRONLY:
	case O_RDWR:
		/* we have to use append otherwise the file gets truncated */
		mode[0] = 'w';
		mode[1] = '+';
		break;
	case O_RDONLY:
		mode[0] = 'r';
		break;
	}

	/* we can use 'r' if the file already exists */
	if (stat(entpath, &st) == 0) {
		exists = 1;
		mode[0] = 'r';
	}

	fp = fopen(entpath, mode);
	if (fp == NULL) {
		if (nodir == 0)
			cvs_log(LP_ERRNO, "cannot open %s for %s", entpath,
			    mode[1] == '+' ? "writing" : "reading");
		return (NULL);
	}

	ep = (CVSENTRIES *)malloc(sizeof(CVSENTRIES));
	if (ep == NULL) {
		cvs_log(LP_ERRNO, "failed to allocate Entries data");
		(void)fclose(fp);
		return (NULL);
	}
	memset(ep, 0, sizeof(*ep));

	ep->cef_path = strdup(entpath);
	if (ep->cef_path == NULL) {
		cvs_log(LP_ERRNO, "failed to copy Entries path");
		free(ep);
		(void)fclose(fp);
		return (NULL);
	}

	ep->cef_bpath = strdup(bpath);
	if (ep->cef_bpath == NULL) {
		cvs_ent_close(ep);
		(void)fclose(fp);
		return (NULL);
	}

	ep->cef_cur = NULL;
	TAILQ_INIT(&(ep->cef_ent));

	while (fgets(ebuf, (int)sizeof(ebuf), fp) != NULL) {
		len = strlen(ebuf);
		if ((len > 0) && (ebuf[len - 1] == '\n'))
			ebuf[--len] = '\0';
		if ((ebuf[0] == 'D') && (ebuf[1] == '\0'))
			break;
		ent = cvs_ent_parse(ebuf);
		if (ent == NULL)
			continue;

		TAILQ_INSERT_TAIL(&(ep->cef_ent), ent, ce_list);
	}

	if (ferror(fp)) {
		cvs_log(LP_ERRNO, "read error on %s", entpath);
		(void)fclose(fp);
		cvs_ent_close(ep);
		return (NULL);
	}

	/* only keep a pointer to the open file if we're in writing mode */
	if ((flags & O_WRONLY) || (flags & O_RDWR))
		ep->cef_flags |= CVS_ENTF_WR;

	(void)fclose(fp);

	/*
	 * look for Entries.Log and add merge it together with our
	 * list of things.
	 */
	len = cvs_path_cat(dir, CVS_PATH_LOGENTRIES, entpath, sizeof(entpath));
	if (len >= sizeof(entpath)) {
		cvs_ent_close(ep);
		return (NULL);
	}

	fp = fopen(entpath, "r");
	if (fp != NULL) {
		while (fgets(ebuf, (int)sizeof(ebuf), fp) != NULL) {
			len = strlen(ebuf);
			if ((len > 0) && (ebuf[len - 1] == '\n'))
				ebuf[--len] = '\0';

			p = &ebuf[2];
			ent = cvs_ent_parse(p);
			if (ent == NULL)
				continue;

			if (ebuf[0] == 'A')
				cvs_ent_add(ep, ent);
			else if (ebuf[0] == 'R')
				cvs_ent_remove(ep, ent->ce_name, 0);
		}
		(void)fclose(fp);

		/* always un-synced here, because we
		 * just added or removed entries.
		 */
		ep->cef_flags &= ~CVS_ENTF_SYNC;
	} else {
		if (exists == 1)
			ep->cef_flags |= CVS_ENTF_SYNC;
	}

	return (ep);
}


/*
 * cvs_ent_close()
 *
 * Close the Entries file <ep> and free all data.  Any reference to entries
 * structure within that file become invalid.
 */
void
cvs_ent_close(CVSENTRIES *ep)
{
	struct cvs_ent *ent;

	if ((cvs_noexec == 0) && (ep->cef_flags & CVS_ENTF_WR) &&
	    !(ep->cef_flags & CVS_ENTF_SYNC)) {
		/* implicit sync with disk */
		(void)cvs_ent_write(ep);
	}

	if (ep->cef_path != NULL)
		free(ep->cef_path);

	if (ep->cef_bpath != NULL)
		free(ep->cef_bpath);

	while (!TAILQ_EMPTY(&(ep->cef_ent))) {
		ent = TAILQ_FIRST(&(ep->cef_ent));
		TAILQ_REMOVE(&(ep->cef_ent), ent, ce_list);
		cvs_ent_free(ent);
	}

	free(ep);
}


/*
 * cvs_ent_add()
 *
 * Add the entry <ent> to the Entries file <ef>.  The disk contents are not
 * modified until a call to cvs_ent_write() is performed.  This is done
 * implicitly on a call to cvs_ent_close() on an Entries file that has been
 * opened for writing.
 * Returns 0 on success, or -1 on failure.
 */
int
cvs_ent_add(CVSENTRIES *ef, struct cvs_ent *ent)
{
	if (!(ef->cef_flags & CVS_ENTF_WR)) {
		cvs_log(LP_ERR, "Entries file is opened in read-only mode");
		return (-1);
	}

	if (cvs_ent_get(ef, ent->ce_name) != NULL) {
		cvs_log(LP_ERR, "attempt to add duplicate entry for `%s'",
		    ent->ce_name);
		return (-1);
	}

	TAILQ_INSERT_TAIL(&(ef->cef_ent), ent, ce_list);

	ef->cef_flags &= ~CVS_ENTF_SYNC;

	return (0);
}


/*
 * cvs_ent_addln()
 *
 * Add a line to the Entries file.
 */
int
cvs_ent_addln(CVSENTRIES *ef, const char *line)
{
	struct cvs_ent *ent;

	if (!(ef->cef_flags & CVS_ENTF_WR)) {
		cvs_log(LP_ERR, "Entries file is opened in read-only mode");
		return (-1);
	}

	ent = cvs_ent_parse(line);
	if (ent == NULL)
		return (-1);

	if (cvs_ent_get(ef, ent->ce_name) != NULL)
		return (-1);

	TAILQ_INSERT_TAIL(&(ef->cef_ent), ent, ce_list);
	ef->cef_flags &= ~CVS_ENTF_SYNC;

	return (0);
}


/*
 * cvs_ent_remove()
 *
 * Remove an entry from the Entries file <ef>.  The entry's name is given
 * by <name>.
 */
int
cvs_ent_remove(CVSENTRIES *ef, const char *name, int useprev)
{
	struct cvs_ent *ent;

	cvs_log(LP_TRACE, "cvs_ent_remove(%s)", name);

	ent = cvs_ent_get(ef, name);
	if (ent == NULL)
		return (-1);

	if (ef->cef_cur == ent) {
		/* if this element was the last one retrieved through a
		 * call to cvs_ent_next(), point to the next element to avoid
		 * keeping an invalid reference.
		 */
		if (useprev) {
			ef->cef_cur = TAILQ_PREV(ef->cef_cur,
			    cvsentrieshead, ce_list);
		} else {
			ef->cef_cur = TAILQ_NEXT(ef->cef_cur, ce_list);
		}
	}
	TAILQ_REMOVE(&(ef->cef_ent), ent, ce_list);
	cvs_ent_free(ent);

	ef->cef_flags &= ~CVS_ENTF_SYNC;

	return (0);
}


/*
 * cvs_ent_get()
 *
 * Get the CVS entry from the Entries file <ef> whose 'name' portion matches
 * <file>.
 * Returns a pointer to the cvs entry structure on success, or NULL on failure.
 */
struct cvs_ent *
cvs_ent_get(CVSENTRIES *ef, const char *file)
{
	struct cvs_ent *ent;

	TAILQ_FOREACH(ent, &(ef->cef_ent), ce_list)
		if (strcmp(ent->ce_name, file) == 0)
			return (ent);

	return (NULL);
}


/*
 * cvs_ent_next()
 *
 * This function is used to iterate over the entries in an Entries file.  The
 * first call will return the first entry of the file and each subsequent call
 * will return the entry following the last one returned.
 * Returns a pointer to the cvs entry structure on success, or NULL on failure.
 */
struct cvs_ent *
cvs_ent_next(CVSENTRIES *ef)
{
	if (ef->cef_cur == NULL)
		ef->cef_cur = TAILQ_FIRST(&(ef->cef_ent));
	else
		ef->cef_cur = TAILQ_NEXT(ef->cef_cur, ce_list);
	return (ef->cef_cur);
}


/*
 * cvs_ent_parse()
 *
 * Parse a single line from a CVS/Entries file and return a cvs_ent structure
 * containing all the parsed information.
 */
struct cvs_ent*
cvs_ent_parse(const char *entry)
{
	int i;
	char *fields[CVS_ENTRIES_NFIELDS], *buf, *sp, *dp;
	struct cvs_ent *ent;

	buf = strdup(entry);
	if (buf == NULL) {
		cvs_log(LP_ERRNO, "failed to allocate entry copy");
		return (NULL);
	}

	sp = buf;
	i = 0;
	do {
		dp = strchr(sp, CVS_ENTRIES_DELIM);
		if (dp != NULL)
			*(dp++) = '\0';
		fields[i++] = sp;
		sp = dp;
	} while ((dp != NULL) && (i < CVS_ENTRIES_NFIELDS));

	if (i < CVS_ENTRIES_NFIELDS) {
		cvs_log(LP_ERR, "missing fields in entry line `%s'", entry);
		return (NULL);
	}

	ent = (struct cvs_ent *)malloc(sizeof(*ent));
	if (ent == NULL) {
		cvs_log(LP_ERRNO, "failed to allocate CVS entry");
		return (NULL);
	}
	memset(ent, 0, sizeof(*ent));
	ent->ce_buf = buf;

	if (*fields[0] == '\0')
		ent->ce_type = CVS_ENT_FILE;
	else if (*fields[0] == 'D')
		ent->ce_type = CVS_ENT_DIR;
	else
		ent->ce_type = CVS_ENT_NONE;

	ent->ce_status = CVS_ENT_REG;
	ent->ce_name = fields[1];
	ent->processed = 0;

	if (ent->ce_type == CVS_ENT_FILE) {
		if (*fields[2] == '-') {
			ent->ce_status = CVS_ENT_REMOVED;
			sp = fields[2] + 1;
		} else {
			sp = fields[2];
			if ((fields[2][0] == '0') && (fields[2][1] == '\0'))
				ent->ce_status = CVS_ENT_ADDED;
		}

		if ((ent->ce_rev = rcsnum_parse(sp)) == NULL) {
			cvs_ent_free(ent);
			return (NULL);
		}

		if (cvs_cmdop == CVS_OP_SERVER) {
			if (!strcmp(fields[3], "up to date"))
				ent->ce_status = CVS_ENT_UPTODATE;
		} else {
			if ((strcmp(fields[3], CVS_DATE_DUMMY) == 0) ||
			    (strncmp(fields[3], "Initial ", 8) == 0))
				ent->ce_mtime = CVS_DATE_DMSEC;
			else
				ent->ce_mtime = cvs_date_parse(fields[3]);
		}
	}

	ent->ce_opts = fields[4];
	ent->ce_tag = fields[5];
	return (ent);
}

/*
 * cvs_ent_free()
 *
 * Free a single CVS entries structure.
 */
void
cvs_ent_free(struct cvs_ent *ent)
{
	if (ent->ce_rev != NULL)
		rcsnum_free(ent->ce_rev);
	if (ent->ce_buf != NULL)
		free(ent->ce_buf);
	free(ent);
}

/*
 * cvs_ent_write()
 *
 * Explicitly write the contents of the Entries file <ef> to disk.
 * Returns 0 on success, or -1 on failure.
 */
int
cvs_ent_write(CVSENTRIES *ef)
{
	size_t len;
	char revbuf[64], timebuf[32];
	struct cvs_ent *ent;
	FILE *fp;

	if (ef->cef_flags & CVS_ENTF_SYNC)
		return (0);

	if ((fp = fopen(ef->cef_bpath, "w")) == NULL) {
		cvs_log(LP_ERRNO, "failed to open Entries `%s'", ef->cef_bpath);
		return (-1);
	}

	TAILQ_FOREACH(ent, &(ef->cef_ent), ce_list) {
		if (ent->ce_type == CVS_ENT_DIR) {
			putc('D', fp);
			timebuf[0] = '\0';
			revbuf[0] = '\0';
		} else {
			rcsnum_tostr(ent->ce_rev, revbuf, sizeof(revbuf));
			if ((ent->ce_mtime == CVS_DATE_DMSEC) &&
			    (ent->ce_status != CVS_ENT_ADDED))
				strlcpy(timebuf, CVS_DATE_DUMMY,
				    sizeof(timebuf));
			else if (ent->ce_status == CVS_ENT_ADDED) {
				strlcpy(timebuf, "Initial ", sizeof(timebuf));
				strlcat(timebuf, ent->ce_name, sizeof(timebuf));
			} else {
				ctime_r(&(ent->ce_mtime), timebuf);
				len = strlen(timebuf);
				if ((len > 0) && (timebuf[len - 1] == '\n'))
					timebuf[--len] = '\0';
			}
		}

		if (cvs_cmdop == CVS_OP_SERVER) {
			if (ent->ce_status == CVS_ENT_UPTODATE)
				strlcpy(timebuf, "up to date", sizeof(timebuf));
			else
				timebuf[0] = '\0';
		}

		fprintf(fp, "/%s/%s%s/%s/%s/%s\n", ent->ce_name,
		    (ent->ce_status == CVS_ENT_REMOVED) ? "-" : "", revbuf,
		    timebuf, (ent->ce_opts != NULL) ? ent->ce_opts : "",
		    (ent->ce_tag != NULL) ? ent->ce_tag : "");
	}

	/* terminating line */
	putc('D', fp);
	putc('\n', fp);

	ef->cef_flags |= CVS_ENTF_SYNC;
	fclose(fp);

	/* rename Entries.Backup to Entries */
	cvs_rename(ef->cef_bpath, ef->cef_path);

	/* remove Entries.Log */
	cvs_unlink(CVS_PATH_LOGENTRIES);

	return (0);
}
