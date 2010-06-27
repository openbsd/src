/*	$OpenBSD: pf_ioctl.c,v 1.233 2010/06/27 01:28:44 mcbride Exp $ */

/*
 * Copyright (c) 2001 Daniel Hartmeier
 * Copyright (c) 2002,2003 Henning Brauer
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *    - Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    - Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Effort sponsored in part by the Defense Advanced Research Projects
 * Agency (DARPA) and Air Force Research Laboratory, Air Force
 * Materiel Command, USAF, under agreement number F30602-01-2-0537.
 *
 */

#include "pfsync.h"
#include "pflog.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/filio.h>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/kernel.h>
#include <sys/time.h>
#include <sys/timeout.h>
#include <sys/pool.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/kthread.h>
#include <sys/rwlock.h>
#include <sys/syslog.h>
#include <uvm/uvm_extern.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/ip_icmp.h>

#include <dev/rndvar.h>
#include <crypto/md5.h>
#include <net/pfvar.h>

#if NPFSYNC > 0
#include <net/if_pfsync.h>
#endif /* NPFSYNC > 0 */

#if NPFLOG > 0
#include <net/if_pflog.h>
#endif /* NPFLOG > 0 */

#ifdef INET6
#include <netinet/ip6.h>
#include <netinet/in_pcb.h>
#endif /* INET6 */

#ifdef ALTQ
#include <altq/altq.h>
#endif

void			 pfattach(int);
void			 pf_thread_create(void *);
int			 pfopen(dev_t, int, int, struct proc *);
int			 pfclose(dev_t, int, int, struct proc *);
int			 pfioctl(dev_t, u_long, caddr_t, int, struct proc *);
#ifdef ALTQ
int			 pf_begin_altq(u_int32_t *);
int			 pf_rollback_altq(u_int32_t);
int			 pf_commit_altq(u_int32_t);
int			 pf_enable_altq(struct pf_altq *);
int			 pf_disable_altq(struct pf_altq *);
#endif /* ALTQ */
int			 pf_begin_rules(u_int32_t *, const char *);
int			 pf_rollback_rules(u_int32_t, char *);
int			 pf_setup_pfsync_matching(struct pf_ruleset *);
void			 pf_hash_rule(MD5_CTX *, struct pf_rule *);
void			 pf_hash_rule_addr(MD5_CTX *, struct pf_rule_addr *);
int			 pf_commit_rules(u_int32_t, char *);
int			 pf_addr_setup(struct pf_ruleset *,
			    struct pf_addr_wrap *, sa_family_t);
int			 pf_kif_setup(char *, struct pfi_kif **);
void			 pf_addr_copyout(struct pf_addr_wrap *);
void			 pf_trans_set_commit(void);

struct pf_rule		 pf_default_rule, pf_default_rule_new;
struct rwlock		 pf_consistency_lock = RWLOCK_INITIALIZER("pfcnslk");
#ifdef ALTQ
static int		 pf_altq_running;
#endif

struct {
	char		statusif[IFNAMSIZ];
	u_int32_t	debug;
	u_int32_t	hostid;
	u_int32_t	reass;
	u_int32_t	mask;
} pf_trans_set;

#define	PF_TSET_STATUSIF	0x01
#define	PF_TSET_DEBUG		0x02
#define	PF_TSET_HOSTID		0x04
#define	PF_TSET_REASS		0x08

#define	TAGID_MAX	 50000
TAILQ_HEAD(pf_tags, pf_tagname)	pf_tags = TAILQ_HEAD_INITIALIZER(pf_tags),
				pf_qids = TAILQ_HEAD_INITIALIZER(pf_qids);

#if (PF_QNAME_SIZE != PF_TAG_NAME_SIZE)
#error PF_QNAME_SIZE must be equal to PF_TAG_NAME_SIZE
#endif
u_int16_t		 tagname2tag(struct pf_tags *, char *);
void			 tag2tagname(struct pf_tags *, u_int16_t, char *);
void			 tag_unref(struct pf_tags *, u_int16_t);
int			 pf_rtlabel_add(struct pf_addr_wrap *);
void			 pf_rtlabel_remove(struct pf_addr_wrap *);
void			 pf_rtlabel_copyout(struct pf_addr_wrap *);


void
pfattach(int num)
{
	u_int32_t *timeout = pf_default_rule.timeout;

	pool_init(&pf_rule_pl, sizeof(struct pf_rule), 0, 0, 0, "pfrulepl",
	    &pool_allocator_nointr);
	pool_init(&pf_src_tree_pl, sizeof(struct pf_src_node), 0, 0, 0,
	    "pfsrctrpl", NULL);
	pool_init(&pf_sn_item_pl, sizeof(struct pf_sn_item), 0, 0, 0,
	    "pfsnitempl", NULL);
	pool_init(&pf_state_pl, sizeof(struct pf_state), 0, 0, 0, "pfstatepl",
	    NULL);
	pool_init(&pf_state_key_pl, sizeof(struct pf_state_key), 0, 0, 0,
	    "pfstatekeypl", NULL);
	pool_init(&pf_state_item_pl, sizeof(struct pf_state_item), 0, 0, 0,
	    "pfstateitempl", NULL);
	pool_init(&pf_rule_item_pl, sizeof(struct pf_rule_item), 0, 0, 0,
	    "pfruleitempl", NULL);
	pool_init(&pf_altq_pl, sizeof(struct pf_altq), 0, 0, 0, "pfaltqpl",
	    &pool_allocator_nointr);
	pfr_initialize();
	pfi_initialize();
	pf_osfp_initialize();

	pool_sethardlimit(pf_pool_limits[PF_LIMIT_STATES].pp,
	    pf_pool_limits[PF_LIMIT_STATES].limit, NULL, 0);

	if (physmem <= atop(100*1024*1024))
		pf_pool_limits[PF_LIMIT_TABLE_ENTRIES].limit =
		    PFR_KENTRY_HIWAT_SMALL;

	RB_INIT(&tree_src_tracking);
	RB_INIT(&pf_anchors);
	pf_init_ruleset(&pf_main_ruleset);
	TAILQ_INIT(&pf_altqs[0]);
	TAILQ_INIT(&pf_altqs[1]);
	pf_altqs_active = &pf_altqs[0];
	pf_altqs_inactive = &pf_altqs[1];
	TAILQ_INIT(&state_list);

	/* default rule should never be garbage collected */
	pf_default_rule.entries.tqe_prev = &pf_default_rule.entries.tqe_next;
	pf_default_rule.action = PF_PASS;
	pf_default_rule.nr = -1;
	pf_default_rule.rtableid = -1;

	/* initialize default timeouts */
	timeout[PFTM_TCP_FIRST_PACKET] = PFTM_TCP_FIRST_PACKET_VAL;
	timeout[PFTM_TCP_OPENING] = PFTM_TCP_OPENING_VAL;
	timeout[PFTM_TCP_ESTABLISHED] = PFTM_TCP_ESTABLISHED_VAL;
	timeout[PFTM_TCP_CLOSING] = PFTM_TCP_CLOSING_VAL;
	timeout[PFTM_TCP_FIN_WAIT] = PFTM_TCP_FIN_WAIT_VAL;
	timeout[PFTM_TCP_CLOSED] = PFTM_TCP_CLOSED_VAL;
	timeout[PFTM_UDP_FIRST_PACKET] = PFTM_UDP_FIRST_PACKET_VAL;
	timeout[PFTM_UDP_SINGLE] = PFTM_UDP_SINGLE_VAL;
	timeout[PFTM_UDP_MULTIPLE] = PFTM_UDP_MULTIPLE_VAL;
	timeout[PFTM_ICMP_FIRST_PACKET] = PFTM_ICMP_FIRST_PACKET_VAL;
	timeout[PFTM_ICMP_ERROR_REPLY] = PFTM_ICMP_ERROR_REPLY_VAL;
	timeout[PFTM_OTHER_FIRST_PACKET] = PFTM_OTHER_FIRST_PACKET_VAL;
	timeout[PFTM_OTHER_SINGLE] = PFTM_OTHER_SINGLE_VAL;
	timeout[PFTM_OTHER_MULTIPLE] = PFTM_OTHER_MULTIPLE_VAL;
	timeout[PFTM_FRAG] = PFTM_FRAG_VAL;
	timeout[PFTM_INTERVAL] = PFTM_INTERVAL_VAL;
	timeout[PFTM_SRC_NODE] = PFTM_SRC_NODE_VAL;
	timeout[PFTM_TS_DIFF] = PFTM_TS_DIFF_VAL;
	timeout[PFTM_ADAPTIVE_START] = PFSTATE_ADAPT_START;
	timeout[PFTM_ADAPTIVE_END] = PFSTATE_ADAPT_END;

	pf_default_rule.src.addr.type =  PF_ADDR_ADDRMASK;
	pf_default_rule.dst.addr.type =  PF_ADDR_ADDRMASK;
	pf_default_rule.rdr.addr.type =  PF_ADDR_NONE;
	pf_default_rule.nat.addr.type =  PF_ADDR_NONE;
	pf_default_rule.route.addr.type =  PF_ADDR_NONE;

	pf_normalize_init();
	bzero(&pf_status, sizeof(pf_status));
	pf_status.debug = LOG_ERR;
	pf_status.reass = PF_REASS_ENABLED;

	/* XXX do our best to avoid a conflict */
	pf_status.hostid = arc4random();

	/* require process context to purge states, so perform in a thread */
	kthread_create_deferred(pf_thread_create, NULL);
}

void
pf_thread_create(void *v)
{
	if (kthread_create(pf_purge_thread, NULL, NULL, "pfpurge"))
		panic("pfpurge thread");
}

int
pfopen(dev_t dev, int flags, int fmt, struct proc *p)
{
	if (minor(dev) >= 1)
		return (ENXIO);
	return (0);
}

int
pfclose(dev_t dev, int flags, int fmt, struct proc *p)
{
	if (minor(dev) >= 1)
		return (ENXIO);
	return (0);
}

void
pf_rm_rule(struct pf_rulequeue *rulequeue, struct pf_rule *rule)
{
	if (rulequeue != NULL) {
		if (rule->states_cur <= 0) {
			/*
			 * XXX - we need to remove the table *before* detaching
			 * the rule to make sure the table code does not delete
			 * the anchor under our feet.
			 */
			pf_tbladdr_remove(&rule->src.addr);
			pf_tbladdr_remove(&rule->dst.addr);
			pf_tbladdr_remove(&rule->rdr.addr);
			pf_tbladdr_remove(&rule->nat.addr);
			pf_tbladdr_remove(&rule->route.addr);
			if (rule->overload_tbl)
				pfr_detach_table(rule->overload_tbl);
		}
		TAILQ_REMOVE(rulequeue, rule, entries);
		rule->entries.tqe_prev = NULL;
		rule->nr = -1;
	}

	if (rule->states_cur > 0 || rule->src_nodes > 0 ||
	    rule->entries.tqe_prev != NULL)
		return;
	pf_tag_unref(rule->tag);
	pf_tag_unref(rule->match_tag);
#ifdef ALTQ
	if (rule->pqid != rule->qid)
		pf_qid_unref(rule->pqid);
	pf_qid_unref(rule->qid);
#endif
	pf_rtlabel_remove(&rule->src.addr);
	pf_rtlabel_remove(&rule->dst.addr);
	pfi_dynaddr_remove(&rule->src.addr);
	pfi_dynaddr_remove(&rule->dst.addr);
	pfi_dynaddr_remove(&rule->rdr.addr);
	pfi_dynaddr_remove(&rule->nat.addr);
	pfi_dynaddr_remove(&rule->route.addr);
	if (rulequeue == NULL) {
		pf_tbladdr_remove(&rule->src.addr);
		pf_tbladdr_remove(&rule->dst.addr);
		pf_tbladdr_remove(&rule->rdr.addr);
		pf_tbladdr_remove(&rule->nat.addr);
		pf_tbladdr_remove(&rule->route.addr);
		if (rule->overload_tbl)
			pfr_detach_table(rule->overload_tbl);
	}
	pfi_kif_unref(rule->rcv_kif, PFI_KIF_REF_RULE);
	pfi_kif_unref(rule->kif, PFI_KIF_REF_RULE);
	pfi_kif_unref(rule->rdr.kif, PFI_KIF_REF_RULE);
	pfi_kif_unref(rule->nat.kif, PFI_KIF_REF_RULE);
	pfi_kif_unref(rule->route.kif, PFI_KIF_REF_RULE);
	pf_anchor_remove(rule);
	pool_put(&pf_rule_pl, rule);
}

u_int16_t
tagname2tag(struct pf_tags *head, char *tagname)
{
	struct pf_tagname	*tag, *p = NULL;
	u_int16_t		 new_tagid = 1;

	TAILQ_FOREACH(tag, head, entries)
		if (strcmp(tagname, tag->name) == 0) {
			tag->ref++;
			return (tag->tag);
		}

	/*
	 * to avoid fragmentation, we do a linear search from the beginning
	 * and take the first free slot we find. if there is none or the list
	 * is empty, append a new entry at the end.
	 */

	/* new entry */
	if (!TAILQ_EMPTY(head))
		for (p = TAILQ_FIRST(head); p != NULL &&
		    p->tag == new_tagid; p = TAILQ_NEXT(p, entries))
			new_tagid = p->tag + 1;

	if (new_tagid > TAGID_MAX)
		return (0);

	/* allocate and fill new struct pf_tagname */
	tag = malloc(sizeof(*tag), M_TEMP, M_NOWAIT|M_ZERO);
	if (tag == NULL)
		return (0);
	strlcpy(tag->name, tagname, sizeof(tag->name));
	tag->tag = new_tagid;
	tag->ref++;

	if (p != NULL)	/* insert new entry before p */
		TAILQ_INSERT_BEFORE(p, tag, entries);
	else	/* either list empty or no free slot in between */
		TAILQ_INSERT_TAIL(head, tag, entries);

	return (tag->tag);
}

void
tag2tagname(struct pf_tags *head, u_int16_t tagid, char *p)
{
	struct pf_tagname	*tag;

	TAILQ_FOREACH(tag, head, entries)
		if (tag->tag == tagid) {
			strlcpy(p, tag->name, PF_TAG_NAME_SIZE);
			return;
		}
}

void
tag_unref(struct pf_tags *head, u_int16_t tag)
{
	struct pf_tagname	*p, *next;

	if (tag == 0)
		return;

	for (p = TAILQ_FIRST(head); p != NULL; p = next) {
		next = TAILQ_NEXT(p, entries);
		if (tag == p->tag) {
			if (--p->ref == 0) {
				TAILQ_REMOVE(head, p, entries);
				free(p, M_TEMP);
			}
			break;
		}
	}
}

u_int16_t
pf_tagname2tag(char *tagname)
{
	return (tagname2tag(&pf_tags, tagname));
}

void
pf_tag2tagname(u_int16_t tagid, char *p)
{
	tag2tagname(&pf_tags, tagid, p);
}

void
pf_tag_ref(u_int16_t tag)
{
	struct pf_tagname *t;

	TAILQ_FOREACH(t, &pf_tags, entries)
		if (t->tag == tag)
			break;
	if (t != NULL)
		t->ref++;
}

void
pf_tag_unref(u_int16_t tag)
{
	tag_unref(&pf_tags, tag);
}

int
pf_rtlabel_add(struct pf_addr_wrap *a)
{
	if (a->type == PF_ADDR_RTLABEL &&
	    (a->v.rtlabel = rtlabel_name2id(a->v.rtlabelname)) == 0)
		return (-1);
	return (0);
}

void
pf_rtlabel_remove(struct pf_addr_wrap *a)
{
	if (a->type == PF_ADDR_RTLABEL)
		rtlabel_unref(a->v.rtlabel);
}

void
pf_rtlabel_copyout(struct pf_addr_wrap *a)
{
	const char	*name;

	if (a->type == PF_ADDR_RTLABEL && a->v.rtlabel) {
		if ((name = rtlabel_id2name(a->v.rtlabel)) == NULL)
			strlcpy(a->v.rtlabelname, "?",
			    sizeof(a->v.rtlabelname));
		else
			strlcpy(a->v.rtlabelname, name,
			    sizeof(a->v.rtlabelname));
	}
}

#ifdef ALTQ
u_int32_t
pf_qname2qid(char *qname)
{
	return ((u_int32_t)tagname2tag(&pf_qids, qname));
}

void
pf_qid2qname(u_int32_t qid, char *p)
{
	tag2tagname(&pf_qids, (u_int16_t)qid, p);
}

void
pf_qid_unref(u_int32_t qid)
{
	tag_unref(&pf_qids, (u_int16_t)qid);
}

int
pf_begin_altq(u_int32_t *ticket)
{
	struct pf_altq	*altq;
	int		 error = 0;

	/* Purge the old altq list */
	while ((altq = TAILQ_FIRST(pf_altqs_inactive)) != NULL) {
		TAILQ_REMOVE(pf_altqs_inactive, altq, entries);
		if (altq->qname[0] == 0) {
			/* detach and destroy the discipline */
			error = altq_remove(altq);
		} else
			pf_qid_unref(altq->qid);
		pool_put(&pf_altq_pl, altq);
	}
	if (error)
		return (error);
	*ticket = ++ticket_altqs_inactive;
	altqs_inactive_open = 1;
	return (0);
}

int
pf_rollback_altq(u_int32_t ticket)
{
	struct pf_altq	*altq;
	int		 error = 0;

	if (!altqs_inactive_open || ticket != ticket_altqs_inactive)
		return (0);
	/* Purge the old altq list */
	while ((altq = TAILQ_FIRST(pf_altqs_inactive)) != NULL) {
		TAILQ_REMOVE(pf_altqs_inactive, altq, entries);
		if (altq->qname[0] == 0) {
			/* detach and destroy the discipline */
			error = altq_remove(altq);
		} else
			pf_qid_unref(altq->qid);
		pool_put(&pf_altq_pl, altq);
	}
	altqs_inactive_open = 0;
	return (error);
}

int
pf_commit_altq(u_int32_t ticket)
{
	struct pf_altqqueue	*old_altqs;
	struct pf_altq		*altq;
	int			 s, err, error = 0;

	if (!altqs_inactive_open || ticket != ticket_altqs_inactive)
		return (EBUSY);

	/* swap altqs, keep the old. */
	s = splsoftnet();
	old_altqs = pf_altqs_active;
	pf_altqs_active = pf_altqs_inactive;
	pf_altqs_inactive = old_altqs;
	ticket_altqs_active = ticket_altqs_inactive;

	/* Attach new disciplines */
	TAILQ_FOREACH(altq, pf_altqs_active, entries) {
		if (altq->qname[0] == 0) {
			/* attach the discipline */
			error = altq_pfattach(altq);
			if (error == 0 && pf_altq_running)
				error = pf_enable_altq(altq);
			if (error != 0) {
				splx(s);
				return (error);
			}
		}
	}

	/* Purge the old altq list */
	while ((altq = TAILQ_FIRST(pf_altqs_inactive)) != NULL) {
		TAILQ_REMOVE(pf_altqs_inactive, altq, entries);
		if (altq->qname[0] == 0) {
			/* detach and destroy the discipline */
			if (pf_altq_running)
				error = pf_disable_altq(altq);
			err = altq_pfdetach(altq);
			if (err != 0 && error == 0)
				error = err;
			err = altq_remove(altq);
			if (err != 0 && error == 0)
				error = err;
		} else
			pf_qid_unref(altq->qid);
		pool_put(&pf_altq_pl, altq);
	}
	splx(s);

	altqs_inactive_open = 0;
	return (error);
}

int
pf_enable_altq(struct pf_altq *altq)
{
	struct ifnet		*ifp;
	struct tb_profile	 tb;
	int			 s, error = 0;

	if ((ifp = ifunit(altq->ifname)) == NULL)
		return (EINVAL);

	if (ifp->if_snd.altq_type != ALTQT_NONE)
		error = altq_enable(&ifp->if_snd);

	/* set tokenbucket regulator */
	if (error == 0 && ifp != NULL && ALTQ_IS_ENABLED(&ifp->if_snd)) {
		tb.rate = altq->ifbandwidth;
		tb.depth = altq->tbrsize;
		s = splnet();
		error = tbr_set(&ifp->if_snd, &tb);
		splx(s);
	}

	return (error);
}

int
pf_disable_altq(struct pf_altq *altq)
{
	struct ifnet		*ifp;
	struct tb_profile	 tb;
	int			 s, error;

	if ((ifp = ifunit(altq->ifname)) == NULL)
		return (EINVAL);

	/*
	 * when the discipline is no longer referenced, it was overridden
	 * by a new one.  if so, just return.
	 */
	if (altq->altq_disc != ifp->if_snd.altq_disc)
		return (0);

	error = altq_disable(&ifp->if_snd);

	if (error == 0) {
		/* clear tokenbucket regulator */
		tb.rate = 0;
		s = splnet();
		error = tbr_set(&ifp->if_snd, &tb);
		splx(s);
	}

	return (error);
}
#endif /* ALTQ */

int
pf_begin_rules(u_int32_t *ticket, const char *anchor)
{
	struct pf_ruleset	*rs;
	struct pf_rule		*rule;

	if ((rs = pf_find_or_create_ruleset(anchor)) == NULL)
		return (EINVAL);
	while ((rule = TAILQ_FIRST(rs->rules.inactive.ptr)) != NULL) {
		pf_rm_rule(rs->rules.inactive.ptr, rule);
		rs->rules.inactive.rcount--;
	}
	*ticket = ++rs->rules.inactive.ticket;
	rs->rules.inactive.open = 1;
	return (0);
}

int
pf_rollback_rules(u_int32_t ticket, char *anchor)
{
	struct pf_ruleset	*rs;
	struct pf_rule		*rule;

	rs = pf_find_ruleset(anchor);
	if (rs == NULL || !rs->rules.inactive.open ||
	    rs->rules.inactive.ticket != ticket)
		return (0);
	while ((rule = TAILQ_FIRST(rs->rules.inactive.ptr)) != NULL) {
		pf_rm_rule(rs->rules.inactive.ptr, rule);
		rs->rules.inactive.rcount--;
	}
	rs->rules.inactive.open = 0;
	return (0);
}

#define PF_MD5_UPD(st, elm)						\
		MD5Update(ctx, (u_int8_t *) &(st)->elm, sizeof((st)->elm))

#define PF_MD5_UPD_STR(st, elm)						\
		MD5Update(ctx, (u_int8_t *) (st)->elm, strlen((st)->elm))

#define PF_MD5_UPD_HTONL(st, elm, stor) do {				\
		(stor) = htonl((st)->elm);				\
		MD5Update(ctx, (u_int8_t *) &(stor), sizeof(u_int32_t));\
} while (0)

#define PF_MD5_UPD_HTONS(st, elm, stor) do {				\
		(stor) = htons((st)->elm);				\
		MD5Update(ctx, (u_int8_t *) &(stor), sizeof(u_int16_t));\
} while (0)

void
pf_hash_rule_addr(MD5_CTX *ctx, struct pf_rule_addr *pfr)
{
	PF_MD5_UPD(pfr, addr.type);
	switch (pfr->addr.type) {
		case PF_ADDR_DYNIFTL:
			PF_MD5_UPD(pfr, addr.v.ifname);
			PF_MD5_UPD(pfr, addr.iflags);
			break;
		case PF_ADDR_TABLE:
			PF_MD5_UPD(pfr, addr.v.tblname);
			break;
		case PF_ADDR_ADDRMASK:
			/* XXX ignore af? */
			PF_MD5_UPD(pfr, addr.v.a.addr.addr32);
			PF_MD5_UPD(pfr, addr.v.a.mask.addr32);
			break;
		case PF_ADDR_RTLABEL:
			PF_MD5_UPD(pfr, addr.v.rtlabelname);
			break;
	}

	PF_MD5_UPD(pfr, port[0]);
	PF_MD5_UPD(pfr, port[1]);
	PF_MD5_UPD(pfr, neg);
	PF_MD5_UPD(pfr, port_op);
}

void
pf_hash_rule(MD5_CTX *ctx, struct pf_rule *rule)
{
	u_int16_t x;
	u_int32_t y;

	pf_hash_rule_addr(ctx, &rule->src);
	pf_hash_rule_addr(ctx, &rule->dst);
	PF_MD5_UPD_STR(rule, label);
	PF_MD5_UPD_STR(rule, ifname);
	PF_MD5_UPD_STR(rule, rcv_ifname);
	PF_MD5_UPD_STR(rule, match_tagname);
	PF_MD5_UPD_HTONS(rule, match_tag, x); /* dup? */
	PF_MD5_UPD_HTONL(rule, os_fingerprint, y);
	PF_MD5_UPD_HTONL(rule, prob, y);
	PF_MD5_UPD_HTONL(rule, uid.uid[0], y);
	PF_MD5_UPD_HTONL(rule, uid.uid[1], y);
	PF_MD5_UPD(rule, uid.op);
	PF_MD5_UPD_HTONL(rule, gid.gid[0], y);
	PF_MD5_UPD_HTONL(rule, gid.gid[1], y);
	PF_MD5_UPD(rule, gid.op);
	PF_MD5_UPD_HTONL(rule, rule_flag, y);
	PF_MD5_UPD(rule, action);
	PF_MD5_UPD(rule, direction);
	PF_MD5_UPD(rule, af);
	PF_MD5_UPD(rule, quick);
	PF_MD5_UPD(rule, ifnot);
	PF_MD5_UPD(rule, match_tag_not);
	PF_MD5_UPD(rule, keep_state);
	PF_MD5_UPD(rule, proto);
	PF_MD5_UPD(rule, type);
	PF_MD5_UPD(rule, code);
	PF_MD5_UPD(rule, flags);
	PF_MD5_UPD(rule, flagset);
	PF_MD5_UPD(rule, allow_opts);
	PF_MD5_UPD(rule, rt);
	PF_MD5_UPD(rule, tos);
}

int
pf_commit_rules(u_int32_t ticket, char *anchor)
{
	struct pf_ruleset	*rs;
	struct pf_rule		*rule, **old_array;
	struct pf_rulequeue	*old_rules;
	int			 s, error;
	u_int32_t		 old_rcount;

	rs = pf_find_ruleset(anchor);
	if (rs == NULL || !rs->rules.inactive.open ||
	    ticket != rs->rules.inactive.ticket)
		return (EBUSY);

	/* Calculate checksum for the main ruleset */
	if (rs == &pf_main_ruleset) {
		error = pf_setup_pfsync_matching(rs);
		if (error != 0)
			return (error);
	}

	/* Swap rules, keep the old. */
	s = splsoftnet();
	old_rules = rs->rules.active.ptr;
	old_rcount = rs->rules.active.rcount;
	old_array = rs->rules.active.ptr_array;

	rs->rules.active.ptr = rs->rules.inactive.ptr;
	rs->rules.active.ptr_array = rs->rules.inactive.ptr_array;
	rs->rules.active.rcount = rs->rules.inactive.rcount;
	rs->rules.inactive.ptr = old_rules;
	rs->rules.inactive.ptr_array = old_array;
	rs->rules.inactive.rcount = old_rcount;

	rs->rules.active.ticket = rs->rules.inactive.ticket;
	pf_calc_skip_steps(rs->rules.active.ptr);


	/* Purge the old rule list. */
	while ((rule = TAILQ_FIRST(old_rules)) != NULL)
		pf_rm_rule(old_rules, rule);
	if (rs->rules.inactive.ptr_array)
		free(rs->rules.inactive.ptr_array, M_TEMP);
	rs->rules.inactive.ptr_array = NULL;
	rs->rules.inactive.rcount = 0;
	rs->rules.inactive.open = 0;
	pf_remove_if_empty_ruleset(rs);
	splx(s);
	return (0);
}

int
pf_setup_pfsync_matching(struct pf_ruleset *rs)
{
	MD5_CTX			 ctx;
	struct pf_rule		*rule;
	u_int8_t		 digest[PF_MD5_DIGEST_LENGTH];

	MD5Init(&ctx);
	if (rs->rules.inactive.ptr_array)
		free(rs->rules.inactive.ptr_array, M_TEMP);
	rs->rules.inactive.ptr_array = NULL;

	if (rs->rules.inactive.rcount) {
		rs->rules.inactive.ptr_array = malloc(sizeof(caddr_t) *
		    rs->rules.inactive.rcount,  M_TEMP, M_NOWAIT);

		if (!rs->rules.inactive.ptr_array)
			return (ENOMEM);
	}

	TAILQ_FOREACH(rule, rs->rules.inactive.ptr, entries) {
		pf_hash_rule(&ctx, rule);
		(rs->rules.inactive.ptr_array)[rule->nr] = rule;
	}

	MD5Final(digest, &ctx);
	memcpy(pf_status.pf_chksum, digest, sizeof(pf_status.pf_chksum));
	return (0);
}

int
pf_addr_setup(struct pf_ruleset *ruleset, struct pf_addr_wrap *addr,
    sa_family_t af)
{
	if (pfi_dynaddr_setup(addr, af) ||
	    pf_tbladdr_setup(ruleset, addr) ||
	    pf_rtlabel_add(addr))
		return (EINVAL);

	return (0);
}

int
pf_kif_setup(char *ifname, struct pfi_kif **kif)
{
	if (ifname[0]) {
		*kif = pfi_kif_get(ifname);
		if (*kif == NULL)
			return (EINVAL);

		pfi_kif_ref(*kif, PFI_KIF_REF_RULE);
	} else
		*kif = NULL;

	return (0);
}

void
pf_addr_copyout(struct pf_addr_wrap *addr)
{
	pfi_dynaddr_copyout(addr);
	pf_tbladdr_copyout(addr);
	pf_rtlabel_copyout(addr);
}

int
pfioctl(dev_t dev, u_long cmd, caddr_t addr, int flags, struct proc *p)
{
	int			 s;
	int			 error = 0;

	/* XXX keep in sync with switch() below */
	if (securelevel > 1)
		switch (cmd) {
		case DIOCGETRULES:
		case DIOCGETRULE:
		case DIOCGETSTATE:
		case DIOCSETSTATUSIF:
		case DIOCGETSTATUS:
		case DIOCCLRSTATUS:
		case DIOCNATLOOK:
		case DIOCSETDEBUG:
		case DIOCGETSTATES:
		case DIOCGETTIMEOUT:
		case DIOCCLRRULECTRS:
		case DIOCGETLIMIT:
		case DIOCGETALTQS:
		case DIOCGETALTQ:
		case DIOCGETQSTATS:
		case DIOCGETRULESETS:
		case DIOCGETRULESET:
		case DIOCRGETTABLES:
		case DIOCRGETTSTATS:
		case DIOCRCLRTSTATS:
		case DIOCRCLRADDRS:
		case DIOCRADDADDRS:
		case DIOCRDELADDRS:
		case DIOCRSETADDRS:
		case DIOCRGETASTATS:
		case DIOCRCLRASTATS:
		case DIOCRTSTADDRS:
		case DIOCOSFPGET:
		case DIOCGETSRCNODES:
		case DIOCCLRSRCNODES:
		case DIOCIGETIFACES:
		case DIOCSETIFFLAG:
		case DIOCCLRIFFLAG:
			break;
		case DIOCRCLRTABLES:
		case DIOCRADDTABLES:
		case DIOCRDELTABLES:
		case DIOCRSETTFLAGS:
			if (((struct pfioc_table *)addr)->pfrio_flags &
			    PFR_FLAG_DUMMY)
				break; /* dummy operation ok */
			return (EPERM);
		default:
			return (EPERM);
		}

	if (!(flags & FWRITE))
		switch (cmd) {
		case DIOCGETRULES:
		case DIOCGETSTATE:
		case DIOCGETSTATUS:
		case DIOCGETSTATES:
		case DIOCGETTIMEOUT:
		case DIOCGETLIMIT:
		case DIOCGETALTQS:
		case DIOCGETALTQ:
		case DIOCGETQSTATS:
		case DIOCGETRULESETS:
		case DIOCGETRULESET:
		case DIOCNATLOOK:
		case DIOCRGETTABLES:
		case DIOCRGETTSTATS:
		case DIOCRGETADDRS:
		case DIOCRGETASTATS:
		case DIOCRTSTADDRS:
		case DIOCOSFPGET:
		case DIOCGETSRCNODES:
		case DIOCIGETIFACES:
			break;
		case DIOCRCLRTABLES:
		case DIOCRADDTABLES:
		case DIOCRDELTABLES:
		case DIOCRCLRTSTATS:
		case DIOCRCLRADDRS:
		case DIOCRADDADDRS:
		case DIOCRDELADDRS:
		case DIOCRSETADDRS:
		case DIOCRSETTFLAGS:
			if (((struct pfioc_table *)addr)->pfrio_flags &
			    PFR_FLAG_DUMMY) {
				flags |= FWRITE; /* need write lock for dummy */
				break; /* dummy operation ok */
			}
			return (EACCES);
		case DIOCGETRULE:
			if (((struct pfioc_rule *)addr)->action ==
			    PF_GET_CLR_CNTR)
				return (EACCES);
			break;
		default:
			return (EACCES);
		}

	if (flags & FWRITE)
		rw_enter_write(&pf_consistency_lock);
	else
		rw_enter_read(&pf_consistency_lock);

	s = splsoftnet();
	switch (cmd) {

	case DIOCSTART:
		if (pf_status.running)
			error = EEXIST;
		else {
			pf_status.running = 1;
			pf_status.since = time_second;
			if (pf_status.stateid == 0) {
				pf_status.stateid = time_second;
				pf_status.stateid = pf_status.stateid << 32;
			}
			DPFPRINTF(LOG_NOTICE, "pf: started");
		}
		break;

	case DIOCSTOP:
		if (!pf_status.running)
			error = ENOENT;
		else {
			pf_status.running = 0;
			pf_status.since = time_second;
			DPFPRINTF(LOG_NOTICE, "pf: stopped");
		}
		break;

	case DIOCADDRULE: {
		struct pfioc_rule	*pr = (struct pfioc_rule *)addr;
		struct pf_ruleset	*ruleset;
		struct pf_rule		*rule, *tail;

		pr->anchor[sizeof(pr->anchor) - 1] = 0;
		ruleset = pf_find_ruleset(pr->anchor);
		if (ruleset == NULL) {
			error = EINVAL;
			break;
		}
		if (pr->rule.return_icmp >> 8 > ICMP_MAXTYPE) {
			error = EINVAL;
			break;
		}
		if (pr->ticket != ruleset->rules.inactive.ticket) {
			error = EBUSY;
			break;
		}
		rule = pool_get(&pf_rule_pl, PR_WAITOK|PR_LIMITFAIL);
		if (rule == NULL) {
			error = ENOMEM;
			break;
		}
		bcopy(&pr->rule, rule, sizeof(struct pf_rule));
		rule->cuid = p->p_cred->p_ruid;
		rule->cpid = p->p_pid;
		rule->anchor = NULL;
		rule->kif = NULL;
		rule->rcv_kif = NULL;
		/* initialize refcounting */
		rule->states_cur = 0;
		rule->src_nodes = 0;
		rule->entries.tqe_prev = NULL;

		switch (rule->af) {
		case 0:
			break;
#ifdef INET
		case AF_INET:
			break;
#endif /* INET */
#ifdef INET6
		case AF_INET6:
			break;
#endif /* INET6 */
		default:
			pool_put(&pf_rule_pl, rule);
			error = EAFNOSUPPORT;
			goto fail;
		}
		tail = TAILQ_LAST(ruleset->rules.inactive.ptr,
		    pf_rulequeue);
		if (tail)
			rule->nr = tail->nr + 1;
		else
			rule->nr = 0;

		if (rule->src.addr.type == PF_ADDR_NONE ||
		    rule->dst.addr.type == PF_ADDR_NONE)
			error = EINVAL;

		if (pf_kif_setup(rule->ifname, &rule->kif))
			error = EINVAL;
		if (pf_kif_setup(rule->rcv_ifname, &rule->rcv_kif))
			error = EINVAL;
		if (pf_kif_setup(rule->rdr.ifname, &rule->rdr.kif))
			error = EINVAL;
		if (pf_kif_setup(rule->nat.ifname, &rule->nat.kif))
			error = EINVAL;
		if (pf_kif_setup(rule->route.ifname, &rule->route.kif))
			error = EINVAL;

		if (rule->rtableid > 0 && !rtable_exists(rule->rtableid))
			error = EBUSY;

#ifdef ALTQ
		/* set queue IDs */
		if (rule->qname[0] != 0) {
			if ((rule->qid = pf_qname2qid(rule->qname)) == 0)
				error = EBUSY;
			else if (rule->pqname[0] != 0) {
				if ((rule->pqid =
				    pf_qname2qid(rule->pqname)) == 0)
					error = EBUSY;
			} else
				rule->pqid = rule->qid;
		}
#endif
		if (rule->tagname[0])
			if ((rule->tag = pf_tagname2tag(rule->tagname)) == 0)
				error = EBUSY;
		if (rule->match_tagname[0])
			if ((rule->match_tag =
			    pf_tagname2tag(rule->match_tagname)) == 0)
				error = EBUSY;
		if (rule->rt && !rule->direction)
			error = EINVAL;
#if NPFLOG > 0
		if (!rule->log)
			rule->logif = 0;
		if (rule->logif >= PFLOGIFS_MAX)
			error = EINVAL;
#endif
		if (pf_rtlabel_add(&rule->src.addr) ||
		    pf_rtlabel_add(&rule->dst.addr))
			error = EBUSY;
		if (pf_addr_setup(ruleset, &rule->src.addr, rule->af))
			error = EINVAL;
		if (pf_addr_setup(ruleset, &rule->dst.addr, rule->af))
			error = EINVAL;
		if (pf_addr_setup(ruleset, &rule->rdr.addr, rule->af))
			error = EINVAL;
		if (pf_addr_setup(ruleset, &rule->nat.addr, rule->af))
			error = EINVAL;
		if (pf_addr_setup(ruleset, &rule->route.addr, rule->af))
			error = EINVAL;
		if (pf_anchor_setup(rule, ruleset, pr->anchor_call))
			error = EINVAL;

		if (rule->overload_tblname[0]) {
			if ((rule->overload_tbl = pfr_attach_table(ruleset,
			    rule->overload_tblname, 0)) == NULL)
				error = EINVAL;
			else
				rule->overload_tbl->pfrkt_flags |=
				    PFR_TFLAG_ACTIVE;
		}

		if (error) {
			pf_rm_rule(NULL, rule);
			break;
		}
		rule->evaluations = rule->packets[0] = rule->packets[1] =
		    rule->bytes[0] = rule->bytes[1] = 0;
		TAILQ_INSERT_TAIL(ruleset->rules.inactive.ptr,
		    rule, entries);
		ruleset->rules.inactive.rcount++;
		break;
	}

	case DIOCGETRULES: {
		struct pfioc_rule	*pr = (struct pfioc_rule *)addr;
		struct pf_ruleset	*ruleset;
		struct pf_rule		*tail;

		pr->anchor[sizeof(pr->anchor) - 1] = 0;
		ruleset = pf_find_ruleset(pr->anchor);
		if (ruleset == NULL) {
			error = EINVAL;
			break;
		}
		tail = TAILQ_LAST(ruleset->rules.active.ptr, pf_rulequeue);
		if (tail)
			pr->nr = tail->nr + 1;
		else
			pr->nr = 0;
		pr->ticket = ruleset->rules.active.ticket;
		break;
	}

	case DIOCGETRULE: {
		struct pfioc_rule	*pr = (struct pfioc_rule *)addr;
		struct pf_ruleset	*ruleset;
		struct pf_rule		*rule;
		int			 i;

		pr->anchor[sizeof(pr->anchor) - 1] = 0;
		ruleset = pf_find_ruleset(pr->anchor);
		if (ruleset == NULL) {
			error = EINVAL;
			break;
		}
		if (pr->ticket != ruleset->rules.active.ticket) {
			error = EBUSY;
			break;
		}
		rule = TAILQ_FIRST(ruleset->rules.active.ptr);
		while ((rule != NULL) && (rule->nr != pr->nr))
			rule = TAILQ_NEXT(rule, entries);
		if (rule == NULL) {
			error = EBUSY;
			break;
		}
		bcopy(rule, &pr->rule, sizeof(struct pf_rule));
		if (pf_anchor_copyout(ruleset, rule, pr)) {
			error = EBUSY;
			break;
		}
		pf_addr_copyout(&pr->rule.src.addr);
		pf_addr_copyout(&pr->rule.dst.addr);
		pf_addr_copyout(&pr->rule.rdr.addr);
		pf_addr_copyout(&pr->rule.nat.addr);
		pf_addr_copyout(&pr->rule.route.addr);
		for (i = 0; i < PF_SKIP_COUNT; ++i)
			if (rule->skip[i].ptr == NULL)
				pr->rule.skip[i].nr = -1;
			else
				pr->rule.skip[i].nr =
				    rule->skip[i].ptr->nr;

		if (pr->action == PF_GET_CLR_CNTR) {
			rule->evaluations = 0;
			rule->packets[0] = rule->packets[1] = 0;
			rule->bytes[0] = rule->bytes[1] = 0;
			rule->states_tot = 0;
		}
		break;
	}

	case DIOCCHANGERULE: {
		struct pfioc_rule	*pcr = (struct pfioc_rule *)addr;
		struct pf_ruleset	*ruleset;
		struct pf_rule		*oldrule = NULL, *newrule = NULL;
		u_int32_t		 nr = 0;

		if (pcr->action < PF_CHANGE_ADD_HEAD ||
		    pcr->action > PF_CHANGE_GET_TICKET) {
			error = EINVAL;
			break;
		}
		ruleset = pf_find_ruleset(pcr->anchor);
		if (ruleset == NULL) {
			error = EINVAL;
			break;
		}

		if (pcr->action == PF_CHANGE_GET_TICKET) {
			pcr->ticket = ++ruleset->rules.active.ticket;
			break;
		} else {
			if (pcr->ticket !=
			    ruleset->rules.active.ticket) {
				error = EINVAL;
				break;
			}
			if (pcr->rule.return_icmp >> 8 > ICMP_MAXTYPE) {
				error = EINVAL;
				break;
			}
		}

		if (pcr->action != PF_CHANGE_REMOVE) {
			newrule = pool_get(&pf_rule_pl, PR_WAITOK|PR_LIMITFAIL);
			if (newrule == NULL) {
				error = ENOMEM;
				break;
			}
			bcopy(&pcr->rule, newrule, sizeof(struct pf_rule));
			newrule->cuid = p->p_cred->p_ruid;
			newrule->cpid = p->p_pid;
			/* initialize refcounting */
			newrule->states_cur = 0;
			newrule->entries.tqe_prev = NULL;

			switch (newrule->af) {
			case 0:
				break;
#ifdef INET
			case AF_INET:
				break;
#endif /* INET */
#ifdef INET6
			case AF_INET6:
				break;
#endif /* INET6 */
			default:
				pool_put(&pf_rule_pl, newrule);
				error = EAFNOSUPPORT;
				goto fail;
			}

			if (pf_kif_setup(newrule->ifname, &newrule->kif))
				error = EINVAL;
			if (pf_kif_setup(newrule->rcv_ifname, &newrule->rcv_kif))
				error = EINVAL;
			if (pf_kif_setup(newrule->rdr.ifname, &newrule->rdr.kif))
				error = EINVAL;
			if (pf_kif_setup(newrule->nat.ifname, &newrule->nat.kif))
				error = EINVAL;
			if (pf_kif_setup(newrule->route.ifname, &newrule->route.kif))
				error = EINVAL;

			if (newrule->rtableid > 0 &&
			    !rtable_exists(newrule->rtableid))
				error = EBUSY;

#ifdef ALTQ
			/* set queue IDs */
			if (newrule->qname[0] != 0) {
				if ((newrule->qid =
				    pf_qname2qid(newrule->qname)) == 0)
					error = EBUSY;
				else if (newrule->pqname[0] != 0) {
					if ((newrule->pqid =
					    pf_qname2qid(newrule->pqname)) == 0)
						error = EBUSY;
				} else
					newrule->pqid = newrule->qid;
			}
#endif /* ALTQ */
			if (newrule->tagname[0])
				if ((newrule->tag =
				    pf_tagname2tag(newrule->tagname)) == 0)
					error = EBUSY;
			if (newrule->match_tagname[0])
				if ((newrule->match_tag = pf_tagname2tag(
				    newrule->match_tagname)) == 0)
					error = EBUSY;
			if (newrule->rt && !newrule->direction)
				error = EINVAL;
#if NPFLOG > 0
			if (!newrule->log)
				newrule->logif = 0;
			if (newrule->logif >= PFLOGIFS_MAX)
				error = EINVAL;
#endif
			if (pf_rtlabel_add(&newrule->src.addr) ||
			    pf_rtlabel_add(&newrule->dst.addr))
				error = EBUSY;
			if (pf_addr_setup(ruleset, &newrule->src.addr, newrule->af))
				error = EINVAL;
			if (pf_addr_setup(ruleset, &newrule->dst.addr, newrule->af))
				error = EINVAL;
			if (pf_addr_setup(ruleset, &newrule->rdr.addr, newrule->af))
				error = EINVAL;
			if (pf_addr_setup(ruleset, &newrule->nat.addr, newrule->af))
				error = EINVAL;
			if (pf_addr_setup(ruleset, &newrule->route.addr, newrule->af))
				error = EINVAL;
			if (pf_anchor_setup(newrule, ruleset, pcr->anchor_call))
				error = EINVAL;

			if (newrule->overload_tblname[0]) {
				if ((newrule->overload_tbl = pfr_attach_table(
				    ruleset, newrule->overload_tblname, 0)) ==
				    NULL)
					error = EINVAL;
				else
					newrule->overload_tbl->pfrkt_flags |=
					    PFR_TFLAG_ACTIVE;
			}

			if (error) {
				pf_rm_rule(NULL, newrule);
				break;
			}
			newrule->evaluations = 0;
			newrule->packets[0] = newrule->packets[1] = 0;
			newrule->bytes[0] = newrule->bytes[1] = 0;
		}

		if (pcr->action == PF_CHANGE_ADD_HEAD)
			oldrule = TAILQ_FIRST(ruleset->rules.active.ptr);
		else if (pcr->action == PF_CHANGE_ADD_TAIL)
			oldrule = TAILQ_LAST(ruleset->rules.active.ptr,
			    pf_rulequeue);
		else {
			oldrule = TAILQ_FIRST(ruleset->rules.active.ptr);
			while ((oldrule != NULL) && (oldrule->nr != pcr->nr))
				oldrule = TAILQ_NEXT(oldrule, entries);
			if (oldrule == NULL) {
				if (newrule != NULL)
					pf_rm_rule(NULL, newrule);
				error = EINVAL;
				break;
			}
		}

		if (pcr->action == PF_CHANGE_REMOVE) {
			pf_rm_rule(ruleset->rules.active.ptr, oldrule);
			ruleset->rules.active.rcount--;
		} else {
			if (oldrule == NULL)
				TAILQ_INSERT_TAIL(
				    ruleset->rules.active.ptr,
				    newrule, entries);
			else if (pcr->action == PF_CHANGE_ADD_HEAD ||
			    pcr->action == PF_CHANGE_ADD_BEFORE)
				TAILQ_INSERT_BEFORE(oldrule, newrule, entries);
			else
				TAILQ_INSERT_AFTER(
				    ruleset->rules.active.ptr,
				    oldrule, newrule, entries);
			ruleset->rules.active.rcount++;
		}

		nr = 0;
		TAILQ_FOREACH(oldrule, ruleset->rules.active.ptr, entries)
			oldrule->nr = nr++;

		ruleset->rules.active.ticket++;

		pf_calc_skip_steps(ruleset->rules.active.ptr);
		pf_remove_if_empty_ruleset(ruleset);

		break;
	}

	case DIOCCLRSTATES: {
		struct pf_state		*s, *nexts;
		struct pfioc_state_kill *psk = (struct pfioc_state_kill *)addr;
		u_int			 killed = 0;

		for (s = RB_MIN(pf_state_tree_id, &tree_id); s; s = nexts) {
			nexts = RB_NEXT(pf_state_tree_id, &tree_id, s);

			if (!psk->psk_ifname[0] || !strcmp(psk->psk_ifname,
			    s->kif->pfik_name)) {
#if NPFSYNC > 0
				/* don't send out individual delete messages */
				SET(s->state_flags, PFSTATE_NOSYNC);
#endif
				pf_unlink_state(s);
				killed++;
			}
		}
		psk->psk_killed = killed;
#if NPFSYNC > 0
		pfsync_clear_states(pf_status.hostid, psk->psk_ifname);
#endif
		break;
	}

	case DIOCKILLSTATES: {
		struct pf_state		*s, *nexts;
		struct pf_state_key	*sk;
		struct pf_addr		*srcaddr, *dstaddr;
		u_int16_t		 srcport, dstport;
		struct pfioc_state_kill	*psk = (struct pfioc_state_kill *)addr;
		u_int			 killed = 0;

		if (psk->psk_pfcmp.id) {
			if (psk->psk_pfcmp.creatorid == 0)
				psk->psk_pfcmp.creatorid = pf_status.hostid;
			if ((s = pf_find_state_byid(&psk->psk_pfcmp))) {
				pf_unlink_state(s);
				psk->psk_killed = 1;
			}
			break;
		}

		for (s = RB_MIN(pf_state_tree_id, &tree_id); s;
		    s = nexts) {
			nexts = RB_NEXT(pf_state_tree_id, &tree_id, s);
			sk = s->key[PF_SK_WIRE];

			if (s->direction == PF_OUT) {
				srcaddr = &sk->addr[1];
				dstaddr = &sk->addr[0];
				srcport = sk->port[0];
				dstport = sk->port[0];
			} else {
				srcaddr = &sk->addr[0];
				dstaddr = &sk->addr[1];
				srcport = sk->port[0];
				dstport = sk->port[0];
			}
			if ((!psk->psk_af || sk->af == psk->psk_af)
			    && (!psk->psk_proto || psk->psk_proto ==
			    sk->proto) && psk->psk_rdomain == sk->rdomain &&
			    PF_MATCHA(psk->psk_src.neg,
			    &psk->psk_src.addr.v.a.addr,
			    &psk->psk_src.addr.v.a.mask,
			    srcaddr, sk->af) &&
			    PF_MATCHA(psk->psk_dst.neg,
			    &psk->psk_dst.addr.v.a.addr,
			    &psk->psk_dst.addr.v.a.mask,
			    dstaddr, sk->af) &&
			    (psk->psk_src.port_op == 0 ||
			    pf_match_port(psk->psk_src.port_op,
			    psk->psk_src.port[0], psk->psk_src.port[1],
			    srcport)) &&
			    (psk->psk_dst.port_op == 0 ||
			    pf_match_port(psk->psk_dst.port_op,
			    psk->psk_dst.port[0], psk->psk_dst.port[1],
			    dstport)) &&
			    (!psk->psk_label[0] || (s->rule.ptr->label[0] &&
			    !strcmp(psk->psk_label, s->rule.ptr->label))) &&
			    (!psk->psk_ifname[0] || !strcmp(psk->psk_ifname,
			    s->kif->pfik_name))) {
				pf_unlink_state(s);
				killed++;
			}
		}
		psk->psk_killed = killed;
		break;
	}

	case DIOCADDSTATE: {
		struct pfioc_state	*ps = (struct pfioc_state *)addr;
		struct pfsync_state	*sp = &ps->state;

		if (sp->timeout >= PFTM_MAX &&
		    sp->timeout != PFTM_UNTIL_PACKET) {
			error = EINVAL;
			break;
		}
		error = pfsync_state_import(sp, PFSYNC_SI_IOCTL);
		break;
	}

	case DIOCGETSTATE: {
		struct pfioc_state	*ps = (struct pfioc_state *)addr;
		struct pf_state		*s;
		struct pf_state_cmp	 id_key;

		bzero(&id_key, sizeof(id_key));
		bcopy(ps->state.id, &id_key.id, sizeof(id_key.id));
		id_key.creatorid = ps->state.creatorid;

		s = pf_find_state_byid(&id_key);
		if (s == NULL) {
			error = ENOENT;
			break;
		}

		pfsync_state_export(&ps->state, s);
		break;
	}

	case DIOCGETSTATES: {
		struct pfioc_states	*ps = (struct pfioc_states *)addr;
		struct pf_state		*state;
		struct pfsync_state	*p, *pstore;
		u_int32_t		 nr = 0;

		if (ps->ps_len == 0) {
			nr = pf_status.states;
			ps->ps_len = sizeof(struct pfsync_state) * nr;
			break;
		}

		pstore = malloc(sizeof(*pstore), M_TEMP, M_WAITOK);

		p = ps->ps_states;

		state = TAILQ_FIRST(&state_list);
		while (state) {
			if (state->timeout != PFTM_UNLINKED) {
				if ((nr+1) * sizeof(*p) > (unsigned)ps->ps_len)
					break;
				pfsync_state_export(pstore, state);
				error = copyout(pstore, p, sizeof(*p));
				if (error) {
					free(pstore, M_TEMP);
					goto fail;
				}
				p++;
				nr++;
			}
			state = TAILQ_NEXT(state, entry_list);
		}

		ps->ps_len = sizeof(struct pfsync_state) * nr;

		free(pstore, M_TEMP);
		break;
	}

	case DIOCGETSTATUS: {
		struct pf_status *s = (struct pf_status *)addr;
		bcopy(&pf_status, s, sizeof(struct pf_status));
		pfi_update_status(s->ifname, s);
		break;
	}

	case DIOCSETSTATUSIF: {
		struct pfioc_if	*pi = (struct pfioc_if *)addr;

		if (pi->ifname[0] == 0) {
			bzero(pf_status.ifname, IFNAMSIZ);
			break;
		}
		strlcpy(pf_trans_set.statusif, pi->ifname, IFNAMSIZ);
		pf_trans_set.mask |= PF_TSET_STATUSIF;
		break;
	}

	case DIOCCLRSTATUS: {
		bzero(pf_status.counters, sizeof(pf_status.counters));
		bzero(pf_status.fcounters, sizeof(pf_status.fcounters));
		bzero(pf_status.scounters, sizeof(pf_status.scounters));
		pf_status.since = time_second;
		if (*pf_status.ifname)
			pfi_update_status(pf_status.ifname, NULL);
		break;
	}

	case DIOCNATLOOK: {
		struct pfioc_natlook	*pnl = (struct pfioc_natlook *)addr;
		struct pf_state_key	*sk;
		struct pf_state		*state;
		struct pf_state_key_cmp	 key;
		int			 m = 0, direction = pnl->direction;
		int			 sidx, didx;

		/* NATLOOK src and dst are reversed, so reverse sidx/didx */
		sidx = (direction == PF_IN) ? 1 : 0;
		didx = (direction == PF_IN) ? 0 : 1;

		if (!pnl->proto ||
		    PF_AZERO(&pnl->saddr, pnl->af) ||
		    PF_AZERO(&pnl->daddr, pnl->af) ||
		    ((pnl->proto == IPPROTO_TCP ||
		    pnl->proto == IPPROTO_UDP) &&
		    (!pnl->dport || !pnl->sport)) ||
		    pnl->rdomain > RT_TABLEID_MAX)
			error = EINVAL;
		else {
			key.af = pnl->af;
			key.proto = pnl->proto;
			key.rdomain = pnl->rdomain;
			PF_ACPY(&key.addr[sidx], &pnl->saddr, pnl->af);
			key.port[sidx] = pnl->sport;
			PF_ACPY(&key.addr[didx], &pnl->daddr, pnl->af);
			key.port[didx] = pnl->dport;

			state = pf_find_state_all(&key, direction, &m);

			if (m > 1)
				error = E2BIG;	/* more than one state */
			else if (state != NULL) {
				sk = state->key[sidx];
				PF_ACPY(&pnl->rsaddr, &sk->addr[sidx], sk->af);
				pnl->rsport = sk->port[sidx];
				PF_ACPY(&pnl->rdaddr, &sk->addr[didx], sk->af);
				pnl->rdport = sk->port[didx];
			} else
				error = ENOENT;
		}
		break;
	}

	case DIOCSETTIMEOUT: {
		struct pfioc_tm	*pt = (struct pfioc_tm *)addr;

		if (pt->timeout < 0 || pt->timeout >= PFTM_MAX ||
		    pt->seconds < 0) {
			error = EINVAL;
			goto fail;
		}
		if (pt->timeout == PFTM_INTERVAL && pt->seconds == 0)
			pt->seconds = 1;
		pf_default_rule_new.timeout[pt->timeout] = pt->seconds;
		pt->seconds = pf_default_rule.timeout[pt->timeout];
		break;
	}

	case DIOCGETTIMEOUT: {
		struct pfioc_tm	*pt = (struct pfioc_tm *)addr;

		if (pt->timeout < 0 || pt->timeout >= PFTM_MAX) {
			error = EINVAL;
			goto fail;
		}
		pt->seconds = pf_default_rule.timeout[pt->timeout];
		break;
	}

	case DIOCGETLIMIT: {
		struct pfioc_limit	*pl = (struct pfioc_limit *)addr;

		if (pl->index < 0 || pl->index >= PF_LIMIT_MAX) {
			error = EINVAL;
			goto fail;
		}
		pl->limit = pf_pool_limits[pl->index].limit;
		break;
	}

	case DIOCSETLIMIT: {
		struct pfioc_limit	*pl = (struct pfioc_limit *)addr;

		if (pl->index < 0 || pl->index >= PF_LIMIT_MAX ||
		    pf_pool_limits[pl->index].pp == NULL) {
			error = EINVAL;
			goto fail;
		}
		if (((struct pool *)pf_pool_limits[pl->index].pp)->pr_nout >
		    pl->limit) {
			error = EBUSY;
			goto fail;
		}
		pf_pool_limits[pl->index].limit_new = pl->limit;
		pl->limit = pf_pool_limits[pl->index].limit;
		break;
	}

	case DIOCSETDEBUG: {
		u_int32_t	*level = (u_int32_t *)addr;

		pf_trans_set.debug = *level;
		pf_trans_set.mask |= PF_TSET_DEBUG;
		break;
	}

	case DIOCCLRRULECTRS: {
		/* obsoleted by DIOCGETRULE with action=PF_GET_CLR_CNTR */
		struct pf_ruleset	*ruleset = &pf_main_ruleset;
		struct pf_rule		*rule;

		TAILQ_FOREACH(rule,
		    ruleset->rules.active.ptr, entries) {
			rule->evaluations = 0;
			rule->packets[0] = rule->packets[1] = 0;
			rule->bytes[0] = rule->bytes[1] = 0;
		}
		break;
	}

#ifdef ALTQ
	case DIOCSTARTALTQ: {
		struct pf_altq		*altq;

		/* enable all altq interfaces on active list */
		TAILQ_FOREACH(altq, pf_altqs_active, entries) {
			if (altq->qname[0] == 0) {
				error = pf_enable_altq(altq);
				if (error != 0)
					break;
			}
		}
		if (error == 0)
			pf_altq_running = 1;
		DPFPRINTF(LOG_NOTICE, "altq: started");
		break;
	}

	case DIOCSTOPALTQ: {
		struct pf_altq		*altq;

		/* disable all altq interfaces on active list */
		TAILQ_FOREACH(altq, pf_altqs_active, entries) {
			if (altq->qname[0] == 0) {
				error = pf_disable_altq(altq);
				if (error != 0)
					break;
			}
		}
		if (error == 0)
			pf_altq_running = 0;
		DPFPRINTF(LOG_NOTICE, "altq: stopped");
		break;
	}

	case DIOCADDALTQ: {
		struct pfioc_altq	*pa = (struct pfioc_altq *)addr;
		struct pf_altq		*altq, *a;

		if (pa->ticket != ticket_altqs_inactive) {
			error = EBUSY;
			break;
		}
		altq = pool_get(&pf_altq_pl, PR_WAITOK|PR_LIMITFAIL);
		if (altq == NULL) {
			error = ENOMEM;
			break;
		}
		bcopy(&pa->altq, altq, sizeof(struct pf_altq));

		/*
		 * if this is for a queue, find the discipline and
		 * copy the necessary fields
		 */
		if (altq->qname[0] != 0) {
			if ((altq->qid = pf_qname2qid(altq->qname)) == 0) {
				error = EBUSY;
				pool_put(&pf_altq_pl, altq);
				break;
			}
			altq->altq_disc = NULL;
			TAILQ_FOREACH(a, pf_altqs_inactive, entries) {
				if (strncmp(a->ifname, altq->ifname,
				    IFNAMSIZ) == 0 && a->qname[0] == 0) {
					altq->altq_disc = a->altq_disc;
					break;
				}
			}
		}

		error = altq_add(altq);
		if (error) {
			pool_put(&pf_altq_pl, altq);
			break;
		}

		TAILQ_INSERT_TAIL(pf_altqs_inactive, altq, entries);
		bcopy(altq, &pa->altq, sizeof(struct pf_altq));
		break;
	}

	case DIOCGETALTQS: {
		struct pfioc_altq	*pa = (struct pfioc_altq *)addr;
		struct pf_altq		*altq;

		pa->nr = 0;
		TAILQ_FOREACH(altq, pf_altqs_active, entries)
			pa->nr++;
		pa->ticket = ticket_altqs_active;
		break;
	}

	case DIOCGETALTQ: {
		struct pfioc_altq	*pa = (struct pfioc_altq *)addr;
		struct pf_altq		*altq;
		u_int32_t		 nr;

		if (pa->ticket != ticket_altqs_active) {
			error = EBUSY;
			break;
		}
		nr = 0;
		altq = TAILQ_FIRST(pf_altqs_active);
		while ((altq != NULL) && (nr < pa->nr)) {
			altq = TAILQ_NEXT(altq, entries);
			nr++;
		}
		if (altq == NULL) {
			error = EBUSY;
			break;
		}
		bcopy(altq, &pa->altq, sizeof(struct pf_altq));
		break;
	}

	case DIOCCHANGEALTQ:
		/* CHANGEALTQ not supported yet! */
		error = ENODEV;
		break;

	case DIOCGETQSTATS: {
		struct pfioc_qstats	*pq = (struct pfioc_qstats *)addr;
		struct pf_altq		*altq;
		u_int32_t		 nr;
		int			 nbytes;

		if (pq->ticket != ticket_altqs_active) {
			error = EBUSY;
			break;
		}
		nbytes = pq->nbytes;
		nr = 0;
		altq = TAILQ_FIRST(pf_altqs_active);
		while ((altq != NULL) && (nr < pq->nr)) {
			altq = TAILQ_NEXT(altq, entries);
			nr++;
		}
		if (altq == NULL) {
			error = EBUSY;
			break;
		}
		error = altq_getqstats(altq, pq->buf, &nbytes);
		if (error == 0) {
			pq->scheduler = altq->scheduler;
			pq->nbytes = nbytes;
		}
		break;
	}
#endif /* ALTQ */

	case DIOCGETRULESETS: {
		struct pfioc_ruleset	*pr = (struct pfioc_ruleset *)addr;
		struct pf_ruleset	*ruleset;
		struct pf_anchor	*anchor;

		pr->path[sizeof(pr->path) - 1] = 0;
		if ((ruleset = pf_find_ruleset(pr->path)) == NULL) {
			error = EINVAL;
			break;
		}
		pr->nr = 0;
		if (ruleset->anchor == NULL) {
			/* XXX kludge for pf_main_ruleset */
			RB_FOREACH(anchor, pf_anchor_global, &pf_anchors)
				if (anchor->parent == NULL)
					pr->nr++;
		} else {
			RB_FOREACH(anchor, pf_anchor_node,
			    &ruleset->anchor->children)
				pr->nr++;
		}
		break;
	}

	case DIOCGETRULESET: {
		struct pfioc_ruleset	*pr = (struct pfioc_ruleset *)addr;
		struct pf_ruleset	*ruleset;
		struct pf_anchor	*anchor;
		u_int32_t		 nr = 0;

		pr->path[sizeof(pr->path) - 1] = 0;
		if ((ruleset = pf_find_ruleset(pr->path)) == NULL) {
			error = EINVAL;
			break;
		}
		pr->name[0] = 0;
		if (ruleset->anchor == NULL) {
			/* XXX kludge for pf_main_ruleset */
			RB_FOREACH(anchor, pf_anchor_global, &pf_anchors)
				if (anchor->parent == NULL && nr++ == pr->nr) {
					strlcpy(pr->name, anchor->name,
					    sizeof(pr->name));
					break;
				}
		} else {
			RB_FOREACH(anchor, pf_anchor_node,
			    &ruleset->anchor->children)
				if (nr++ == pr->nr) {
					strlcpy(pr->name, anchor->name,
					    sizeof(pr->name));
					break;
				}
		}
		if (!pr->name[0])
			error = EBUSY;
		break;
	}

	case DIOCRCLRTABLES: {
		struct pfioc_table *io = (struct pfioc_table *)addr;

		if (io->pfrio_esize != 0) {
			error = ENODEV;
			break;
		}
		error = pfr_clr_tables(&io->pfrio_table, &io->pfrio_ndel,
		    io->pfrio_flags | PFR_FLAG_USERIOCTL);
		break;
	}

	case DIOCRADDTABLES: {
		struct pfioc_table *io = (struct pfioc_table *)addr;

		if (io->pfrio_esize != sizeof(struct pfr_table)) {
			error = ENODEV;
			break;
		}
		error = pfr_add_tables(io->pfrio_buffer, io->pfrio_size,
		    &io->pfrio_nadd, io->pfrio_flags | PFR_FLAG_USERIOCTL);
		break;
	}

	case DIOCRDELTABLES: {
		struct pfioc_table *io = (struct pfioc_table *)addr;

		if (io->pfrio_esize != sizeof(struct pfr_table)) {
			error = ENODEV;
			break;
		}
		error = pfr_del_tables(io->pfrio_buffer, io->pfrio_size,
		    &io->pfrio_ndel, io->pfrio_flags | PFR_FLAG_USERIOCTL);
		break;
	}

	case DIOCRGETTABLES: {
		struct pfioc_table *io = (struct pfioc_table *)addr;

		if (io->pfrio_esize != sizeof(struct pfr_table)) {
			error = ENODEV;
			break;
		}
		error = pfr_get_tables(&io->pfrio_table, io->pfrio_buffer,
		    &io->pfrio_size, io->pfrio_flags | PFR_FLAG_USERIOCTL);
		break;
	}

	case DIOCRGETTSTATS: {
		struct pfioc_table *io = (struct pfioc_table *)addr;

		if (io->pfrio_esize != sizeof(struct pfr_tstats)) {
			error = ENODEV;
			break;
		}
		error = pfr_get_tstats(&io->pfrio_table, io->pfrio_buffer,
		    &io->pfrio_size, io->pfrio_flags | PFR_FLAG_USERIOCTL);
		break;
	}

	case DIOCRCLRTSTATS: {
		struct pfioc_table *io = (struct pfioc_table *)addr;

		if (io->pfrio_esize != sizeof(struct pfr_table)) {
			error = ENODEV;
			break;
		}
		error = pfr_clr_tstats(io->pfrio_buffer, io->pfrio_size,
		    &io->pfrio_nzero, io->pfrio_flags | PFR_FLAG_USERIOCTL);
		break;
	}

	case DIOCRSETTFLAGS: {
		struct pfioc_table *io = (struct pfioc_table *)addr;

		if (io->pfrio_esize != sizeof(struct pfr_table)) {
			error = ENODEV;
			break;
		}
		error = pfr_set_tflags(io->pfrio_buffer, io->pfrio_size,
		    io->pfrio_setflag, io->pfrio_clrflag, &io->pfrio_nchange,
		    &io->pfrio_ndel, io->pfrio_flags | PFR_FLAG_USERIOCTL);
		break;
	}

	case DIOCRCLRADDRS: {
		struct pfioc_table *io = (struct pfioc_table *)addr;

		if (io->pfrio_esize != 0) {
			error = ENODEV;
			break;
		}
		error = pfr_clr_addrs(&io->pfrio_table, &io->pfrio_ndel,
		    io->pfrio_flags | PFR_FLAG_USERIOCTL);
		break;
	}

	case DIOCRADDADDRS: {
		struct pfioc_table *io = (struct pfioc_table *)addr;

		if (io->pfrio_esize != sizeof(struct pfr_addr)) {
			error = ENODEV;
			break;
		}
		error = pfr_add_addrs(&io->pfrio_table, io->pfrio_buffer,
		    io->pfrio_size, &io->pfrio_nadd, io->pfrio_flags |
		    PFR_FLAG_USERIOCTL);
		break;
	}

	case DIOCRDELADDRS: {
		struct pfioc_table *io = (struct pfioc_table *)addr;

		if (io->pfrio_esize != sizeof(struct pfr_addr)) {
			error = ENODEV;
			break;
		}
		error = pfr_del_addrs(&io->pfrio_table, io->pfrio_buffer,
		    io->pfrio_size, &io->pfrio_ndel, io->pfrio_flags |
		    PFR_FLAG_USERIOCTL);
		break;
	}

	case DIOCRSETADDRS: {
		struct pfioc_table *io = (struct pfioc_table *)addr;

		if (io->pfrio_esize != sizeof(struct pfr_addr)) {
			error = ENODEV;
			break;
		}
		error = pfr_set_addrs(&io->pfrio_table, io->pfrio_buffer,
		    io->pfrio_size, &io->pfrio_size2, &io->pfrio_nadd,
		    &io->pfrio_ndel, &io->pfrio_nchange, io->pfrio_flags |
		    PFR_FLAG_USERIOCTL, 0);
		break;
	}

	case DIOCRGETADDRS: {
		struct pfioc_table *io = (struct pfioc_table *)addr;

		if (io->pfrio_esize != sizeof(struct pfr_addr)) {
			error = ENODEV;
			break;
		}
		error = pfr_get_addrs(&io->pfrio_table, io->pfrio_buffer,
		    &io->pfrio_size, io->pfrio_flags | PFR_FLAG_USERIOCTL);
		break;
	}

	case DIOCRGETASTATS: {
		struct pfioc_table *io = (struct pfioc_table *)addr;

		if (io->pfrio_esize != sizeof(struct pfr_astats)) {
			error = ENODEV;
			break;
		}
		error = pfr_get_astats(&io->pfrio_table, io->pfrio_buffer,
		    &io->pfrio_size, io->pfrio_flags | PFR_FLAG_USERIOCTL);
		break;
	}

	case DIOCRCLRASTATS: {
		struct pfioc_table *io = (struct pfioc_table *)addr;

		if (io->pfrio_esize != sizeof(struct pfr_addr)) {
			error = ENODEV;
			break;
		}
		error = pfr_clr_astats(&io->pfrio_table, io->pfrio_buffer,
		    io->pfrio_size, &io->pfrio_nzero, io->pfrio_flags |
		    PFR_FLAG_USERIOCTL);
		break;
	}

	case DIOCRTSTADDRS: {
		struct pfioc_table *io = (struct pfioc_table *)addr;

		if (io->pfrio_esize != sizeof(struct pfr_addr)) {
			error = ENODEV;
			break;
		}
		error = pfr_tst_addrs(&io->pfrio_table, io->pfrio_buffer,
		    io->pfrio_size, &io->pfrio_nmatch, io->pfrio_flags |
		    PFR_FLAG_USERIOCTL);
		break;
	}

	case DIOCRINADEFINE: {
		struct pfioc_table *io = (struct pfioc_table *)addr;

		if (io->pfrio_esize != sizeof(struct pfr_addr)) {
			error = ENODEV;
			break;
		}
		error = pfr_ina_define(&io->pfrio_table, io->pfrio_buffer,
		    io->pfrio_size, &io->pfrio_nadd, &io->pfrio_naddr,
		    io->pfrio_ticket, io->pfrio_flags | PFR_FLAG_USERIOCTL);
		break;
	}

	case DIOCOSFPADD: {
		struct pf_osfp_ioctl *io = (struct pf_osfp_ioctl *)addr;
		error = pf_osfp_add(io);
		break;
	}

	case DIOCOSFPGET: {
		struct pf_osfp_ioctl *io = (struct pf_osfp_ioctl *)addr;
		error = pf_osfp_get(io);
		break;
	}

	case DIOCXBEGIN: {
		struct pfioc_trans	*io = (struct pfioc_trans *)addr;
		struct pfioc_trans_e	*ioe;
		struct pfr_table	*table;
		int			 i;

		if (io->esize != sizeof(*ioe)) {
			error = ENODEV;
			goto fail;
		}
		ioe = malloc(sizeof(*ioe), M_TEMP, M_WAITOK);
		table = malloc(sizeof(*table), M_TEMP, M_WAITOK);
		pf_default_rule_new = pf_default_rule;
		bzero(&pf_trans_set, sizeof(pf_trans_set));
		for (i = 0; i < io->size; i++) {
			if (copyin(io->array+i, ioe, sizeof(*ioe))) {
				free(table, M_TEMP);
				free(ioe, M_TEMP);
				error = EFAULT;
				goto fail;
			}
			switch (ioe->type) {
#ifdef ALTQ
			case PF_TRANS_ALTQ:
				if (ioe->anchor[0]) {
					free(table, M_TEMP);
					free(ioe, M_TEMP);
					error = EINVAL;
					goto fail;
				}
				if ((error = pf_begin_altq(&ioe->ticket))) {
					free(table, M_TEMP);
					free(ioe, M_TEMP);
					goto fail;
				}
				break;
#endif /* ALTQ */
			case PF_TRANS_TABLE:
				bzero(table, sizeof(*table));
				strlcpy(table->pfrt_anchor, ioe->anchor,
				    sizeof(table->pfrt_anchor));
				if ((error = pfr_ina_begin(table,
				    &ioe->ticket, NULL, 0))) {
					free(table, M_TEMP);
					free(ioe, M_TEMP);
					goto fail;
				}
				break;
			default:
				if ((error = pf_begin_rules(&ioe->ticket,
				    ioe->anchor))) {
					free(table, M_TEMP);
					free(ioe, M_TEMP);
					goto fail;
				}
				break;
			}
			if (copyout(ioe, io->array+i, sizeof(io->array[i]))) {
				free(table, M_TEMP);
				free(ioe, M_TEMP);
				error = EFAULT;
				goto fail;
			}
		}
		free(table, M_TEMP);
		free(ioe, M_TEMP);
		break;
	}

	case DIOCXROLLBACK: {
		struct pfioc_trans	*io = (struct pfioc_trans *)addr;
		struct pfioc_trans_e	*ioe;
		struct pfr_table	*table;
		int			 i;

		if (io->esize != sizeof(*ioe)) {
			error = ENODEV;
			goto fail;
		}
		ioe = malloc(sizeof(*ioe), M_TEMP, M_WAITOK);
		table = malloc(sizeof(*table), M_TEMP, M_WAITOK);
		for (i = 0; i < io->size; i++) {
			if (copyin(io->array+i, ioe, sizeof(*ioe))) {
				free(table, M_TEMP);
				free(ioe, M_TEMP);
				error = EFAULT;
				goto fail;
			}
			switch (ioe->type) {
#ifdef ALTQ
			case PF_TRANS_ALTQ:
				if (ioe->anchor[0]) {
					free(table, M_TEMP);
					free(ioe, M_TEMP);
					error = EINVAL;
					goto fail;
				}
				if ((error = pf_rollback_altq(ioe->ticket))) {
					free(table, M_TEMP);
					free(ioe, M_TEMP);
					goto fail; /* really bad */
				}
				break;
#endif /* ALTQ */
			case PF_TRANS_TABLE:
				bzero(table, sizeof(*table));
				strlcpy(table->pfrt_anchor, ioe->anchor,
				    sizeof(table->pfrt_anchor));
				if ((error = pfr_ina_rollback(table,
				    ioe->ticket, NULL, 0))) {
					free(table, M_TEMP);
					free(ioe, M_TEMP);
					goto fail; /* really bad */
				}
				break;
			default:
				if ((error = pf_rollback_rules(ioe->ticket,
				    ioe->anchor))) {
					free(table, M_TEMP);
					free(ioe, M_TEMP);
					goto fail; /* really bad */
				}
				break;
			}
		}
		free(table, M_TEMP);
		free(ioe, M_TEMP);
		break;
	}

	case DIOCXCOMMIT: {
		struct pfioc_trans	*io = (struct pfioc_trans *)addr;
		struct pfioc_trans_e	*ioe;
		struct pfr_table	*table;
		struct pf_ruleset	*rs;
		int			 i;

		if (io->esize != sizeof(*ioe)) {
			error = ENODEV;
			goto fail;
		}
		ioe = malloc(sizeof(*ioe), M_TEMP, M_WAITOK);
		table = malloc(sizeof(*table), M_TEMP, M_WAITOK);
		/* first makes sure everything will succeed */
		for (i = 0; i < io->size; i++) {
			if (copyin(io->array+i, ioe, sizeof(*ioe))) {
				free(table, M_TEMP);
				free(ioe, M_TEMP);
				error = EFAULT;
				goto fail;
			}
			switch (ioe->type) {
#ifdef ALTQ
			case PF_TRANS_ALTQ:
				if (ioe->anchor[0]) {
					free(table, M_TEMP);
					free(ioe, M_TEMP);
					error = EINVAL;
					goto fail;
				}
				if (!altqs_inactive_open || ioe->ticket !=
				    ticket_altqs_inactive) {
					free(table, M_TEMP);
					free(ioe, M_TEMP);
					error = EBUSY;
					goto fail;
				}
				break;
#endif /* ALTQ */
			case PF_TRANS_TABLE:
				rs = pf_find_ruleset(ioe->anchor);
				if (rs == NULL || !rs->topen || ioe->ticket !=
				     rs->tticket) {
					free(table, M_TEMP);
					free(ioe, M_TEMP);
					error = EBUSY;
					goto fail;
				}
				break;
			default:
				rs = pf_find_ruleset(ioe->anchor);
				if (rs == NULL ||
				    !rs->rules.inactive.open ||
				    rs->rules.inactive.ticket !=
				    ioe->ticket) {
					free(table, M_TEMP);
					free(ioe, M_TEMP);
					error = EBUSY;
					goto fail;
				}
				break;
			}
		}
		/*
		 * Checked already in DIOCSETLIMIT, but check again as the
		 * situation might have changed.
		 */
		for (i = 0; i < PF_LIMIT_MAX; i++) {
			if (((struct pool *)pf_pool_limits[i].pp)->pr_nout >
			    pf_pool_limits[i].limit_new) {
				free(table, M_TEMP);
				free(ioe, M_TEMP);
				error = EBUSY;
				goto fail;
			}
		}
		/* now do the commit - no errors should happen here */
		for (i = 0; i < io->size; i++) {
			if (copyin(io->array+i, ioe, sizeof(*ioe))) {
				free(table, M_TEMP);
				free(ioe, M_TEMP);
				error = EFAULT;
				goto fail;
			}
			switch (ioe->type) {
#ifdef ALTQ
			case PF_TRANS_ALTQ:
				if ((error = pf_commit_altq(ioe->ticket))) {
					free(table, M_TEMP);
					free(ioe, M_TEMP);
					goto fail; /* really bad */
				}
				break;
#endif /* ALTQ */
			case PF_TRANS_TABLE:
				bzero(table, sizeof(*table));
				strlcpy(table->pfrt_anchor, ioe->anchor,
				    sizeof(table->pfrt_anchor));
				if ((error = pfr_ina_commit(table, ioe->ticket,
				    NULL, NULL, 0))) {
					free(table, M_TEMP);
					free(ioe, M_TEMP);
					goto fail; /* really bad */
				}
				break;
			default:
				if ((error = pf_commit_rules(ioe->ticket,
				    ioe->anchor))) {
					free(table, M_TEMP);
					free(ioe, M_TEMP);
					goto fail; /* really bad */
				}
				break;
			}
		}
		for (i = 0; i < PF_LIMIT_MAX; i++) {
			if (pf_pool_limits[i].limit_new !=
			    pf_pool_limits[i].limit &&
			    pool_sethardlimit(pf_pool_limits[i].pp,
			    pf_pool_limits[i].limit_new, NULL, 0) != 0) {
				free(table, M_TEMP);
				free(ioe, M_TEMP);
				error = EBUSY;
				goto fail; /* really bad */
			}
			pf_pool_limits[i].limit = pf_pool_limits[i].limit_new;
		}
		for (i = 0; i < PFTM_MAX; i++) {
			int old = pf_default_rule.timeout[i];

			pf_default_rule.timeout[i] =
			    pf_default_rule_new.timeout[i];
			if (pf_default_rule.timeout[i] == PFTM_INTERVAL &&
			    pf_default_rule.timeout[i] < old)
				wakeup(pf_purge_thread);
		}
		pfi_xcommit();
		pf_trans_set_commit();
		free(table, M_TEMP);
		free(ioe, M_TEMP);
		break;
	}

	case DIOCGETSRCNODES: {
		struct pfioc_src_nodes	*psn = (struct pfioc_src_nodes *)addr;
		struct pf_src_node	*n, *p, *pstore;
		u_int32_t		 nr = 0;
		int			 space = psn->psn_len;

		if (space == 0) {
			RB_FOREACH(n, pf_src_tree, &tree_src_tracking)
				nr++;
			psn->psn_len = sizeof(struct pf_src_node) * nr;
			break;
		}

		pstore = malloc(sizeof(*pstore), M_TEMP, M_WAITOK);

		p = psn->psn_src_nodes;
		RB_FOREACH(n, pf_src_tree, &tree_src_tracking) {
			int	secs = time_second, diff;

			if ((nr + 1) * sizeof(*p) > (unsigned)psn->psn_len)
				break;

			bcopy(n, pstore, sizeof(*pstore));
			if (n->rule.ptr != NULL)
				pstore->rule.nr = n->rule.ptr->nr;
			pstore->creation = secs - pstore->creation;
			if (pstore->expire > secs)
				pstore->expire -= secs;
			else
				pstore->expire = 0;

			/* adjust the connection rate estimate */
			diff = secs - n->conn_rate.last;
			if (diff >= n->conn_rate.seconds)
				pstore->conn_rate.count = 0;
			else
				pstore->conn_rate.count -=
				    n->conn_rate.count * diff /
				    n->conn_rate.seconds;

			error = copyout(pstore, p, sizeof(*p));
			if (error) {
				free(pstore, M_TEMP);
				goto fail;
			}
			p++;
			nr++;
		}
		psn->psn_len = sizeof(struct pf_src_node) * nr;

		free(pstore, M_TEMP);
		break;
	}

	case DIOCCLRSRCNODES: {
		struct pf_src_node	*n;
		struct pf_state		*state;

		RB_FOREACH(state, pf_state_tree_id, &tree_id)
			pf_src_tree_remove_state(state);
		RB_FOREACH(n, pf_src_tree, &tree_src_tracking)
			n->expire = 1;
		pf_purge_expired_src_nodes(1);
		break;
	}

	case DIOCKILLSRCNODES: {
		struct pf_src_node	*sn;
		struct pf_state		*s;
		struct pfioc_src_node_kill *psnk =
		    (struct pfioc_src_node_kill *)addr;
		u_int			killed = 0;

		RB_FOREACH(sn, pf_src_tree, &tree_src_tracking) {
			if (PF_MATCHA(psnk->psnk_src.neg,
				&psnk->psnk_src.addr.v.a.addr,
				&psnk->psnk_src.addr.v.a.mask,
				&sn->addr, sn->af) &&
			    PF_MATCHA(psnk->psnk_dst.neg,
				&psnk->psnk_dst.addr.v.a.addr,
				&psnk->psnk_dst.addr.v.a.mask,
				&sn->raddr, sn->af)) {
				/* Handle state to src_node linkage */
				if (sn->states != 0)
					RB_FOREACH(s, pf_state_tree_id,
					   &tree_id)
						pf_state_rm_src_node(s, sn);
				sn->expire = 1;
				killed++;
			}
		}

		if (killed > 0)
			pf_purge_expired_src_nodes(1);

		psnk->psnk_killed = killed;
		break;
	}

	case DIOCSETHOSTID: {
		u_int32_t	*hostid = (u_int32_t *)addr;

		if (*hostid == 0)
			pf_trans_set.hostid = arc4random();
		else
			pf_trans_set.hostid = *hostid;
		pf_trans_set.mask |= PF_TSET_HOSTID;
		break;
	}

	case DIOCOSFPFLUSH:
		pf_osfp_flush();
		break;

	case DIOCIGETIFACES: {
		struct pfioc_iface *io = (struct pfioc_iface *)addr;

		if (io->pfiio_esize != sizeof(struct pfi_kif)) {
			error = ENODEV;
			break;
		}
		error = pfi_get_ifaces(io->pfiio_name, io->pfiio_buffer,
		    &io->pfiio_size);
		break;
	}

	case DIOCSETIFFLAG: {
		struct pfioc_iface *io = (struct pfioc_iface *)addr;

		error = pfi_set_flags(io->pfiio_name, io->pfiio_flags);
		break;
	}

	case DIOCCLRIFFLAG: {
		struct pfioc_iface *io = (struct pfioc_iface *)addr;

		error = pfi_clear_flags(io->pfiio_name, io->pfiio_flags);
		break;
	}

	case DIOCSETREASS: {
		u_int32_t	*reass = (u_int32_t *)addr;

		pf_trans_set.reass = *reass;
		pf_trans_set.mask |= PF_TSET_REASS;
		break;
	}

	default:
		error = ENODEV;
		break;
	}
fail:
	splx(s);
	if (flags & FWRITE)
		rw_exit_write(&pf_consistency_lock);
	else
		rw_exit_read(&pf_consistency_lock);
	return (error);
}

void
pf_trans_set_commit(void)
{
	if (pf_trans_set.mask & PF_TSET_STATUSIF)
		strlcpy(pf_status.ifname, pf_trans_set.statusif, IFNAMSIZ);
	if (pf_trans_set.mask & PF_TSET_DEBUG)
		pf_status.debug = pf_trans_set.debug;
	if (pf_trans_set.mask & PF_TSET_HOSTID)
		pf_status.hostid = pf_trans_set.hostid;
	if (pf_trans_set.mask & PF_TSET_REASS)
		pf_status.reass = pf_trans_set.reass;
}
