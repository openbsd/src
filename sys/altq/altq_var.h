/*	$OpenBSD: altq_var.h,v 1.14 2004/04/27 02:56:20 kjc Exp $	*/
/*	$KAME: altq_var.h,v 1.8 2001/02/09 09:44:41 kjc Exp $	*/

/*
 * Copyright (C) 1998-2000
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
#ifndef _ALTQ_ALTQ_VAR_H_
#define	_ALTQ_ALTQ_VAR_H_

#ifdef _KERNEL

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/queue.h>

#ifndef ALTQ_RED
#define ALTQ_RED		/* RED is enabled by default */
#endif
#ifndef ALTQ_CBQ
#define ALTQ_CBQ		/* CBQ is enabled by default */
#endif
#ifndef ALTQ_PRIQ
#define ALTQ_PRIQ		/* PRIQ is enabled by default */
#endif
#ifndef ALTQ_HFSC
#define ALTQ_HFSC		/* HFSC is enabled by default */
#endif

/*
 * machine dependent clock
 * a 64bit high resolution time counter.
 */
extern int machclk_usepcc;
extern u_int32_t machclk_freq;
extern u_int32_t machclk_per_tick;
extern void init_machclk(void);
extern u_int64_t read_machclk(void);

/*
 * debug support
 */
#ifdef ALTQ_DEBUG
#ifdef __STDC__
#define	ASSERT(e)	((e) ? (void)0 : altq_assert(__FILE__, __LINE__, #e))
#else	/* PCC */
#define	ASSERT(e)	((e) ? (void)0 : altq_assert(__FILE__, __LINE__, "e"))
#endif
#else
#define	ASSERT(e)	((void)0)
#endif

/*
 * misc stuff for compatibility
 */

/* macro for timeout/untimeout */
#include <sys/timeout.h>
/* callout structure as a wrapper of struct timeout */
struct callout {
	struct timeout	c_to;
};
#define	CALLOUT_INIT(c)		do { bzero((c), sizeof(*(c))); } while (0)
#define	CALLOUT_RESET(c,t,f,a)	do { if (!timeout_initialized(&(c)->c_to))  \
					 timeout_set(&(c)->c_to, (f), (a)); \
				     timeout_add(&(c)->c_to, (t)); } while (0)
#define	CALLOUT_STOP(c)		timeout_del(&(c)->c_to)
#define	CALLOUT_INITIALIZER	{ { { NULL }, NULL, NULL, 0, 0 } }

typedef void (timeout_t)(void *);

#define	m_pktlen(m)		((m)->m_pkthdr.len)

struct ifnet; struct mbuf;
struct pf_altq; struct pf_qstats;

void	*altq_lookup(char *, int);
u_int8_t read_dsfield(struct mbuf *, struct altq_pktattr *);
void	write_dsfield(struct mbuf *, struct altq_pktattr *, u_int8_t);
void	altq_assert(const char *, int, const char *);
int	tbr_set(struct ifaltq *, struct tb_profile *);
int	tbr_get(struct ifaltq *, struct tb_profile *);
int	altq_pfattach(struct pf_altq *);

int	altq_pfdetach(struct pf_altq *);
int	altq_add(struct pf_altq *);
int	altq_remove(struct pf_altq *);
int	altq_add_queue(struct pf_altq *);
int	altq_remove_queue(struct pf_altq *);
int	altq_getqstats(struct pf_altq *, void *, int *);

int	cbq_pfattach(struct pf_altq *);
int	cbq_add_altq(struct pf_altq *);
int	cbq_remove_altq(struct pf_altq *);
int	cbq_add_queue(struct pf_altq *);
int	cbq_remove_queue(struct pf_altq *);
int	cbq_getqstats(struct pf_altq *, void *, int *);

int	priq_pfattach(struct pf_altq *);
int	priq_add_altq(struct pf_altq *);
int	priq_remove_altq(struct pf_altq *);
int	priq_add_queue(struct pf_altq *);
int	priq_remove_queue(struct pf_altq *);
int	priq_getqstats(struct pf_altq *, void *, int *);

int	hfsc_pfattach(struct pf_altq *);
int	hfsc_add_altq(struct pf_altq *);
int	hfsc_remove_altq(struct pf_altq *);
int	hfsc_add_queue(struct pf_altq *);
int	hfsc_remove_queue(struct pf_altq *);
int	hfsc_getqstats(struct pf_altq *, void *, int *);

#endif /* _KERNEL */
#endif /* _ALTQ_ALTQ_VAR_H_ */
