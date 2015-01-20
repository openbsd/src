/*	$OpenBSD: lka_session.c,v 1.69 2015/01/20 17:37:54 deraadt Exp $	*/

/*
 * Copyright (c) 2011 Gilles Chehade <gilles@poolp.org>
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
#include <sys/socket.h>
#include <sys/wait.h>

#include <netinet/in.h>

#include <ctype.h>
#include <errno.h>
#include <event.h>
#include <imsg.h>
#include <resolv.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

#include "smtpd.h"
#include "log.h"

#define	EXPAND_DEPTH	10

#define	F_WAITING	0x01

struct lka_session {
	uint64_t		 id; /* given by smtp */

	TAILQ_HEAD(, envelope)	 deliverylist;
	struct expand		 expand;

	int			 flags;
	int			 error;
	const char		*errormsg;
	struct envelope		 envelope;
	struct xnodes		 nodes;
	/* waiting for fwdrq */
	struct rule		*rule;
	struct expandnode	*node;
};

static void lka_expand(struct lka_session *, struct rule *,
    struct expandnode *);
static void lka_submit(struct lka_session *, struct rule *,
    struct expandnode *);
static void lka_resume(struct lka_session *);
static size_t lka_expand_format(char *, size_t, const struct envelope *,
    const struct userinfo *);
static void mailaddr_to_username(const struct mailaddr *, char *, size_t);

static int mod_lowercase(char *, size_t);
static int mod_uppercase(char *, size_t);
static int mod_strip(char *, size_t);

struct modifiers {
	char	*name;
	int	(*f)(char *buf, size_t len);
} token_modifiers[] = {
	{ "lowercase",	mod_lowercase },
	{ "uppercase",	mod_uppercase },
	{ "strip",	mod_strip },
	{ "raw",	NULL },		/* special case, must stay last */
};

static int		init;
static struct tree	sessions;

#define	MAXTOKENLEN	128

void
lka_session(uint64_t id, struct envelope *envelope)
{
	struct lka_session	*lks;
	struct expandnode	 xn;

	if (init == 0) {
		init = 1;
		tree_init(&sessions);
	}

	lks = xcalloc(1, sizeof(*lks), "lka_session");
	lks->id = id;
	RB_INIT(&lks->expand.tree);
	TAILQ_INIT(&lks->deliverylist);
	tree_xset(&sessions, lks->id, lks);

	lks->envelope = *envelope;

	TAILQ_INIT(&lks->nodes);
	memset(&xn, 0, sizeof xn);
	xn.type = EXPAND_ADDRESS;
	xn.u.mailaddr = lks->envelope.rcpt;
	lks->expand.rule = NULL;
	lks->expand.queue = &lks->nodes;
	expand_insert(&lks->expand, &xn);
	lka_resume(lks);
}

void
lka_session_forward_reply(struct forward_req *fwreq, int fd)
{
	struct lka_session     *lks;
	struct rule	       *rule;
	struct expandnode      *xn;
	int			ret;

	lks = tree_xget(&sessions, fwreq->id);
	xn = lks->node;
	rule = lks->rule;

	lks->flags &= ~F_WAITING;

	switch (fwreq->status) {
	case 0:
		/* permanent failure while lookup ~/.forward */
		log_trace(TRACE_EXPAND, "expand: ~/.forward failed for user %s",
		    fwreq->user);
		lks->error = LKA_PERMFAIL;
		break;
	case 1:
		if (fd == -1) {
			if (lks->expand.rule->r_forwardonly) {
				log_trace(TRACE_EXPAND, "expand: no .forward "
				    "for user %s on forward-only rule", fwreq->user);
				lks->error = LKA_TEMPFAIL;
			}
			else if (lks->expand.rule->r_action == A_NONE) {
				log_trace(TRACE_EXPAND, "expand: no .forward "
				    "for user %s and no default action on rule", fwreq->user);
				lks->error = LKA_PERMFAIL;
			}
			else {
				log_trace(TRACE_EXPAND, "expand: no .forward for "
				    "user %s, just deliver", fwreq->user);
				lka_submit(lks, rule, xn);
			}
		}
		else {
			/* expand for the current user and rule */
			lks->expand.rule = rule;
			lks->expand.parent = xn;
			lks->expand.alias = 0;
			xn->mapping = rule->r_mapping;
			xn->userbase = rule->r_userbase;
			/* forwards_get() will close the descriptor no matter what */
			ret = forwards_get(fd, &lks->expand);
			if (ret == -1) {
				log_trace(TRACE_EXPAND, "expand: temporary "
				    "forward error for user %s", fwreq->user);
				lks->error = LKA_TEMPFAIL;
			}
			else if (ret == 0) {
				if (lks->expand.rule->r_forwardonly) {
					log_trace(TRACE_EXPAND, "expand: empty .forward "
					    "for user %s on forward-only rule", fwreq->user);
					lks->error = LKA_TEMPFAIL;
				}
				else if (lks->expand.rule->r_action == A_NONE) {
					log_trace(TRACE_EXPAND, "expand: empty .forward "
					    "for user %s and no default action on rule", fwreq->user);
					lks->error = LKA_PERMFAIL;
				}
				else {
					log_trace(TRACE_EXPAND, "expand: empty .forward "
					    "for user %s, just deliver", fwreq->user);
					lka_submit(lks, rule, xn);
				}
			}
		}
		break;
	default:
		/* temporary failure while looking up ~/.forward */
		lks->error = LKA_TEMPFAIL;
	}

	lka_resume(lks);
}

static void
lka_resume(struct lka_session *lks)
{
	struct envelope		*ep;
	struct expandnode	*xn;

	if (lks->error)
		goto error;

	/* pop next node and expand it */
	while ((xn = TAILQ_FIRST(&lks->nodes))) {
		TAILQ_REMOVE(&lks->nodes, xn, tq_entry);
		lka_expand(lks, xn->rule, xn);
		if (lks->flags & F_WAITING)
			return;
		if (lks->error)
			goto error;
	}

	/* delivery list is empty, reject */
	if (TAILQ_FIRST(&lks->deliverylist) == NULL) {
		log_trace(TRACE_EXPAND, "expand: lka_done: expanded to empty "
		    "delivery list");
		lks->error = LKA_PERMFAIL;
	}
    error:
	if (lks->error) {
		m_create(p_pony, IMSG_SMTP_EXPAND_RCPT, 0, 0, -1);
		m_add_id(p_pony, lks->id);
		m_add_int(p_pony, lks->error);

		if (lks->errormsg)
			m_add_string(p_pony, lks->errormsg);
		else {
			if (lks->error == LKA_PERMFAIL)
				m_add_string(p_pony, "550 Invalid recipient");
			else if (lks->error == LKA_TEMPFAIL)
				m_add_string(p_pony, "451 Temporary failure");
		}

		m_close(p_pony);
		while ((ep = TAILQ_FIRST(&lks->deliverylist)) != NULL) {
			TAILQ_REMOVE(&lks->deliverylist, ep, entry);
			free(ep);
		}
	}
	else {
		/* Process the delivery list and submit envelopes to queue */
		while ((ep = TAILQ_FIRST(&lks->deliverylist)) != NULL) {
			TAILQ_REMOVE(&lks->deliverylist, ep, entry);
			m_create(p_queue, IMSG_LKA_ENVELOPE_SUBMIT, 0, 0, -1);
			m_add_id(p_queue, lks->id);
			m_add_envelope(p_queue, ep);
			m_close(p_queue);
			free(ep);
		}

		m_create(p_queue, IMSG_LKA_ENVELOPE_COMMIT, 0, 0, -1);
		m_add_id(p_queue, lks->id);
		m_close(p_queue);
	}

	expand_clear(&lks->expand);
	tree_xpop(&sessions, lks->id);
	free(lks);
}

static void
lka_expand(struct lka_session *lks, struct rule *rule, struct expandnode *xn)
{
	struct forward_req	fwreq;
	struct envelope		ep;
	struct expandnode	node;
	struct mailaddr		maddr;
	int			r;
	union lookup		lk;

	if (xn->depth >= EXPAND_DEPTH) {
		log_trace(TRACE_EXPAND, "expand: lka_expand: node too deep.");
		lks->error = LKA_PERMFAIL;
		return;
	}

	switch (xn->type) {
	case EXPAND_INVALID:
	case EXPAND_INCLUDE:
		fatalx("lka_expand: unexpected type");
		break;

	case EXPAND_ADDRESS:

		log_trace(TRACE_EXPAND, "expand: lka_expand: address: %s@%s "
		    "[depth=%d]",
		    xn->u.mailaddr.user, xn->u.mailaddr.domain, xn->depth);

		/* Pass the node through the ruleset */
		ep = lks->envelope;
		ep.dest = xn->u.mailaddr;
		if (xn->parent) /* nodes with parent are forward addresses */
			ep.flags |= EF_INTERNAL;
		rule = ruleset_match(&ep);
		if (rule == NULL || rule->r_decision == R_REJECT) {
			lks->error = (errno == EAGAIN) ?
			    LKA_TEMPFAIL : LKA_PERMFAIL;
			break;
		}

		xn->mapping = rule->r_mapping;
		xn->userbase = rule->r_userbase;

		if (rule->r_action == A_RELAY || rule->r_action == A_RELAYVIA) {
			lka_submit(lks, rule, xn);
		}
		else if (rule->r_desttype == DEST_VDOM) {
			/* expand */
			lks->expand.rule = rule;
			lks->expand.parent = xn;
			lks->expand.alias = 1;

			/* temporary replace the mailaddr with a copy where
			 * we eventually strip the '+'-part before lookup.
			 */
			maddr = xn->u.mailaddr;
			mailaddr_to_username(&xn->u.mailaddr, maddr.user,
			    sizeof maddr.user);

			r = aliases_virtual_get(&lks->expand, &maddr);
			if (r == -1) {
				lks->error = LKA_TEMPFAIL;
				log_trace(TRACE_EXPAND, "expand: lka_expand: "
				    "error in virtual alias lookup");
			}
			else if (r == 0) {
				lks->error = LKA_PERMFAIL;
				log_trace(TRACE_EXPAND, "expand: lka_expand: "
				    "no aliases for virtual");
			}
		}
		else {
			lks->expand.rule = rule;
			lks->expand.parent = xn;
			lks->expand.alias = 1;
			memset(&node, 0, sizeof node);
			node.type = EXPAND_USERNAME;
			mailaddr_to_username(&xn->u.mailaddr, node.u.user,
				sizeof node.u.user);
			node.mapping = rule->r_mapping;
			node.userbase = rule->r_userbase;
			expand_insert(&lks->expand, &node);
		}
		break;

	case EXPAND_USERNAME:
		log_trace(TRACE_EXPAND, "expand: lka_expand: username: %s "
		    "[depth=%d]", xn->u.user, xn->depth);

		if (xn->sameuser) {
			log_trace(TRACE_EXPAND, "expand: lka_expand: same "
			    "user, submitting");
			lka_submit(lks, rule, xn);
			break;
		}

		/* expand aliases with the given rule */
		lks->expand.rule = rule;
		lks->expand.parent = xn;
		lks->expand.alias = 1;
		xn->mapping = rule->r_mapping;
		xn->userbase = rule->r_userbase;
		if (rule->r_mapping) {
			r = aliases_get(&lks->expand, xn->u.user);
			if (r == -1) {
				log_trace(TRACE_EXPAND, "expand: lka_expand: "
				    "error in alias lookup");
				lks->error = LKA_TEMPFAIL;
			}
			if (r)
				break;
		}

		/* A username should not exceed the size of a system user */
		if (strlen(xn->u.user) >= sizeof fwreq.user) {
			log_trace(TRACE_EXPAND, "expand: lka_expand: "
			    "user-part too long to be a system user");
			lks->error = LKA_PERMFAIL;
			break;
		}

		r = table_lookup(rule->r_userbase, NULL, xn->u.user, K_USERINFO, &lk);
		if (r == -1) {
			log_trace(TRACE_EXPAND, "expand: lka_expand: "
			    "backend error while searching user");
			lks->error = LKA_TEMPFAIL;
			break;
		}
		if (r == 0) {
			log_trace(TRACE_EXPAND, "expand: lka_expand: "
			    "user-part does not match system user");
			lks->error = LKA_PERMFAIL;
			break;
		}

		/* no aliases found, query forward file */
		lks->rule = rule;
		lks->node = xn;

		memset(&fwreq, 0, sizeof(fwreq));
		fwreq.id = lks->id;
		(void)strlcpy(fwreq.user, lk.userinfo.username, sizeof(fwreq.user));
		(void)strlcpy(fwreq.directory, lk.userinfo.directory, sizeof(fwreq.directory));
		fwreq.uid = lk.userinfo.uid;
		fwreq.gid = lk.userinfo.gid;

		m_compose(p_parent, IMSG_LKA_OPEN_FORWARD, 0, 0, -1,
		    &fwreq, sizeof(fwreq));
		lks->flags |= F_WAITING;
		break;

	case EXPAND_FILENAME:
		if (rule->r_forwardonly) {
			log_trace(TRACE_EXPAND, "expand: filename matched on forward-only rule");
			lks->error = LKA_TEMPFAIL;
			break;
		}
		log_trace(TRACE_EXPAND, "expand: lka_expand: filename: %s "
		    "[depth=%d]", xn->u.buffer, xn->depth);
		lka_submit(lks, rule, xn);
		break;

	case EXPAND_ERROR:
		if (rule->r_forwardonly) {
			log_trace(TRACE_EXPAND, "expand: error matched on forward-only rule");
			lks->error = LKA_TEMPFAIL;
			break;
		}
		log_trace(TRACE_EXPAND, "expand: lka_expand: error: %s "
		    "[depth=%d]", xn->u.buffer, xn->depth);
		if (xn->u.buffer[0] == '4')
			lks->error = LKA_TEMPFAIL;
		else if (xn->u.buffer[0] == '5')
			lks->error = LKA_PERMFAIL;
		lks->errormsg = xn->u.buffer;
		break;

	case EXPAND_FILTER:
		if (rule->r_forwardonly) {
			log_trace(TRACE_EXPAND, "expand: filter matched on forward-only rule");
			lks->error = LKA_TEMPFAIL;
			break;
		}
		log_trace(TRACE_EXPAND, "expand: lka_expand: filter: %s "
		    "[depth=%d]", xn->u.buffer, xn->depth);
		lka_submit(lks, rule, xn);
		break;
	}
}

static struct expandnode *
lka_find_ancestor(struct expandnode *xn, enum expand_type type)
{
	while (xn && (xn->type != type))
		xn = xn->parent;
	if (xn == NULL) {
		log_warnx("warn: lka_find_ancestor: no ancestors of type %d",
		    type);
		fatalx(NULL);
	}
	return (xn);
}

static void
lka_submit(struct lka_session *lks, struct rule *rule, struct expandnode *xn)
{
	union lookup		 lk;
	struct envelope		*ep;
	struct expandnode	*xn2;
	int			 r;

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

		/* only rewrite if not a bounce */
		if (ep->sender.user[0] && rule->r_as && rule->r_as->user[0])
			(void)strlcpy(ep->sender.user, rule->r_as->user,
			    sizeof ep->sender.user);
		if (ep->sender.user[0] && rule->r_as && rule->r_as->domain[0])
			(void)strlcpy(ep->sender.domain, rule->r_as->domain,
			    sizeof ep->sender.domain);
		break;
	case A_NONE:
	case A_MBOX:
	case A_MAILDIR:
	case A_FILENAME:
	case A_MDA:
	case A_LMTP:
		ep->type = D_MDA;
		ep->dest = lka_find_ancestor(xn, EXPAND_ADDRESS)->u.mailaddr;

		/* set username */
		if ((xn->type == EXPAND_FILTER || xn->type == EXPAND_FILENAME)
		    && xn->alias) {
			(void)strlcpy(ep->agent.mda.username, SMTPD_USER,
			    sizeof(ep->agent.mda.username));
		}
		else {
			xn2 = lka_find_ancestor(xn, EXPAND_USERNAME);
			(void)strlcpy(ep->agent.mda.username, xn2->u.user,
			    sizeof(ep->agent.mda.username));
		}

		r = table_lookup(rule->r_userbase, NULL, ep->agent.mda.username,
		    K_USERINFO, &lk);
		if (r <= 0) {
			lks->error = (r == -1) ? LKA_TEMPFAIL : LKA_PERMFAIL;
			free(ep);
			return;
		}
		(void)strlcpy(ep->agent.mda.usertable, rule->r_userbase->t_name,
		    sizeof ep->agent.mda.usertable);
		(void)strlcpy(ep->agent.mda.username, lk.userinfo.username,
		    sizeof ep->agent.mda.username);

		if (xn->type == EXPAND_FILENAME) {
			ep->agent.mda.method = A_FILENAME;
			(void)strlcpy(ep->agent.mda.buffer, xn->u.buffer,
			    sizeof ep->agent.mda.buffer);
		}
		else if (xn->type == EXPAND_FILTER) {
			ep->agent.mda.method = A_MDA;
			(void)strlcpy(ep->agent.mda.buffer, xn->u.buffer,
			    sizeof ep->agent.mda.buffer);
		}
		else if (xn->type == EXPAND_USERNAME) {
			ep->agent.mda.method = rule->r_action;
			(void)strlcpy(ep->agent.mda.buffer, rule->r_value.buffer,
			    sizeof ep->agent.mda.buffer);
		}
		else
			fatalx("lka_deliver: bad node type");

		r = lka_expand_format(ep->agent.mda.buffer,
		    sizeof(ep->agent.mda.buffer), ep, &lk.userinfo);
		if (!r) {
			lks->error = LKA_TEMPFAIL;
			log_warnx("warn: format string error while"
			    " expanding for user %s", ep->agent.mda.username);
			free(ep);
			return;
		}
		break;
	default:
		fatalx("lka_submit: bad rule action");
	}

	TAILQ_INSERT_TAIL(&lks->deliverylist, ep, entry);
}


static size_t
lka_expand_token(char *dest, size_t len, const char *token,
    const struct envelope *ep, const struct userinfo *ui)
{
	char		rtoken[MAXTOKENLEN];
	char		tmp[EXPAND_BUFFER];
	const char     *string;
	char	       *lbracket, *rbracket, *content, *sep, *mods;
	ssize_t		i;
	ssize_t		begoff, endoff;
	const char     *errstr = NULL;
	int		replace = 1;
	int		raw = 0;

	begoff = 0;
	endoff = EXPAND_BUFFER;
	mods = NULL;

	if (strlcpy(rtoken, token, sizeof rtoken) >= sizeof rtoken)
		return 0;

	/* token[x[:y]] -> extracts optional x and y, converts into offsets */
	if ((lbracket = strchr(rtoken, '[')) &&
	    (rbracket = strchr(rtoken, ']'))) {
		/* ] before [ ... or empty */
		if (rbracket < lbracket || rbracket - lbracket <= 1)
			return 0;

		*lbracket = *rbracket = '\0';
		 content  = lbracket + 1;

		 if ((sep = strchr(content, ':')) == NULL)
			 endoff = begoff = strtonum(content, -EXPAND_BUFFER,
			     EXPAND_BUFFER, &errstr);
		 else {
			 *sep = '\0';
			 if (content != sep)
				 begoff = strtonum(content, -EXPAND_BUFFER,
				     EXPAND_BUFFER, &errstr);
			 if (*(++sep)) {
				 if (errstr == NULL)
					 endoff = strtonum(sep, -EXPAND_BUFFER,
					     EXPAND_BUFFER, &errstr);
			 }
		 }
		 if (errstr)
			 return 0;

		 /* token:mod_1,mod_2,mod_n -> extract modifiers */
		 mods = strchr(rbracket + 1, ':');
	} else {
		if ((mods = strchr(rtoken, ':')) != NULL)
			*mods++ = '\0';
	}

	/* token -> expanded token */
	if (! strcasecmp("sender", rtoken)) {
		if (snprintf(tmp, sizeof tmp, "%s@%s",
			ep->sender.user, ep->sender.domain) <= 0)
			return 0;
		string = tmp;
	}
	else if (! strcasecmp("dest", rtoken)) {
		if (snprintf(tmp, sizeof tmp, "%s@%s",
			ep->dest.user, ep->dest.domain) <= 0)
			return 0;
		string = tmp;
	}
	else if (! strcasecmp("rcpt", rtoken)) {
		if (snprintf(tmp, sizeof tmp, "%s@%s",
			ep->rcpt.user, ep->rcpt.domain) <= 0)
			return 0;
		string = tmp;
	}
	else if (! strcasecmp("sender.user", rtoken))
		string = ep->sender.user;
	else if (! strcasecmp("sender.domain", rtoken))
		string = ep->sender.domain;
	else if (! strcasecmp("user.username", rtoken))
		string = ui->username;
	else if (! strcasecmp("user.directory", rtoken)) {
		string = ui->directory;
		replace = 0;
	}
	else if (! strcasecmp("dest.user", rtoken))
		string = ep->dest.user;
	else if (! strcasecmp("dest.domain", rtoken))
		string = ep->dest.domain;
	else if (! strcasecmp("rcpt.user", rtoken))
		string = ep->rcpt.user;
	else if (! strcasecmp("rcpt.domain", rtoken))
		string = ep->rcpt.domain;
	else
		return 0;

	if (string != tmp) {
		if (strlcpy(tmp, string, sizeof tmp) >= sizeof tmp)
			return 0;
		string = tmp;
	}

	/*  apply modifiers */
	if (mods != NULL) {
		do {
			if ((sep = strchr(mods, '|')) != NULL)
				*sep++ = '\0';
			for (i = 0; (size_t)i < nitems(token_modifiers); ++i) {
				if (! strcasecmp(token_modifiers[i].name, mods)) {
					if (token_modifiers[i].f == NULL) {
						raw = 1;
						break;
					}
					if (! token_modifiers[i].f(tmp, sizeof tmp))
						return 0; /* modifier error */
					break;
				}
			}
			if ((size_t)i == nitems(token_modifiers))
				return 0; /* modifier not found */
		} while ((mods = sep) != NULL);
	}
	
	if (! raw && replace)
		for (i = 0; (size_t)i < strlen(tmp); ++i)
			if (strchr(MAILADDR_ESCAPE, tmp[i]))
				tmp[i] = ':';

	/* expanded string is empty */
	i = strlen(string);
	if (i == 0)
		return 0;

	/* begin offset beyond end of string */
	if (begoff >= i)
		return 0;

	/* end offset beyond end of string, make it end of string */
	if (endoff >= i)
		endoff = i - 1;

	/* negative begin offset, make it relative to end of string */
	if (begoff < 0)
		begoff += i;
	/* negative end offset, make it relative to end of string,
	 * note that end offset is inclusive.
	 */
	if (endoff < 0)
		endoff += i - 1;

	/* check that final offsets are valid */
	if (begoff < 0 || endoff < 0 || endoff < begoff)
		return 0;
	endoff += 1; /* end offset is inclusive */

	/* check that substring does not exceed destination buffer length */
	i = endoff - begoff;
	if ((size_t)i + 1 >= len)
		return 0;

	string += begoff;
	for (; i; i--) {
		*dest = (replace && *string == '/') ? ':' : *string;
		dest++;
		string++;
	}

	return endoff - begoff;
}


static size_t
lka_expand_format(char *buf, size_t len, const struct envelope *ep,
    const struct userinfo *ui)
{
	char		tmpbuf[EXPAND_BUFFER], *ptmp, *pbuf, *ebuf;
	char		exptok[EXPAND_BUFFER];
	size_t		exptoklen;
	char		token[MAXTOKENLEN];
	size_t		ret, tmpret;

	if (len < sizeof tmpbuf)
		fatalx("lka_expand_format: tmp buffer < rule buffer");

	memset(tmpbuf, 0, sizeof tmpbuf);
	pbuf = buf;
	ptmp = tmpbuf;
	ret = tmpret = 0;

	/* special case: ~/ only allowed expanded at the beginning */
	if (strncmp(pbuf, "~/", 2) == 0) {
		tmpret = snprintf(ptmp, sizeof tmpbuf, "%s/", ui->directory);
		if (tmpret >= sizeof tmpbuf) {
			log_warnx("warn: user directory for %s too large",
			    ui->directory);
			return 0;
		}
		ret  += tmpret;
		ptmp += tmpret;
		pbuf += 2;
	}


	/* expansion loop */
	for (; *pbuf && ret < sizeof tmpbuf; ret += tmpret) {
		if (*pbuf == '%' && *(pbuf + 1) == '%') {
			*ptmp++ = *pbuf++;
			pbuf  += 1;
			tmpret = 1;
			continue;
		}

		if (*pbuf != '%' || *(pbuf + 1) != '{') {
			*ptmp++ = *pbuf++;
			tmpret = 1;
			continue;
		}

		/* %{...} otherwise fail */
		if (*(pbuf+1) != '{' || (ebuf = strchr(pbuf+1, '}')) == NULL)
			return 0;

		/* extract token from %{token} */
		if ((size_t)(ebuf - pbuf) - 1 >= sizeof token)
			return 0;

		memcpy(token, pbuf+2, ebuf-pbuf-1);
		if (strchr(token, '}') == NULL)
			return 0;
		*strchr(token, '}') = '\0';

		exptoklen = lka_expand_token(exptok, sizeof exptok, token, ep,
		    ui);
		if (exptoklen == 0)
			return 0;

		memcpy(ptmp, exptok, exptoklen);
		pbuf   = ebuf + 1;
		ptmp  += exptoklen;
		tmpret = exptoklen;
	}
	if (ret >= sizeof tmpbuf)
		return 0;

	if ((ret = strlcpy(buf, tmpbuf, len)) >= len)
		return 0;

	return ret;
}

static void
mailaddr_to_username(const struct mailaddr *maddr, char *dst, size_t len)
{
	char	*tag;

	xlowercase(dst, maddr->user, len);

	/* gilles+hackers@ -> gilles@ */
	if ((tag = strchr(dst, TAG_CHAR)) != NULL)
		*tag++ = '\0';
}

static int 
mod_lowercase(char *buf, size_t len)
{
	char tmp[EXPAND_BUFFER];

	if (! lowercase(tmp, buf, sizeof tmp))
		return 0;
	if (strlcpy(buf, tmp, len) >= len)
		return 0;
	return 1;
}

static int 
mod_uppercase(char *buf, size_t len)
{
	char tmp[EXPAND_BUFFER];

	if (! uppercase(tmp, buf, sizeof tmp))
		return 0;
	if (strlcpy(buf, tmp, len) >= len)
		return 0;
	return 1;
}

static int
mod_strip(char *buf, size_t len)
{
	char *tag, *at;
	unsigned int i;

	/* gilles+hackers -> gilles */
	if ((tag = strchr(buf, TAG_CHAR)) != NULL) {
		/* gilles+hackers@poolp.org -> gilles@poolp.org */
		if ((at = strchr(tag, '@')) != NULL) {
			for (i = 0; i <= strlen(at); ++i)
				tag[i] = at[i];
		} else
			*tag = '\0';
	}
	return 1;
}
