/*	$OpenBSD: lka_session.c,v 1.34 2012/09/24 08:56:12 eric Exp $	*/

/*
 * Copyright (c) 2011 Gilles Chehade <gilles@openbsd.org>
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
#include <sys/tree.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/wait.h>

#include <netinet/in.h>

#include <ctype.h>
#include <errno.h>
#include <event.h>
#include <imsg.h>
#include <resolv.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "smtpd.h"
#include "log.h"

enum lka_session_flags {
	F_ERROR		= 0x1
};

struct lka_session {
	SPLAY_ENTRY(lka_session)	 nodes;
	uint64_t			 id;

	TAILQ_HEAD(, envelope)		 deliverylist;
	struct expand			 expand;

	uint8_t				 iterations;
	uint32_t			 pending;
	enum lka_session_flags		 flags;
	struct submit_status		 ss;
};

static void lka_session_fail(struct lka_session *);
static void lka_session_destroy(struct lka_session *);
static void lka_session_pickup(struct lka_session *, struct envelope *);
static int lka_session_envelope_expand(struct lka_session *, struct envelope *);
static int lka_session_resume(struct lka_session *, struct envelope *);
static void lka_session_done(struct lka_session *);
static size_t lka_session_expand_format(char *, size_t, struct envelope *);
static void lka_session_request_forwardfile(struct lka_session *,
    struct envelope *, char *);
static void lka_session_deliver(struct lka_session *, struct envelope *);
static int lka_session_resolve_node(struct envelope *, struct expandnode *);
static int lka_session_rcpt_action(struct envelope *);

static struct tree	sessions = SPLAY_INITIALIZER(&sessions);

void
lka_session(struct submit_status *ss)
{
	struct lka_session *lks;

	lks = xcalloc(1, sizeof(*lks), "lka_session");
	lks->id = generate_uid();
	lks->ss = *ss;
	lks->ss.code = 250;
	RB_INIT(&lks->expand.tree);
	TAILQ_INIT(&lks->deliverylist);
	tree_xset(&sessions, lks->id, lks);

	if (! lka_session_envelope_expand(lks, &ss->envelope))
		lka_session_fail(lks);
	else
		lka_session_pickup(lks, &ss->envelope);
}

static int
lka_session_envelope_expand(struct lka_session *lks, struct envelope *ep)
{
	char *user;
	char *tag;
	struct user_backend *ub;
	struct mta_user u;
	char username[MAX_LOCALPART_SIZE];

	/* remote delivery, no need to process further */
	if (ep->type == D_MTA) {
		lka_session_deliver(lks, ep);
		return 1;
	}

	switch (ep->rule.r_condition.c_type) {
	case C_ALL:
	case C_DOM:
		if (ep->agent.mda.to.user[0] == '\0')
			user = ep->dest.user;
		else
			user = ep->agent.mda.to.user;
		xlowercase(username, user, sizeof(username));

		/* gilles+hackers@ -> gilles@ */
		if ((tag = strchr(username, '+')) != NULL) {
			*tag++ = '\0';

			/* skip dots after the '+' */
			while (*tag == '.')
				tag++;
		}

		if (aliases_get(ep->rule.r_amap, &lks->expand, username))
			return 1;

		bzero(&u, sizeof (u));
		ub = user_backend_lookup(USER_PWD);
		if (! ub->getbyname(&u, username))
			return 0;

		(void)strlcpy(ep->agent.mda.as_user, u.username,
		    sizeof (ep->agent.mda.as_user));

		ep->type = D_MDA;
		switch (ep->rule.r_action) {
		case A_MBOX:
			ep->agent.mda.method = A_MBOX;
			(void)strlcpy(ep->agent.mda.to.user,
			    u.username,
			    sizeof (ep->agent.mda.to.user));
			break;
		case A_MAILDIR:
		case A_FILENAME:
		case A_MDA:
			ep->agent.mda.method = ep->rule.r_action;
			(void)strlcpy(ep->agent.mda.to.buffer,
			    ep->rule.r_value.buffer,
			    sizeof (ep->agent.mda.to.buffer));
			
			if (tag && *tag) {
				(void)strlcat(ep->agent.mda.to.buffer, "/.",
				    sizeof (ep->agent.mda.to.buffer));
				(void)strlcat(ep->agent.mda.to.buffer, tag,
				    sizeof (ep->agent.mda.to.buffer));
			}
			break;
		default:
			fatalx("lka_session_envelope_expand: unexpected rule action");
			return 0;
		}

		lka_session_request_forwardfile(lks, ep, u.username);
		return 1;

	case C_VDOM:
		if (aliases_virtual_get(ep->rule.r_condition.c_map,
		    &lks->expand, &ep->dest))
			return 1;

		return 0;

	default:
		fatalx("lka_session_envelope_expand: unexpected type");
		return 0;
	}

	return 0;
}

void
lka_session_forward_reply(struct forward_req *fwreq, int fd)
{
	struct lka_session *lks;
	struct envelope *ep;

	lks = tree_xget(&sessions, fwreq->id);
	lks->pending--;
	
	ep = &fwreq->envelope;

	if (fd != -1) {
		/* opened .forward okay */
		strlcpy(lks->expand.user, fwreq->as_user,
		    sizeof lks->expand.user);
		if (! forwards_get(fd, &lks->expand)) {
			lks->ss.code = 530;
			lks->flags |= F_ERROR;
		}
		close(fd);
		lka_session_pickup(lks, ep);
		return;
	}

	if (fwreq->status) {
		/* .forward not present */
		lka_session_deliver(lks, ep);
		lka_session_pickup(lks, ep);
		return;
	}

	/* opening .forward failed */
	lks->ss.code = 530;
	lks->flags |= F_ERROR;
	lka_session_pickup(lks, ep);
}

static void
lka_session_pickup(struct lka_session *lks, struct envelope *ep)
{
	int ret;

	/* we want to do five iterations of lka_session_resume() but
	 * we need to be interruptible in case lka_session_resume()
	 * has sent an imsg and expects an answer.
	 */
	while (! (lks->flags & F_ERROR) &&
	    ! lks->pending && lks->iterations < 5) {
		++lks->iterations;
		ret = lka_session_resume(lks, ep);
		if (ret == -1) {
			lks->ss.code = 530;
			lks->flags |= F_ERROR;
		}

		if (lks->pending || ret <= 0)
			break;
	}

	if (lks->pending)
		return;

	lka_session_done(lks);
}

static int
lka_session_resume(struct lka_session *lks, struct envelope *ep)
{
	struct expandnode *xn;
	uint8_t done = 1;

	RB_FOREACH(xn, expandtree, &lks->expand.tree) {

		/* this node has already been expanded, skip */
		if (xn->done)
			continue;
		done = 0;

		switch (lka_session_resolve_node(ep, xn)) {
		case 0:
			if (! lka_session_envelope_expand(lks, ep))
				return -1;
			break;
		case 1:
			lka_session_deliver(lks, ep);
			break;
		default:
			return -1;
		}

		xn->done = 1;
	}

	/* still not done after 5 iterations ? loop detected ... reject */
	if (!done && lks->iterations == 5)
		return -1;

	/* we're done expanding, no need for another iteration */
	if (RB_ROOT(&lks->expand.tree) == NULL || done)
		return 0;

	return 1;
}

static void
lka_session_done(struct lka_session *lks)
{
	struct envelope *ep;

	/* delivery list is empty OR expansion led to an error, reject */
	if (TAILQ_FIRST(&lks->deliverylist) == NULL) {
		log_info("lka_session_done: expansion led to empty delivery list");
		lks->flags |= F_ERROR;
	}
	if (lks->flags & F_ERROR)
		goto done;

	/* process the delivery list and submit envelopes to queue */
	while ((ep = TAILQ_FIRST(&lks->deliverylist)) != NULL) {
		imsg_compose_event(env->sc_ievs[PROC_QUEUE],
		    IMSG_QUEUE_SUBMIT_ENVELOPE, 0, 0, -1, ep, sizeof *ep);
		TAILQ_REMOVE(&lks->deliverylist, ep, entry);
		free(ep);
	}
	ep = &lks->ss.envelope;
	imsg_compose_event(env->sc_ievs[PROC_QUEUE],
	    IMSG_QUEUE_COMMIT_ENVELOPES, 0, 0, -1, ep, sizeof *ep);

done:
	if (lks->flags & F_ERROR) {
		lks->ss.code = 530;
		imsg_compose_event(env->sc_ievs[PROC_MFA], IMSG_LKA_RCPT, 0, 0,
		    -1, &lks->ss, sizeof(struct submit_status));
	}
	lka_session_destroy(lks);
}

static void
lka_session_fail(struct lka_session *lks)
{
	lks->ss.code = 530;
	imsg_compose_event(env->sc_ievs[PROC_MFA], IMSG_LKA_RCPT, 0, 0, -1,
	    &lks->ss, sizeof(lks->ss));
	lka_session_destroy(lks);
}

static void
lka_session_destroy(struct lka_session *lks)
{
	struct envelope *ep;

	while ((ep = TAILQ_FIRST(&lks->deliverylist)) != NULL) {
		TAILQ_REMOVE(&lks->deliverylist, ep, entry);
		free(ep);
	}

	expand_free(&lks->expand);
	tree_xpop(&sessions, lks->id);
	free(lks);
}

static void
lka_session_deliver(struct lka_session *lks, struct envelope *ep)
{
	struct envelope *new_ep;

	new_ep = xmemdup(ep, sizeof *ep, "lka_session_deliver");
	if (new_ep->type == D_MDA) {
		switch (new_ep->agent.mda.method) {
		case A_MAILDIR:
		case A_FILENAME:
		case A_MDA:
			if (! lka_session_expand_format(
			    new_ep->agent.mda.to.buffer,
			    sizeof(new_ep->agent.mda.to.buffer), new_ep))
				lks->flags |= F_ERROR;
		default:
			break;
		}
	}
	else if (new_ep->type == D_MTA) {
		new_ep->agent.mta.relay = ep->rule.r_value.relayhost;
		if (ep->rule.r_as) {
			if (ep->rule.r_as->user[0]) {
				strlcpy(new_ep->sender.user,
				    ep->rule.r_as->user,
				    sizeof new_ep->sender.user);
			}
			if (ep->rule.r_as->domain[0]) {
				strlcpy(new_ep->sender.domain,
				    ep->rule.r_as->domain,
				    sizeof new_ep->sender.domain);
			}
		}
	}
	TAILQ_INSERT_TAIL(&lks->deliverylist, new_ep, entry);
}

static int
lka_session_resolve_node(struct envelope *ep, struct expandnode *xn)
{
	struct envelope	oldep;

	memcpy(&oldep, ep, sizeof (*ep));
	bzero(&ep->agent, sizeof (ep->agent));

	switch (xn->type) {
	case EXPAND_INVALID:
	case EXPAND_INCLUDE:
		fatalx("lka_session_resolve_node: unexpected type");
		break;

	case EXPAND_ADDRESS:
		log_debug("lka_resolve_node: node is address: %s@%s",
		    xn->u.mailaddr.user, xn->u.mailaddr.domain);
		ep->dest = xn->u.mailaddr;

		/* evaluation of ruleset assumes local source
		 * since we're expanding on already accepted
		 * source.
		 */
		ep->flags |= DF_INTERNAL;
		if (! lka_session_rcpt_action(ep))
			return -1;
		return 0;

	case EXPAND_USERNAME:
		log_debug("lka_resolve_node: node is local username: %s",
		    xn->u.user);
		ep->type  = D_MDA;
		ep->agent.mda.to = xn->u;

		/* overwrite the initial condition before we expand the
		 * envelope again. if we came from a C_VDOM, not doing
		 * so would lead to a VDOM loop causing recipient to be
		 * rejected.
		 *
		 * i'll find a more elegant solution later, for now it
		 * fixes an annoying bug.
		 */
		ep->rule.r_condition.c_type = C_DOM;

		/* if expansion of a user results in same user ... deliver */
		if (strcmp(xn->u.user, xn->as_user) == 0) {
			ep->agent.mda.method = oldep.agent.mda.method;
			break;
		}

		/* otherwise rewrite delivery user with expansion result */
		(void)strlcpy(ep->agent.mda.to.user, xn->u.user,
		    sizeof (ep->agent.mda.to.user));
		(void)strlcpy(ep->agent.mda.as_user, xn->u.user,
		    sizeof (ep->agent.mda.as_user));
		return 0;

	case EXPAND_FILENAME:
		log_debug("lka_resolve_node: node is filename: %s",
		    xn->u.buffer);
		ep->type  = D_MDA;
		ep->agent.mda.to = xn->u;
		ep->agent.mda.method = A_FILENAME;
		(void)strlcpy(ep->agent.mda.as_user, xn->as_user,
		    sizeof (ep->agent.mda.as_user));
		break;

	case EXPAND_FILTER:
		log_debug("lka_resolve_node: node is filter: %s",
		    xn->u.buffer);
		ep->type  = D_MDA;
		ep->agent.mda.to = xn->u;
		ep->agent.mda.method = A_MDA;
		(void)strlcpy(ep->agent.mda.as_user, xn->as_user,
		    sizeof (ep->agent.mda.as_user));
		break;
	}

	return 1;
}

static size_t
lka_session_expand_format(char *buf, size_t len, struct envelope *ep)
{
	char *p, *pbuf;
	size_t ret, lret = 0;
	struct user_backend *ub;
	struct mta_user u;
	char lbuffer[MAX_RULEBUFFER_LEN];
	char tmpbuf[MAX_RULEBUFFER_LEN];
	
	bzero(lbuffer, sizeof (lbuffer));
	pbuf = lbuffer;

	ret = 0;
	for (p = buf; *p != '\0';
	     ++p, len -= lret, pbuf += lret, ret += lret) {
		if (p == buf && *p == '~') {
			if (*(p + 1) == '/' || *(p + 1) == '\0') {

				bzero(&u, sizeof (u));
				ub = user_backend_lookup(USER_PWD);
				if (! ub->getbyname(&u, ep->agent.mda.as_user))
					return 0;
				
				lret = strlcat(pbuf, u.directory, len);
				if (lret >= len)
					return 0;
				continue;
			}
			
			if (*(p + 1) != '/') {
				char username[MAXLOGNAME];
				char *delim;
				
				lret = strlcpy(username, p + 1,
				    sizeof(username));
				if (lret >= sizeof(username))
					return 0;

				delim = strchr(username, '/');
				if (delim == NULL)
					goto copy;
				*delim = '\0';

				bzero(&u, sizeof (u));
				ub = user_backend_lookup(USER_PWD);
				if (! ub->getbyname(&u, username))
					return 0;

				lret = strlcat(pbuf, u.directory, len);
				if (lret >= len)
					return 0;
				p += strlen(username);
				continue;
			}
		}
		if (*p == '%') {
			char	*string, *tmp = p + 1;
			int	 digit = 0;

			if (isdigit((int)*tmp)) {
			    digit = 1;
			    tmp++;
			}
			switch (*tmp) {
			case 'A':
				string = ep->sender.user;
				break;
			case 'D':
				string = ep->sender.domain;
				break;
			case 'u':
				string = ep->agent.mda.as_user;
				break;
			case 'a':
				string = ep->dest.user;
				break;
			case 'd':
				string = ep->dest.domain;
				break;
			default:
				goto copy;
			}

			if (! lowercase(tmpbuf, string, sizeof tmpbuf))
				return 0;
			string = tmpbuf;
			
			if (digit == 1) {
				size_t idx = *(tmp - 1) - '0';

				lret = 1;
				if (idx < strlen(string))
					*pbuf++ = string[idx];
				else { /* fail */
					return 0;
				}

				p += 2; /* loop only does ++ */
				continue;
			}
			lret = strlcat(pbuf, string, len);
			if (lret >= len)
				return 0;
			p++;
			continue;
		}
copy:
		lret = 1;
		*pbuf = *p;
	}
	
	/* + 1 to include the NUL byte. */
	memcpy(buf, lbuffer, ret + 1);

	return ret;
}

static int
lka_session_rcpt_action(struct envelope *ep)
{
	struct rule *r;

	r = ruleset_match(ep);
	if (r == NULL) {
		ep->type = D_MTA;
		return 0;
	}

	ep->rule = *r;
	switch (ep->rule.r_action) {
	case A_MBOX:
	case A_MAILDIR:
	case A_FILENAME:
	case A_MDA:
		ep->type = D_MDA;
		break;
	default:
		ep->type = D_MTA;
	}

	return 1;
}

static void
lka_session_request_forwardfile(struct lka_session *lks,
    struct envelope *ep, char *as_user)
{
	struct forward_req fwreq;

	fwreq.id = lks->id;
	fwreq.envelope = *ep;
	(void)strlcpy(fwreq.as_user, as_user, sizeof(fwreq.as_user));
	imsg_compose_event(env->sc_ievs[PROC_PARENT],
	    IMSG_PARENT_FORWARD_OPEN, 0, 0, -1, &fwreq, sizeof(fwreq));
	++lks->pending;
}
