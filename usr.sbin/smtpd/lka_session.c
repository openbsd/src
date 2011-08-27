/*	$OpenBSD: lka_session.c,v 1.9 2011/08/27 22:32:41 gilles Exp $	*/

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

void lka_session(struct submit_status *);
void lka_session_forward_reply(struct forward_req *, int);

struct lka_session *lka_session_init(struct submit_status *);
struct lka_session *lka_session_find(u_int64_t);
struct lka_session *lka_session_xfind(u_int64_t);
void lka_session_fail(struct lka_session *);
void lka_session_destroy(struct lka_session *);
void lka_session_pickup(struct lka_session *, struct envelope *);
int lka_session_envelope_expand(struct lka_session *, struct envelope *);
int lka_session_resume(struct lka_session *, struct envelope *);
void lka_session_done(struct lka_session *);
size_t lka_session_expand_format(char *, size_t, struct envelope *);
void lka_session_request_forwardfile(struct lka_session *,
    struct envelope *, char *);
void lka_session_deliver(struct lka_session *, struct envelope *);
int lka_session_resolve_node(struct envelope *, struct expandnode *);
int lka_session_rcpt_action(struct envelope *);
struct rule *ruleset_match(struct envelope *);

void
lka_session(struct submit_status *ss)
{
	struct lka_session *lks;

	lks = lka_session_init(ss);
	if (! lka_session_envelope_expand(lks, &ss->envelope))
		lka_session_fail(lks);
	else
		lka_session_pickup(lks, &ss->envelope);
}

int
lka_session_envelope_expand(struct lka_session *lks, struct envelope *ep)
{
	char *user;
	char *sep;
	struct user_backend *ub;
	struct user u;
	char username[MAX_LOCALPART_SIZE];

	/* remote delivery, no need to process further */
	if (ep->delivery.type == D_MTA) {
		lka_session_deliver(lks, ep);
		return 1;
	}

	switch (ep->rule.r_condition.c_type) {
	case C_ALL:
	case C_NET:
	case C_DOM: {
		if (ep->delivery.agent.mda.to.user[0] == '\0')
			user = ep->delivery.rcpt.user;
		else
			user = ep->delivery.agent.mda.to.user;
		lowercase(username, user, sizeof(username));

		/* gilles+hackers@ -> gilles@ */
		if ((sep = strchr(username, '+')) != NULL)
			*sep = '\0';

		if (aliases_exist(ep->rule.r_amap, username)) {
			if (! aliases_get(ep->rule.r_amap,
				&lks->expandtree, username)) {
				return 0;
			}
			return 1;
		}

		bzero(&u, sizeof (u));
		ub = user_backend_lookup(USER_GETPWNAM);
		if (! ub->getbyname(&u, username))
			return 0;

		(void)strlcpy(ep->delivery.agent.mda.as_user, u.username,
		    sizeof (ep->delivery.agent.mda.as_user));

		ep->delivery.type = D_MDA;
		switch (ep->rule.r_action) {
		case A_MBOX:
			ep->delivery.agent.mda.method = A_MBOX;
			(void)strlcpy(ep->delivery.agent.mda.to.user,
			    u.username,
			    sizeof (ep->delivery.agent.mda.to.user));
			break;
		case A_MAILDIR:
		case A_FILENAME:
		case A_EXT:
			ep->delivery.agent.mda.method = ep->rule.r_action;
			(void)strlcpy(ep->delivery.agent.mda.to.buffer,
			    ep->rule.r_value.buffer,
			    sizeof (ep->delivery.agent.mda.to.buffer));
			break;
		default:
			fatalx("lka_session_envelope_expand: unexpected rule action");
			return 0;
		}

		lka_session_request_forwardfile(lks, ep, u.username);
		return 1;
	}

	case C_VDOM: {
		if (aliases_virtual_exist(ep->rule.r_condition.c_map, &ep->delivery.rcpt)) {
			if (! aliases_virtual_get(ep->rule.r_condition.c_map,
				&lks->expandtree, &ep->delivery.rcpt))
				return 0;
			return 1;
		}
		return 0;
	}

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

	lks = lka_session_xfind(fwreq->id);
	lks->pending--;
	
	ep = &fwreq->envelope;

	if (fd != -1) {
		/* opened .forward okay */
		if (! forwards_get(fd, &lks->expandtree, fwreq->as_user)) {
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

struct lka_session *
lka_session_init(struct submit_status *ss)
{
	struct lka_session *lks;

	lks = calloc(1, sizeof(*lks));
	if (lks == NULL)
		fatal("lka_session_init: calloc");

	lks->id = generate_uid();
	lks->ss = *ss;
	lks->ss.code = 250;

	RB_INIT(&lks->expandtree);
	TAILQ_INIT(&lks->deliverylist);
	SPLAY_INSERT(lkatree, &env->lka_sessions, lks);

	return lks;
}

void
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

int
lka_session_resume(struct lka_session *lks, struct envelope *ep)
{
	struct expandnode *xn;
        u_int8_t done = 1;

	RB_FOREACH(xn, expandtree, &lks->expandtree) {

		/* this node has already been expanded, skip */
                if (xn->flags & F_EXPAND_DONE)
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

                /* decrement refcount on this node and flag it as processed */
                expandtree_decrement_node(&lks->expandtree, xn);
                xn->flags |= F_EXPAND_DONE;
	}

        /* still not done after 5 iterations ? loop detected ... reject */
        if (!done && lks->iterations == 5)
                return -1;

        /* we're done expanding, no need for another iteration */
	if (RB_ROOT(&lks->expandtree) == NULL || done)
		return 0;

	return 1;
}

void
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
		queue_submit_envelope(ep);
		TAILQ_REMOVE(&lks->deliverylist, ep, entry);
		free(ep);
	}
	queue_commit_envelopes(&lks->ss.envelope);

done:
	if (lks->flags & F_ERROR) {
		lks->ss.code = 530;
		imsg_compose_event(env->sc_ievs[PROC_MFA], IMSG_LKA_RCPT, 0, 0,
		    -1, &lks->ss, sizeof(struct submit_status));
	}
	lka_session_destroy(lks);
}

struct lka_session *
lka_session_find(u_int64_t id)
{
	struct lka_session key;

	key.id = id;
	return SPLAY_FIND(lkatree, &env->lka_sessions, &key);
}

struct lka_session *
lka_session_xfind(u_int64_t id)
{
	struct lka_session *lks;

	lks = lka_session_find(id);
	if (lks == NULL)
		fatalx("lka_session_xfind: lka session missing");

	return lks;
}

void
lka_session_fail(struct lka_session *lks)
{
	lks->ss.code = 530;
	imsg_compose_event(env->sc_ievs[PROC_MFA], IMSG_LKA_RCPT, 0, 0, -1,
            &lks->ss, sizeof(lks->ss));
        lka_session_destroy(lks);
}

void
lka_session_destroy(struct lka_session *lks)
{
	struct envelope *ep;
	struct expandnode *xn;

	while ((ep = TAILQ_FIRST(&lks->deliverylist)) != NULL) {
		TAILQ_REMOVE(&lks->deliverylist, ep, entry);
		free(ep);
	}

	while ((xn = RB_ROOT(&lks->expandtree)) != NULL) {
		RB_REMOVE(expandtree, &lks->expandtree, xn);
		free(xn);
	}

	SPLAY_REMOVE(lkatree, &env->lka_sessions, lks);
	free(lks);
}

void
lka_session_deliver(struct lka_session *lks, struct envelope *ep)
{
	struct envelope *new_ep;
	struct delivery_mda *d_mda;

	new_ep = calloc(1, sizeof (*ep));
	if (new_ep == NULL)
		fatal("lka_session_deliver: calloc");
	*new_ep = *ep;
	if (new_ep->delivery.type == D_MDA) {
		d_mda = &new_ep->delivery.agent.mda;
		if (d_mda->method == A_INVALID)
			fatalx("lka_session_deliver: mda method == A_INVALID");

		switch (d_mda->method) {
		case A_MAILDIR:
		case A_FILENAME:
		case A_EXT: {
			char *buf = d_mda->to.buffer;
			size_t bufsz = sizeof(d_mda->to.buffer);
			if (! lka_session_expand_format(buf, bufsz, new_ep))
				lks->flags |= F_ERROR;
			break;
		}
		default:
			break;
		}
	}
	else if (new_ep->delivery.type == D_MTA) {
		if (ep->rule.r_as)
			new_ep->delivery.from = *ep->rule.r_as;
	}
	TAILQ_INSERT_TAIL(&lks->deliverylist, new_ep, entry);
}

int
lka_session_resolve_node(struct envelope *ep, struct expandnode *xn)
{
	struct delivery *dlv;
	struct delivery olddlv;

	dlv = &ep->delivery;
	memcpy(&olddlv, dlv, sizeof (*dlv));
        bzero(&dlv->agent, sizeof (dlv->agent));

	switch (xn->type) {
        case EXPAND_INVALID:
        case EXPAND_INCLUDE:
                fatalx("lka_session_resolve_node: unexpected type");
                break;

        case EXPAND_ADDRESS:
                log_debug("lka_resolve_node: node is address: %s@%s",
		    xn->u.mailaddr.user, xn->u.mailaddr.domain);
		dlv->rcpt = xn->u.mailaddr;

		/* evaluation of ruleset assumes local source
		 * since we're expanding on already accepted
		 * source.
		 */
		dlv->flags |= DF_INTERNAL;
                if (! lka_session_rcpt_action(ep))
			return -1;
		return 0;

        case EXPAND_USERNAME:
                log_debug("lka_resolve_node: node is local username: %s",
		    xn->u.user);
		dlv->type  = D_MDA;
		dlv->agent.mda.to = xn->u;

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
			ep->delivery.agent.mda.method = olddlv.agent.mda.method;
			break;
		}

		/* otherwise rewrite delivery user with expansion result */
		(void)strlcpy(dlv->agent.mda.to.user, xn->u.user,
		    sizeof (dlv->agent.mda.to.user));
		(void)strlcpy(dlv->agent.mda.as_user, xn->u.user,
		    sizeof (dlv->agent.mda.as_user));
		return 0;

        case EXPAND_FILENAME:
                log_debug("lka_resolve_node: node is filename: %s",
		    xn->u.buffer);
		dlv->type  = D_MDA;
		dlv->agent.mda.to = xn->u;
		dlv->agent.mda.method = A_FILENAME;
		(void)strlcpy(dlv->agent.mda.as_user, xn->as_user,
		    sizeof (dlv->agent.mda.as_user));
                break;

        case EXPAND_FILTER:
                log_debug("lka_resolve_node: node is filter: %s",
		    xn->u.buffer);
		dlv->type  = D_MDA;
		dlv->agent.mda.to = xn->u;
		dlv->agent.mda.method = A_EXT;
		(void)strlcpy(dlv->agent.mda.as_user, xn->as_user,
		    sizeof (dlv->agent.mda.as_user));
                break;
	}

	return 1;
}

size_t
lka_session_expand_format(char *buf, size_t len, struct envelope *ep)
{
	char *p, *pbuf;
	size_t ret, lret = 0;
	struct user_backend *ub;
	struct user u;
	char lbuffer[MAX_RULEBUFFER_LEN];
	struct delivery *dlv = &ep->delivery;
	
	bzero(lbuffer, sizeof (lbuffer));
	pbuf = lbuffer;

	ret = 0;
	for (p = buf; *p != '\0';
	     ++p, len -= lret, pbuf += lret, ret += lret) {
		if (p == buf && *p == '~') {
			if (*(p + 1) == '/' || *(p + 1) == '\0') {

				bzero(&u, sizeof (u));
				ub = user_backend_lookup(USER_GETPWNAM);
				if (! ub->getbyname(&u, dlv->agent.mda.as_user))
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
				ub = user_backend_lookup(USER_GETPWNAM);
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
			case 'U':
				string = dlv->from.user;
				break;
			case 'D':
				string = dlv->from.domain;
				break;
			case 'a':
				string = dlv->agent.mda.as_user;
				break;
			case 'u':
				string = dlv->rcpt.user;
				break;
			case 'd':
				string = dlv->rcpt.domain;
				break;
			default:
				goto copy;
			}

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

int
lka_session_rcpt_action(struct envelope *ep)
{
        struct rule *r;

	r = ruleset_match(ep);
	if (r == NULL) {
		ep->delivery.type = D_MTA;
		return 0;
	}

        ep->rule = *r;
	switch (ep->rule.r_action) {
	case A_MBOX:
	case A_MAILDIR:
	case A_FILENAME:
	case A_EXT:
		ep->delivery.type = D_MDA;
		break;
	default:
		ep->delivery.type = D_MTA;
	}

	return 1;
}

void
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

int
lka_session_cmp(struct lka_session *s1, struct lka_session *s2)
{
	/*
	 * do not return u_int64_t's
	 */
	if (s1->id < s2->id)
		return -1;

	if (s1->id > s2->id)
		return 1;

	return 0;
}

SPLAY_GENERATE(lkatree, lka_session, nodes, lka_session_cmp);
