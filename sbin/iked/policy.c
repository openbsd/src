/*	$OpenBSD: policy.c,v 1.18 2011/04/18 09:54:41 reyk Exp $	*/
/*	$vantronix: policy.c,v 1.29 2010/05/28 15:34:35 reyk Exp $	*/

/*
 * Copyright (c) 2010, 2011 Reyk Floeter <reyk@vantronix.net>
 * Copyright (c) 2001 Daniel Hartmeier
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

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/tree.h>

#include <net/if.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <event.h>

#include "iked.h"
#include "ikev2.h"

static __inline int
	 sa_cmp(struct iked_sa *, struct iked_sa *);
static __inline int
	 user_cmp(struct iked_user *, struct iked_user *);
static __inline int
	 childsa_cmp(struct iked_childsa *, struct iked_childsa *);
static __inline int
	 flow_cmp(struct iked_flow *, struct iked_flow *);


void
policy_init(struct iked *env)
{
	TAILQ_INIT(&env->sc_policies);
	RB_INIT(&env->sc_users);
	RB_INIT(&env->sc_sas);
	RB_INIT(&env->sc_activesas);
	RB_INIT(&env->sc_activeflows);
}

int
policy_lookup(struct iked *env, struct iked_message *msg)
{
	struct iked_policy	 pol;

	if (msg->msg_sa != NULL && msg->msg_sa->sa_policy != NULL) {
		/* Existing SA with policy */
		msg->msg_policy = msg->msg_sa->sa_policy;
		goto found;
	}

	bzero(&pol, sizeof(pol));
	pol.pol_af = msg->msg_peer.ss_family;
	memcpy(&pol.pol_peer.addr, &msg->msg_peer, sizeof(msg->msg_peer));
	memcpy(&pol.pol_local.addr, &msg->msg_local, sizeof(msg->msg_local));

	/* Try to find a matching policy for this message */
	if ((msg->msg_policy = policy_test(env, &pol)) != NULL)
		goto found;

	/* No matching policy found, try the default */
	if ((msg->msg_policy = env->sc_defaultcon) != NULL)
		goto found;

	/* No policy found */
	return (-1);

 found:
	return (0);
}

struct iked_policy *
policy_test(struct iked *env, struct iked_policy *key)
{
	struct iked_policy	*p = NULL, *pol = NULL;
	struct iked_flow	*flow = NULL, *flowkey;
	u_int			 cnt = 0;

	p = TAILQ_FIRST(&env->sc_policies);
	while (p != NULL) {
		cnt++;
		if (p->pol_flags & IKED_POLICY_SKIP)
			p = p->pol_skip[IKED_SKIP_FLAGS];
		else if (key->pol_af && p->pol_af &&
		    key->pol_af != p->pol_af)
			p = p->pol_skip[IKED_SKIP_AF];
		else if (key->pol_ipproto && p->pol_ipproto &&
		    key->pol_ipproto != p->pol_ipproto)
			p = p->pol_skip[IKED_SKIP_PROTO];
		else if (sockaddr_cmp((struct sockaddr *)&key->pol_peer.addr,
		    (struct sockaddr *)&p->pol_peer.addr,
		    p->pol_peer.addr_mask) != 0)
			p = p->pol_skip[IKED_SKIP_DST_ADDR];
		else if (sockaddr_cmp((struct sockaddr *)&key->pol_local.addr,
		    (struct sockaddr *)&p->pol_local.addr,
		    p->pol_local.addr_mask) != 0)
			p = p->pol_skip[IKED_SKIP_SRC_ADDR];
		else {
			/*
			 * Check if a specific flow is requested
			 * (eg. for acquire messages from the kernel)
			 * and find a matching flow.
			 */
			if (key->pol_nflows &&
			    (flowkey = RB_MIN(iked_flows,
			    &key->pol_flows)) != NULL &&
			    (flow = RB_FIND(iked_flows, &p->pol_flows,
			    flowkey)) == NULL) {
				p = TAILQ_NEXT(p, pol_entry);
				continue;
			}

			/* Policy matched */
			pol = p;

			if (pol->pol_flags & IKED_POLICY_QUICK)
				break;

			/* Continue to find last matching policy */
			p = TAILQ_NEXT(p, pol_entry);
		}
	}

	return (pol);
}

#define	IKED_SET_SKIP_STEPS(i)						\
	do {								\
		while (head[i] != cur) {				\
			head[i]->pol_skip[i] = cur;			\
			head[i] = TAILQ_NEXT(head[i], pol_entry);	\
		}							\
	} while (0)

/* This code is derived from pf_calc_skip_steps() from pf.c */
void
policy_calc_skip_steps(struct iked_policies *policies)
{
	struct iked_policy	*head[IKED_SKIP_COUNT], *cur, *prev;
	int			 i;

	cur = TAILQ_FIRST(policies);
	prev = cur;
	for (i = 0; i < IKED_SKIP_COUNT; ++i)
		head[i] = cur;
	while (cur != NULL) {
		if (cur->pol_flags & IKED_POLICY_SKIP)
			IKED_SET_SKIP_STEPS(IKED_SKIP_FLAGS);
		else if (cur->pol_af != AF_UNSPEC &&
		    prev->pol_af != AF_UNSPEC &&
		    cur->pol_af != prev->pol_af)
			IKED_SET_SKIP_STEPS(IKED_SKIP_AF);
		else if (cur->pol_ipproto && prev->pol_ipproto &&
		    cur->pol_ipproto != prev->pol_ipproto)
			IKED_SET_SKIP_STEPS(IKED_SKIP_PROTO);
		else if (IKED_ADDR_NEQ(&cur->pol_peer, &prev->pol_peer))
			IKED_SET_SKIP_STEPS(IKED_SKIP_DST_ADDR);
		else if (IKED_ADDR_NEQ(&cur->pol_local, &prev->pol_local))
			IKED_SET_SKIP_STEPS(IKED_SKIP_SRC_ADDR);

		prev = cur;
		cur = TAILQ_NEXT(cur, pol_entry);
	}
	for (i = 0; i < IKED_SKIP_COUNT; ++i)
		IKED_SET_SKIP_STEPS(i);
}

void
policy_ref(struct iked *env, struct iked_policy *pol)
{
	pol->pol_refcnt++;
	pol->pol_flags |= IKED_POLICY_REFCNT;
}

void
policy_unref(struct iked *env, struct iked_policy *pol)
{
	if (pol == NULL || (pol->pol_flags & IKED_POLICY_REFCNT) == 0)
		return;
	if (--(pol->pol_refcnt) <= 0)
		config_free_policy(env, pol);
}

void
sa_state(struct iked *env, struct iked_sa *sa, int state)
{
	const char		*a;
	const char		*b;

	a = print_map(sa->sa_state, ikev2_state_map);
	b = print_map(state, ikev2_state_map);

	if (state > sa->sa_state) {
		switch (state) {
		case IKEV2_STATE_ESTABLISHED:
		case IKEV2_STATE_CLOSED:
			log_info("%s: %s -> %s from %s to %s policy '%s'",
			    __func__, a, b,
			    print_host(&sa->sa_peer.addr, NULL, 0),
			    print_host(&sa->sa_local.addr, NULL, 0),
			    sa->sa_policy->pol_name);
			break;
		default:
			log_debug("%s: %s -> %s", __func__, a, b);
			break;
		}
	}

	sa->sa_state = state;
}

void
sa_stateflags(struct iked_sa *sa, u_int flags)
{
	u_int	require;

	if (sa->sa_state > IKEV2_STATE_SA_INIT)
		require = sa->sa_statevalid;
	else
		require = sa->sa_stateinit;

	log_debug("%s: 0x%02x -> 0x%02x %s (required 0x%02x %s)", __func__,
	    sa->sa_stateflags, sa->sa_stateflags | flags,
	    print_bits(sa->sa_stateflags | flags, IKED_REQ_BITS), require,
	    print_bits(require, IKED_REQ_BITS));

	sa->sa_stateflags |= flags;
}

int
sa_stateok(struct iked_sa *sa, int state)
{
	u_int	 require;

	if (sa->sa_state < state)
		return (0);

	if (state == IKEV2_STATE_SA_INIT)
		require = sa->sa_stateinit;
	else
		require = sa->sa_statevalid;

	if (state == IKEV2_STATE_SA_INIT ||
	    state == IKEV2_STATE_VALID ||
	    state == IKEV2_STATE_EAP) {
		log_debug("%s: %s flags 0x%02x, require 0x%02x %s", __func__,
		    print_map(state, ikev2_state_map),
		    (sa->sa_stateflags & require), require,
		    print_bits(require, IKED_REQ_BITS));

		if ((sa->sa_stateflags & require) != require)
			return (0);	/* not ready, ignore */
	}
	return (1);
}

struct iked_sa *
sa_new(struct iked *env, u_int64_t ispi, u_int64_t rspi,
    u_int initiator, struct iked_policy *pol)
{
	struct iked_sa	*sa;
	struct iked_id	*localid;
	u_int		 diff;

	if ((ispi == 0 && rspi == 0) ||
	    (sa = sa_lookup(env, ispi, rspi, initiator)) == NULL) {
		/* Create new SA */
		sa = config_new_sa(env, initiator);
	}
	if (sa == NULL) {
		log_debug("%s: failed to get sa", __func__);
		return (NULL);
	}
	if (sa->sa_policy == NULL)
		sa->sa_policy = pol;
	else
		pol = sa->sa_policy;

	sa->sa_statevalid = IKED_REQ_AUTH|IKED_REQ_SA;
	if (pol != NULL && pol->pol_auth.auth_eap) {
		sa->sa_statevalid |= IKED_REQ_CERT;
	} else if (pol != NULL && pol->pol_auth.auth_method !=
	    IKEV2_AUTH_SHARED_KEY_MIC) {
		sa->sa_statevalid |= IKED_REQ_VALID|IKED_REQ_CERT;
	}

	if (initiator) {
		localid = &sa->sa_iid;
		diff = IKED_REQ_VALID|IKED_REQ_SA;
		sa->sa_stateinit = sa->sa_statevalid & ~diff;
		sa->sa_statevalid = sa->sa_statevalid & diff;
	} else
		localid = &sa->sa_rid;

	if (!ibuf_length(localid->id_buf) &&
	    ikev2_policy2id(&pol->pol_localid, localid, 1) != 0) {
		log_debug("%s: failed to get local id", __func__);
		sa_free(env, sa);
		return (NULL);
	}

	if (sa->sa_hdr.sh_ispi == 0)
		sa->sa_hdr.sh_ispi = ispi;
	if (sa->sa_hdr.sh_rspi == 0)
		sa->sa_hdr.sh_rspi = rspi;

	/* Re-insert node into the tree */
	RB_INSERT(iked_sas, &env->sc_sas, sa);

	return (sa);
}

void
sa_free(struct iked *env, struct iked_sa *sa)
{
	log_debug("%s: ispi %s rspi %s", __func__,
	    print_spi(sa->sa_hdr.sh_ispi, 8),
	    print_spi(sa->sa_hdr.sh_rspi, 8));

	config_free_sa(env, sa);
}

void
sa_free_flows(struct iked *env, struct iked_saflows *head)
{
	struct iked_flow	*flow, *next;

	for (flow = TAILQ_FIRST(head); flow != NULL; flow = next) {
		next = TAILQ_NEXT(flow, flow_entry);

		log_debug("%s: free %p", __func__, flow);

		if (flow->flow_loaded)
			RB_REMOVE(iked_flows, &env->sc_activeflows, flow);
		TAILQ_REMOVE(head, flow, flow_entry);
		(void)pfkey_flow_delete(env->sc_pfkey, flow);
		flow_free(flow);
	}
}


int
sa_address(struct iked_sa *sa, struct iked_addr *addr,
    struct sockaddr_storage *peer)
{
	struct iked_policy	*pol = sa->sa_policy;

	if (pol == NULL) {
		log_debug("%s: invalid policy", __func__);
		return (-1);
	}

	bzero(addr, sizeof(*addr));
	addr->addr_af = peer->ss_family;
	addr->addr_port = htons(socket_getport(peer));
	memcpy(&addr->addr, peer, sizeof(*peer));
	if (socket_af((struct sockaddr *)&addr->addr, addr->addr_port) == -1) {
		log_debug("%s: invalid address", __func__);
		return (-1);
	}

	if (addr == &sa->sa_peer) {
		/* XXX Re-insert node into the tree */
		(void)RB_REMOVE(iked_sapeers, &pol->pol_sapeers, sa);
		RB_INSERT(iked_sapeers, &pol->pol_sapeers, sa);
	}

	return (0);
}

void
childsa_free(struct iked_childsa *sa)
{
	ibuf_release(sa->csa_encrkey);
	ibuf_release(sa->csa_integrkey);
	free(sa);
}

struct iked_childsa *
childsa_lookup(struct iked_sa *sa, u_int64_t spi, u_int8_t protoid)
{
	struct iked_childsa	*csa;

	if (sa == NULL || spi == 0 || protoid == 0)
		return (NULL);

	TAILQ_FOREACH(csa, &sa->sa_childsas, csa_entry) {
		if (csa->csa_spi.spi_protoid == protoid &&
		    (csa->csa_spi.spi == spi))
			break;
	}
	return (csa);
}

void
flow_free(struct iked_flow *flow)
{
	free(flow);
}

struct iked_sa *
sa_lookup(struct iked *env, u_int64_t ispi, u_int64_t rspi,
    u_int initiator)
{
	struct iked_sa	*sa, key;

	key.sa_hdr.sh_ispi = ispi;
	key.sa_hdr.sh_rspi = rspi;
	key.sa_hdr.sh_initiator = initiator;

	if ((sa = RB_FIND(iked_sas, &env->sc_sas, &key)) != NULL) {
		gettimeofday(&sa->sa_timeused, NULL);

		/* Validate if SPIr matches */
		if ((sa->sa_hdr.sh_rspi != 0) &&
		    (sa->sa_hdr.sh_rspi != rspi))
			return (NULL);
	}

	return (sa);
}

static __inline int
sa_cmp(struct iked_sa *a, struct iked_sa *b)
{
	if (a->sa_hdr.sh_initiator != b->sa_hdr.sh_initiator)
		return (-2);

	if (a->sa_hdr.sh_ispi > b->sa_hdr.sh_ispi)
		return (-1);
	if (a->sa_hdr.sh_ispi < b->sa_hdr.sh_ispi)
		return (1);

#if 0
	/* Responder SPI is not yet set in the local IKE SADB */
	if ((b->sa_type == IKED_SATYPE_LOCAL && b->sa_hdr.sh_rspi == 0) ||
	    (a->sa_type == IKED_SATYPE_LOCAL && a->sa_hdr.sh_rspi == 0))
		return (0);

	if (a->sa_hdr.sh_rspi > b->sa_hdr.sh_rspi)
		return (-1);
	if (a->sa_hdr.sh_rspi < b->sa_hdr.sh_rspi)
		return (1);
#endif

	return (0);
}

RB_GENERATE(iked_sas, iked_sa, sa_entry, sa_cmp);

struct iked_sa *
sa_peer_lookup(struct iked_policy *pol, struct sockaddr_storage *peer)
{
	struct iked_sa	 key;

	memcpy(&key.sa_peer.addr, peer, sizeof(*peer));
	return (RB_FIND(iked_sapeers, &pol->pol_sapeers, &key));
}

static __inline int
sa_peer_cmp(struct iked_sa *a, struct iked_sa *b)
{
	return (sockaddr_cmp((struct sockaddr *)&a->sa_peer.addr,
	    (struct sockaddr *)&b->sa_peer.addr, -1));
}

RB_GENERATE(iked_sapeers, iked_sa, sa_peer_entry, sa_peer_cmp);

struct iked_user *
user_lookup(struct iked *env, const char *user)
{
	struct iked_user	 key;

	if (strlcpy(key.usr_name, user,
	    sizeof(key.usr_name)) >= sizeof(key.usr_name))
		return (NULL);

	return (RB_FIND(iked_users, &env->sc_users, &key));
}

static __inline int
user_cmp(struct iked_user *a, struct iked_user *b)
{
	return (strcmp(a->usr_name, b->usr_name));
}

RB_GENERATE(iked_users, iked_user, usr_entry, user_cmp);

static __inline int
childsa_cmp(struct iked_childsa *a, struct iked_childsa *b)
{
	if (a->csa_spi.spi > b->csa_spi.spi)
		return (1);
	if (a->csa_spi.spi < b->csa_spi.spi)
		return (-1);
	return (0);
}

RB_GENERATE(iked_activesas, iked_childsa, csa_node, childsa_cmp);

static __inline int
addr_cmp(struct iked_addr *a, struct iked_addr *b, int useports)
{
	int		diff = 0;

	diff = sockaddr_cmp((struct sockaddr *)&a->addr,
	    (struct sockaddr *)&b->addr, 128);
	if (!diff)
		diff = (int)a->addr_mask - (int)b->addr_mask;
	if (!diff && useports)
		diff = a->addr_port - b->addr_port;

	return (diff);
}

static __inline int
flow_cmp(struct iked_flow *a, struct iked_flow *b)
{
	int		diff = 0;

	if (a->flow_peer && b->flow_peer)
		diff = addr_cmp(a->flow_peer, b->flow_peer, 0);
	if (!diff)
		diff = addr_cmp(&a->flow_dst, &b->flow_dst, 1);
	if (!diff)
		diff = addr_cmp(&a->flow_src, &b->flow_src, 1);
	if (!diff && a->flow_dir && b->flow_dir)
		diff = (int)a->flow_dir - (int)b->flow_dir;

	return (diff);
}

RB_GENERATE(iked_flows, iked_flow, flow_node, flow_cmp);
