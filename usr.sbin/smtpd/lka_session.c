/*	$OpenBSD: lka_session.c,v 1.54 2013/02/05 11:45:18 gilles Exp $	*/

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
#include <sys/param.h>
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
static const char * mailaddr_tag(const struct mailaddr *);

static struct tree	sessions = SPLAY_INITIALIZER(&sessions);

#define	MAXTOKENLEN	128

void
lka_session(uint64_t id, struct envelope *envelope)
{
	struct lka_session	*lks;
	struct expandnode	 xn;

	lks = xcalloc(1, sizeof(*lks), "lka_session");
	lks->id = id;
	RB_INIT(&lks->expand.tree);
	TAILQ_INIT(&lks->deliverylist);
	tree_xset(&sessions, lks->id, lks);

	lks->envelope = *envelope;

	TAILQ_INIT(&lks->nodes);
	bzero(&xn, sizeof xn);
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

	lks = tree_xget(&sessions, fwreq->id);
	xn = lks->node;
	rule = lks->rule;

	lks->flags &= ~F_WAITING;

	switch (fwreq->status) {
	case 0:
		/* permanent failure while lookup ~/.forward */
		log_trace(TRACE_LOOKUP, "lookup: ~/.forward failed for user %s",
		    fwreq->user);
		lks->error = LKA_PERMFAIL;
		break;
	case 1:
		if (fd == -1) {
			log_trace(TRACE_LOOKUP, "lookup: no .forward for "
			    "user %s, just deliver", fwreq->user);
			lka_submit(lks, rule, xn);
		}
		else {
			/* expand for the current user and rule */
			lks->expand.rule = rule;
			lks->expand.parent = xn;
			lks->expand.alias = 0;

			/* forwards_get() will close the descriptor no matter what */
			if (! forwards_get(fd, &lks->expand)) {
				log_trace(TRACE_LOOKUP, "lookup: temporary "
				    "forward error for user %s", fwreq->user);
				lks->error = LKA_TEMPFAIL;
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
		log_trace(TRACE_LOOKUP, "lookup: lka_done: expanded to empty "
		    "delivery list");
		lks->error = LKA_PERMFAIL;
	}
    error:
	if (lks->error) {
		m_create(p_smtp, IMSG_LKA_EXPAND_RCPT, 0, 0, -1, 24);
		m_add_id(p_smtp, lks->id);
		m_add_int(p_smtp, lks->error);
		m_close(p_smtp);
		while ((ep = TAILQ_FIRST(&lks->deliverylist)) != NULL) {
			TAILQ_REMOVE(&lks->deliverylist, ep, entry);
			free(ep);
		}
	}
	else {
		/* Process the delivery list and submit envelopes to queue */
		while ((ep = TAILQ_FIRST(&lks->deliverylist)) != NULL) {
			TAILQ_REMOVE(&lks->deliverylist, ep, entry);
			m_create(p_queue, IMSG_QUEUE_SUBMIT_ENVELOPE, 0, 0, -1,
			    MSZ_EVP);
			m_add_id(p_queue, lks->id);
			m_add_envelope(p_queue, ep);
			m_close(p_queue);
			free(ep);
		}

		m_create(p_queue, IMSG_QUEUE_COMMIT_ENVELOPES, 0, 0, -1, 9);
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
	int			r;
	struct userinfo	       *tu = NULL;

	if (xn->depth >= EXPAND_DEPTH) {
		log_trace(TRACE_LOOKUP, "lookup: lka_expand: node too deep.");
		lks->error = LKA_PERMFAIL;
		return;
	}

	switch (xn->type) {
	case EXPAND_INVALID:
	case EXPAND_INCLUDE:
		fatalx("lka_expand: unexpected type");
		break;

	case EXPAND_ADDRESS:

		log_trace(TRACE_LOOKUP, "lookup: lka_expand: address: %s@%s "
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

		if (rule->r_action == A_RELAY || rule->r_action == A_RELAYVIA) {
			lka_submit(lks, rule, xn);
		}
		else if (rule->r_desttype == DEST_VDOM) {
			/* expand */
			lks->expand.rule = rule;
			lks->expand.parent = xn;
			lks->expand.alias = 1;
			r = aliases_virtual_get(rule->r_mapping,
			    &lks->expand, &xn->u.mailaddr);
			if (r == -1) {
				lks->error = LKA_TEMPFAIL;
				log_trace(TRACE_LOOKUP, "lookup: lka_expand: "
				    "error in virtual alias lookup");
			}
			else if (r == 0) {
				lks->error = LKA_PERMFAIL;
				log_trace(TRACE_LOOKUP, "lookup: lka_expand: "
				    "no aliases for virtual");
			}
		}
		else {
			lks->expand.rule = rule;
			lks->expand.parent = xn;
			lks->expand.alias = 1;
			bzero(&node, sizeof node);
			node.type = EXPAND_USERNAME;
			mailaddr_to_username(&xn->u.mailaddr, node.u.user,
				sizeof node.u.user);
			expand_insert(&lks->expand, &node);
		}
		break;

	case EXPAND_USERNAME:
		log_trace(TRACE_LOOKUP, "lookup: lka_expand: username: %s "
		    "[depth=%d]", xn->u.user, xn->depth);

		if (xn->sameuser) {
			log_trace(TRACE_LOOKUP, "lookup: lka_expand: same "
			    "user, submitting");
			lka_submit(lks, rule, xn);
			break;
		}

		/* expand aliases with the given rule */
		lks->expand.rule = rule;
		lks->expand.parent = xn;
		lks->expand.alias = 1;
		if (rule->r_mapping) {
			r = aliases_get(rule->r_mapping, &lks->expand,
			    xn->u.user);
			if (r == -1) {
				log_trace(TRACE_LOOKUP, "lookup: lka_expand: "
				    "error in alias lookup");
				lks->error = LKA_TEMPFAIL;
			}
			if (r)
				break;
		}

		/* A username should not exceed the size of a system user */
		if (strlen(xn->u.user) >= sizeof fwreq.user) {
			log_trace(TRACE_LOOKUP, "lookup: lka_expand: "
			    "user-part too long to be a system user");
			lks->error = LKA_PERMFAIL;
			break;
		}

		r = table_lookup(rule->r_users, xn->u.user, K_USERINFO, (void **)&tu);
		if (r == -1) {
			log_trace(TRACE_LOOKUP, "lookup: lka_expand: "
			    "backend error while searching user");
			lks->error = LKA_TEMPFAIL;
			break;
		}
		if (r == 0) {
			log_trace(TRACE_LOOKUP, "lookup: lka_expand: "
			    "user-part does not match system user");
			lks->error = LKA_PERMFAIL;
			break;
		}

		/* no aliases found, query forward file */
		lks->rule = rule;
		lks->node = xn;
		fwreq.id = lks->id;
		(void)strlcpy(fwreq.user, tu->username, sizeof(fwreq.user));
		(void)strlcpy(fwreq.directory, tu->directory, sizeof(fwreq.directory));
		fwreq.uid = tu->uid;
		fwreq.gid = tu->gid;
		m_compose(p_parent, IMSG_PARENT_FORWARD_OPEN, 0, 0, -1,
		    &fwreq, sizeof(fwreq));
		lks->flags |= F_WAITING;
		free(tu);
		break;

	case EXPAND_FILENAME:
		log_trace(TRACE_LOOKUP, "lookup: lka_expand: filename: %s "
		    "[depth=%d]", xn->u.buffer, xn->depth);
		lka_submit(lks, rule, xn);
		break;

	case EXPAND_FILTER:
		log_trace(TRACE_LOOKUP, "lookup: lka_expand: filter: %s "
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
		log_warnx("warn: lka_find_ancestor: no ancestors of type %i",
		    type);
		fatalx(NULL);
	}
	return (xn);
}

static void
lka_submit(struct lka_session *lks, struct rule *rule, struct expandnode *xn)
{
	struct userinfo		*tu;
	struct envelope		*ep;
	struct expandnode	*xn2;
	const char		*tag;
	int			r;

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
			strlcpy(ep->agent.mda.username, SMTPD_USER,
			    sizeof(ep->agent.mda.username));
		}
		else {
			xn2 = lka_find_ancestor(xn, EXPAND_USERNAME);
			strlcpy(ep->agent.mda.username, xn2->u.user,
			    sizeof(ep->agent.mda.username));
		}

		r = table_lookup(rule->r_users, ep->agent.mda.username, K_USERINFO,
		    (void **)&tu);
		if (r <= 0) {
			lks->error = (r == -1) ? LKA_TEMPFAIL : LKA_PERMFAIL;
			free(ep);
			return;
		}
		strlcpy(ep->agent.mda.usertable, rule->r_users->t_name,
		    sizeof ep->agent.mda.usertable);
		strlcpy(ep->agent.mda.username, tu->username,
		    sizeof ep->agent.mda.username);

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
				    sizeof(ep->agent.mda.buffer));
				strlcat(ep->agent.mda.buffer, tag,
				    sizeof(ep->agent.mda.buffer));
			}
		}
		else
			fatalx("lka_deliver: bad node type");

		r = lka_expand_format(ep->agent.mda.buffer,
		    sizeof(ep->agent.mda.buffer), ep, tu);
		free(tu);
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
	char	       *lbracket, *rbracket, *content, *sep;
	ssize_t		i;
	ssize_t		begoff, endoff;
	const char     *errstr = NULL;

	begoff = 0;
	endoff = EXPAND_BUFFER;

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
	else if (! strcasecmp("user.directory", rtoken))
		string = ui->directory;
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
		*dest = (*string == '/') ? ':' : *string;
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

	bzero(tmpbuf, sizeof tmpbuf);
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

		if (! lowercase(exptok, exptok, sizeof exptok))
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
