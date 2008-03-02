/*	$OpenBSD: config.h,v 1.10 2008/03/02 11:58:45 joris Exp $	*/
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

#ifndef CONFIG_H
#define CONFIG_H

void cvs_modules_list(void);

void cvs_read_config(char *name, int (*cb)(char *, int));

void cvs_parse_configfile(void);
void cvs_parse_modules(void);

int config_parse_line(char *, int);
int modules_parse_line(char *, int);

#include <sys/queue.h>
#include "file.h"

/* module stuff */

#define MODULE_ALIAS		0x01
#define MODULE_TARGETDIR	0x02
#define MODULE_NORECURSE	0x04
#define MODULE_RUN_ON_COMMIT	0x08
#define MODULE_RUN_ON_CHECKOUT	0x10

struct module_checkout {
	char			*mc_name;
	char			*mc_prog;

	int			 mc_flags;
	int			 mc_canfree;

	struct cvs_flisthead	 mc_modules;
	struct cvs_flisthead	 mc_ignores;
};

struct module_info {
	char				*mi_name;
	char				*mi_prog;
	char				*mi_str;
	int				 mi_flags;

	struct cvs_flisthead		 mi_modules;
	struct cvs_flisthead		 mi_ignores;

	TAILQ_ENTRY(module_info)	 m_list;
};

struct module_checkout *cvs_module_lookup(char *);
#endif
