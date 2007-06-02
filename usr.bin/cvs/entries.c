/*	$OpenBSD: entries.c,v 1.78 2007/06/02 09:00:19 niallo Exp $	*/
/*
 * Copyright (c) 2006 Joris Vink <joris@openbsd.org>
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

#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "cvs.h"

#define CVS_ENTRIES_NFIELDS	6
#define CVS_ENTRIES_DELIM	'/'

static struct cvs_ent_line *ent_get_line(CVSENTRIES *, const char *);

CVSENTRIES *
cvs_ent_open(const char *dir)
{
	FILE *fp;
	size_t len;
	CVSENTRIES *ep;
	char *p, buf[MAXPATHLEN];
	struct cvs_ent *ent;
	struct cvs_ent_line *line;

	ep = (CVSENTRIES *)xmalloc(sizeof(*ep));
	memset(ep, 0, sizeof(*ep));

	(void)xsnprintf(buf, sizeof(buf), "%s/%s", dir, CVS_PATH_ENTRIES);

	ep->cef_path = xstrdup(buf);

	(void)xsnprintf(buf, sizeof(buf), "%s/%s",
	    dir, CVS_PATH_BACKUPENTRIES);

	ep->cef_bpath = xstrdup(buf);

	(void)xsnprintf(buf, sizeof(buf), "%s/%s", dir, CVS_PATH_LOGENTRIES);

	ep->cef_lpath = xstrdup(buf);

	TAILQ_INIT(&(ep->cef_ent));

	if ((fp = fopen(ep->cef_path, "r")) != NULL) {
		while (fgets(buf, sizeof(buf), fp)) {
			len = strlen(buf);
			if (len > 0 && buf[len - 1] == '\n')
				buf[len - 1] = '\0';

			if (buf[0] == 'D' && buf[1] == '\0')
				break;

			line = (struct cvs_ent_line *)xmalloc(sizeof(*line));
			line->buf = xstrdup(buf);
			TAILQ_INSERT_TAIL(&(ep->cef_ent), line, entries_list);
		}

		(void)fclose(fp);
	}

	if ((fp = fopen(ep->cef_lpath, "r")) != NULL) {
		while (fgets(buf, sizeof(buf), fp)) {
			len = strlen(buf);
			if (len > 0 && buf[len - 1] == '\n')
				buf[len - 1] = '\0';

			p = &buf[1];

			if (buf[0] == 'A') {
				line = xmalloc(sizeof(*line));
				line->buf = xstrdup(p);
				TAILQ_INSERT_TAIL(&(ep->cef_ent), line,
				    entries_list);
			} else if (buf[0] == 'R') {
				ent = cvs_ent_parse(p);
				line = ent_get_line(ep, ent->ce_name);
				if (line != NULL) {
					TAILQ_REMOVE(&(ep->cef_ent), line,
					    entries_list);
					xfree(line->buf);
					xfree(line);
				}
				cvs_ent_free(ent);
			}
		}

		(void)fclose(fp);
	}

	return (ep);
}

struct cvs_ent *
cvs_ent_parse(const char *entry)
{
	int i;
	struct tm t;
	struct cvs_ent *ent;
	char *fields[CVS_ENTRIES_NFIELDS], *buf, *sp, *dp;

	buf = sp = xstrdup(entry);
	i = 0;
	do {
		dp = strchr(sp, CVS_ENTRIES_DELIM);
		if (dp != NULL)
			*(dp++) = '\0';
		fields[i++] = sp;
		sp = dp;
	} while (dp != NULL && i < CVS_ENTRIES_NFIELDS);

	if (i < CVS_ENTRIES_NFIELDS)
		fatal("missing fields in entry line '%s'", entry);

	ent = xmalloc(sizeof(*ent));
	ent->ce_buf = buf;

	if (*fields[0] == '\0')
		ent->ce_type = CVS_ENT_FILE;
	else if (*fields[0] == 'D')
		ent->ce_type = CVS_ENT_DIR;
	else
		ent->ce_type = CVS_ENT_NONE;

	ent->ce_status = CVS_ENT_REG;
	ent->ce_name = fields[1];
	ent->ce_rev = NULL;

	if (ent->ce_type == CVS_ENT_FILE) {
		if (*fields[2] == '-') {
			ent->ce_status = CVS_ENT_REMOVED;
			sp = fields[2] + 1;
		} else {
			sp = fields[2];
			if (fields[2][0] == '0' && fields[2][1] == '\0')
				ent->ce_status = CVS_ENT_ADDED;
		}

		if ((ent->ce_rev = rcsnum_parse(sp)) == NULL)
			fatal("failed to parse entry revision '%s'", entry);

		if (fields[3][0] == '\0' ||
		    strncmp(fields[3], CVS_DATE_DUMMY, sizeof(CVS_DATE_DUMMY) - 1) == 0 ||
		    strncmp(fields[3], "Initial ", 8) == 0 ||
		    strncmp(fields[3], "Result of merge", 15) == 0)
			ent->ce_mtime = CVS_DATE_DMSEC;
		else {
			/* Date field can be a '+=' with remote to indicate
			 * conflict.  In this case do nothing. */
			if (strptime(fields[3], "%a %b %d %T %Y", &t) != NULL) {

				t.tm_isdst = -1;	/* Figure out DST. */
				t.tm_gmtoff = 0;
				ent->ce_mtime = mktime(&t);
				ent->ce_mtime += t.tm_gmtoff;
			}
		}
	}

	ent->ce_conflict = fields[3];
	if ((dp = strchr(ent->ce_conflict, '+')) != NULL)
		*dp = '\0';
	else
		ent->ce_conflict = NULL;

	if (strcmp(fields[4], ""))
		ent->ce_opts = fields[4];
	else
		ent->ce_opts = NULL;

	if (strcmp(fields[5], ""))
		ent->ce_tag = fields[5] + 1;
	else
		ent->ce_tag = NULL;

	return (ent);
}

struct cvs_ent *
cvs_ent_get(CVSENTRIES *ep, const char *name)
{
	struct cvs_ent *ent;
	struct cvs_ent_line *l;

	l = ent_get_line(ep, name);
	if (l == NULL)
		return (NULL);

	ent = cvs_ent_parse(l->buf);
	return (ent);
}

int
cvs_ent_exists(CVSENTRIES *ep, const char *name)
{
	struct cvs_ent_line *l;

	l = ent_get_line(ep, name);
	if (l == NULL)
		return (0);

	return (1);
}

void
cvs_ent_close(CVSENTRIES *ep, int writefile)
{
	FILE *fp;
	struct cvs_ent_line *l;

	if (writefile) {
		if ((fp = fopen(ep->cef_bpath, "w")) == NULL)
			fatal("cvs_ent_close: fopen: `%s': %s",
			    ep->cef_path, strerror(errno));
	}

	while ((l = TAILQ_FIRST(&(ep->cef_ent))) != NULL) {
		if (writefile) {
			fputs(l->buf, fp);
			fputc('\n', fp);
		}

		TAILQ_REMOVE(&(ep->cef_ent), l, entries_list);
		xfree(l->buf);
		xfree(l);
	}

	if (writefile) {
		fputc('D', fp);
		fputc('\n', fp);
		(void)fclose(fp);

		if (rename(ep->cef_bpath, ep->cef_path) == -1)
			fatal("cvs_ent_close: rename: `%s'->`%s': %s",
			    ep->cef_bpath, ep->cef_path, strerror(errno));

		(void)unlink(ep->cef_lpath);
	}

	xfree(ep->cef_path);
	xfree(ep->cef_bpath);
	xfree(ep->cef_lpath);
	xfree(ep);
}

void
cvs_ent_add(CVSENTRIES *ep, const char *line)
{
	FILE *fp;
	struct cvs_ent_line *l;
	struct cvs_ent *ent;

	if ((ent = cvs_ent_parse(line)) == NULL)
		fatal("cvs_ent_add: parsing failed '%s'", line);

	l = ent_get_line(ep, ent->ce_name);
	if (l != NULL)
		cvs_ent_remove(ep, ent->ce_name);

	cvs_ent_free(ent);

	if (cvs_server_active == 0)
		cvs_log(LP_TRACE, "cvs_ent_add(%s, %s)", ep->cef_path, line);

	if ((fp = fopen(ep->cef_lpath, "a")) == NULL)
		fatal("cvs_ent_add: fopen: `%s': %s",
		    ep->cef_lpath, strerror(errno));

	fputc('A', fp);
	fputs(line, fp);
	fputc('\n', fp);

	(void)fclose(fp);

	l = (struct cvs_ent_line *)xmalloc(sizeof(*l));
	l->buf = xstrdup(line);
	TAILQ_INSERT_TAIL(&(ep->cef_ent), l, entries_list);
}

void
cvs_ent_remove(CVSENTRIES *ep, const char *name)
{
	FILE *fp;
	struct cvs_ent_line *l;

	if (cvs_server_active == 0)
		cvs_log(LP_TRACE, "cvs_ent_remove(%s, %s)", ep->cef_path, name);

	l = ent_get_line(ep, name);
	if (l == NULL)
		return;

	if ((fp = fopen(ep->cef_lpath, "a")) == NULL)
		fatal("cvs_ent_remove: fopen: `%s': %s", ep->cef_lpath,
		    strerror(errno));

	fputc('R', fp);
	fputs(l->buf, fp);
	fputc('\n', fp);

	(void)fclose(fp);

	TAILQ_REMOVE(&(ep->cef_ent), l, entries_list);
	xfree(l->buf);
	xfree(l);
}

void
cvs_ent_free(struct cvs_ent *ent)
{
	if (ent->ce_rev != NULL)
		rcsnum_free(ent->ce_rev);
	xfree(ent->ce_buf);
	xfree(ent);
}

static struct cvs_ent_line *
ent_get_line(CVSENTRIES *ep, const char *name)
{
	char *p, *s;
	struct cvs_ent_line *l;

	TAILQ_FOREACH(l, &(ep->cef_ent), entries_list) {
		if (l->buf[0] == 'D')
			p = &(l->buf[2]);
		else
			p = &(l->buf[1]);

		if ((s = strchr(p, '/')) == NULL)
			fatal("ent_get_line: bad entry line '%s'", l->buf);

		*s = '\0';

		if (!strcmp(p, name)) {
			*s = '/';
			return (l);
		}

		*s = '/';
	}

	return (NULL);
}

void
cvs_parse_tagfile(char *dir, char **tagp, char **datep, int *nbp)
{
	FILE *fp;
	int i, linenum;
	size_t len;
	char linebuf[128], tagpath[MAXPATHLEN];

	if (tagp != NULL)
		*tagp = NULL;

	if (datep != NULL)
		*datep = NULL;

	if (nbp != NULL)
		*nbp = 0;

	i = snprintf(tagpath, MAXPATHLEN, "%s/%s", dir, CVS_PATH_TAG);
	if (i < 0 || i >= MAXPATHLEN)
		return;

	if ((fp = fopen(tagpath, "r")) == NULL) {
		if (errno != ENOENT)
			cvs_log(LP_NOTICE, "failed to open `%s' : %s", tagpath,
			    strerror(errno));
		return;
        }

	linenum = 0;

	while (fgets(linebuf, (int)sizeof(linebuf), fp) != NULL) {
		linenum++;
		if ((len = strlen(linebuf)) == 0)
			continue;
		if (linebuf[len - 1] != '\n') {
			cvs_log(LP_NOTICE, "line too long in `%s:%d'",
			    tagpath, linenum);
			break;
		}
		linebuf[--len] = '\0';

		switch (*linebuf) {
		case 'T':
			if (tagp != NULL)
				*tagp = xstrdup(linebuf + 1);
			break;
		case 'D':
			if (datep != NULL)
				*datep = xstrdup(linebuf + 1);
			break;
		case 'N':
			if (tagp != NULL)
				*tagp = xstrdup(linebuf + 1);
			if (nbp != NULL)
				*nbp = 1;
			break;
		default:
			break;
		}
	}
	if (ferror(fp))
		cvs_log(LP_NOTICE, "failed to read line from `%s'", tagpath);

	(void)fclose(fp);
}

void
cvs_write_tagfile(const char *dir, char *tag, char *date, int nb)
{
	FILE *fp;
	char tagpath[MAXPATHLEN];
	int i;

	if (cvs_noexec == 1)
		return;

	i = snprintf(tagpath, MAXPATHLEN, "%s/%s", dir, CVS_PATH_TAG);
	if (i < 0 || i >= MAXPATHLEN)
		return;

	if ((tag != NULL) || (date != NULL)) {
		if ((fp = fopen(tagpath, "w+")) == NULL) {
			if (errno != ENOENT) {
				cvs_log(LP_NOTICE, "failed to open `%s' : %s",
				    tagpath, strerror(errno));
			}
			return;
		}
		if (tag != NULL) {
			if (nb != 0)
				(void)fprintf(fp, "N%s\n", tag);
			else
				(void)fprintf(fp, "T%s\n", tag);
		} else
			(void)fprintf(fp, "D%s\n", date);

		(void)fclose(fp);
	} else {
		(void)cvs_unlink(tagpath);
	}
}
