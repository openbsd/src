/*	$OpenBSD: radiusd_bsdauth.c,v 1.1 2015/07/21 04:06:04 yasuoka Exp $	*/

/*
 * Copyright (c) 2015 YASUOKA Masahiko <yasuoka@yasuoka.net>
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

#include <bsd_auth.h>
#include <err.h>
#include <grp.h>
#include <login_cap.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "radiusd.h"
#include "radiusd_module.h"

struct module_bsdauth {
	struct module_base	 *base;
	char			**okgroups;
};

static void module_bsdauth_config_set(void *, const char *, int,
    char * const *);
static void module_bsdauth_userpass(void *, u_int, const char *, const char *);

static struct module_handlers module_bsdauth_handlers = {
	.userpass = module_bsdauth_userpass,
	.config_set = module_bsdauth_config_set
};

int
main(int argc, char *argv[])
{
	struct module_bsdauth	 module_bsdauth;

	memset(&module_bsdauth, 0, sizeof(module_bsdauth));

	openlog(NULL, LOG_PID, LOG_DAEMON);

	if ((module_bsdauth.base = module_create(STDIN_FILENO, &module_bsdauth,
	    &module_bsdauth_handlers)) == NULL)
		err(1, "Could not create a module instance");

	module_load(module_bsdauth.base);
	while (module_run(module_bsdauth.base) == 0)
		;

	module_destroy(module_bsdauth.base);

	exit(EXIT_SUCCESS);
}

static void
module_bsdauth_config_set(void *ctx, const char *name, int argc,
    char * const * argv)
{
	struct module_bsdauth	 *_this = ctx;
	int			  i;
	char			**groups = NULL;

	if (strcmp(name, "restrict-group") == 0) {
		if (_this->okgroups != NULL) {
			module_send_message(_this->base, IMSG_NG,
			    "`restrict-group' is already defined");
			goto on_error;
		}
		if ((groups = calloc(sizeof(char *), argc + 1)) == NULL) {
			module_send_message(_this->base, IMSG_NG,
			    "Out of memory");
			goto on_error;
		}
		for (i = 0; i < argc; i++) {
			if (getgrnam(argv[i]) == NULL) {
				module_send_message(_this->base, IMSG_NG,
				    "group `%s' is not found", argv[i]);
				endgrent();
				goto on_error;
			} else {
				if ((groups[i] = strdup(argv[i])) == NULL) {
					endgrent();
					module_send_message(_this->base,
					    IMSG_NG, "Out of memory");
					goto on_error;
				}
			}
		}
		groups[i] = NULL;
		_this->okgroups = groups;
		endgrent();
		module_send_message(_this->base, IMSG_OK, NULL);
	} else
		module_send_message(_this->base, IMSG_NG,
		    "Unknown config parameter `%s'", name);
	return;
on_error:
	if (groups != NULL) {
		for (i = 0; groups[i] != NULL; i++)
			free(groups[i]);
		free(groups);
	}
	return;
}


static void
module_bsdauth_userpass(void *ctx, u_int q_id, const char *user,
    const char *pass)
{
	struct module_bsdauth	*_this = ctx;
	u_int			 i, j;
	auth_session_t		*auth = NULL;
	struct passwd		*pw;
	struct group   		 gr0, *gr;
	char           		 g_buf[4096];
	const char		*reason;

	if (pass == NULL)
		pass = "";

	if ((auth = auth_usercheck((char *)user, NULL, NULL, (char *)pass))
	    == NULL || (auth_getstate(auth) & AUTH_OKAY) == 0) {
		reason = "Authentication failed";
		goto auth_ng;
	}
	if (_this->okgroups != NULL) {
		reason = "Group restriction is not allowed";
		auth_setpwd(auth, NULL);
		if ((pw = auth_getpwd(auth)) == NULL) {
			syslog(LOG_WARNING, "auth_getpwd() for user %s "
			    "failed: %m", user);
			goto auth_ng;
		}
		for (i = 0; _this->okgroups[i] != NULL; i++) {
			if (getgrnam_r(_this->okgroups[i], &gr0, g_buf,
			    sizeof(g_buf), &gr) == -1) {
				syslog(LOG_DEBUG, "group %s is not found",
				    _this->okgroups[i]);
				continue;
			}
			if (gr->gr_gid == pw->pw_gid)
				goto group_ok;
			for (j = 0; gr->gr_mem[j] != NULL; j++) {
				if (strcmp(gr->gr_mem[j], pw->pw_name) == 0)
					goto group_ok;
			}
		}
		endgrent();
		goto auth_ng;
group_ok:
		endgrent();
	}
	module_userpass_ok(_this->base, q_id, "Authentication succeeded");
	if (auth != NULL)
		auth_close(auth);
	return;
auth_ng:
	module_userpass_fail(_this->base, q_id, reason);
	if (auth != NULL)
		auth_close(auth);
	return;
}
