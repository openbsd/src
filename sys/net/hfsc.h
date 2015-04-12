/*	$OpenBSD: hfsc.h,v 1.8 2015/04/12 09:58:46 dlg Exp $	*/

/*
 * Copyright (c) 2012-2013 Henning Brauer <henning@openbsd.org>
 * Copyright (c) 1997-1999 Carnegie Mellon University. All Rights Reserved.
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation is hereby granted (including for commercial or
 * for-profit use), provided that both the copyright notice and this
 * permission notice appear in all copies of the software, derivative
 * works, or modified versions, and any portions thereof.
 *
 * THIS SOFTWARE IS EXPERIMENTAL AND IS KNOWN TO HAVE BUGS, SOME OF
 * WHICH MAY HAVE SERIOUS CONSEQUENCES.  CARNEGIE MELLON PROVIDES THIS
 * SOFTWARE IN ITS ``AS IS'' CONDITION, AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL CARNEGIE MELLON UNIVERSITY BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * Carnegie Mellon encourages (but does not require) users of this
 * software to return any improvements or extensions that they make,
 * and to grant Carnegie Mellon the rights to redistribute these
 * changes without encumbrance.
 */
#ifndef _HFSC_H_
#define	_HFSC_H_

/* hfsc class flags */
#define	HFSC_RED		0x0001	/* use RED */
#define	HFSC_ECN		0x0002  /* use RED/ECN */
#define	HFSC_RIO		0x0004  /* use RIO */
#define	HFSC_DEFAULTCLASS	0x1000	/* default class */

struct hfsc_pktcntr {
	u_int64_t	packets;
	u_int64_t	bytes;
};

#define	PKTCNTR_INC(cntr, len)	\
	do { (cntr)->packets++; (cntr)->bytes += len; } while (0)

struct hfsc_sc {
	u_int	m1;	/* slope of the first segment in bits/sec */
	u_int	d;	/* the x-projection of the first segment in msec */
	u_int	m2;	/* slope of the second segment in bits/sec */
};

/* special class handles */
#define	HFSC_NULLCLASS_HANDLE	0
#define	HFSC_DEFAULT_CLASSES	64
#define	HFSC_MAX_CLASSES	65535

/* service curve types */
#define	HFSC_REALTIMESC		1
#define	HFSC_LINKSHARINGSC	2
#define	HFSC_UPPERLIMITSC	4
#define	HFSC_DEFAULTSC		(HFSC_REALTIMESC|HFSC_LINKSHARINGSC)

struct hfsc_class_stats {
	u_int			class_id;
	u_int32_t		class_handle;
	struct hfsc_sc		rsc;
	struct hfsc_sc		fsc;
	struct hfsc_sc		usc;	/* upper limit service curve */

	u_int64_t		total;	/* total work in bytes */
	u_int64_t		cumul;	/* cumulative work in bytes
					   done by real-time criteria */
	u_int64_t		d;		/* deadline */
	u_int64_t		e;		/* eligible time */
	u_int64_t		vt;		/* virtual time */
	u_int64_t		f;		/* fit time for upper-limit */

	/* info helpful for debugging */
	u_int64_t		initvt;		/* init virtual time */
	u_int64_t		vtoff;		/* cl_vt_ipoff */
	u_int64_t		cvtmax;		/* cl_maxvt */
	u_int64_t		myf;		/* cl_myf */
	u_int64_t		cfmin;		/* cl_mincf */
	u_int64_t		cvtmin;		/* cl_mincvt */
	u_int64_t		myfadj;		/* cl_myfadj */
	u_int64_t		vtadj;		/* cl_vtadj */
	u_int64_t		cur_time;
	u_int32_t		machclk_freq;

	u_int			qlength;
	u_int			qlimit;
	struct hfsc_pktcntr	xmit_cnt;
	struct hfsc_pktcntr	drop_cnt;
	u_int			period;

	u_int			vtperiod;	/* vt period sequence no */
	u_int			parentperiod;	/* parent's vt period seqno */
	int			nactive;	/* number of active children */

	/* red and rio related info */
	int			qtype;
/*	struct redstats		red[3]; */
};

#ifdef _KERNEL
struct ifnet;
struct ifqueue;
struct pf_queuespec;
struct hfsc_if;

#define	HFSC_ENABLED(ifq)	((ifq)->ifq_hfsc != NULL)
#define	HFSC_DEFAULT_QLIMIT	50

void		 hfsc_initialize(void);
int		 hfsc_attach(struct ifnet *);
int		 hfsc_detach(struct ifnet *);
void		 hfsc_purge(struct ifqueue *);
int		 hfsc_enqueue(struct ifqueue *, struct mbuf *);
struct mbuf	*hfsc_dequeue(struct ifqueue *, int);
u_int64_t	 hfsc_microuptime(void);
int		 hfsc_addqueue(struct pf_queuespec *);
int		 hfsc_delqueue(struct pf_queuespec *);
int		 hfsc_qstats(struct pf_queuespec *, void *, int *);

#endif /* _KERNEL */
#endif /* _HFSC_H_ */
