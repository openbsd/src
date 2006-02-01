/*	$OpenBSD: ifstated.c,v 1.26 2006/02/01 23:13:09 mpf Exp $	*/

/*
 * Copyright (c) 2004 Marco Pfatschbacher <mpf@openbsd.org>
 * Copyright (c) 2004 Ryan McBride <mcbride@openbsd.org>
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

/*
 * ifstated listens to link_state transitions on interfaces
 * and executes predefined commands.
 */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/wait.h>

#include <net/if.h>
#include <net/route.h>
#include <netinet/in.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <err.h>
#include <event.h>
#include <util.h>
#include <unistd.h>
#include <syslog.h>
#include <stdarg.h>
#include <ifaddrs.h>

#include "ifstated.h"

struct	 ifsd_config *conf = NULL, *newconf = NULL;

int	 opts = 0;
int	 opt_debug = 0;
int	 opt_inhibit = 0;
char	*configfile = "/etc/ifstated.conf";
struct event	rt_msg_ev, sighup_ev, startup_ev, sigchld_ev;

void	startup_handler(int, short, void *);
void	sighup_handler(int, short, void *);
int	load_config(void);
void	sigchld_handler(int, short, void *);
void	rt_msg_handler(int, short, void *);
void	external_handler(int, short, void *);
void	external_exec(struct ifsd_external *, int);
void	check_external_status(struct ifsd_state *);
void	external_evtimer_setup(struct ifsd_state *, int);
void	scan_ifstate(int, int, int);
int	scan_ifstate_single(int, int, struct ifsd_state *);
void	fetch_state(void);
void	usage(void);
void	adjust_expressions(struct ifsd_expression_list *, int);
void	adjust_external_expressions(struct ifsd_state *);
void	eval_state(struct ifsd_state *);
int	state_change(void);
void	do_action(struct ifsd_action *);
void	remove_action(struct ifsd_action *, struct ifsd_state *);
void	remove_expression(struct ifsd_expression *, struct ifsd_state *);
void	log_init(int);
void	logit(int, const char *, ...);

void
usage(void)
{
	extern char *__progname;

	fprintf(stderr, "usage: %s [-dhinv] [-D macro=value] [-f file]\n",
	    __progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	struct timeval tv;
	int ch;

	while ((ch = getopt(argc, argv, "dD:f:hniv")) != -1) {
		switch (ch) {
		case 'd':
			opt_debug = 1;
			break;
		case 'D':
			if (cmdline_symset(optarg) < 0)
				errx(1, "could not parse macro definition %s",
				    optarg);
			break;
		case 'f':
			configfile = optarg;
			break;
		case 'h':
			usage();
			break;
		case 'n':
			opts |= IFSD_OPT_NOACTION;
			break;
		case 'i':
			opt_inhibit = 1;
			break;
		case 'v':
			if (opts & IFSD_OPT_VERBOSE)
				opts |= IFSD_OPT_VERBOSE2;
			opts |= IFSD_OPT_VERBOSE;
			break;
		default:
			usage();
		}
	}

	if (opts & IFSD_OPT_NOACTION) {
		if ((newconf = parse_config(configfile, opts)) == NULL)
			exit(1);
		warnx("configuration OK");
		exit(0);
	}

	if (!opt_debug) {
		daemon(0, 0);
		setproctitle(NULL);
	}

	event_init();
	log_init(opt_debug);

	signal_set(&sigchld_ev, SIGCHLD, sigchld_handler, &sigchld_ev);
	signal_add(&sigchld_ev, NULL);

	/* Loading the config needs to happen in the event loop */
	tv.tv_usec = 0;
	tv.tv_sec = 0;
	evtimer_set(&startup_ev, startup_handler, &startup_ev);
	evtimer_add(&startup_ev, &tv);

	event_loop(0);
	exit(0);
}

void
startup_handler(int fd, short event, void *arg)
{
	int rt_fd;

	if ((rt_fd = socket(PF_ROUTE, SOCK_RAW, 0)) < 0)
		err(1, "no routing socket");

	if (load_config() != 0) {
		logit(IFSD_LOG_NORMAL, "unable to load config");
		exit(1);
	}

	event_set(&rt_msg_ev, rt_fd, EV_READ|EV_PERSIST,
	    rt_msg_handler, &rt_msg_ev);
	event_add(&rt_msg_ev, NULL);

	signal_set(&sighup_ev, SIGHUP, sighup_handler, &sighup_ev);
	signal_add(&sighup_ev, NULL);

	logit(IFSD_LOG_NORMAL, "started");
}

void
sighup_handler(int fd, short event, void *arg)
{
	logit(IFSD_LOG_NORMAL, "reloading config");
	if (load_config() != 0)
		logit(IFSD_LOG_NORMAL, "unable to reload config");
}

int
load_config(void)
{
	if ((newconf = parse_config(configfile, opts)) == NULL)
		return (-1);
	if (conf != NULL)
		clear_config(conf);
	conf = newconf;
	conf->always.entered = time(NULL);
	fetch_state();
	external_evtimer_setup(&conf->always, IFSD_EVTIMER_ADD);
	adjust_external_expressions(&conf->always);
	eval_state(&conf->always);
	if (conf->curstate != NULL) {
		logit(IFSD_LOG_NORMAL,
		    "initial state: %s", conf->curstate->name);
		conf->curstate->entered = time(NULL);
		conf->nextstate = conf->curstate;
		conf->curstate = NULL;
		while (state_change())
			do_action(conf->curstate->always);
	}
	return (0);
}

void
rt_msg_handler(int fd, short event, void *arg)
{
	char msg[2048];
	struct rt_msghdr *rtm = (struct rt_msghdr *)&msg;
	struct if_msghdr ifm;
	int len;

	len = read(fd, msg, sizeof(msg));

	/* XXX ignore errors? */
	if (len < sizeof(struct rt_msghdr))
		return;

	if (rtm->rtm_version != RTM_VERSION)
		return;

	if (rtm->rtm_type != RTM_IFINFO)
		return;

	memcpy(&ifm, rtm, sizeof(ifm));
	scan_ifstate(ifm.ifm_index, ifm.ifm_data.ifi_link_state, 1);
}

void
sigchld_handler(int fd, short event, void *arg)
{
	check_external_status(&conf->always);
	if (conf->curstate != NULL)
		check_external_status(conf->curstate);
}

void
external_handler(int fd, short event, void *arg)
{
	struct ifsd_external *external = (struct ifsd_external *)arg;
	struct timeval tv;

	/* re-schedule */
	tv.tv_usec = 0;
	tv.tv_sec = external->frequency;
	evtimer_set(&external->ev, external_handler, external);
	evtimer_add(&external->ev, &tv);

	/* execute */
	external_exec(external, 1);
}

void
external_exec(struct ifsd_external *external, int async)
{
	char *argp[] = {"sh", "-c", NULL, NULL};
	pid_t pid;
	int s;

	if (external->pid > 0) {
		logit(IFSD_LOG_NORMAL,
		    "previous command %s [%d] still running, killing it",
		    external->command, external->pid);
		kill(external->pid, SIGKILL);
		waitpid(external->pid, &s, 0);
		external->pid = 0;
	}

	argp[2] = external->command;
	logit(IFSD_LOG_VERBOSE, "running %s", external->command);
	pid = fork();
	if (pid < 0) {
		logit(IFSD_LOG_QUIET, "fork error");
	} else if (pid == 0) {
		execv("/bin/sh", argp);
		_exit(1);
		/* NOTREACHED */
	} else {
		external->pid = pid;
	}
	if (!async) {
		waitpid(external->pid, &s, 0);
		external->pid = 0;
		if (WIFEXITED(s))
			external->prevstatus = WEXITSTATUS(s);
	}
}

void
adjust_external_expressions(struct ifsd_state *state)
{
	struct ifsd_external *external;
	struct ifsd_expression_list expressions;

	TAILQ_INIT(&expressions);
	TAILQ_FOREACH(external, &state->external_tests, entries) {
		struct ifsd_expression *expression;

		if (external->prevstatus == -1)
			continue;

		TAILQ_FOREACH(expression, &external->expressions, entries) {
			TAILQ_INSERT_TAIL(&expressions,
			    expression, eval);
			if (external->prevstatus == 0)
				expression->truth = 1;
			else
				expression->truth = 0;
		}
		adjust_expressions(&expressions, conf->maxdepth);
	}
}

void
check_external_status(struct ifsd_state *state)
{
	struct ifsd_external *external, *end = NULL;
	int status, s, changed = 0;

	/* Do this manually; change ordering so the oldest is first */
	external = TAILQ_FIRST(&state->external_tests);
	while (external != NULL && external != end) {
		struct ifsd_external *newexternal;

		newexternal = TAILQ_NEXT(external, entries);

		if (external->pid <= 0)
			goto loop;

		if (wait4(external->pid, &s, WNOHANG, NULL) == 0)
			goto loop;

		external->pid = 0;
		if (end == NULL)
			end = external;
		if (WIFEXITED(s))
			status = WEXITSTATUS(s);
		else {
			logit(IFSD_LOG_QUIET,
			    "%s exited abnormally", external->command);
			goto loop;
		}

		if (external->prevstatus != status &&
		    (external->prevstatus != -1 || !opt_inhibit)) {
			changed = 1;
			external->prevstatus = status;
		}
		external->lastexec = time(NULL);
		TAILQ_REMOVE(&state->external_tests, external, entries);
		TAILQ_INSERT_TAIL(&state->external_tests, external, entries);
loop:
		external = newexternal;
	}

	if (changed) {
		adjust_external_expressions(state);
		eval_state(state);
	}
}

void
external_evtimer_setup(struct ifsd_state *state, int action)
{
	struct ifsd_external *external;
	int s;

	if (state != NULL) {
		switch (action) {
		case IFSD_EVTIMER_ADD:
			TAILQ_FOREACH(external,
			    &state->external_tests, entries) {
				struct timeval tv;

				/* run it once right away */
				external_exec(external, 0);

				/* schedule it for later */
				tv.tv_usec = 0;
				tv.tv_sec = external->frequency;
				evtimer_set(&external->ev, external_handler,
				    external);
				evtimer_add(&external->ev, &tv);
			}
			break;
		case IFSD_EVTIMER_DEL:
			TAILQ_FOREACH(external,
			    &state->external_tests, entries) {
				if (external->pid > 0) {
					kill(external->pid, SIGKILL);
					waitpid(external->pid, &s, 0);
					external->pid = 0;
				}
				evtimer_del(&external->ev);
			}
			break;
		}
	}
}

int
scan_ifstate_single(int ifindex, int s, struct ifsd_state *state)
{
	struct ifsd_ifstate *ifstate;
	struct ifsd_expression_list expressions;
	int changed = 0;

	TAILQ_INIT(&expressions);

	TAILQ_FOREACH(ifstate, &state->interface_states, entries) {
		if (ifstate->ifindex == ifindex) {
			if (ifstate->prevstate != s &&
			    (ifstate->prevstate != -1 || !opt_inhibit)) {
				struct ifsd_expression *expression;
				int truth;

				if (ifstate->ifstate == s)
					truth = 1;
				else
					truth = 0;

				TAILQ_FOREACH(expression,
				    &ifstate->expressions, entries) {
					expression->truth = truth;
					TAILQ_INSERT_TAIL(&expressions,
					    expression, eval);
					changed = 1;
				}
				ifstate->prevstate = s;
			}
		}
	}

	if (changed)
		adjust_expressions(&expressions, conf->maxdepth);
	return (changed);
}

void
scan_ifstate(int ifindex, int s, int do_eval)
{
	struct ifsd_state *state;
	int cur_eval = 0;

	if (scan_ifstate_single(ifindex, s, &conf->always) && do_eval)
		eval_state(&conf->always);
	TAILQ_FOREACH(state, &conf->states, entries) {
		if (scan_ifstate_single(ifindex, s, state) &&
		    (do_eval && state == conf->curstate))
			cur_eval = 1;
	}
	/* execute actions _after_ all expressions have been adjusted */
	if (cur_eval)
		eval_state(conf->curstate);
}

/*
 * Do a bottom-up ajustment of the expression tree's truth value,
 * level-by-level to ensure that each expression's subexpressions have been
 * evaluated.
 */
void
adjust_expressions(struct ifsd_expression_list *expressions, int depth)
{
	struct ifsd_expression_list nexpressions;
	struct ifsd_expression *expression;

	TAILQ_INIT(&nexpressions);
	while ((expression = TAILQ_FIRST(expressions)) != NULL) {
		TAILQ_REMOVE(expressions, expression, eval);
		if (expression->depth == depth) {
			struct ifsd_expression *te;

			switch (expression->type) {
			case IFSD_OPER_AND:
				if (expression->left->truth &&
				    expression->right->truth)
					expression->truth = 1;
				else
					expression->truth = 0;
				break;
			case IFSD_OPER_OR:
				if (expression->left->truth ||
				    expression->right->truth)
					expression->truth = 1;
				else
					expression->truth = 0;
				break;
			case IFSD_OPER_NOT:
				if (expression->right->truth)
					expression->truth = 0;
				else
					expression->truth = 1;
				break;
			default:
				break;
			}
			if (expression->parent != NULL) {
				if (TAILQ_EMPTY(&nexpressions))
				te = NULL;
				TAILQ_FOREACH(te, &nexpressions, eval)
					if (expression->parent == te)
						break;
				if (te == NULL)
					TAILQ_INSERT_TAIL(&nexpressions,
					    expression->parent, eval);
			}
		} else
			TAILQ_INSERT_TAIL(&nexpressions, expression, eval);
	}
	if (depth > 0)
		adjust_expressions(&nexpressions, depth - 1);
}

void
eval_state(struct ifsd_state *state)
{
	struct ifsd_external *external = TAILQ_FIRST(&state->external_tests);
	if (external == NULL || external->lastexec >= state->entered ||
	    external->lastexec == 0) {
		do_action(state->always);
		while (state_change())
			do_action(conf->curstate->always);
	}
}

/*
 *If a previous action included a state change, process it.
 */
int
state_change(void)
{
	if (conf->nextstate != NULL && conf->curstate != conf->nextstate) {
		logit(IFSD_LOG_NORMAL, "changing state to %s",
		    conf->nextstate->name);
		if (conf->curstate != NULL) {
			evtimer_del(&conf->curstate->ev);
			external_evtimer_setup(conf->curstate,
			    IFSD_EVTIMER_DEL);
		}
		conf->curstate = conf->nextstate;
		conf->nextstate = NULL;
		conf->curstate->entered = time(NULL);
		external_evtimer_setup(conf->curstate, IFSD_EVTIMER_ADD);
		adjust_external_expressions(conf->curstate);
		do_action(conf->curstate->init);
		return (1);
	}
	return (0);
}

/*
 * Run recursively through the tree of actions.
 */
void
do_action(struct ifsd_action *action)
{
	struct ifsd_action *subaction;

	switch (action->type) {
	case IFSD_ACTION_COMMAND:
		logit(IFSD_LOG_NORMAL, "running %s", action->act.command);
		system(action->act.command);
		break;
	case IFSD_ACTION_CHANGESTATE:
		conf->nextstate = action->act.nextstate;
		break;
	case IFSD_ACTION_CONDITION:
		if ((action->act.c.expression != NULL &&
		    action->act.c.expression->truth) ||
		    action->act.c.expression == NULL) {
			TAILQ_FOREACH(subaction, &action->act.c.actions,
			    entries)
				do_action(subaction);
		}
		break;
	default:
		logit(IFSD_LOG_DEBUG, "do_action: unknown action %d",
		    action->type);
		break;
	}
}

/*
 * Fetch the current link states.
 */
void
fetch_state(void)
{
	struct ifaddrs *ifap, *ifa;
	char *oname = NULL;
	int sock = socket(AF_INET, SOCK_DGRAM, 0);

	if (getifaddrs(&ifap) != 0)
		err(1, "getifaddrs");

	for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
		struct ifreq ifr;
		struct if_data  ifrdat;

		if (oname && !strcmp(oname, ifa->ifa_name))
			continue;
		oname = ifa->ifa_name;

		strlcpy(ifr.ifr_name, ifa->ifa_name, sizeof(ifr.ifr_name));
		ifr.ifr_data = (caddr_t)&ifrdat;

		if (ioctl(sock, SIOCGIFDATA, (caddr_t)&ifr) == -1)
			continue;

		scan_ifstate(if_nametoindex(ifa->ifa_name),
		    ifrdat.ifi_link_state, 0);
	}
	freeifaddrs(ifap);
	close(sock);
}



/*
 * Clear the config.
 */
void
clear_config(struct ifsd_config *oconf)
{
	struct ifsd_state *state;

	external_evtimer_setup(&conf->always, IFSD_EVTIMER_DEL);
	if (conf != NULL && conf->curstate != NULL)
		external_evtimer_setup(conf->curstate, IFSD_EVTIMER_DEL);
	while ((state = TAILQ_FIRST(&oconf->states)) != NULL) {
		TAILQ_REMOVE(&oconf->states, state, entries);
		remove_action(state->init, state);
		remove_action(state->always, state);
		free(state->name);
		free(state);
	}
	remove_action(oconf->always.init, &oconf->always);
	remove_action(oconf->always.always, &oconf->always);
	free(oconf);
}

void
remove_action(struct ifsd_action *action, struct ifsd_state *state)
{
	struct ifsd_action *subaction;

	if (action == NULL || state == NULL)
		return;

	switch (action->type) {
	case IFSD_ACTION_LOG:
		free(action->act.logmessage);
		break;
	case IFSD_ACTION_COMMAND:
		free(action->act.command);
		break;
	case IFSD_ACTION_CHANGESTATE:
		break;
	case IFSD_ACTION_CONDITION:
		if (action->act.c.expression != NULL)
			remove_expression(action->act.c.expression, state);
		while ((subaction =
		    TAILQ_FIRST(&action->act.c.actions)) != NULL) {
			TAILQ_REMOVE(&action->act.c.actions,
			    subaction, entries);
			remove_action(subaction, state);
		}
	}
	free(action);
}

void
remove_expression(struct ifsd_expression *expression,
    struct ifsd_state *state)
{
	switch (expression->type) {
	case IFSD_OPER_IFSTATE:
		TAILQ_REMOVE(&expression->u.ifstate->expressions, expression,
		    entries);
		if (--expression->u.ifstate->refcount == 0) {
			TAILQ_REMOVE(&state->interface_states,
			    expression->u.ifstate, entries);
			free(expression->u.ifstate);
		}
		break;
	case IFSD_OPER_EXTERNAL:
		TAILQ_REMOVE(&expression->u.external->expressions, expression,
		    entries);
		if (--expression->u.external->refcount == 0) {
			TAILQ_REMOVE(&state->external_tests,
			    expression->u.external, entries);
			free(expression->u.external->command);
			event_del(&expression->u.external->ev);
			free(expression->u.external);
		}
		break;
	default:
		if (expression->left != NULL)
			remove_expression(expression->left, state);
		if (expression->right != NULL)
			remove_expression(expression->right, state);
		break;
	}
	free(expression);
}

void
log_init(int n_debug)
{
	extern char	*__progname;

	if (!n_debug)
		openlog(__progname, LOG_PID | LOG_NDELAY, LOG_DAEMON);
}

void
logit(int level, const char *fmt, ...)
{
	va_list	 ap;
	char	*nfmt;

	if (conf == NULL || level > conf->loglevel)
		return;

	va_start(ap, fmt);
	if (opt_debug) {
		/* best effort in out of mem situations */
		if (asprintf(&nfmt, "ifstated: %s\n", fmt) != -1) {
			vfprintf(stderr, nfmt, ap);
			free(nfmt);
		} else {
			vfprintf(stderr, fmt, ap);
			fprintf(stderr, "\n");
		}
	} else
		vsyslog(LOG_NOTICE, fmt, ap);

	va_end(ap);
}
