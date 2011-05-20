/*	$OpenBSD: trigger.c,v 1.20 2011/05/20 19:22:47 nicm Exp $	*/
/*
 * Copyright (c) 2008 Tobias Stoeckmann <tobias@openbsd.org>
 * Copyright (c) 2008 Jonathan Armani <dbd@asystant.net>
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

#include <sys/queue.h>
#include <sys/types.h>

#include <ctype.h>
#include <errno.h>
#include <libgen.h>
#include <pwd.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config.h"
#include "cvs.h"

static int	 expand_args(BUF *, struct file_info_list *, const char *,
    const char *, char *);
static int	 expand_var(BUF *, const char *);
static char	*parse_cmd(int, char *, const char *, struct file_info_list *);

static int
expand_args(BUF *buf, struct file_info_list *file_info, const char *repo,
    const char *allowed_args, char *format)
{
	int oldstyle, quote;
	struct file_info *fi = NULL;
	char *p, valbuf[2] = { '\0', '\0' };
	const char *val;

	if (file_info != NULL && !TAILQ_EMPTY(file_info))
		fi = TAILQ_FIRST(file_info);

	quote = oldstyle = 0;

	/* Why does GNU cvs print something if it encounters %{}? */
	if (*format == '\0')
		oldstyle = 1;

	for (p = format; *p != '\0'; p++) {
		if (*p != '%' && strchr(allowed_args, *p) == NULL)
			return 1;

		switch (*p) {
		case 's':
		case 'V':
		case 'v':
			quote = 1;
			oldstyle = 1;
			break;
		default:
			break;
		}
	}
	if (quote)
		buf_putc(buf, '"');
	if (oldstyle) {
		buf_puts(buf, repo);
		buf_putc(buf, ' ');
	}

	if (*format == '\0')
		return 0;

	/*
	 * check like this, add only uses loginfo for directories anyway
	 */
	if (cvs_cmdop == CVS_OP_ADD) {
		buf_puts(buf, "- New directory");
		if (quote)
			buf_putc(buf, '"');
		return (0);
	}

	if (cvs_cmdop == CVS_OP_IMPORT) {
		buf_puts(buf, "- Imported sources");
		if (quote)
			buf_putc(buf, '"');
		return (0);
	}

	for (;;) {
		for (p = format; *p != '\0';) {
			val = NULL;

			switch (*p) {
			case '%':
				val = "%";
				break;
			case 'b':
				if (fi != NULL) {
					valbuf[0] = fi->tag_type;
					val = valbuf;
				}
				break;
			case 'o':
				if (fi != NULL)
					val = fi->tag_op;
				break;
			case 'p':
				val = current_cvsroot->cr_dir;
				break;
			case 'r':
				val = repo;
				break;
			case 'l':
			case 'S':
			case 's':
				if (fi != NULL)
					val = fi->file_path;
				break;
			case 't':
				if (fi != NULL)
					val = fi->tag_new;
				break;
			case 'V':
				if (fi != NULL) {
					if (fi->crevstr != NULL &&
					    !strcmp(fi->crevstr,
					    "Non-existent"))
						val = "NONE";
					else
						val = fi->crevstr;
				}
				break;
			case 'v':
				if (fi != NULL) {
					if (fi->nrevstr != NULL &&
					    !strcmp(fi->nrevstr, "Removed"))
						val = "NONE";
					else
						val = fi->nrevstr;
				}
				break;
			default:
				return 1;
			}

			if (val != NULL)
				buf_puts(buf, val);

			if (*(++p) != '\0')
				buf_putc(buf, ',');
		}

		if (fi != NULL)
			fi = TAILQ_NEXT(fi, flist);
		if (fi == NULL)
			break;

		if (strlen(format) == 1 && (*format == '%' || *format == 'o' ||
		    *format == 'p' || *format == 'r' || *format == 't'))
			break;

		buf_putc(buf, ' ');
	}

	if (quote)
		buf_putc(buf, '"');

	return 0;
}

static int
expand_var(BUF *buf, const char *var)
{
	struct passwd *pw;
	const char *val;

	if (*var == '=') {
		if ((val = cvs_var_get(++var)) == NULL) {
			cvs_log(LP_ERR, "no such user variable ${=%s}", var);
			return (1);
		}
		buf_puts(buf, val);
	} else {
		if (strcmp(var, "CVSEDITOR") == 0 ||
		    strcmp(var, "EDITOR") == 0 ||
		    strcmp(var, "VISUAL") == 0)
			buf_puts(buf, cvs_editor);
		else if (strcmp(var, "CVSROOT") == 0)
			buf_puts(buf, current_cvsroot->cr_dir);
		else if (strcmp(var, "USER") == 0) {
			pw = getpwuid(geteuid());
			if (pw == NULL) {
				cvs_log(LP_ERR, "unable to retrieve "
				    "caller ID");
				return (1);
			}
			buf_puts(buf, pw->pw_name);
		} else if (strcmp(var, "RCSBIN") == 0) {
			cvs_log(LP_ERR, "RCSBIN internal variable is no "
			    "longer supported");
			return (1);
		} else {
			cvs_log(LP_ERR, "no such internal variable $%s", var);
			return (1);
		}
	}

	return (0);
}

static char *
parse_cmd(int type, char *cmd, const char *repo,
    struct file_info_list *file_info)
{
	int expanded = 0;
	char argbuf[2] = { '\0', '\0' };
	char *allowed_args, *default_args, *args, *file, *p, *q = NULL;
	size_t pos;
	BUF *buf;

	switch (type) {
	case CVS_TRIGGER_COMMITINFO:
		allowed_args = "prsS{}";
		default_args = " %p/%r %S";
		file = CVS_PATH_COMMITINFO;
		break;
	case CVS_TRIGGER_LOGINFO:
		allowed_args = "prsSvVt{}";
		default_args = NULL;
		file = CVS_PATH_LOGINFO;
		break;
	case CVS_TRIGGER_VERIFYMSG:
		allowed_args = "l";
		default_args = " %l";
		file = CVS_PATH_VERIFYMSG;
		break;
	case CVS_TRIGGER_TAGINFO:
		allowed_args = "btoprsSvV{}";
		default_args = " %t %o %p/%r %{sv}";
		file = CVS_PATH_TAGINFO;
		break;
	default:
		return (NULL);
	}

	/* before doing any stuff, check if the command starts with % */
	for (p = cmd; *p != '%' && !isspace(*p) && *p != '\0'; p++)
		;
	if (*p == '%')
		return (NULL);

	buf = buf_alloc(1024);

	p = cmd;
again:
	for (; *p != '\0'; p++) {
		if ((pos = strcspn(p, "$%")) != 0) {
			buf_append(buf, p, pos);
			p += pos;
		}

		q = NULL;
		if (*p == '\0')
			break;
		if (*p++ == '$') {
			if (*p == '{') {
				pos = strcspn(++p, "}");
				if (p[pos] == '\0')
					goto bad;
			} else {
				for (pos = 0; isalpha(p[pos]); pos++)
					;
				if (pos == 0) {
					cvs_log(LP_ERR,
					    "unrecognized variable syntax");
					goto bad;
				}
			}
			q = xmalloc(pos + 1);
			memcpy(q, p, pos);
			q[pos] = '\0';
			if (expand_var(buf, q))
				goto bad;
			p += pos - (*(p - 1) == '{' ? 0 : 1);
		} else {
			switch (*p) {
			case '\0':
				goto bad;
			case '{':
				if (strchr(allowed_args, '{') == NULL)
					goto bad;
				pos = strcspn(++p, "}");
				if (p[pos] == '\0')
					goto bad;
				q = xmalloc(pos + 1);
				memcpy(q, p, pos);
				q[pos] = '\0';
				args = q;
				p += pos;
				break;
			default:
				argbuf[0] = *p;
				args = argbuf;
				break;
			}
	
			if (expand_args(buf, file_info, repo, allowed_args,
			    args))
				goto bad;
			expanded = 1;
		}

		if (q != NULL)
			xfree(q);
	}

	if (!expanded && default_args != NULL) {
		p = default_args;
		expanded = 1;
		goto again;
	}

	buf_putc(buf, '\0');
	return (buf_release(buf));

bad:
	if (q != NULL)
		xfree(q);
	cvs_log(LP_NOTICE, "%s contains malformed command '%s'", file, cmd);
	buf_free(buf);
	return (NULL);
}

int
cvs_trigger_handle(int type, char *repo, char *in, struct trigger_list *list,
    struct file_info_list *files)
{
	int r;
	char *cmd;
	struct trigger_line *line;

	TAILQ_FOREACH(line, list, flist) {
		if ((cmd = parse_cmd(type, line->line, repo, files)) == NULL)
			return (1);
		switch(type) {
		case CVS_TRIGGER_COMMITINFO:
		case CVS_TRIGGER_TAGINFO:
		case CVS_TRIGGER_VERIFYMSG:
			if ((r = cvs_exec(cmd, NULL, 1)) != 0) {
				xfree(cmd);
				return (r);
			}
			break;
		default:
			(void)cvs_exec(cmd, in, 1);
			break;
		}
		xfree(cmd);
	}

	return (0);
}

struct trigger_list *
cvs_trigger_getlines(char * file, char * repo)
{
	FILE *fp;
	int allow_all, lineno, match = 0;
	size_t len;
	regex_t preg;
	struct trigger_list *list;
	struct trigger_line *tline;
	char fpath[MAXPATHLEN];
	char *currentline, *defaultline = NULL, *nline, *p, *q, *regex;

	if (strcmp(file, CVS_PATH_EDITINFO) == 0 ||
	    strcmp(file, CVS_PATH_VERIFYMSG) == 0)
		allow_all = 0;
	else
		allow_all = 1;

	(void)xsnprintf(fpath, MAXPATHLEN, "%s/%s", current_cvsroot->cr_dir,
	    file);

	if ((fp = fopen(fpath, "r")) == NULL) {
		if (errno != ENOENT)
			cvs_log(LP_ERRNO, "cvs_trigger_getlines: %s", file);
		return (NULL);
	}

	list = xmalloc(sizeof(*list));
	TAILQ_INIT(list);
	
	lineno = 0;
	nline = NULL;
	while ((currentline = fgetln(fp, &len)) != NULL) {
		if (currentline[len - 1] == '\n') {
			currentline[len - 1] = '\0';
		} else {
			nline = xmalloc(len + 1);
			memcpy(nline, currentline, len);
			nline[len] = '\0';
			currentline = nline;
		}

		lineno++;

		for (p = currentline; isspace(*p); p++)
			;

		if (*p == '\0' || *p == '#')
			continue;

		for (q = p; !isspace(*q) && *q != '\0'; q++)
			;

		if (*q == '\0')
			goto bad;

		*q++ = '\0';
		regex = p;

		for (; isspace(*q); q++)
			;

		if (*q == '\0')
			goto bad;

		if (strcmp(regex, "ALL") == 0 && allow_all) {
			tline = xmalloc(sizeof(*tline));
			tline->line = xstrdup(q);
			TAILQ_INSERT_TAIL(list, tline, flist);
		} else if (defaultline == NULL && !match &&
		    strcmp(regex, "DEFAULT") == 0) {
			defaultline = xstrdup(q);
		} else if (!match) {
			if (regcomp(&preg, regex, REG_NOSUB|REG_EXTENDED))
				goto bad;

			if (regexec(&preg, repo, 0, NULL, 0) != REG_NOMATCH) {
				match = 1;

				tline = xmalloc(sizeof(*tline));
				tline->line = xstrdup(q);
				TAILQ_INSERT_HEAD(list, tline, flist);
			}
			regfree(&preg);
		}
	}

	if (nline != NULL)
		xfree(nline);

	if (defaultline != NULL) {
		if (!match) {
			tline = xmalloc(sizeof(*tline));
			tline->line = defaultline;
			TAILQ_INSERT_HEAD(list, tline, flist);
		} else
			xfree(defaultline);
	}

	(void)fclose(fp);
	
	if (TAILQ_EMPTY(list)) {
		xfree(list);
		list = NULL;
	}

	return (list);

bad:
	cvs_log(LP_NOTICE, "%s: malformed line %d", file, lineno);

	if (defaultline != NULL)
		xfree(defaultline);
	cvs_trigger_freelist(list);

	(void)fclose(fp);
	
	return (NULL);
}

void
cvs_trigger_freelist(struct trigger_list * list)
{
	struct trigger_line *line;

	while ((line = TAILQ_FIRST(list)) != NULL) {
		TAILQ_REMOVE(list, line, flist);
		xfree(line->line);
		xfree(line);
	}

	xfree(list);
}

void
cvs_trigger_freeinfo(struct file_info_list * list)
{
	struct file_info * fi;

	while ((fi = TAILQ_FIRST(list)) != NULL) {
		TAILQ_REMOVE(list, fi, flist);

		if (fi->file_path != NULL)
			xfree(fi->file_path);
		if (fi->file_wd != NULL)
			xfree(fi->file_wd);
		if (fi->crevstr != NULL)
			xfree(fi->crevstr);
		if (fi->nrevstr != NULL)
			xfree(fi->nrevstr);
		if (fi->tag_new != NULL)
			xfree(fi->tag_new);
		if (fi->tag_old != NULL)
			xfree(fi->tag_old);

		xfree(fi);
	}
}

void
cvs_trigger_loginfo_header(BUF *buf, char *repo)
{
	char *dir, pwd[MAXPATHLEN];
	char hostname[MAXHOSTNAMELEN];

	if (gethostname(hostname, sizeof(hostname)) == -1) {
		fatal("cvs_trigger_loginfo_header: gethostname failed %s",
		    strerror(errno));
	}

	if (getcwd(pwd, sizeof(pwd)) == NULL)
		fatal("cvs_trigger_loginfo_header: Cannot get working "
		    "directory");

	if ((dir = dirname(pwd)) == NULL) {
		fatal("cvs_trigger_loginfo_header: dirname failed %s",
		    strerror(errno));
	}

	buf_puts(buf, "Update of ");
	buf_puts(buf, current_cvsroot->cr_dir);
	buf_putc(buf, '/');
	buf_puts(buf, repo);
	buf_putc(buf, '\n');

	buf_puts(buf, "In directory ");
	buf_puts(buf, hostname);
	buf_puts(buf, ":");
	buf_puts(buf, dir);
	buf_putc(buf, '/');
	buf_puts(buf, repo);
	buf_putc(buf, '\n');
	buf_putc(buf, '\n');
}

