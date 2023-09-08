/*	$OpenBSD: radiusd_standard.c,v 1.1 2023/09/08 05:56:22 yasuoka Exp $	*/

/*
 * Copyright (c) 2013, 2023 Internet Initiative Japan Inc.
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
#include <sys/queue.h>

#include <err.h>
#include <errno.h>
#include <radius.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "radiusd.h"
#include "radiusd_module.h"

TAILQ_HEAD(attrs,attr);

struct attr {
	uint8_t			 type;
	uint32_t		 vendor;
	uint32_t		 vtype;
	TAILQ_ENTRY(attr)	 next;
};

struct module_standard {
	struct module_base	*base;
	bool			 strip_atmark_realm;
	bool			 strip_nt_domain;
	struct attrs		 remove_reqattrs;
	struct attrs		 remove_resattrs;
};

static void	 module_standard_config_set(void *, const char *, int,
		    char * const *);
static void	 module_standard_reqdeco(void *, u_int, const u_char *, size_t);
static void	 module_standard_resdeco(void *, u_int, const u_char *, size_t);

int
main(int argc, char *argv[])
{
	struct module_standard module_standard;
	struct module_handlers handlers = {
		.config_set = module_standard_config_set,
		.request_decoration = module_standard_reqdeco,
		.response_decoration = module_standard_resdeco
	};
	struct attr		*attr;

	memset(&module_standard, 0, sizeof(module_standard));
	TAILQ_INIT(&module_standard.remove_reqattrs);
	TAILQ_INIT(&module_standard.remove_resattrs);

	if ((module_standard.base = module_create(
	    STDIN_FILENO, &module_standard, &handlers)) == NULL)
		err(1, "Could not create a module instance");

	module_drop_privilege(module_standard.base);
	if (pledge("stdio", NULL) == -1)
		err(1, "pledge");

	module_load(module_standard.base);

	openlog(NULL, LOG_PID, LOG_DAEMON);

	while (module_run(module_standard.base) == 0)
		;

	module_destroy(module_standard.base);
	while ((attr = TAILQ_FIRST(&module_standard.remove_reqattrs)) != NULL) {
		TAILQ_REMOVE(&module_standard.remove_reqattrs, attr, next);
		freezero(attr, sizeof(struct attr));
	}
	while ((attr = TAILQ_FIRST(&module_standard.remove_resattrs)) != NULL) {
		TAILQ_REMOVE(&module_standard.remove_resattrs, attr, next);
		freezero(attr, sizeof(struct attr));
	}

	exit(EXIT_SUCCESS);
}

static void
module_standard_config_set(void *ctx, const char *name, int argc,
    char * const * argv)
{
	struct module_standard	*module = ctx;
	struct attr		*attr;
	const char		*errmsg = "none";
	const char		*errstr;

	if (strcmp(name, "strip-atmark-realm") == 0) {
		SYNTAX_ASSERT(argc == 1,
		    "`strip-atmark-realm' must have only one argment");
		if (strcmp(argv[0], "true") == 0)
			module->strip_atmark_realm = true;
		else if (strcmp(argv[0], "false") == 0)
			module->strip_atmark_realm = false;
		else
			SYNTAX_ASSERT(0,
			    "`strip-atmark-realm' must `true' or `false'");
	} else if (strcmp(name, "strip-nt-domain") == 0) {
		SYNTAX_ASSERT(argc == 1,
		    "`strip-nt-domain' must have only one argment");
		if (strcmp(argv[0], "true") == 0)
			module->strip_nt_domain = true;
		else if (strcmp(argv[0], "false") == 0)
			module->strip_nt_domain = false;
		else
			SYNTAX_ASSERT(0,
			    "`strip-nt-domain' must `true' or `false'");
	} else if (strcmp(name, "remove-request-attribute") == 0 ||
	    strcmp(name, "remove-response-attribute") == 0) {
		struct attrs		*attrs;

		if (strcmp(name, "remove-request-attribute") == 0) {
			SYNTAX_ASSERT(argc == 1 || argc == 2,
			    "`remove-request-attribute' must have one or two "
			    "argment");
			attrs = &module->remove_reqattrs;
		} else {
			SYNTAX_ASSERT(argc == 1 || argc == 2,
			    "`remove-response-attribute' must have one or two "
			    "argment");
			attrs = &module->remove_resattrs;
		}
		if ((attr = calloc(1, sizeof(struct attr))) == NULL) {
			module_send_message(module->base, IMSG_NG,
			    "Out of memory: %s", strerror(errno));
		}
		if (argc == 1) {
			attr->type = strtonum(argv[0], 0, 255, &errstr);
			if (errstr == NULL &&
			    attr->type != RADIUS_TYPE_VENDOR_SPECIFIC) {
				TAILQ_INSERT_TAIL(attrs, attr, next);
				attr = NULL;
			}
		} else {
			attr->type = RADIUS_TYPE_VENDOR_SPECIFIC;
			attr->vendor = strtonum(argv[0], 0, UINT32_MAX,
			    &errstr);
			if (errstr == NULL)
				attr->vtype = strtonum(argv[1], 0, 255,
				    &errstr);
			if (errstr == NULL) {
				TAILQ_INSERT_TAIL(attrs, attr, next);
				attr = NULL;
			}
		}
		freezero(attr, sizeof(struct attr));
		if (strcmp(name, "remove-request-attribute") == 0)
			SYNTAX_ASSERT(attr == NULL,
			    "wrong number for `remove-request-attribute`");
		else
			SYNTAX_ASSERT(attr == NULL,
			    "wrong number for `remove-response-attribute`");
	} else if (strncmp(name, "_", 1) == 0)
		/* nothing */; /* ignore all internal messages */
	else {
		module_send_message(module->base, IMSG_NG,
		    "Unknown config parameter name `%s'", name);
		return;
	}
	module_send_message(module->base, IMSG_OK, NULL);
	return;

 syntax_error:
	module_send_message(module->base, IMSG_NG, "%s", errmsg);
}

/* request message decoration */
static void
module_standard_reqdeco(void *ctx, u_int q_id, const u_char *pkt, size_t pktlen)
{
	struct module_standard	*module = ctx;
	RADIUS_PACKET		*radpkt = NULL;
	int			 changed = 0;
	char			*ch, *username, buf[256];
	struct attr		*attr;

	if (module->strip_atmark_realm || module->strip_nt_domain) {
		if ((radpkt = radius_convert_packet(pkt, pktlen)) == NULL) {
			syslog(LOG_ERR,
			    "%s: radius_convert_packet() failed: %m", __func__);
			module_stop(module->base);
			return;
		}

		username = buf;
		if (radius_get_string_attr(radpkt, RADIUS_TYPE_USER_NAME,
		    username, sizeof(buf)) != 0) {
			syslog(LOG_WARNING,
			    "standard: q=%u could not get User-Name attribute",
			    q_id);
			goto skip;
		}

		if (module->strip_atmark_realm &&
		    (ch = strrchr(username, '@')) != NULL) {
			*ch = '\0';
			changed++;
		}
		if (module->strip_nt_domain &&
		    (ch = strchr(username, '\\')) != NULL) {
			username = ch + 1;
			changed++;
		}
		if (changed > 0) {
			radius_del_attr_all(radpkt, RADIUS_TYPE_USER_NAME);
			radius_put_string_attr(radpkt,
			    RADIUS_TYPE_USER_NAME, username);
		}
	}
 skip:
	TAILQ_FOREACH(attr, &module->remove_reqattrs, next) {
		if (radpkt == NULL &&
		    (radpkt = radius_convert_packet(pkt, pktlen)) == NULL) {
			syslog(LOG_ERR,
			    "%s: radius_convert_packet() failed: %m", __func__);
			module_stop(module->base);
			return;
		}
		if (attr->type != RADIUS_TYPE_VENDOR_SPECIFIC)
			radius_del_attr_all(radpkt, attr->type);
		else
			radius_del_vs_attr_all(radpkt, attr->vendor,
			    attr->vtype);
	}
	if (radpkt == NULL) {
		pkt = NULL;
		pktlen = 0;
	} else {
		pkt = radius_get_data(radpkt);
		pktlen = radius_get_length(radpkt);
	}
	if (module_reqdeco_done(module->base, q_id, pkt, pktlen) == -1) {
		syslog(LOG_ERR, "%s: module_reqdeco_done() failed: %m",
		    __func__);
		module_stop(module->base);
	}
	if (radpkt != NULL)
		radius_delete_packet(radpkt);
}

/* response message decoration */
static void
module_standard_resdeco(void *ctx, u_int q_id, const u_char *pkt, size_t pktlen)
{
	struct module_standard	*module = ctx;
	RADIUS_PACKET		*radpkt = NULL;
	struct attr		*attr;

	TAILQ_FOREACH(attr, &module->remove_reqattrs, next) {
		if (radpkt == NULL &&
		    (radpkt = radius_convert_packet(pkt, pktlen)) == NULL) {
			syslog(LOG_ERR,
			    "%s: radius_convert_packet() failed: %m", __func__);
			module_stop(module->base);
			return;
		}
		if (attr->type != RADIUS_TYPE_VENDOR_SPECIFIC)
			radius_del_attr_all(radpkt, attr->type);
		else
			radius_del_vs_attr_all(radpkt, attr->vendor,
			    attr->vtype);
	}
	if (radpkt == NULL) {
		pkt = NULL;
		pktlen = 0;
	} else {
		pkt = radius_get_data(radpkt);
		pktlen = radius_get_length(radpkt);
	}
	if (module_resdeco_done(module->base, q_id, pkt, pktlen) == -1) {
		syslog(LOG_ERR, "%s: module_resdeco_done() failed: %m",
		    __func__);
		module_stop(module->base);
	}
	if (radpkt != NULL)
		radius_delete_packet(radpkt);
}
