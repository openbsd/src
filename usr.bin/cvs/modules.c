/*	$OpenBSD: modules.c,v 1.3 2008/02/03 17:20:14 joris Exp $	*/
/*
 * Copyright (c) 2008 Joris Vink <joris@openbsd.org>
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

#include <sys/param.h>
#include <sys/dirent.h>
#include <sys/resource.h>

#include <stdlib.h>
#include <string.h>

#include "cvs.h"
#include "config.h"

TAILQ_HEAD(, module_info)	modules;

struct module_checkout *current_module = NULL;
char *module_repo_root = NULL;

void
cvs_parse_modules(void)
{
	cvs_log(LP_TRACE, "cvs_parse_modules()");

	TAILQ_INIT(&modules);
	cvs_read_config(CVS_PATH_MODULES, modules_parse_line);
}

void
modules_parse_line(char *line)
{
	int flags;
	char *val, *p, *module;
	struct module_info *mi;

	flags = 0;
	p = val = line;
	while (*p != ' ' && *p != '\t')
		p++;

	*(p++) = '\0';
	module = val;

	while (*p == ' ' || *p == '\t')
		p++;

	if (*p == '\0') {
		cvs_log(LP_NOTICE, "premature ending of CVSROOT/modules line");
		return;
	}

	val = p;
	while (*p != ' ' && *p != '\t')
		p++;

	if (*p == '\0') {
		cvs_log(LP_NOTICE, "premature ending of CVSROOT/modules line");
		return;
	}

	while (val[0] == '-') {
		p = val;
		while (*p != ' ' && *p != '\t' && *p != '\0')
			p++;

		if (*p == '\0') {
			cvs_log(LP_NOTICE,
			    "misplaced option in CVSROOT/modules");
			return;
		}

		*(p++) = '\0';

		switch (val[1]) {
		case 'a':
			if (flags & MODULE_TARGETDIR) {
				cvs_log(LP_NOTICE, "cannot use -a with -d");
				return;
			}
			flags |= MODULE_ALIAS;
			break;
		case 'd':
			if (flags & MODULE_ALIAS) {
				cvs_log(LP_NOTICE, "cannot use -d with -a");
				return;
			}
			flags |= MODULE_TARGETDIR;
			break;
		case 'l':
			flags |= MODULE_NORECURSE;
			break;
		}

		val = p;
	}

	mi = xmalloc(sizeof(*mi));
	mi->mi_name = xstrdup(module);
	mi->mi_repository = xstrdup(val);
	mi->mi_flags = flags;
	TAILQ_INSERT_TAIL(&modules, mi, m_list);
}

struct module_checkout *
cvs_module_lookup(char *name)
{
	struct module_checkout *mc;
	struct module_info *mi;

	mc = xmalloc(sizeof(*mc));

	TAILQ_FOREACH(mi, &modules, m_list) {
		if (!strcmp(name, mi->mi_name)) {
			mc = xmalloc(sizeof(*mc));
			mc->mc_repo = xstrdup(mi->mi_repository);
			if (mi->mi_flags & MODULE_ALIAS)
				mc->mc_wdir = xstrdup(mi->mi_repository);
			else
				mc->mc_wdir = xstrdup(mi->mi_name);
			mc->mc_flags = mi->mi_flags;
			return (mc);
		}
	}

	mc->mc_repo = xstrdup(name);
	mc->mc_wdir = xstrdup(name);
	mc->mc_flags |= MODULE_ALIAS;

	return (mc);
}
