/* $OpenBSD: pop_auth.c,v 1.2 2001/09/21 20:22:06 camield Exp $ */

/*
 * AUTHORIZATION state handling.
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <syslog.h>

#include "misc.h"
#include "params.h"
#include "protocol.h"
#include "pop_auth.h"
#if POP_VIRTUAL
#include "virtual.h"
#endif

static char *pop_user, *pop_pass;

static int pop_auth_quit(char *params)
{
	if (params) return POP_ERROR;
	return POP_LEAVE;
}

static int pop_auth_user(char *params)
{
	char *user;

	user = pop_get_param(&params);
	if (!user || pop_user || params) return POP_ERROR;
	if (!(pop_user = strdup(user))) return POP_CRASH_SERVER;
	return POP_OK;
}

static int pop_auth_pass(char *params)
{
	if (!params || !pop_user) return POP_ERROR;
	if (!(pop_pass = strdup(params))) return POP_CRASH_SERVER;
	return POP_STATE;
}

static struct pop_command pop_auth_commands[] = {
	{"QUIT", pop_auth_quit},
	{"USER", pop_auth_user},
	{"PASS", pop_auth_pass},
	{NULL}
};

int do_pop_auth(int channel)
{
	pop_init();

	if (pop_reply_ok()) return 1;

	pop_user = NULL;
	if (pop_handle_state(pop_auth_commands) == POP_STATE) {
		pop_clean();
		write_loop(channel, (char *)&pop_buffer, sizeof(pop_buffer));
		write_loop(channel, pop_user, strlen(pop_user) + 1);
		write_loop(channel, pop_pass, strlen(pop_pass) + 1);
		if (close(channel)) return 1;
	}

	return 0;
}

void log_pop_auth(int result, char *user)
{
	if (result == AUTH_NONE) {
		syslog(SYSLOG_PRI_LO, "Didn't attempt authentication");
		return;
	}

#if POP_VIRTUAL
	if (virtual_domain) {
		syslog(result == AUTH_OK ? SYSLOG_PRI_LO : SYSLOG_PRI_HI,
			"Authentication %s for %s@%s",
			result == AUTH_OK ? "passed" : "failed",
			user ? user : "UNKNOWN USER",
			virtual_domain);
		return;
	}
#endif
	syslog(result == AUTH_OK ? SYSLOG_PRI_LO : SYSLOG_PRI_HI,
		"Authentication %s for %s",
		result == AUTH_OK ? "passed" : "failed",
		user ? user : "UNKNOWN USER");
}
