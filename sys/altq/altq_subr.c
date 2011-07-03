/*	$OpenBSD: altq_subr.c,v 1.27 2011/07/03 23:48:41 henning Exp $	*/
/*	$KAME: altq_subr.c,v 1.11 2002/01/11 08:11:49 kjc Exp $	*/

/*
 * Copyright (C) 1997-2002
 *	Sony Computer Science Laboratories Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY SONY CSL AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL SONY CSL OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/kernel.h>
#include <sys/errno.h>
#include <sys/syslog.h>
#include <sys/sysctl.h>
#include <sys/queue.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#ifdef INET6
#include <netinet/ip6.h>
#endif
#include <netinet/tcp.h>
#include <netinet/udp.h>

#include <net/pfvar.h>
#include <altq/altq.h>

/*
 * internal function prototypes
 */
static void	tbr_timeout(void *);
int (*altq_input)(struct mbuf *, int) = NULL;
static int tbr_timer = 0;	/* token bucket regulator timer */
static struct callout tbr_callout = CALLOUT_INITIALIZER;

/*
 * alternate queueing support routines
 */

/* look up the queue state by the interface name and the queueing type. */
void *
altq_lookup(name, type)
	char *name;
	int type;
{
	struct ifnet *ifp;

	if ((ifp = ifunit(name)) != NULL) {
		if (type != ALTQT_NONE && ifp->if_snd.altq_type == type)
			return (ifp->if_snd.altq_disc);
	}

	return NULL;
}

int
altq_attach(ifq, type, discipline, enqueue, dequeue, request, clfier, classify)
	struct ifaltq *ifq;
	int type;
	void *discipline;
	int (*enqueue)(struct ifaltq *, struct mbuf *, struct altq_pktattr *);
	struct mbuf *(*dequeue)(struct ifaltq *, int);
	int (*request)(struct ifaltq *, int, void *);
	void *clfier;
	void *(*classify)(void *, struct mbuf *, int);
{
	if (!ALTQ_IS_READY(ifq))
		return ENXIO;

#if 0	/* pfaltq can override the existing discipline */
	if (ALTQ_IS_ENABLED(ifq))
		return EBUSY;
	if (ALTQ_IS_ATTACHED(ifq))
		return EEXIST;
#endif
	ifq->altq_type     = type;
	ifq->altq_disc     = discipline;
	ifq->altq_enqueue  = enqueue;
	ifq->altq_dequeue  = dequeue;
	ifq->altq_request  = request;
	ifq->altq_clfier   = clfier;
	ifq->altq_classify = classify;
	ifq->altq_flags &= (ALTQF_CANTCHANGE|ALTQF_ENABLED);

	return 0;
}

int
altq_detach(ifq)
	struct ifaltq *ifq;
{
	if (!ALTQ_IS_READY(ifq))
		return ENXIO;
	if (ALTQ_IS_ENABLED(ifq))
		return EBUSY;
	if (!ALTQ_IS_ATTACHED(ifq))
		return (0);

	ifq->altq_type     = ALTQT_NONE;
	ifq->altq_disc     = NULL;
	ifq->altq_enqueue  = NULL;
	ifq->altq_dequeue  = NULL;
	ifq->altq_request  = NULL;
	ifq->altq_clfier   = NULL;
	ifq->altq_classify = NULL;
	ifq->altq_flags &= ALTQF_CANTCHANGE;
	return 0;
}

int
altq_enable(ifq)
	struct ifaltq *ifq;
{
	int s;

	if (!ALTQ_IS_READY(ifq))
		return ENXIO;
	if (ALTQ_IS_ENABLED(ifq))
		return 0;

	s = splnet();
	IFQ_PURGE(ifq);
	ASSERT(ifq->ifq_len == 0);
	ifq->altq_flags |= ALTQF_ENABLED;
	if (ifq->altq_clfier != NULL)
		ifq->altq_flags |= ALTQF_CLASSIFY;
	splx(s);

	return 0;
}

int
altq_disable(ifq)
	struct ifaltq *ifq;
{
	int s;

	if (!ALTQ_IS_ENABLED(ifq))
		return 0;

	s = splnet();
	IFQ_PURGE(ifq);
	ASSERT(ifq->ifq_len == 0);
	ifq->altq_flags &= ~(ALTQF_ENABLED|ALTQF_CLASSIFY);
	splx(s);
	return 0;
}

void
altq_assert(file, line, failedexpr)
	const char *file, *failedexpr;
	int line;
{
	(void)printf("altq assertion \"%s\" failed: file \"%s\", line %d\n",
		     failedexpr, file, line);
	panic("altq assertion");
	/* NOTREACHED */
}

/*
 * internal representation of token bucket parameters
 *	rate:	byte_per_unittime << 32
 *		(((bits_per_sec) / 8) << 32) / machclk_freq
 *	depth:	byte << 32
 *
 */
#define	TBR_SHIFT	32
#define	TBR_SCALE(x)	((int64_t)(x) << TBR_SHIFT)
#define	TBR_UNSCALE(x)	((x) >> TBR_SHIFT)

struct mbuf *
tbr_dequeue(ifq, op)
	struct ifaltq *ifq;
	int op;
{
	struct tb_regulator *tbr;
	struct mbuf *m;
	int64_t interval;
	u_int64_t now;

	tbr = ifq->altq_tbr;
	if (op == ALTDQ_REMOVE && tbr->tbr_lastop == ALTDQ_POLL) {
		/* if this is a remove after poll, bypass tbr check */
	} else {
		/* update token only when it is negative */
		if (tbr->tbr_token <= 0) {
			now = read_machclk();
			interval = now - tbr->tbr_last;
			if (interval >= tbr->tbr_filluptime)
				tbr->tbr_token = tbr->tbr_depth;
			else {
				tbr->tbr_token += interval * tbr->tbr_rate;
				if (tbr->tbr_token > tbr->tbr_depth)
					tbr->tbr_token = tbr->tbr_depth;
			}
			tbr->tbr_last = now;
		}
		/* if token is still negative, don't allow dequeue */
		if (tbr->tbr_token <= 0)
			return (NULL);
	}

	if (ALTQ_IS_ENABLED(ifq))
		m = (*ifq->altq_dequeue)(ifq, op);
	else {
		if (op == ALTDQ_POLL)
			IF_POLL(ifq, m);
		else
			IF_DEQUEUE(ifq, m);
	}

	if (m != NULL && op == ALTDQ_REMOVE)
		tbr->tbr_token -= TBR_SCALE(m_pktlen(m));
	tbr->tbr_lastop = op;
	return (m);
}

/*
 * set a token bucket regulator.
 * if the specified rate is zero, the token bucket regulator is deleted.
 */
int
tbr_set(ifq, profile)
	struct ifaltq *ifq;
	struct tb_profile *profile;
{
	struct tb_regulator *tbr, *otbr;

	if (machclk_freq == 0)
		init_machclk();
	if (machclk_freq == 0) {
		printf("tbr_set: no cpu clock available!\n");
		return (ENXIO);
	}

	if (profile->rate == 0) {
		/* delete this tbr */
		if ((tbr = ifq->altq_tbr) == NULL)
			return (ENOENT);
		ifq->altq_tbr = NULL;
		free(tbr, M_DEVBUF);
		return (0);
	}

	tbr = malloc(sizeof(struct tb_regulator), M_DEVBUF, M_WAITOK|M_ZERO);

	tbr->tbr_rate = TBR_SCALE(profile->rate / 8) / machclk_freq;
	tbr->tbr_depth = TBR_SCALE(profile->depth);
	if (tbr->tbr_rate > 0)
		tbr->tbr_filluptime = tbr->tbr_depth / tbr->tbr_rate;
	else
		tbr->tbr_filluptime = 0xffffffffffffffffLL;
	tbr->tbr_token = tbr->tbr_depth;
	tbr->tbr_last = read_machclk();
	tbr->tbr_lastop = ALTDQ_REMOVE;

	otbr = ifq->altq_tbr;
	ifq->altq_tbr = tbr;	/* set the new tbr */

	if (otbr != NULL)
		free(otbr, M_DEVBUF);
	else {
		if (tbr_timer == 0) {
			CALLOUT_RESET(&tbr_callout, 1, tbr_timeout, (void *)0);
			tbr_timer = 1;
		}
	}
	return (0);
}

/*
 * tbr_timeout goes through the interface list, and kicks the drivers
 * if necessary.
 */
static void
tbr_timeout(arg)
	void *arg;
{
	struct ifnet *ifp;
	int active, s;

	active = 0;
	s = splnet();
	for (ifp = TAILQ_FIRST(&ifnet); ifp; ifp = TAILQ_NEXT(ifp, if_list)) {
		if (!TBR_IS_ENABLED(&ifp->if_snd))
			continue;
		active++;
		if (!IFQ_IS_EMPTY(&ifp->if_snd) && ifp->if_start != NULL)
			if_start(ifp);
	}
	splx(s);
	if (active > 0)
		CALLOUT_RESET(&tbr_callout, 1, tbr_timeout, (void *)0);
	else
		tbr_timer = 0;	/* don't need tbr_timer anymore */
}

/*
 * get token bucket regulator profile
 */
int
tbr_get(ifq, profile)
	struct ifaltq *ifq;
	struct tb_profile *profile;
{
	struct tb_regulator *tbr;

	if ((tbr = ifq->altq_tbr) == NULL) {
		profile->rate = 0;
		profile->depth = 0;
	} else {
		profile->rate =
		    (u_int)TBR_UNSCALE(tbr->tbr_rate * 8 * machclk_freq);
		profile->depth = (u_int)TBR_UNSCALE(tbr->tbr_depth);
	}
	return (0);
}

/*
 * attach a discipline to the interface.  if one already exists, it is
 * overridden.
 */
int
altq_pfattach(struct pf_altq *a)
{
	int error = 0;

	switch (a->scheduler) {
	case ALTQT_NONE:
		break;
#ifdef ALTQ_CBQ
	case ALTQT_CBQ:
		error = cbq_pfattach(a);
		break;
#endif
#ifdef ALTQ_PRIQ
	case ALTQT_PRIQ:
		error = priq_pfattach(a);
		break;
#endif
#ifdef ALTQ_HFSC
	case ALTQT_HFSC:
		error = hfsc_pfattach(a);
		break;
#endif
	default:
		error = ENXIO;
	}

	return (error);
}

/*
 * detach a discipline from the interface.
 * it is possible that the discipline was already overridden by another
 * discipline.
 */
int
altq_pfdetach(struct pf_altq *a)
{
	struct ifnet *ifp;
	int s, error = 0;

	if ((ifp = ifunit(a->ifname)) == NULL)
		return (EINVAL);

	/* if this discipline is no longer referenced, just return */
	if (a->altq_disc == NULL || a->altq_disc != ifp->if_snd.altq_disc)
		return (0);

	s = splnet();
	if (ALTQ_IS_ENABLED(&ifp->if_snd))
		error = altq_disable(&ifp->if_snd);
	if (error == 0)
		error = altq_detach(&ifp->if_snd);
	splx(s);

	return (error);
}

/*
 * add a discipline or a queue
 */
int
altq_add(struct pf_altq *a)
{
	int error = 0;

	if (a->qname[0] != 0)
		return (altq_add_queue(a));

	if (machclk_freq == 0)
		init_machclk();
	if (machclk_freq == 0)
		panic("altq_add: no cpu clock");

	switch (a->scheduler) {
#ifdef ALTQ_CBQ
	case ALTQT_CBQ:
		error = cbq_add_altq(a);
		break;
#endif
#ifdef ALTQ_PRIQ
	case ALTQT_PRIQ:
		error = priq_add_altq(a);
		break;
#endif
#ifdef ALTQ_HFSC
	case ALTQT_HFSC:
		error = hfsc_add_altq(a);
		break;
#endif
	default:
		error = ENXIO;
	}

	return (error);
}

/*
 * remove a discipline or a queue
 */
int
altq_remove(struct pf_altq *a)
{
	int error = 0;

	if (a->qname[0] != 0)
		return (altq_remove_queue(a));

	switch (a->scheduler) {
#ifdef ALTQ_CBQ
	case ALTQT_CBQ:
		error = cbq_remove_altq(a);
		break;
#endif
#ifdef ALTQ_PRIQ
	case ALTQT_PRIQ:
		error = priq_remove_altq(a);
		break;
#endif
#ifdef ALTQ_HFSC
	case ALTQT_HFSC:
		error = hfsc_remove_altq(a);
		break;
#endif
	default:
		error = ENXIO;
	}

	return (error);
}

/*
 * add a queue to the discipline
 */
int
altq_add_queue(struct pf_altq *a)
{
	int error = 0;

	switch (a->scheduler) {
#ifdef ALTQ_CBQ
	case ALTQT_CBQ:
		error = cbq_add_queue(a);
		break;
#endif
#ifdef ALTQ_PRIQ
	case ALTQT_PRIQ:
		error = priq_add_queue(a);
		break;
#endif
#ifdef ALTQ_HFSC
	case ALTQT_HFSC:
		error = hfsc_add_queue(a);
		break;
#endif
	default:
		error = ENXIO;
	}

	return (error);
}

/*
 * remove a queue from the discipline
 */
int
altq_remove_queue(struct pf_altq *a)
{
	int error = 0;

	switch (a->scheduler) {
#ifdef ALTQ_CBQ
	case ALTQT_CBQ:
		error = cbq_remove_queue(a);
		break;
#endif
#ifdef ALTQ_PRIQ
	case ALTQT_PRIQ:
		error = priq_remove_queue(a);
		break;
#endif
#ifdef ALTQ_HFSC
	case ALTQT_HFSC:
		error = hfsc_remove_queue(a);
		break;
#endif
	default:
		error = ENXIO;
	}

	return (error);
}

/*
 * get queue statistics
 */
int
altq_getqstats(struct pf_altq *a, void *ubuf, int *nbytes)
{
	int error = 0;

	switch (a->scheduler) {
#ifdef ALTQ_CBQ
	case ALTQT_CBQ:
		error = cbq_getqstats(a, ubuf, nbytes);
		break;
#endif
#ifdef ALTQ_PRIQ
	case ALTQT_PRIQ:
		error = priq_getqstats(a, ubuf, nbytes);
		break;
#endif
#ifdef ALTQ_HFSC
	case ALTQT_HFSC:
		error = hfsc_getqstats(a, ubuf, nbytes);
		break;
#endif
	default:
		error = ENXIO;
	}

	return (error);
}

#define	MACHCLK_SHIFT	8

u_int32_t machclk_tc = 0;
u_int32_t machclk_freq = 0;
u_int32_t machclk_per_tick = 0;

void
init_machclk(void)
{
#if defined(__HAVE_TIMECOUNTER)
	/*
	 * If we have timecounters, microtime is good enough and we can
	 * avoid problems on machines with variable cycle counter
	 * frequencies.
	 */
	machclk_tc = 1;
#endif

	if (machclk_tc == 1) {
		/* emulate 256MHz using microtime() */
		machclk_freq = 1000000 << MACHCLK_SHIFT;
		machclk_per_tick = machclk_freq / hz;
#ifdef ALTQ_DEBUG
		printf("altq: emulating %uHz cpu clock\n", machclk_freq);
#endif
		return;
	}

	/*
	 * if we don't know the clock frequency, measure it.
	 */
	if (machclk_freq == 0) {
		static int	wait;
		struct timeval	tv_start, tv_end;
		u_int64_t	start, end, diff;
		int		timo;

		microtime(&tv_start);
		start = read_machclk();
		timo = hz;	/* 1 sec */
		(void)tsleep(&wait, PWAIT | PCATCH, "init_machclk", timo);
		microtime(&tv_end);
		end = read_machclk();
		diff = (u_int64_t)(tv_end.tv_sec - tv_start.tv_sec) * 1000000
		    + tv_end.tv_usec - tv_start.tv_usec;
		if (diff != 0)
			machclk_freq = (u_int)((end - start) * 1000000 / diff);
	}

	machclk_per_tick = machclk_freq / hz;

#ifdef ALTQ_DEBUG
	printf("altq: CPU clock: %uHz\n", machclk_freq);
#endif
}

u_int64_t
read_machclk(void)
{
	struct timeval tv;
	u_int64_t val;

	microuptime(&tv);
	val = (((u_int64_t)(tv.tv_sec) * 1000000 +
	    tv.tv_usec) << MACHCLK_SHIFT);
	return (val);
}
