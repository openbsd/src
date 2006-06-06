/*	$OpenBSD: entries.c,v 1.58 2006/06/06 05:13:39 joris Exp $	*/
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

#include "includes.h"

#include "cvs.h"
#include "log.h"

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

	cvs_path_cat(dir, CVS_PATH_ENTRIES, buf, sizeof(buf));
	ep->cef_path = xstrdup(buf);

	cvs_path_cat(dir, CVS_PATH_BACKUPENTRIES, buf, sizeof(buf));
	ep->cef_bpath = xstrdup(buf);

	cvs_path_cat(dir, CVS_PATH_LOGENTRIES, buf, sizeof(buf));
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
			if (len > 0 && buf[strlen(buf) - 1] == '\n')
				buf[strlen(buf) - 1] = '\0';

			p = &buf[1];

			if (buf[0] == 'A') {
				line = xmalloc(sizeof(*line));
				line->buf = xstrdup(p);
				TAILQ_INSERT_TAIL(&(ep->cef_ent), line,
				    entries_list);
			} else if (buf[0] == 'R') {
				ent = cvs_ent_parse(p);
				line = ent_get_line(ep, ent->ce_name);
				if (line != NULL)
					TAILQ_REMOVE(&(ep->cef_ent), line,
					    entries_list);
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
	struct cvs_ent *ent;
	char *fields[CVS_ENTRIES_NFIELDS], *buf, *sp, *dp;

	buf = xstrdup(entry);
	sp = buf;
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

	ent = (struct cvs_ent *)xmalloc(sizeof(*ent));
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

		if (strcmp(fields[3], CVS_DATE_DUMMY) == 0 ||
		    strncmp(fields[3], "Initial ", 8) == 0 ||
		    strncmp(fields[3], "Result of merge", 15) == 0)
			ent->ce_mtime = CVS_DATE_DMSEC;
		else
			ent->ce_mtime = cvs_date_parse(fields[3]);
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
			fatal("cvs_ent_close: failed to write %s",
			    ep->cef_path);
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
		(void)fclose(fp);

		if (rename(ep->cef_bpath, ep->cef_path) == -1)
			fatal("cvs_ent_close: %s: %s", ep->cef_path,
			     strerror(errno));

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

	cvs_log(LP_TRACE, "cvs_ent_add(%s, %s)", ep->cef_path, line);

	if ((fp = fopen(ep->cef_lpath, "a")) == NULL)
		fatal("cvs_ent_add: failed to open '%s'", ep->cef_lpath);

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

	cvs_log(LP_TRACE, "cvs_ent_remove(%s, %s)", ep->cef_path, name);

	l = ent_get_line(ep, name);
	if (l == NULL)
		return;

	if ((fp = fopen(ep->cef_lpath, "a")) == NULL)
		fatal("cvs_ent_remove: failed to open '%s'",
		    ep->cef_lpath);

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
