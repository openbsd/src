/*	$OpenBSD: altq_var.h,v 1.6 2002/10/08 05:12:08 kjc Exp $	*/
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

/*
 * filter structure for altq common classifier
 */
struct acc_filter {
	LIST_ENTRY(acc_filter)	f_chain;
	void			*f_class;	/* pointer to the class */
	u_long			f_handle;	/* filter id */
	u_int32_t		f_fbmask;	/* filter bitmask */
	struct flow_filter	f_filter;	/* filter value */
};

/*
 * XXX ACC_FILTER_TABLESIZE can't be larger than 2048 unless we fix
 * the handle assignment.
 */
#define	ACC_FILTER_TABLESIZE	(256+1)
#define	ACC_FILTER_MASK		(ACC_FILTER_TABLESIZE - 2)
#define	ACC_WILDCARD_INDEX	(ACC_FILTER_TABLESIZE - 1)
#ifdef __GNUC__
#define	ACC_GET_HASH_INDEX(addr) \
	({int x = (addr) + ((addr) >> 16); (x + (x >> 8)) & ACC_FILTER_MASK;})
#else
#define	ACC_GET_HASH_INDEX(addr) \
	(((addr) + ((addr) >> 8) + ((addr) >> 16) + ((addr) >> 24)) \
	& ACC_FILTER_MASK)
#endif
#define	ACC_GET_HINDEX(handle) ((handle) >> 20)

struct acc_classifier {
	u_int32_t			acc_fbmask;
	LIST_HEAD(filt, acc_filter)	acc_filters[ACC_FILTER_TABLESIZE];
};

/*
 * flowinfo mask bits used by classifier
 */
/* for ipv4 */
#define	FIMB4_PROTO	0x0001
#define	FIMB4_TOS	0x0002
#define	FIMB4_DADDR	0x0004
#define	FIMB4_SADDR	0x0008
#define	FIMB4_DPORT	0x0010
#define	FIMB4_SPORT	0x0020
#define	FIMB4_GPI	0x0040
#define	FIMB4_ALL	0x007f
/* for ipv6 */
#define	FIMB6_PROTO	0x0100
#define	FIMB6_TCLASS	0x0200
#define	FIMB6_DADDR	0x0400
#define	FIMB6_SADDR	0x0800
#define	FIMB6_DPORT	0x1000
#define	FIMB6_SPORT	0x2000
#define	FIMB6_GPI	0x4000
#define	FIMB6_FLABEL	0x8000
#define	FIMB6_ALL	0xff00

#define	FIMB_ALL	(FIMB4_ALL|FIMB6_ALL)

#define	FIMB4_PORTS	(FIMB4_DPORT|FIMB4_SPORT|FIMB4_GPI)
#define	FIMB6_PORTS	(FIMB6_DPORT|FIMB6_SPORT|FIMB6_GPI)

/*
 * machine dependent clock
 * a 64bit high resolution time counter.
 */
extern u_int32_t machclk_freq;
extern u_int32_t machclk_per_tick;
extern void init_machclk(void);

#if defined(__i386__) && !defined(I586_CPU) && !defined(I686_CPU)
#ifndef ALTQ_NOPCC
#define	ALTQ_NOPCC	/* TSC is not available, ALTQ_NOPCC needed */
#endif
#endif

#if defined(__i386__) && !defined(ALTQ_NOPCC)
/* for pentium tsc */
#include <machine/cpufunc.h>

#define	read_machclk()		rdtsc()
#ifdef __OpenBSD__
static __inline u_int64_t
rdtsc(void)
{
	u_int64_t rv;
	__asm __volatile(".byte 0x0f, 0x31" : "=A" (rv));
	return (rv);
}
#endif /* __OpenBSD__ */

#elif defined(__alpha__) && !defined(ALTQ_NOPCC)
/* for alpha rpcc */
extern u_int64_t read_machclk(void);

#else /* !i386 && !alpha */
/* emulate 256MHz using microtime() */
#define	MACHCLK_SHIFT	8
static __inline u_int64_t
read_machclk(void)
{
	struct timeval tv;
	microtime(&tv);
	return (((u_int64_t)(tv.tv_sec - boottime.tv_sec) * 1000000
		 + tv.tv_usec) << MACHCLK_SHIFT);
}
#endif /* !i386 && !alpha */

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
/* ioctl cmd type */
#if defined(__FreeBSD__) && (__FreeBSD__ < 3)
typedef int ioctlcmd_t;
#else
typedef u_long ioctlcmd_t;
#endif

/*
 * queue macros:
 * the interface of TAILQ_LAST macro changed after the introduction
 * of softupdate. redefine it here to make it work with pre-2.2.7.
 */
#undef TAILQ_LAST
#define	TAILQ_LAST(head, headname) \
	(*(((struct headname *)((head)->tqh_last))->tqh_last))

#ifndef TAILQ_EMPTY
#define	TAILQ_EMPTY(head) ((head)->tqh_first == NULL)
#endif
#ifndef TAILQ_FOREACH
#define TAILQ_FOREACH(var, head, field)					\
	for (var = TAILQ_FIRST(head); var; var = TAILQ_NEXT(var, field))
#endif

/* macro for timeout/untimeout */
#if (__FreeBSD_version > 300000) || defined(__NetBSD__)
/* use callout */
#include <sys/callout.h>

#define	CALLOUT_INIT(c)		callout_init((c))
#define	CALLOUT_RESET(c,t,f,a)	callout_reset((c),(t),(f),(a))
#define	CALLOUT_STOP(c)		callout_stop((c))
#ifndef CALLOUT_INITIALIZER
#define	CALLOUT_INITIALIZER	{ { { NULL } }, 0, NULL, NULL, 0 }
#endif
#elif defined(__OpenBSD__)
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
#else
/* use old-style timeout/untimeout */
/* dummy callout structure */
struct callout {
	void		*c_arg;			/* function argument */
	void		(*c_func)(void *);	/* function to call */
};
#define	CALLOUT_INIT(c)		do { bzero((c), sizeof(*(c))); } while (0)
#define	CALLOUT_RESET(c,t,f,a)	do {	(c)->c_arg = (a);	\
					(c)->c_func = (f);	\
					timeout((f),(a),(t)); } while (0)
#define	CALLOUT_STOP(c)		untimeout((c)->c_func,(c)->c_arg)
#define	CALLOUT_INITIALIZER	{ NULL, NULL }
#endif
#if !defined(__FreeBSD__)
typedef void (timeout_t)(void *);
#endif

#define	m_pktlen(m)		((m)->m_pkthdr.len)

struct ifnet; struct mbuf; struct flowinfo;
struct pf_altq; struct pf_qstats;

void *altq_lookup(char *, int);
int altq_extractflow(struct mbuf *, int, struct flowinfo *, u_int32_t);
int acc_add_filter(struct acc_classifier *, struct flow_filter *,
			   void *, u_long *);
int acc_delete_filter(struct acc_classifier *, u_long);
int acc_discard_filters(struct acc_classifier *, void *, int);
void *acc_classify(void *, struct mbuf *, int);
u_int8_t read_dsfield(struct mbuf *, struct altq_pktattr *);
void write_dsfield(struct mbuf *, struct altq_pktattr *, u_int8_t);
void altq_assert(const char *, int, const char *);
int tbr_set(struct ifaltq *, struct tb_profile *);
int tbr_get(struct ifaltq *, struct tb_profile *);
int	altq_pfattach(struct pf_altq *);
int	altq_pfdetach(struct pf_altq *);
int	altq_add(struct pf_altq *);
int	altq_remove(struct pf_altq *);
int	altq_add_queue(struct pf_altq *);
int	altq_remove_queue(struct pf_altq *);
int	altq_getqstats(struct pf_altq *, void *, int *);

#endif /* _KERNEL */
#endif /* _ALTQ_ALTQ_VAR_H_ */
