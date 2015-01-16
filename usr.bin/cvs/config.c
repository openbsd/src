/*	$OpenBSD: config.c,v 1.16 2015/01/16 06:40:07 deraadt Exp $	*/
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

#include <sys/types.h>
#include <sys/dirent.h>
#include <sys/resource.h>

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "cvs.h"
#include "config.h"

void
cvs_parse_configfile(void)
{
	cvs_log(LP_TRACE, "cvs_parse_configfile()");
	cvs_read_config(CVS_PATH_CONFIG, config_parse_line);
}

int
config_parse_line(char *line, int lineno)
{
	struct rlimit rl;
	const char *errstr;
	char *val, *opt, *ep;

	opt = line;
	if ((val = strrchr(opt, '=')) == NULL)
		fatal("cvs_parse_configfile: bad option '%s'", opt);

	*(val++) = '\0';

	if (!strcmp(opt, "tag")) {
		if (cvs_tagname != NULL)
			xfree(cvs_tagname);
		cvs_tagname = xstrdup(val);
	} else if (!strcmp(opt, "umask")) {
		cvs_umask = strtol(val, &ep, 8);

		if (val == ep || *ep != '\0')
			fatal("cvs_parse_configfile: umask %s is "
			    "invalid", val);
		if (cvs_umask < 0 || cvs_umask > 07777)
			fatal("cvs_parse_configfile: umask %s is "
			    "invalid", val);
	} else if (!strcmp(opt, "dlimit")) {
		if (getrlimit(RLIMIT_DATA, &rl) != -1) {
			rl.rlim_cur = (int)strtonum(val, 0, INT_MAX,
			    &errstr);
			if (errstr != NULL)
				fatal("cvs_parse_configfile: %s: %s",
				    val, errstr);
			rl.rlim_cur = rl.rlim_cur * 1024;
			(void)setrlimit(RLIMIT_DATA, &rl);
		}
	} else {
		cvs_log(LP_ERR, "ignoring unknown option '%s'", opt);
	}

	return (0);
}

void
cvs_read_config(char *name, int (*cb)(char *, int))
{
	FILE *fp;
	size_t len;
	int lineno;
	char *p, *buf, *lbuf, fpath[PATH_MAX];

	(void)xsnprintf(fpath, sizeof(fpath), "%s/%s",
	    current_cvsroot->cr_dir, name);

	if ((fp = fopen(fpath, "r")) == NULL)
		return;

	lbuf = NULL;
	lineno = 0;
	while ((buf = fgetln(fp, &len)) != NULL) {
		lineno++;
		if (buf[len - 1] == '\n') {
			buf[len - 1] = '\0';
		} else {
			lbuf = xmalloc(len + 1);
			memcpy(lbuf, buf, len);
			lbuf[len] = '\0';
			buf = lbuf;
		}

		p = buf;
		while (*p == ' ' || *p == '\t')
			p++;

		if (p[0] == '#' || p[0] == '\0')
			continue;

		if (cb(p, lineno) < 0)
			break;
	}

	if (lbuf != NULL)
		xfree(lbuf);

	(void)fclose(fp);
}
