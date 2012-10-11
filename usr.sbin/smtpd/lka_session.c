/*	$OpenBSD: lka_session.c,v 1.43 2012/10/11 21:14:32 gilles Exp $	*/

/*
 * Copyright (c) 2011 Gilles Chehade <gilles@openbsd.org>
 * Copyright (c) 2012 Eric Faurot <eric@openbsd.org>
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

#define	EXPAND_DEPTH	10

#define	F_ERROR		0x01
#define	F_WAITING	0x02

struct lka_session {
	uint64_t		 id;

	TAILQ_HEAD(, envelope)	 deliverylist;
	struct expand		 expand;

	int			 flags;
	struct submit_status	 ss;

	struct envelope		 envelope;

	struct xnodes		 nodes;
	/* waiting for fwdrq */
	struct rule		*rule;
	struct expandnode	*node;
};

static void lka_expand(struct lka_session *, struct rule *, struct expandnode *);
static void lka_submit(struct lka_session *, struct rule *, struct expandnode *);
static void lka_resume(struct lka_session *);
static size_t lka_expand_format(char *, size_t, const struct envelope *);
static void mailaddr_to_username(const struct mailaddr *, char *, size_t);
static const char * mailaddr_tag(const struct mailaddr *);

static struct tree	sessions = SPLAY_INITIALIZER(&sessions);

void
lka_session(struct submit_status *ss)
{
	struct lka_session	*lks;
	struct expandnode	 xn;

	lks = xcalloc(1, sizeof(*lks), "lka_session");
	lks->id = generate_uid();
	lks->ss = *ss;
	lks->ss.code = 250;
	RB_INIT(&lks->expand.tree);
	TAILQ_INIT(&lks->deliverylist);
	tree_xset(&sessions, lks->id, lks);

	lks->envelope = ss->envelope;

	TAILQ_INIT(&lks->nodes);
	bzero(&xn, sizeof xn);
	xn.type = EXPAND_ADDRESS;
	xn.u.mailaddr = lks->envelope.dest; /* XXX we should only have rcpt */
	lks->expand.rule = NULL;
	lks->expand.queue = &lks->nodes;
	expand_insert(&lks->expand, &xn);
	lka_resume(lks);
}

void
lka_session_forward_reply(struct forward_req *fwreq, int fd)
{
	struct lka_session	*lks;
	struct rule		*rule;
	struct expandnode	*xn;

	lks = tree_xget(&sessions, fwreq->id);
	xn = lks->node;
	rule = lks->rule;

	lks->flags &= ~F_WAITING;

	if (fd == -1 && fwreq->status) {
		/* no .forward, just deliver to local user */
		log_debug("lka: no .forward for user %s, just deliver",
		    fwreq->as_user),
		lka_submit(lks, rule, xn);
	}
	else if (fd == -1) {
		log_debug("lka: opening .forward failed for user %s",
		    fwreq->as_user),
		lks->ss.code = 530;
		lks->flags |= F_ERROR;
	}
	else {
		/* expand for the current user and rule */
		lks->expand.rule = rule;
		lks->expand.parent = xn;
		lks->expand.alias = 0;
		if (forwards_get(fd, &lks->expand) == 0) {
			/* no aliases */
			lks->ss.code = 530;
			lks->flags |= F_ERROR;
		}
		close(fd);
	}
	lka_resume(lks);
}

static void
lka_resume(struct lka_session *lks)
{
	struct envelope		*ep;
	struct expandnode	*xn;

	if (lks->flags & F_ERROR)
		goto error;

	/* pop next node and expand it */
	while((xn = TAILQ_FIRST(&lks->nodes))) {
		TAILQ_REMOVE(&lks->nodes, xn, tq_entry);
		lka_expand(lks, xn->rule, xn);
		if (lks->flags & F_WAITING)
			return;
		if (lks->flags & F_ERROR)
			goto error;
	}

	/* delivery list is empty, reject */
	if (TAILQ_FIRST(&lks->deliverylist) == NULL) {
		log_info("lka_done: expansion led to empty delivery list");
		lks->flags |= F_ERROR;
	}
    error:
	if (lks->flags & F_ERROR) {
		lks->ss.code = 530;
		imsg_compose_event(env->sc_ievs[PROC_MFA], IMSG_LKA_RCPT, 0, 0,
		    -1, &lks->ss, sizeof(struct submit_status));
		while ((ep = TAILQ_FIRST(&lks->deliverylist)) != NULL) {
			TAILQ_REMOVE(&lks->deliverylist, ep, entry);
			free(ep);
		}
	}
	else {
		/* process the delivery list and submit envelopes to queue */
		while ((ep = TAILQ_FIRST(&lks->deliverylist)) != NULL) {
			TAILQ_REMOVE(&lks->deliverylist, ep, entry);
			imsg_compose_event(env->sc_ievs[PROC_QUEUE],
			    IMSG_QUEUE_SUBMIT_ENVELOPE, 0, 0, -1,
			    ep, sizeof *ep);
			free(ep);
		}
		ep = &lks->ss.envelope;
		imsg_compose_event(env->sc_ievs[PROC_QUEUE],
		    IMSG_QUEUE_COMMIT_ENVELOPES, 0, 0, -1, ep, sizeof *ep);
	}

	expand_free(&lks->expand);
	tree_xpop(&sessions, lks->id);
	free(lks);
}

static void
lka_expand(struct lka_session *lks, struct rule *rule, struct expandnode *xn)
{
	struct forward_req	fwreq;
	struct envelope		ep;
	struct expandnode	node;

	if (xn->depth >= EXPAND_DEPTH) {
		log_debug("lka_expand: node too deep.");
		lks->flags |= F_ERROR;
		lks->ss.code = 530;
		return;
	}

	switch (xn->type) {
	case EXPAND_INVALID:
	case EXPAND_INCLUDE:
		fatalx("lka_expand: unexpected type");
		break;

	case EXPAND_ADDRESS:
		log_debug("lka_expand: expanding address: %s@%s [depth=%d]",
		    xn->u.mailaddr.user, xn->u.mailaddr.domain, xn->depth);

		/* Pass the node through the ruleset */
		ep = lks->envelope;
		ep.dest = xn->u.mailaddr;
		if (xn->parent) /* nodes with parent are forward addresses */
			ep.flags |= DF_INTERNAL;
		rule = ruleset_match(&ep);
		if (rule == NULL || rule->r_decision == R_REJECT) {
			lks->flags |= F_ERROR;
			lks->ss.code = 530;
			break; /* no rule for address or REJECT match */
		}
		if (rule->r_action == A_RELAY || rule->r_action == A_RELAYVIA) {
			lka_submit(lks, rule, xn);
		}
		else if (rule->r_condition.c_type == COND_VDOM) {
			/* expand */
			lks->expand.rule = rule;
			lks->expand.parent = xn;
			lks->expand.alias = 1;
			if (aliases_virtual_get(rule->r_condition.c_map,
			    &lks->expand, &xn->u.mailaddr) == 0) {
				log_debug("lka_expand: no aliases for virtual");
				lks->flags |= F_ERROR;
				lks->ss.code = 530;
			}
		}
		else {
			lks->expand.rule = rule;
			lks->expand.parent = xn;
			lks->expand.alias = 1;
			node.type = EXPAND_USERNAME;
			mailaddr_to_username(&xn->u.mailaddr, node.u.user,
				sizeof node.u.user);
			expand_insert(&lks->expand, &node);
		}
		break;

	case EXPAND_USERNAME:
		log_debug("lka_expand: expanding username: %s [depth=%d]", xn->u.user, xn->depth);

		if (xn->sameuser) {
			log_debug("lka_expand: same user, submitting");
			lka_submit(lks, rule, xn);
			break;
		}

		/* expand aliases with the given rule */
		lks->expand.rule = rule;
		lks->expand.parent = xn;
		lks->expand.alias = 1;
		if (rule->r_amap &&
		    aliases_get(rule->r_amap, &lks->expand, xn->u.user))
			break;

		/* a username should not exceed the size of a system user */
		if (strlen(xn->u.user) >= sizeof fwreq.as_user) {
			log_debug("lka_expand: user-part too long to be a system user");
			lks->flags |= F_ERROR;
			lks->ss.code = 530;
			break;
		}

		/* no aliases found, query forward file */
		lks->rule = rule;
		lks->node = xn;
		fwreq.id = lks->id;
		(void)strlcpy(fwreq.as_user, xn->u.user, sizeof(fwreq.as_user));
		imsg_compose_event(env->sc_ievs[PROC_PARENT],
		    IMSG_PARENT_FORWARD_OPEN, 0, 0, -1, &fwreq, sizeof(fwreq));
		lks->flags |= F_WAITING;
		break;

	case EXPAND_FILENAME:
		log_debug("lka_expand: expanding filename: %s [depth=%d]", xn->u.buffer, xn->depth);
		lka_submit(lks, rule, xn);
		break;

	case EXPAND_FILTER:
		log_debug("lka_expand: expanding filter: %s [depth=%d]", xn->u.buffer, xn->depth);
		lka_submit(lks, rule, xn);
		break;
	}
}

static struct expandnode *
lka_find_ancestor(struct expandnode *xn, enum expand_type type)
{
	while(xn && (xn->type != type))
		xn = xn->parent;
	if (xn == NULL) {
		log_warnx("lka_find_ancestor: no ancestors of type %i", type);
		fatalx(NULL);
	}
	return (xn);
}

static void
lka_submit(struct lka_session *lks, struct rule *rule, struct expandnode *xn)
{
	struct envelope		*ep;
	struct expandnode	*xn2;
	const char		*tag;

	ep = xmemdup(&lks->envelope, sizeof *ep, "lka_submit");
	ep->expire = rule->r_qexpire;

	switch (rule->r_action) {
	case A_RELAY:
	case A_RELAYVIA:
		if (xn->type != EXPAND_ADDRESS)
			fatalx("lka_deliver: expect address");
		ep->type = D_MTA;
		ep->dest = xn->u.mailaddr;
		ep->agent.mta.relay = rule->r_value.relayhost;
		if (rule->r_as && rule->r_as->user[0])
			strlcpy(ep->sender.user, rule->r_as->user,
			    sizeof ep->sender.user);
		if (rule->r_as && rule->r_as->domain[0])
			strlcpy(ep->sender.domain, rule->r_as->domain,
			    sizeof ep->sender.domain);
		break;
	case A_MBOX:
	case A_MAILDIR:
	case A_FILENAME:
	case A_MDA:
		ep->type = D_MDA;
		ep->dest = lka_find_ancestor(xn, EXPAND_ADDRESS)->u.mailaddr;

		/* set username */
		if ((xn->type == EXPAND_FILTER || xn->type == EXPAND_FILENAME)
		    && xn->alias) {
			strlcpy(ep->agent.mda.user, SMTPD_USER,
			    sizeof (ep->agent.mda.user));
		}
		else {
			xn2 = lka_find_ancestor(xn, EXPAND_USERNAME);
			strlcpy(ep->agent.mda.user, xn2->u.user,
			    sizeof (ep->agent.mda.user));
		}

		if (xn->type == EXPAND_FILENAME) {
			ep->agent.mda.method = A_FILENAME;
			strlcpy(ep->agent.mda.buffer, xn->u.buffer,
			    sizeof ep->agent.mda.buffer);
		}
		else if (xn->type == EXPAND_FILTER) {
			ep->agent.mda.method = A_MDA;
			strlcpy(ep->agent.mda.buffer, xn->u.buffer,
			    sizeof ep->agent.mda.buffer);
		}
		else if (xn->type == EXPAND_USERNAME) {
			ep->agent.mda.method = rule->r_action;
			strlcpy(ep->agent.mda.buffer, rule->r_value.buffer,
			    sizeof ep->agent.mda.buffer);
			tag = mailaddr_tag(&ep->dest);
			if (rule->r_action == A_MAILDIR && tag && tag[0]) {
				strlcat(ep->agent.mda.buffer, "/.",
				    sizeof (ep->agent.mda.buffer));
				strlcat(ep->agent.mda.buffer, tag,
				    sizeof (ep->agent.mda.buffer));
			}
		}
		else
			fatalx("lka_deliver: bad node type");

		lka_expand_format(ep->agent.mda.buffer,
		    sizeof(ep->agent.mda.buffer), ep);
		break;
	default:
		fatalx("lka_submit: bad rule action");
	}

	TAILQ_INSERT_TAIL(&lks->deliverylist, ep, entry);
}

static size_t
lka_expand_format(char *buf, size_t len, const struct envelope *ep)
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
				if (! ub->getbyname(&u, ep->agent.mda.user))
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
			const char	*string, *tmp = p + 1;
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
				string = ep->agent.mda.user;
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

static void
mailaddr_to_username(const struct mailaddr *maddr, char *dst, size_t len)
{
	char	*tag;

	xlowercase(dst, maddr->user, len);

	/* gilles+hackers@ -> gilles@ */
	if ((tag = strchr(dst, '+')) != NULL)
		*tag++ = '\0';
}

static const char *
mailaddr_tag(const struct mailaddr *maddr)
{
	const char *tag;

	if ((tag = strchr(maddr->user, '+'))) {
		tag++;
		while (*tag == '.')
			tag++;
	}

	return (tag);
}
