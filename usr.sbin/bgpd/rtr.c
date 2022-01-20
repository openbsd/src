/*	$OpenBSD: rtr.c,v 1.5 2022/01/20 18:06:20 claudio Exp $ */

/*
 * Copyright (c) 2020 Claudio Jeker <claudio@openbsd.org>
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
#include <sys/tree.h>
#include <errno.h>
#include <poll.h>
#include <pwd.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "bgpd.h"
#include "session.h"
#include "log.h"

static void	rtr_dispatch_imsg_parent(struct imsgbuf *);
static void	rtr_dispatch_imsg_rde(struct imsgbuf *);

volatile sig_atomic_t		 rtr_quit;
static struct imsgbuf		*ibuf_main;
static struct imsgbuf		*ibuf_rde;
static struct bgpd_config	*conf, *nconf;
static struct timer_head	 expire_timer;

static void
rtr_sighdlr(int sig)
{
	switch (sig) {
	case SIGINT:
	case SIGTERM:
		rtr_quit = 1;
		break;
	}
}

#define PFD_PIPE_MAIN	0
#define PFD_PIPE_RDE	1
#define PFD_PIPE_COUNT	2

#define EXPIRE_TIMEOUT	300

/*
 * Every EXPIRE_TIMEOUT seconds traverse the static roa-set table and expire
 * all elements where the expires timestamp is smaller or equal to now.
 * If any change is done recalculate the RTR table.
 */
static unsigned int
rtr_expire_roas(time_t now)
{
	struct roa *roa, *nr;
	unsigned int recalc = 0;

	RB_FOREACH_SAFE(roa, roa_tree, &conf->roa, nr) {
		if (roa->expires != 0 && roa->expires <= now) {
			recalc++;
			RB_REMOVE(roa_tree, &conf->roa, roa);
			free(roa);
		}
	}
	if (recalc != 0)
		log_warnx("%u roa-set entries expired", recalc);
	return recalc;
}

void
roa_insert(struct roa_tree *rt, struct roa *in)
{
	struct roa *roa;

	if ((roa = malloc(sizeof(*roa))) == NULL)
		fatal("roa alloc");
	memcpy(roa, in, sizeof(*roa));
	if (RB_INSERT(roa_tree, rt, roa) != NULL)
		/* just ignore duplicates */
		free(roa);
}

void
rtr_main(int debug, int verbose)
{
	struct passwd		*pw;
	struct pollfd		*pfd = NULL;
	void			*newp;
	size_t			 pfd_elms = 0, i;
	time_t			 timeout;

	log_init(debug, LOG_DAEMON);
	log_setverbose(verbose);

	log_procinit(log_procnames[PROC_RTR]);

	if ((pw = getpwnam(BGPD_USER)) == NULL)
		fatal("getpwnam");

	if (chroot(pw->pw_dir) == -1)
		fatal("chroot");
	if (chdir("/") == -1)
		fatal("chdir(\"/\")");

	setproctitle("rtr engine");

	if (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		fatal("can't drop privileges");

	if (pledge("stdio recvfd", NULL) == -1)
		fatal("pledge");

	signal(SIGTERM, rtr_sighdlr);
	signal(SIGINT, rtr_sighdlr);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, SIG_IGN);
	signal(SIGALRM, SIG_IGN);
	signal(SIGUSR1, SIG_IGN);

	if ((ibuf_main = malloc(sizeof(struct imsgbuf))) == NULL)
		fatal(NULL);
	imsg_init(ibuf_main, 3);

	conf = new_config();
	log_info("rtr engine ready");

	TAILQ_INIT(&expire_timer);
	timer_set(&expire_timer, Timer_Rtr_Expire, EXPIRE_TIMEOUT);

	while (rtr_quit == 0) {
		i = rtr_count();
		if (pfd_elms < PFD_PIPE_COUNT + i) {
			if ((newp = reallocarray(pfd,
			    PFD_PIPE_COUNT + i,
			    sizeof(struct pollfd))) == NULL)
				fatal("realloc pollfd");
			pfd = newp;
			pfd_elms = PFD_PIPE_COUNT + i;
		}

		/* run the expire timeout every EXPIRE_TIMEOUT seconds */
		timeout = timer_nextduein(&expire_timer, getmonotime());
		if (timeout == -1)
			fatalx("roa-set expire timer no longer runnning");

		bzero(pfd, sizeof(struct pollfd) * pfd_elms);

		set_pollfd(&pfd[PFD_PIPE_MAIN], ibuf_main);
		set_pollfd(&pfd[PFD_PIPE_RDE], ibuf_rde);

		i = PFD_PIPE_COUNT;
		i += rtr_poll_events(pfd + i, pfd_elms - i, &timeout);

		if (poll(pfd, i, timeout * 1000) == -1) {
			if (errno == EINTR)
				continue;
			fatal("poll error");
		}

		if (handle_pollfd(&pfd[PFD_PIPE_MAIN], ibuf_main) == -1)
			fatalx("Lost connection to parent");
		else
			rtr_dispatch_imsg_parent(ibuf_main);

		if (handle_pollfd(&pfd[PFD_PIPE_RDE], ibuf_rde) == -1) {
			log_warnx("RTR: Lost connection to RDE");
			msgbuf_clear(&ibuf_rde->w);
			free(ibuf_rde);
			ibuf_rde = NULL;
		} else
			rtr_dispatch_imsg_rde(ibuf_rde);

		i = PFD_PIPE_COUNT;
		rtr_check_events(pfd + i, pfd_elms - i);

		if (timer_nextisdue(&expire_timer, getmonotime()) != NULL) {
			timer_set(&expire_timer, Timer_Rtr_Expire,
			    EXPIRE_TIMEOUT);
			if (rtr_expire_roas(time(NULL)) != 0)
				rtr_recalc();
		}
	}

	rtr_shutdown();

	free_config(conf);
	free(pfd);

	/* close pipes */
	if (ibuf_rde) {
		msgbuf_clear(&ibuf_rde->w);
		close(ibuf_rde->fd);
		free(ibuf_rde);
	}
	msgbuf_clear(&ibuf_main->w);
	close(ibuf_main->fd);
	free(ibuf_main);

	log_info("rtr engine exiting");
	exit(0);
}

static void
rtr_dispatch_imsg_parent(struct imsgbuf *ibuf)
{
	struct imsg	imsg;
	struct roa	*roa;
	struct rtr_session *rs;
	int		n, fd;

	while (ibuf) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("%s: imsg_get error", __func__);
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_SOCKET_CONN_RTR:
			if ((fd = imsg.fd) == -1) {
				log_warnx("expected to receive imsg fd "
				    "but didn't receive any");
				break;
			}
			if (ibuf_rde) {
				log_warnx("Unexpected imsg ctl "
				    "connection to RDE received");
				msgbuf_clear(&ibuf_rde->w);
				free(ibuf_rde);
			}
			if ((ibuf_rde = malloc(sizeof(struct imsgbuf))) == NULL)
				fatal(NULL);
			imsg_init(ibuf_rde, fd);
			break;
		case IMSG_SOCKET_CONN:
			if ((fd = imsg.fd) == -1) {
				log_warnx("expected to receive imsg fd "
				    "but didn't receive any");
				break;
			}
			if ((rs = rtr_get(imsg.hdr.peerid)) == NULL) {
				log_warnx("IMSG_SOCKET_CONN: unknown rtr id %d",
				    imsg.hdr.peerid);
				close(fd);
				break;
			}
			rtr_open(rs, fd);
			break;
		case IMSG_RECONF_CONF:
			if (imsg.hdr.len - IMSG_HEADER_SIZE !=
			    sizeof(struct bgpd_config))
				fatalx("IMSG_RECONF_CONF bad len");
			nconf = new_config();
			copy_config(nconf, imsg.data);
			rtr_config_prep();
			break;
		case IMSG_RECONF_ROA_ITEM:
			if (imsg.hdr.len - IMSG_HEADER_SIZE !=
			    sizeof(*roa))
				fatalx("IMSG_RECONF_ROA_ITEM bad len");
			roa_insert(&nconf->roa, imsg.data);
			break;
		case IMSG_RECONF_RTR_CONFIG:
			if (imsg.hdr.len - IMSG_HEADER_SIZE != PEER_DESCR_LEN)
				fatalx("IMSG_RECONF_RTR_CONFIG bad len");
			rs = rtr_get(imsg.hdr.peerid);
			if (rs == NULL)
				rtr_new(imsg.hdr.peerid, imsg.data);
			else
				rtr_config_keep(rs);
			break;
		case IMSG_RECONF_DRAIN:
			imsg_compose(ibuf_main, IMSG_RECONF_DRAIN, 0, 0,
			    -1, NULL, 0);
			break;
		case IMSG_RECONF_DONE:
			if (nconf == NULL)
				fatalx("got IMSG_RECONF_DONE but no config");
			copy_config(conf, nconf);
			/* switch the roa, first remove the old one */
			free_roatree(&conf->roa);
			/* then move the RB tree root */
			RB_ROOT(&conf->roa) = RB_ROOT(&nconf->roa);
			RB_ROOT(&nconf->roa) = NULL;
			/* finally merge the rtr session */
			rtr_config_merge();
			rtr_expire_roas(time(NULL));
			rtr_recalc();
			log_info("RTR engine reconfigured");
			imsg_compose(ibuf_main, IMSG_RECONF_DONE, 0, 0,
			    -1, NULL, 0);
			free_config(nconf);
			nconf = NULL;
			break;
		case IMSG_CTL_SHOW_RTR:
			if ((rs = rtr_get(imsg.hdr.peerid)) == NULL) {
				log_warnx("IMSG_CTL_SHOW_RTR: "
				    "unknown rtr id %d", imsg.hdr.peerid);
				break;
			}
			rtr_show(rs, imsg.hdr.pid);
			break;
		case IMSG_CTL_END:
			imsg_compose(ibuf_main, IMSG_CTL_END, 0, imsg.hdr.pid,
			    -1, NULL, 0);
			break;
		}
		imsg_free(&imsg);
	}
}

static void
rtr_dispatch_imsg_rde(struct imsgbuf *ibuf)
{
	struct imsg	imsg;
	int		n;

	while (ibuf) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("%s: imsg_get error", __func__);
		if (n == 0)
			break;

		/* NOTHING */

		imsg_free(&imsg);
	}
}

void
rtr_imsg_compose(int type, uint32_t id, pid_t pid, void *data, size_t datalen)
{
	imsg_compose(ibuf_main, type, id, pid, -1, data, datalen);
}

/*
 * Merge all RPKI ROA trees into one as one big union.
 * Simply try to add all roa entries into a new RB tree.
 * This could be made a fair bit faster but for now this is good enough.
 */
void
rtr_recalc(void)
{
	struct roa_tree rt;
	struct roa *roa, *nr;

	RB_INIT(&rt);

	RB_FOREACH(roa, roa_tree, &conf->roa)
		roa_insert(&rt, roa);
	rtr_roa_merge(&rt);

	imsg_compose(ibuf_rde, IMSG_RECONF_ROA_SET, 0, 0, -1, NULL, 0);
	RB_FOREACH_SAFE(roa, roa_tree, &rt, nr) {
		RB_REMOVE(roa_tree, &rt, roa);
		imsg_compose(ibuf_rde, IMSG_RECONF_ROA_ITEM, 0, 0, -1,
		    roa, sizeof(*roa));
		free(roa);
	}
	imsg_compose(ibuf_rde, IMSG_RECONF_DONE, 0, 0, -1, NULL, 0);
}
