/*	$OpenBSD: ifq.h,v 1.1 2015/12/08 10:06:12 dlg Exp $ */

/*
 * Copyright (c) 2015 David Gwynne <dlg@openbsd.org>
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

#ifndef _NET_IFQ_H_
#define _NET_IFQ_H_

struct ifnet;

struct ifq_ops;

struct ifqueue {
	struct mutex		 ifq_mtx;
	uint64_t		 ifq_drops;
	const struct ifq_ops	*ifq_ops;
	void			*ifq_q;
	unsigned int		 ifq_len;
	unsigned int		 ifq_serializer;
	unsigned int		 ifq_oactive;

	unsigned int		 ifq_maxlen;
};

#ifdef _KERNEL

#define IFQ_MAXLEN		256

struct ifq_ops {
	void			*(*ifqop_alloc)(void *);
	void			 (*ifqop_free)(void *);
	int			 (*ifqop_enq)(struct ifqueue *, struct mbuf *);
	struct mbuf 		*(*ifqop_deq_begin)(struct ifqueue *, void **);
	void			 (*ifqop_deq_commit)(struct ifqueue *,
				    struct mbuf *, void *);
	void	 		 (*ifqop_purge)(struct ifqueue *,
				    struct mbuf_list *);
};

/*
 * Interface send queues.
 */

void		 ifq_init(struct ifqueue *);
void		 ifq_attach(struct ifqueue *, const struct ifq_ops *, void *);
void		 ifq_destroy(struct ifqueue *);
int		 ifq_enqueue_try(struct ifqueue *, struct mbuf *);
int		 ifq_enqueue(struct ifqueue *, struct mbuf *);
struct mbuf	*ifq_deq_begin(struct ifqueue *);
void		 ifq_deq_commit(struct ifqueue *, struct mbuf *);
void		 ifq_deq_rollback(struct ifqueue *, struct mbuf *);
struct mbuf	*ifq_dequeue(struct ifqueue *);
unsigned int	 ifq_purge(struct ifqueue *);
void		*ifq_q_enter(struct ifqueue *, const struct ifq_ops *);
void		 ifq_q_leave(struct ifqueue *, void *);

#define	ifq_len(_ifq)			((_ifq)->ifq_len)
#define	ifq_empty(_ifq)			(ifq_len(_ifq) == 0)
#define	ifq_set_maxlen(_ifq, _l)	((_ifq)->ifq_maxlen = (_l))

static inline void
ifq_set_oactive(struct ifqueue *ifq)
{
	ifq->ifq_oactive = 1;
}

static inline void
ifq_clr_oactive(struct ifqueue *ifq)
{
	ifq->ifq_oactive = 0;
}

static inline unsigned int
ifq_is_oactive(struct ifqueue *ifq)
{
	return (ifq->ifq_oactive);
}

extern const struct ifq_ops * const ifq_priq_ops;

#endif /* _KERNEL */

#endif /* _NET_IFQ_H_ */
