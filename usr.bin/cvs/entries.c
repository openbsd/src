/*	$OpenBSD: entries.c,v 1.20 2004/12/07 17:10:56 tedu Exp $	*/
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

#include <sys/param.h>
#include <sys/stat.h>

#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "log.h"
#include "cvs.h"


#define CVS_ENTRIES_NFIELDS  6
#define CVS_ENTRIES_DELIM   '/'


/*
 * cvs_ent_open()
 *
 * Open the CVS Entries file for the directory <dir>.
 * Returns a pointer to the CVSENTRIES file structure on success, or NULL
 * on failure.
 */
CVSENTRIES*
cvs_ent_open(const char *dir, int flags)
{
	size_t len;
	int exists;
	char entpath[MAXPATHLEN], ebuf[128], mode[4];
	FILE *fp;
	struct stat st;
	struct cvs_ent *ent;
	CVSENTRIES *ep;

	exists = 0;
	memset(mode, 0, sizeof(mode));

	snprintf(entpath, sizeof(entpath), "%s/" CVS_PATH_ENTRIES, dir);

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

	ep->cef_cur = NULL;
	TAILQ_INIT(&(ep->cef_ent));

	while (fgets(ebuf, sizeof(ebuf), fp) != NULL) {
		len = strlen(ebuf);
		if ((len > 0) && (ebuf[len - 1] == '\n'))
			ebuf[--len] = '\0';
		if (strcmp(ebuf, "D") == 0)
			break;
		ent = cvs_ent_parse(ebuf);
		if (ent == NULL)
			continue;

		TAILQ_INSERT_TAIL(&(ep->cef_ent), ent, ce_list);
	}
	if (ferror(fp)) {
		cvs_log(LP_ERRNO, "read error on %s", entpath);
		cvs_ent_close(ep);
		return (NULL);
	}

	/* only keep a pointer to the open file if we're in writing mode */
	if ((flags & O_WRONLY) || (flags & O_RDWR))
		ep->cef_flags |= CVS_ENTF_WR;

	(void)fclose(fp);

	if (exists)
		ep->cef_flags |= CVS_ENTF_SYNC;

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

	if ((ep->cef_flags & CVS_ENTF_WR) &&
	    !(ep->cef_flags & CVS_ENTF_SYNC)) {
		/* implicit sync with disk */
		(void)cvs_ent_write(ep);
	}

	if (ep->cef_file != NULL)
		(void)fclose(ep->cef_file);
	if (ep->cef_path != NULL)
		free(ep->cef_path);

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

	if (cvs_ent_get(ef, ent->ce_name) != NULL)
		return (-1);

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
cvs_ent_remove(CVSENTRIES *ef, const char *name)
{
	struct cvs_ent *ent;

	if (!(ef->cef_flags & CVS_ENTF_WR)) {
		cvs_log(LP_ERR, "Entries file is opened in read-only mode");
		return (-1);
	}

	ent = cvs_ent_get(ef, name);
	if (ent == NULL)
		return (-1);

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
struct cvs_ent*
cvs_ent_get(CVSENTRIES *ef, const char *file)
{
	struct cvs_ent *ep;

	TAILQ_FOREACH(ep, &(ef->cef_ent), ce_list)
		if (strcmp(ep->ce_name, file) == 0)
			return (ep);

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
struct cvs_ent*
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
 * Parse a single line from a CVS/Entries file and return a cvs_entry structure
 * containing all the parsed information.
 */
struct cvs_ent*
cvs_ent_parse(const char *entry)
{
	int i;
	char *fields[CVS_ENTRIES_NFIELDS], *buf, *sp, *dp;
	struct cvs_ent *entp;

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

	entp = (struct cvs_ent *)malloc(sizeof(*entp));
	if (entp == NULL) {
		cvs_log(LP_ERRNO, "failed to allocate CVS entry");
		return (NULL);
	}
	memset(entp, 0, sizeof(*entp));
	entp->ce_buf = buf;

	entp->ce_rev = rcsnum_alloc();
	if (entp->ce_rev == NULL) {
		cvs_ent_free(entp);
		return (NULL);
	}

	if (*fields[0] == '\0')
		entp->ce_type = CVS_ENT_FILE;
	else if (*fields[0] == 'D')
		entp->ce_type = CVS_ENT_DIR;
	else
		entp->ce_type = CVS_ENT_NONE;

	entp->ce_name = fields[1];

	if (entp->ce_type == CVS_ENT_FILE) {
		rcsnum_aton(fields[2], NULL, entp->ce_rev);
		entp->ce_mtime = cvs_datesec(fields[3], CVS_DATE_CTIME, 0);
		entp->ce_opts = fields[4];
		entp->ce_tag = fields[5];
	}

	return (entp);
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
 * cvs_ent_getent()
 *
 * Get a single entry from the CVS/Entries file of the basename portion of
 * path <path> and return that entry.  That entry must later be freed using
 * cvs_ent_free().
 */
struct cvs_ent*
cvs_ent_getent(const char *path)
{
	char base[MAXPATHLEN], *file;
	CVSENTRIES *entf;
	struct cvs_ent *ep;

	cvs_splitpath(path, base, sizeof(base), &file);

	entf = cvs_ent_open(base, O_RDONLY);
	if (entf == NULL)
		return (NULL);

	ep = cvs_ent_get(entf, file);
	if (ep != NULL) {
		/* take it out of the queue so it doesn't get freed */
		TAILQ_REMOVE(&(entf->cef_ent), ep, ce_list);
	}

	cvs_ent_close(entf);
	return (ep);
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

	if (ef->cef_flags & CVS_ENTF_SYNC)
		return (0);

	if (ef->cef_file == NULL) {
		ef->cef_file = fopen(ef->cef_path, "w");
		if (ef->cef_file == NULL) {
			cvs_log(LP_ERRNO, "failed to open Entries `%s'",
			    ef->cef_path);
			return (-1);
		}
	}


	/* reposition ourself at beginning of file */
	rewind(ef->cef_file);
	TAILQ_FOREACH(ent, &(ef->cef_ent), ce_list) {
		if (ent->ce_type == CVS_ENT_DIR) {
			putc('D', ef->cef_file);
			timebuf[0] = '\0';
			revbuf[0] = '\0';
		} else {
			rcsnum_tostr(ent->ce_rev, revbuf, sizeof(revbuf));
			if (ent->ce_mtime == CVS_DATE_DMSEC)
				strlcpy(timebuf, CVS_DATE_DUMMY,
				    sizeof(timebuf));
			else {
				ctime_r(&(ent->ce_mtime), timebuf);
				len = strlen(timebuf);
				if ((len > 0) && (timebuf[len - 1] == '\n'))
					timebuf[--len] = '\0';
			}
		}

		fprintf(ef->cef_file, "/%s/%s/%s/%s/%s\n", ent->ce_name,
		    revbuf, timebuf, "", "");
	}

	/* terminating line */
	fprintf(ef->cef_file, "D\n");

	ef->cef_flags |= CVS_ENTF_SYNC;
	fclose(ef->cef_file);
	ef->cef_file = NULL;

	return (0);
}
