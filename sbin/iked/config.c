/*	$OpenBSD: config.c,v 1.35 2015/02/06 10:39:01 deraadt Exp $	*/

/*
 * Copyright (c) 2010-2013 Reyk Floeter <reyk@openbsd.org>
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

#include <sys/queue.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/uio.h>

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <err.h>
#include <pwd.h>
#include <event.h>

#include "iked.h"
#include "ikev2.h"

struct iked_sa *
config_new_sa(struct iked *env, int initiator)
{
	struct iked_sa	*sa;

	if ((sa = calloc(1, sizeof(*sa))) == NULL)
		return (NULL);

	TAILQ_INIT(&sa->sa_proposals);
	TAILQ_INIT(&sa->sa_childsas);
	TAILQ_INIT(&sa->sa_flows);
	TAILQ_INIT(&sa->sa_requests);
	TAILQ_INIT(&sa->sa_responses);
	sa->sa_hdr.sh_initiator = initiator;
	sa->sa_type = IKED_SATYPE_LOCAL;

	if (initiator)
		sa->sa_hdr.sh_ispi = config_getspi();
	else
		sa->sa_hdr.sh_rspi = config_getspi();

	gettimeofday(&sa->sa_timecreated, NULL);
	memcpy(&sa->sa_timeused, &sa->sa_timecreated, sizeof(sa->sa_timeused));

	return (sa);
}

u_int64_t
config_getspi(void)
{
	u_int64_t	 spi;

	do {
		arc4random_buf(&spi, sizeof spi);
	} while (spi == 0);

	return (spi);
}

void
config_free_kex(struct iked_kex *kex)
{
	if (kex == NULL)
		return;

	ibuf_release(kex->kex_inonce);
	ibuf_release(kex->kex_rnonce);

	if (kex->kex_dhgroup != NULL)
		group_free(kex->kex_dhgroup);
	ibuf_release(kex->kex_dhiexchange);
	ibuf_release(kex->kex_dhrexchange);

	free(kex);
}

void
config_free_sa(struct iked *env, struct iked_sa *sa)
{
	timer_del(env, &sa->sa_timer);
	timer_del(env, &sa->sa_rekey);

	config_free_proposals(&sa->sa_proposals, 0);
	config_free_childsas(env, &sa->sa_childsas, NULL, NULL);
	sa_free_flows(env, &sa->sa_flows);

	if (sa->sa_addrpool) {
		(void)RB_REMOVE(iked_addrpool, &env->sc_addrpool, sa);
		free(sa->sa_addrpool);
	}

	if (sa->sa_policy) {
		(void)RB_REMOVE(iked_sapeers, &sa->sa_policy->pol_sapeers, sa);
		policy_unref(env, sa->sa_policy);
	}

	ikev2_msg_flushqueue(env, &sa->sa_requests);
	ikev2_msg_flushqueue(env, &sa->sa_responses);

	ibuf_release(sa->sa_inonce);
	ibuf_release(sa->sa_rnonce);

	if (sa->sa_dhgroup != NULL)
		group_free(sa->sa_dhgroup);
	ibuf_release(sa->sa_dhiexchange);
	ibuf_release(sa->sa_dhrexchange);

	hash_free(sa->sa_prf);
	hash_free(sa->sa_integr);
	cipher_free(sa->sa_encr);

	ibuf_release(sa->sa_key_d);
	ibuf_release(sa->sa_key_iauth);
	ibuf_release(sa->sa_key_rauth);
	ibuf_release(sa->sa_key_iencr);
	ibuf_release(sa->sa_key_rencr);
	ibuf_release(sa->sa_key_iprf);
	ibuf_release(sa->sa_key_rprf);

	ibuf_release(sa->sa_1stmsg);
	ibuf_release(sa->sa_2ndmsg);

	ibuf_release(sa->sa_iid.id_buf);
	ibuf_release(sa->sa_rid.id_buf);
	ibuf_release(sa->sa_icert.id_buf);
	ibuf_release(sa->sa_rcert.id_buf);

	ibuf_release(sa->sa_eap.id_buf);
	if (sa->sa_eapid != NULL)
		free(sa->sa_eapid);
	ibuf_release(sa->sa_eapmsk);

	free(sa);
}

struct iked_policy *
config_new_policy(struct iked *env)
{
	struct iked_policy	*pol;

	if ((pol = calloc(1, sizeof(*pol))) == NULL)
		return (NULL);

	TAILQ_INIT(&pol->pol_proposals);
	RB_INIT(&pol->pol_sapeers);

	return (pol);
}

void
config_free_policy(struct iked *env, struct iked_policy *pol)
{
	struct iked_sa		*sa;

	if (pol->pol_flags & IKED_POLICY_REFCNT)
		goto remove;

	TAILQ_REMOVE(&env->sc_policies, pol, pol_entry);

	RB_FOREACH(sa, iked_sapeers, &pol->pol_sapeers) {
		/* Remove from the policy tree, but keep for existing SAs */
		if (sa->sa_policy == pol)
			policy_ref(env, pol);
	}

	if (pol->pol_refcnt)
		return;

 remove:
	config_free_proposals(&pol->pol_proposals, 0);
	config_free_flows(env, &pol->pol_flows);
	free(pol);
}

struct iked_proposal *
config_add_proposal(struct iked_proposals *head, u_int id, u_int proto)
{
	struct iked_proposal	*pp;

	TAILQ_FOREACH(pp, head, prop_entry) {
		if (pp->prop_protoid == proto &&
		    pp->prop_id == id)
			return (pp);
	}

	if ((pp = calloc(1, sizeof(*pp))) == NULL)
		return (NULL);

	pp->prop_protoid = proto;
	pp->prop_id = id;

	TAILQ_INSERT_TAIL(head, pp, prop_entry);

	return (pp);
}

void
config_free_proposals(struct iked_proposals *head, u_int proto)
{
	struct iked_proposal	*prop, *next;

	for (prop = TAILQ_FIRST(head); prop != NULL; prop = next) {
		next = TAILQ_NEXT(prop, prop_entry);

		/* Free any proposal or only selected SA proto */
		if (proto != 0 && prop->prop_protoid != proto)
			continue;

		log_debug("%s: free %p", __func__, prop);

		TAILQ_REMOVE(head, prop, prop_entry);
		if (prop->prop_nxforms)
			free(prop->prop_xforms);
		free(prop);
	}
}

void
config_free_flows(struct iked *env, struct iked_flows *head)
{
	struct iked_flow	*flow, *next;

	for (flow = RB_MIN(iked_flows, head); flow != NULL; flow = next) {
		next = RB_NEXT(iked_flows, head, flow);
		log_debug("%s: free %p", __func__, flow);
		RB_REMOVE(iked_flows, head, flow);
		flow_free(flow);
	}
}

void
config_free_childsas(struct iked *env, struct iked_childsas *head,
    struct iked_spi *peerspi, struct iked_spi *localspi)
{
	struct iked_childsa	*csa, *nextcsa;

	if (localspi != NULL)
		bzero(localspi, sizeof(*localspi));

	for (csa = TAILQ_FIRST(head); csa != NULL; csa = nextcsa) {
		nextcsa = TAILQ_NEXT(csa, csa_entry);

		if (peerspi != NULL) {
			/* Only delete matching peer SPIs */
			if (peerspi->spi != csa->csa_peerspi)
				continue;

			/* Store assigned local SPI */
			if (localspi != NULL && localspi->spi == 0)
				memcpy(localspi, &csa->csa_spi,
				    sizeof(*localspi));
		}
		log_debug("%s: free %p", __func__, csa);

		TAILQ_REMOVE(head, csa, csa_entry);
		if (csa->csa_loaded) {
			RB_REMOVE(iked_activesas, &env->sc_activesas, csa);
			(void)pfkey_sa_delete(env->sc_pfkey, csa);
		}
		childsa_free(csa);
	}
}

struct iked_transform *
config_add_transform(struct iked_proposal *prop, u_int type,
    u_int id, u_int length, u_int keylength)
{
	struct iked_transform	*xform;
	struct iked_constmap	*map = NULL;
	int			 score = 1;
	u_int			 i;

	switch (type) {
	case IKEV2_XFORMTYPE_ENCR:
		map = ikev2_xformencr_map;
		break;
	case IKEV2_XFORMTYPE_PRF:
		map = ikev2_xformprf_map;
		break;
	case IKEV2_XFORMTYPE_INTEGR:
		map = ikev2_xformauth_map;
		break;
	case IKEV2_XFORMTYPE_DH:
		map = ikev2_xformdh_map;
		break;
	case IKEV2_XFORMTYPE_ESN:
		map = ikev2_xformesn_map;
		break;
	default:
		log_debug("%s: invalid transform type %d", __func__, type);
		return (NULL);
	}

	for (i = 0; i < prop->prop_nxforms; i++) {
		xform = prop->prop_xforms + i;
		if (xform->xform_type == type &&
		    xform->xform_id == id &&
		    xform->xform_length == length)
			return (xform);
	}

	for (i = 0; i < prop->prop_nxforms; i++) {
		xform = prop->prop_xforms + i;
		if (xform->xform_type == type) {
			switch (type) {
			case IKEV2_XFORMTYPE_ENCR:
			case IKEV2_XFORMTYPE_INTEGR:
				score += 3;
				break;
			case IKEV2_XFORMTYPE_DH:
				score += 2;
				break;
			default:
				score += 1;
				break;
			}
		}
	}

	if ((xform = reallocarray(prop->prop_xforms,
	    prop->prop_nxforms + 1, sizeof(*xform))) == NULL) {
		return (NULL);
	}

	prop->prop_xforms = xform;
	xform = prop->prop_xforms + prop->prop_nxforms++;
	bzero(xform, sizeof(*xform));

	xform->xform_type = type;
	xform->xform_id = id;
	xform->xform_length = length;
	xform->xform_keylength = keylength;
	xform->xform_score = score;
	xform->xform_map = map;

	return (xform);
}

struct iked_transform *
config_findtransform(struct iked_proposals *props, u_int8_t type,
    u_int proto)
{
	struct iked_proposal	*prop;
	struct iked_transform	*xform;
	u_int			 i;

	/* Search of the first transform with the desired type */
	TAILQ_FOREACH(prop, props, prop_entry) {
		/* Find any proposal or only selected SA proto */
		if (proto != 0 && prop->prop_protoid != proto)
			continue;
		for (i = 0; i < prop->prop_nxforms; i++) {
			xform = prop->prop_xforms + i;
			if (xform->xform_type == type)
				return (xform);
		}
	}

	return (NULL);
}

struct iked_user *
config_new_user(struct iked *env, struct iked_user *new)
{
	struct iked_user	*usr, *old;

	if ((usr = calloc(1, sizeof(*usr))) == NULL)
		return (NULL);

	memcpy(usr, new, sizeof(*usr));

	if ((old = RB_INSERT(iked_users, &env->sc_users, usr)) != NULL) {
		/* Update the password of an existing user*/
		memcpy(old, new, sizeof(*old));

		log_debug("%s: updating user %s", __func__, usr->usr_name);
		free(usr);

		return (old);
	}

	log_debug("%s: inserting new user %s", __func__, usr->usr_name);
	return (usr);
}

/*
 * Inter-process communication of configuration items.
 */

int
config_setcoupled(struct iked *env, u_int couple)
{
	u_int	 type;

	type = couple ? IMSG_CTL_COUPLE : IMSG_CTL_DECOUPLE;
	proc_compose_imsg(&env->sc_ps, PROC_IKEV1, -1, type, -1, NULL, 0);
	proc_compose_imsg(&env->sc_ps, PROC_IKEV2, -1, type, -1, NULL, 0);

	return (0);
}

int
config_getcoupled(struct iked *env, u_int type)
{
	return (pfkey_couple(env->sc_pfkey, &env->sc_sas,
	    type == IMSG_CTL_COUPLE ? 1 : 0));
}

int
config_setmode(struct iked *env, u_int passive)
{
	u_int	 type;

	type = passive ? IMSG_CTL_PASSIVE : IMSG_CTL_ACTIVE;
	proc_compose_imsg(&env->sc_ps, PROC_IKEV1, -1, type, -1, NULL, 0);
	proc_compose_imsg(&env->sc_ps, PROC_IKEV2, -1, type, -1, NULL, 0);

	return (0);
}

int
config_getmode(struct iked *env, u_int type)
{
	u_int8_t	 old;
	u_char		*mode[] = { "active", "passive" };

	old = env->sc_passive ? 1 : 0;
	env->sc_passive = type == IMSG_CTL_PASSIVE ? 1 : 0;

	if (old == env->sc_passive)
		return (0);

	log_debug("%s: mode %s -> %s", __func__,
	    mode[old], mode[env->sc_passive]);

	return (0);
}

int
config_setreset(struct iked *env, u_int mode, enum privsep_procid id)
{
	proc_compose_imsg(&env->sc_ps, id, -1,
	    IMSG_CTL_RESET, -1, &mode, sizeof(mode));
	return (0);
}

int
config_getreset(struct iked *env, struct imsg *imsg)
{
	struct iked_policy	*pol, *nextpol;
	struct iked_sa		*sa, *nextsa;
	struct iked_user	*usr, *nextusr;
	u_int			 mode;

	IMSG_SIZE_CHECK(imsg, &mode);
	memcpy(&mode, imsg->data, sizeof(mode));

	if (mode == RESET_ALL || mode == RESET_POLICY) {
		log_debug("%s: flushing policies", __func__);
		for (pol = TAILQ_FIRST(&env->sc_policies);
		    pol != NULL; pol = nextpol) {
			nextpol = TAILQ_NEXT(pol, pol_entry);
			config_free_policy(env, pol);
		}
	}

	if (mode == RESET_ALL || mode == RESET_SA) {
		log_debug("%s: flushing SAs", __func__);
		for (sa = RB_MIN(iked_sas, &env->sc_sas);
		    sa != NULL; sa = nextsa) {
			nextsa = RB_NEXT(iked_sas, &env->sc_sas, sa);
			RB_REMOVE(iked_sas, &env->sc_sas, sa);
			config_free_sa(env, sa);
		}
	}

	if (mode == RESET_ALL || mode == RESET_USER) {
		log_debug("%s: flushing users", __func__);
		for (usr = RB_MIN(iked_users, &env->sc_users);
		    usr != NULL; usr = nextusr) {
			nextusr = RB_NEXT(iked_users, &env->sc_users, usr);
			RB_REMOVE(iked_users, &env->sc_users, usr);
			free(usr);
		}
	}

	return (0);
}

int
config_setsocket(struct iked *env, struct sockaddr_storage *ss,
    in_port_t port, enum privsep_procid id)
{
	int	 s;

	if ((s = udp_bind((struct sockaddr *)ss, port)) == -1)
		return (-1);
	proc_compose_imsg(&env->sc_ps, id, -1,
	    IMSG_UDP_SOCKET, s, ss, sizeof(*ss));
	return (0);
}

int
config_getsocket(struct iked *env, struct imsg *imsg,
    void (*cb)(int, short, void *))
{
	struct iked_socket	*sock, **sptr, **nptr;

	log_debug("%s: received socket fd %d", __func__, imsg->fd);

	if ((sock = calloc(1, sizeof(*sock))) == NULL)
		fatal("config_getsocket: calloc");

	IMSG_SIZE_CHECK(imsg, &sock->sock_addr);

	memcpy(&sock->sock_addr, imsg->data, sizeof(sock->sock_addr));
	sock->sock_fd = imsg->fd;
	sock->sock_env = env;

	switch (sock->sock_addr.ss_family) {
	case AF_INET:
		sptr = &env->sc_sock4[0];
		nptr = &env->sc_sock4[1];
		break;
	case AF_INET6:
		sptr = &env->sc_sock6[0];
		nptr = &env->sc_sock6[1];
		break;
	default:
		fatal("config_getsocket: socket af");
		/* NOTREACHED */
	}
	if (*sptr == NULL)
		*sptr = sock;
	if (*nptr == NULL &&
	    socket_getport((struct sockaddr *)&sock->sock_addr) ==
	    IKED_NATT_PORT)
		*nptr = sock;

	event_set(&sock->sock_ev, sock->sock_fd,
	    EV_READ|EV_PERSIST, cb, sock);
	event_add(&sock->sock_ev, NULL);

	return (0);
}

int
config_setpfkey(struct iked *env, enum privsep_procid id)
{
	int	 s;

	if ((s = pfkey_socket()) == -1)
		return (-1);
	proc_compose_imsg(&env->sc_ps, id, -1,
	    IMSG_PFKEY_SOCKET, s, NULL, 0);
	return (0);
}

int
config_getpfkey(struct iked *env, struct imsg *imsg)
{
	log_debug("%s: received pfkey fd %d", __func__, imsg->fd);
	pfkey_init(env, imsg->fd);
	return (0);
}

int
config_setuser(struct iked *env, struct iked_user *usr, enum privsep_procid id)
{
	if (env->sc_opts & IKED_OPT_NOACTION) {
		print_user(usr);
		return (0);
	}

	proc_compose_imsg(&env->sc_ps, id, -1,
	    IMSG_CFG_USER, -1, usr, sizeof(*usr));
	return (0);
}

int
config_getuser(struct iked *env, struct imsg *imsg)
{
	struct iked_user	 usr;

	IMSG_SIZE_CHECK(imsg, &usr);
	memcpy(&usr, imsg->data, sizeof(usr));

	if (config_new_user(env, &usr) == NULL)
		return (-1);

	print_user(&usr);

	return (0);
}

int
config_setpolicy(struct iked *env, struct iked_policy *pol,
    enum privsep_procid id)
{
	struct iked_proposal	*prop;
	struct iked_flow	*flow;
	struct iked_transform	*xform;
	size_t			 size, iovcnt, j, c = 0;
	struct iovec		 iov[IOV_MAX];

	iovcnt = 1;
	size = sizeof(*pol);
	TAILQ_FOREACH(prop, &pol->pol_proposals, prop_entry) {
		size += (prop->prop_nxforms * sizeof(*xform)) +
		    (sizeof(*prop));
		iovcnt += prop->prop_nxforms + 1;
	}

	iovcnt += pol->pol_nflows;

	if (iovcnt > IOV_MAX) {
		log_warn("%s: too many proposals/flows", __func__);
		return (-1);
	}

	iov[c].iov_base = pol;
	iov[c++].iov_len = sizeof(*pol);

	TAILQ_FOREACH(prop, &pol->pol_proposals, prop_entry) {
		iov[c].iov_base = prop;
		iov[c++].iov_len = sizeof(*prop);

		for (j = 0; j < prop->prop_nxforms; j++) {
			xform = prop->prop_xforms + j;

			iov[c].iov_base = xform;
			iov[c++].iov_len = sizeof(*xform);
		}
	}

	RB_FOREACH(flow, iked_flows, &pol->pol_flows) {
		iov[c].iov_base = flow;
		iov[c++].iov_len = sizeof(*flow);
	}

	if (env->sc_opts & IKED_OPT_NOACTION) {
		print_policy(pol);
		return (0);
	}

	if (proc_composev_imsg(&env->sc_ps, id, -1,
	    IMSG_CFG_POLICY, -1, iov, iovcnt) == -1)
		return (-1);

	return (0);
}

int
config_getpolicy(struct iked *env, struct imsg *imsg)
{
	struct iked_policy	*pol;
	struct iked_proposal	 pp, *prop;
	struct iked_transform	 xf, *xform;
	struct iked_flow	*flow;
	off_t			 offset = 0;
	u_int			 i, j;
	u_int8_t		*buf = (u_int8_t *)imsg->data;

	IMSG_SIZE_CHECK(imsg, pol);
	log_debug("%s: received policy", __func__);

	if ((pol = config_new_policy(NULL)) == NULL)
		fatal("config_getpolicy: new policy");

	memcpy(pol, buf, sizeof(*pol));
	offset += sizeof(*pol);

	TAILQ_INIT(&pol->pol_proposals);
	RB_INIT(&pol->pol_flows);

	for (i = 0; i < pol->pol_nproposals; i++) {
		memcpy(&pp, buf + offset, sizeof(pp));
		offset += sizeof(pp);

		if ((prop = config_add_proposal(&pol->pol_proposals,
		    pp.prop_id, pp.prop_protoid)) == NULL)
			fatal("config_getpolicy: add proposal");

		for (j = 0; j < pp.prop_nxforms; j++) {
			memcpy(&xf, buf + offset, sizeof(xf));
			offset += sizeof(xf);

			if ((xform = config_add_transform(prop, xf.xform_type,
			    xf.xform_id, xf.xform_length,
			    xf.xform_keylength)) == NULL)
				fatal("config_getpolicy: add transform");
		}
	}

	for (i = 0; i < pol->pol_nflows; i++) {
		if ((flow = calloc(1, sizeof(*flow))) == NULL)
			fatal("config_getpolicy: new flow");

		memcpy(flow, buf + offset, sizeof(*flow));
		offset += sizeof(*flow);

		if (RB_INSERT(iked_flows, &pol->pol_flows, flow))
			free(flow);
	}

	TAILQ_INSERT_TAIL(&env->sc_policies, pol, pol_entry);

	if (pol->pol_flags & IKED_POLICY_DEFAULT) {
		/* Only one default policy, just free/unref the old one */
		if (env->sc_defaultcon != NULL)
			config_free_policy(env, env->sc_defaultcon);
		env->sc_defaultcon = pol;
	}

	print_policy(pol);

	return (0);
}

int
config_setcompile(struct iked *env, enum privsep_procid id)
{
	if (env->sc_opts & IKED_OPT_NOACTION)
		return (0);

	proc_compose_imsg(&env->sc_ps, id, -1,
	    IMSG_COMPILE, -1, NULL, 0);
	return (0);
}

int
config_getcompile(struct iked *env, struct imsg *imsg)
{
	/*
	 * Do any necessary steps after configuration, for now we
	 * only need to compile the skip steps.
	 */
	policy_calc_skip_steps(&env->sc_policies);

	log_debug("%s: compilation done", __func__);
	return (0);
}

int
config_setocsp(struct iked *env)
{
	if (env->sc_opts & IKED_OPT_NOACTION)
		return (0);
	proc_compose_imsg(&env->sc_ps, PROC_CERT, -1,
	    IMSG_OCSP_URL, -1, env->sc_ocsp_url,
	    env->sc_ocsp_url ? strlen(env->sc_ocsp_url) : 0);

	return (0);
}

int
config_getocsp(struct iked *env, struct imsg *imsg)
{
	if (env->sc_ocsp_url)
		free(env->sc_ocsp_url);
	if (IMSG_DATA_SIZE(imsg) > 0)
		env->sc_ocsp_url = get_string(imsg->data, IMSG_DATA_SIZE(imsg));
	else
		env->sc_ocsp_url = NULL;
	log_debug("%s: ocsp_url %s", __func__,
	    env->sc_ocsp_url ? env->sc_ocsp_url : "none");
	return (0);
}
