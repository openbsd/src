/*	$OpenBSD: entries.c,v 1.103 2015/01/16 06:40:07 deraadt Exp $	*/
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
#include <time.h>
#include <unistd.h>

#include "cvs.h"
#include "remote.h"

#define CVS_ENTRIES_NFIELDS	6
#define CVS_ENTRIES_DELIM	'/'

static struct cvs_ent_line *ent_get_line(CVSENTRIES *, const char *);

CVSENTRIES *current_list = NULL;

CVSENTRIES *
cvs_ent_open(const char *dir)
{
	FILE *fp;
	CVSENTRIES *ep;
	char *p, buf[PATH_MAX];
	struct cvs_ent *ent;
	struct cvs_ent_line *line;

	cvs_log(LP_TRACE, "cvs_ent_open(%s)", dir);

	(void)xsnprintf(buf, sizeof(buf), "%s/%s", dir, CVS_PATH_ENTRIES);

	if (current_list != NULL && !strcmp(current_list->cef_path, buf))
		return (current_list);

	if (current_list != NULL) {
		cvs_ent_close(current_list, ENT_SYNC);
		current_list = NULL;
	}

	ep = (CVSENTRIES *)xcalloc(1, sizeof(*ep));
	ep->cef_path = xstrdup(buf);

	(void)xsnprintf(buf, sizeof(buf), "%s/%s",
	    dir, CVS_PATH_BACKUPENTRIES);

	ep->cef_bpath = xstrdup(buf);

	(void)xsnprintf(buf, sizeof(buf), "%s/%s", dir, CVS_PATH_LOGENTRIES);

	ep->cef_lpath = xstrdup(buf);

	TAILQ_INIT(&(ep->cef_ent));

	if ((fp = fopen(ep->cef_path, "r")) != NULL) {
		while (fgets(buf, sizeof(buf), fp)) {
			buf[strcspn(buf, "\n")] = '\0';

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
			buf[strcspn(buf, "\n")] = '\0';

			if (strlen(buf) < 2)
				fatal("cvs_ent_open: %s: malformed line %s",
				    ep->cef_lpath, buf);

			p = &buf[2];

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

	current_list = ep;
	return (ep);
}

struct cvs_ent *
cvs_ent_parse(const char *entry)
{
	int i;
	struct tm t, dt;
	struct cvs_ent *ent;
	char *fields[CVS_ENTRIES_NFIELDS], *buf, *sp, *dp, *p;

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
	ent->ce_date = -1;
	ent->ce_tag = NULL;

	if (ent->ce_type == CVS_ENT_FILE) {
		if (*fields[2] == '-') {
			ent->ce_status = CVS_ENT_REMOVED;
			sp = fields[2] + 1;
		} else if (*fields[2] == CVS_SERVER_QUESTIONABLE) {
			sp = NULL;
			ent->ce_status = CVS_ENT_UNKNOWN;
		} else {
			sp = fields[2];
			if (fields[2][0] == '0' && fields[2][1] == '\0')
				ent->ce_status = CVS_ENT_ADDED;
		}

		if (sp != NULL) {
			if ((ent->ce_rev = rcsnum_parse(sp)) == NULL) {
				fatal("failed to parse entry revision '%s'",
				    entry);
			}
		}

		if (fields[3][0] == '\0' ||
		    strncmp(fields[3], CVS_DATE_DUMMY,
		    sizeof(CVS_DATE_DUMMY) - 1) == 0 ||
		    strncmp(fields[3], "Initial ", 8) == 0 ||
		    strcmp(fields[3], "Result of merge") == 0) {
			ent->ce_mtime = CVS_DATE_DMSEC;
		} else if (cvs_server_active == 1 &&
		    strncmp(fields[3], CVS_SERVER_UNCHANGED,
		    strlen(CVS_SERVER_UNCHANGED)) == 0) {
			ent->ce_mtime = CVS_SERVER_UPTODATE;
		} else {
			p = fields[3];
			if (strncmp(fields[3], "Result of merge+", 16) == 0)
				p += 16;

			/* Date field can be a '+=' with remote to indicate
			 * conflict.  In this case do nothing. */
			if (strptime(p, "%a %b %d %T %Y", &t) != NULL) {
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

	if (strcmp(fields[5], "")) {
		switch (*fields[5]) {
		case 'D':
			if (sscanf(fields[5] + 1, "%d.%d.%d.%d.%d.%d",
			    &dt.tm_year, &dt.tm_mon, &dt.tm_mday,
			    &dt.tm_hour, &dt.tm_min, &dt.tm_sec) != 6)
				fatal("wrong date specification");
			dt.tm_year -= 1900;
			dt.tm_mon -= 1;
			ent->ce_date = timegm(&dt);
			ent->ce_tag = NULL;
			break;
		case 'T':
			ent->ce_tag = fields[5] + 1;
			break;
		default:
			fatal("invalid sticky entry");
		}
	}

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

void
cvs_ent_close(CVSENTRIES *ep, int writefile)
{
	FILE *fp;
	struct cvs_ent_line *l;
	int dflag;

	dflag = 1;
	cvs_log(LP_TRACE, "cvs_ent_close(%s, %d)", ep->cef_bpath, writefile);

	if (cvs_cmdop == CVS_OP_EXPORT)
		writefile = 0;

	fp = NULL;
	if (writefile)
		fp = fopen(ep->cef_bpath, "w");

	while ((l = TAILQ_FIRST(&(ep->cef_ent))) != NULL) {
		if (fp != NULL) {
			if (l->buf[0] == 'D')
				dflag = 0;

			fputs(l->buf, fp);
			fputc('\n', fp);
		}

		TAILQ_REMOVE(&(ep->cef_ent), l, entries_list);
		xfree(l->buf);
		xfree(l);
	}

	if (fp != NULL) {
		if (dflag) {
			fputc('D', fp);
			fputc('\n', fp);
		}
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

	fputs("A ", fp);
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

	fputs("R ", fp);
	fputs(l->buf, fp);
	fputc('\n', fp);

	(void)fclose(fp);

	TAILQ_REMOVE(&(ep->cef_ent), l, entries_list);
	xfree(l->buf);
	xfree(l);
}

/*
 * cvs_ent_line_str()
 *
 * Build CVS/Entries line.
 *
 */
void
cvs_ent_line_str(const char *name, char *rev, char *tstamp, char *opts,
    char *sticky, int isdir, int isremoved, char *buf, size_t len)
{
	if (isdir == 1) {
		(void)xsnprintf(buf, len, "D/%s////", name);
		return;
	}

	(void)xsnprintf(buf, len, "/%s/%s%s/%s/%s/%s",
	    name, isremoved == 1 ? "-" : "", rev, tstamp, opts, sticky);
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
	struct tm datetm;
	char linebuf[128], tagpath[PATH_MAX];

	cvs_directory_date = -1;

	if (tagp != NULL)
		*tagp = NULL;

	if (datep != NULL)
		*datep = NULL;

	if (nbp != NULL)
		*nbp = 0;

	i = snprintf(tagpath, PATH_MAX, "%s/%s", dir, CVS_PATH_TAG);
	if (i < 0 || i >= PATH_MAX)
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
			if (sscanf(linebuf + 1, "%d.%d.%d.%d.%d.%d",
			    &datetm.tm_year, &datetm.tm_mon, &datetm.tm_mday,
			    &datetm.tm_hour, &datetm.tm_min, &datetm.tm_sec) !=
			    6)
				fatal("wrong date specification");
			datetm.tm_year -= 1900;
			datetm.tm_mon -= 1;

			cvs_directory_date = timegm(&datetm);

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
cvs_write_tagfile(const char *dir, char *tag, char *date)
{
	FILE *fp;
	RCSNUM *rev;
	char tagpath[PATH_MAX];
	char sticky[CVS_REV_BUFSZ];
	struct tm datetm;
	int i;

	cvs_log(LP_TRACE, "cvs_write_tagfile(%s, %s, %s)", dir,
	    tag != NULL ? tag : "", date != NULL ? date : "");

	if (cvs_noexec == 1)
		return;

	i = snprintf(tagpath, PATH_MAX, "%s/%s", dir, CVS_PATH_TAG);
	if (i < 0 || i >= PATH_MAX)
		return;

	if (tag != NULL || cvs_specified_date != -1 ||
	    cvs_directory_date != -1) {
		if ((fp = fopen(tagpath, "w+")) == NULL) {
			if (errno != ENOENT) {
				cvs_log(LP_NOTICE, "failed to open `%s' : %s",
				    tagpath, strerror(errno));
			}
			return;
		}

		if (tag != NULL) {
			if ((rev = rcsnum_parse(tag)) != NULL) {
				(void)xsnprintf(sticky, sizeof(sticky),
				    "N%s", tag);
				rcsnum_free(rev);
			} else {
				(void)xsnprintf(sticky, sizeof(sticky),
				    "T%s", tag);
			}
		} else {
			if (cvs_specified_date != -1)
				gmtime_r(&cvs_specified_date, &datetm);
			else
				gmtime_r(&cvs_directory_date, &datetm);
			(void)strftime(sticky, sizeof(sticky),
			    "D"CVS_DATE_FMT, &datetm);
		}

		(void)fprintf(fp, "%s\n", sticky);
		(void)fclose(fp);
	}
}
