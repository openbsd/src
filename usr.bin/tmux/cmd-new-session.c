/* $OpenBSD: cmd-new-session.c,v 1.120 2019/06/03 18:28:37 nicm Exp $ */

/*
 * Copyright (c) 2007 Nicholas Marriott <nicholas.marriott@gmail.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "tmux.h"

/*
 * Create a new session and attach to the current terminal unless -d is given.
 */

#define NEW_SESSION_TEMPLATE "#{session_name}:"

static enum cmd_retval	cmd_new_session_exec(struct cmd *, struct cmdq_item *);

const struct cmd_entry cmd_new_session_entry = {
	.name = "new-session",
	.alias = "new",

	.args = { "Ac:dDEF:n:Ps:t:x:Xy:", 0, -1 },
	.usage = "[-AdDEPX] [-c start-directory] [-F format] [-n window-name] "
		 "[-s session-name] " CMD_TARGET_SESSION_USAGE " [-x width] "
		 "[-y height] [command]",

	.target = { 't', CMD_FIND_SESSION, CMD_FIND_CANFAIL },

	.flags = CMD_STARTSERVER,
	.exec = cmd_new_session_exec
};

const struct cmd_entry cmd_has_session_entry = {
	.name = "has-session",
	.alias = "has",

	.args = { "t:", 0, 0 },
	.usage = CMD_TARGET_SESSION_USAGE,

	.target = { 't', CMD_FIND_SESSION, 0 },

	.flags = 0,
	.exec = cmd_new_session_exec
};

static enum cmd_retval
cmd_new_session_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args		*args = self->args;
	struct client		*c = item->client;
	struct session		*s, *as, *groupwith;
	struct environ		*env;
	struct options		*oo;
	struct termios		 tio, *tiop;
	struct session_group	*sg;
	const char		*errstr, *template, *group, *prefix, *tmp;
	char			*cause, *cwd = NULL, *cp, *newname = NULL;
	int			 detached, already_attached, is_control = 0;
	u_int			 sx, sy, dsx, dsy;
	struct spawn_context	 sc;
	enum cmd_retval		 retval;
	struct cmd_find_state    fs;

	if (self->entry == &cmd_has_session_entry) {
		/*
		 * cmd_find_target() will fail if the session cannot be found,
		 * so always return success here.
		 */
		return (CMD_RETURN_NORMAL);
	}

	if (args_has(args, 't') && (args->argc != 0 || args_has(args, 'n'))) {
		cmdq_error(item, "command or window name given with target");
		return (CMD_RETURN_ERROR);
	}

	if (args_has(args, 's')) {
		newname = format_single(item, args_get(args, 's'), c, NULL,
		    NULL, NULL);
		if (!session_check_name(newname)) {
			cmdq_error(item, "bad session name: %s", newname);
			goto fail;
		}
		if ((as = session_find(newname)) != NULL) {
			if (args_has(args, 'A')) {
				retval = cmd_attach_session(item,
				    newname, args_has(args, 'D'),
				    args_has(args, 'X'), 0, NULL,
				    args_has(args, 'E'));
				free(newname);
				return (retval);
			}
			cmdq_error(item, "duplicate session: %s", newname);
			goto fail;
		}
	}

	/* Is this going to be part of a session group? */
	group = args_get(args, 't');
	if (group != NULL) {
		groupwith = item->target.s;
		if (groupwith == NULL) {
			if (!session_check_name(group)) {
				cmdq_error(item, "bad group name: %s", group);
				goto fail;
			}
			sg = session_group_find(group);
		} else
			sg = session_group_contains(groupwith);
		if (sg != NULL)
			prefix = sg->name;
		else if (groupwith != NULL)
			prefix = groupwith->name;
		else
			prefix = group;
	} else {
		groupwith = NULL;
		sg = NULL;
		prefix = NULL;
	}

	/* Set -d if no client. */
	detached = args_has(args, 'd');
	if (c == NULL)
		detached = 1;
	else if (c->flags & CLIENT_CONTROL)
		is_control = 1;

	/* Is this client already attached? */
	already_attached = 0;
	if (c != NULL && c->session != NULL)
		already_attached = 1;

	/* Get the new session working directory. */
	if ((tmp = args_get(args, 'c')) != NULL)
		cwd = format_single(item, tmp, c, NULL, NULL, NULL);
	else
		cwd = xstrdup(server_client_get_cwd(c, NULL));

	/*
	 * If this is a new client, check for nesting and save the termios
	 * settings (part of which is used for new windows in this session).
	 *
	 * tcgetattr() is used rather than using tty.tio since if the client is
	 * detached, tty_open won't be called. It must be done before opening
	 * the terminal as that calls tcsetattr() to prepare for tmux taking
	 * over.
	 */
	if (!detached && !already_attached && c->tty.fd != -1) {
		if (server_client_check_nested(item->client)) {
			cmdq_error(item, "sessions should be nested with care, "
			    "unset $TMUX to force");
			goto fail;
		}
		if (tcgetattr(c->tty.fd, &tio) != 0)
			fatal("tcgetattr failed");
		tiop = &tio;
	} else
		tiop = NULL;

	/* Open the terminal if necessary. */
	if (!detached && !already_attached) {
		if (server_client_open(c, &cause) != 0) {
			cmdq_error(item, "open terminal failed: %s", cause);
			free(cause);
			goto fail;
		}
	}

	/* Get default session size. */
	if (args_has(args, 'x')) {
		tmp = args_get(args, 'x');
		if (strcmp(tmp, "-") == 0) {
			if (c != NULL)
				dsx = c->tty.sx;
			else
				dsx = 80;
		} else {
			dsx = strtonum(tmp, 1, USHRT_MAX, &errstr);
			if (errstr != NULL) {
				cmdq_error(item, "width %s", errstr);
				goto fail;
			}
		}
	}
	if (args_has(args, 'y')) {
		tmp = args_get(args, 'y');
		if (strcmp(tmp, "-") == 0) {
			if (c != NULL)
				dsy = c->tty.sy;
			else
				dsy = 24;
		} else {
			dsy = strtonum(tmp, 1, USHRT_MAX, &errstr);
			if (errstr != NULL) {
				cmdq_error(item, "height %s", errstr);
				goto fail;
			}
		}
	}

	/* Find new session size. */
	if (!detached && !is_control) {
		sx = c->tty.sx;
		sy = c->tty.sy;
		if (sy > 0 && options_get_number(global_s_options, "status"))
			sy--;
	} else {
		tmp = options_get_string(global_s_options, "default-size");
		if (sscanf(tmp, "%ux%u", &sx, &sy) != 2) {
			sx = 80;
			sy = 24;
		}
		if (args_has(args, 'x'))
			sx = dsx;
		if (args_has(args, 'y'))
			sy = dsy;
	}
	if (sx == 0)
		sx = 1;
	if (sy == 0)
		sy = 1;

	/* Create the new session. */
	oo = options_create(global_s_options);
	if (args_has(args, 'x') || args_has(args, 'y')) {
		if (!args_has(args, 'x'))
			dsx = sx;
		if (!args_has(args, 'y'))
			dsy = sy;
		options_set_string(oo, "default-size", 0, "%ux%u", dsx, dsy);
	}
	env = environ_create();
	if (c != NULL && !args_has(args, 'E'))
		environ_update(global_s_options, c->environ, env);
	s = session_create(prefix, newname, cwd, env, oo, tiop);

	/* Spawn the initial window. */
	memset(&sc, 0, sizeof sc);
	sc.item = item;
	sc.s = s;

	sc.name = args_get(args, 'n');
	sc.argc = args->argc;
	sc.argv = args->argv;

	sc.idx = -1;
	sc.cwd = args_get(args, 'c');

	sc.flags = 0;

	if (spawn_window(&sc, &cause) == NULL) {
		session_destroy(s, 0, __func__);
		cmdq_error(item, "create window failed: %s", cause);
		free(cause);
		goto fail;
	}

	/*
	 * If a target session is given, this is to be part of a session group,
	 * so add it to the group and synchronize.
	 */
	if (group != NULL) {
		if (sg == NULL) {
			if (groupwith != NULL) {
				sg = session_group_new(groupwith->name);
				session_group_add(sg, groupwith);
			} else
				sg = session_group_new(group);
		}
		session_group_add(sg, s);
		session_group_synchronize_to(s);
		session_select(s, RB_MIN(winlinks, &s->windows)->idx);
	}
	notify_session("session-created", s);

	/*
	 * Set the client to the new session. If a command client exists, it is
	 * taking this session and needs to get MSG_READY and stay around.
	 */
	if (!detached) {
		if (!already_attached) {
			if (~c->flags & CLIENT_CONTROL)
				proc_send(c->peer, MSG_READY, -1, NULL, 0);
		} else if (c->session != NULL)
			c->last_session = c->session;
		c->session = s;
		if (~item->shared->flags & CMDQ_SHARED_REPEAT)
			server_client_set_key_table(c, NULL);
		tty_update_client_offset(c);
		status_timer_start(c);
		notify_client("client-session-changed", c);
		session_update_activity(s, NULL);
		gettimeofday(&s->last_attached_time, NULL);
		server_redraw_client(c);
	}
	recalculate_sizes();
	server_update_socket();

	/*
	 * If there are still configuration file errors to display, put the new
	 * session's current window into more mode and display them now.
	 */
	if (cfg_finished)
		cfg_show_causes(s);

	/* Print if requested. */
	if (args_has(args, 'P')) {
		if ((template = args_get(args, 'F')) == NULL)
			template = NEW_SESSION_TEMPLATE;
		cp = format_single(item, template, c, s, NULL, NULL);
		cmdq_print(item, "%s", cp);
		free(cp);
	}

	if (!detached) {
		c->flags |= CLIENT_ATTACHED;
		cmd_find_from_session(&item->shared->current, s, 0);
	}

	cmd_find_from_session(&fs, s, 0);
	cmdq_insert_hook(s, item, &fs, "after-new-session");

	free(cwd);
	free(newname);
	return (CMD_RETURN_NORMAL);

fail:
	free(cwd);
	free(newname);
	return (CMD_RETURN_ERROR);
}
