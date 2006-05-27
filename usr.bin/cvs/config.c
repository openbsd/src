/*	$OpenBSD: config.c,v 1.2 2006/05/27 21:10:53 joris Exp $	*/
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
#include "config.h"
#include "diff.h"
#include "log.h"
#include "proto.h"

void
cvs_parse_configfile(void)
{
	int i;
	FILE *fp;
	size_t len;
	const char *errstr;
	char *p, *buf, *lbuf, *opt, *val, fpath[MAXPATHLEN];

	i = snprintf(fpath, sizeof(fpath), "%s/%s", current_cvsroot->cr_dir,
	    CVS_PATH_CONFIG);
	if (i == -1 || i >= (int)sizeof(fpath))
		fatal("cvs_parse_configfile: overflow");

	if ((fp = fopen(fpath, "r")) == NULL)
		fatal("cvs_config_parse: %s: %s",
		    CVS_PATH_CONFIG, strerror(errno));

	lbuf = NULL;
	while ((buf = fgetln(fp, &len))) {
		if (buf[len - 1] == '\n') {
			buf[len - 1] = '\0';
		} else {
			lbuf = xmalloc(len + 1);
			strlcpy(lbuf, buf, len);
			buf = lbuf;
		}

		p = buf;
		while (*p == ' ')
			p++;

		if (p[0] == '#' || p[0] == '\0') {
			if (lbuf != NULL)
				xfree(lbuf);
			continue;
		}

		opt = p;
		if ((val = strrchr(opt, '=')) == NULL)
			fatal("cvs_parse_configfile: bad option '%s'", opt);

		*(val++) = '\0';

		if (!strcmp(opt, "tag")) {
			cvs_tagname = xstrdup(val);
		} else if (!strcmp(opt, "umask")) {
			cvs_umask = (int)strtonum(val, 0, INT_MAX, &errstr);
			if (errstr != NULL)
				fatal("cvs_parse_configfile: %s: %s", val,
				    errstr);
		} else {
			cvs_log(LP_ERR, "ignoring unknown option '%s'", opt);
		}

		if (lbuf != NULL)
			xfree(lbuf);
	}

	(void)fclose(fp);
}
