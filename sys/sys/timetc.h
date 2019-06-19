/*	$OpenBSD: timetc.h,v 1.9 2019/05/22 19:59:37 cheloha Exp $ */

/*
 * Copyright (c) 2000 Poul-Henning Kamp <phk@FreeBSD.org>
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

/*
 * If we meet some day, and you think this stuff is worth it, you
 * can buy me a beer in return. Poul-Henning Kamp
 */

#ifndef _SYS_TIMETC_H_
#define	_SYS_TIMETC_H_

#ifndef _KERNEL
#error "no user-serviceable parts inside"
#endif

#include <sys/queue.h>

/*-
 * `struct timecounter' is the interface between the hardware which implements
 * a timecounter and the MI code which uses this to keep track of time.
 *
 * A timecounter is a binary counter which has two properties:
 *    * it runs at a fixed, known frequency.
 *    * it has sufficient bits to not roll over in less than approximately
 *      max(2 msec, 2/HZ seconds).  (The value 2 here is really 1 + delta,
 *      for some indeterminate value of delta.)
 */

struct timecounter;
typedef u_int timecounter_get_t(struct timecounter *);
typedef void timecounter_pps_t(struct timecounter *);

/*
 * Locks used to protect struct members in this file:
 *	I	immutable after initialization
 *	t	tc_lock
 *	w	windup_mtx
 */

struct timecounter {
	timecounter_get_t	*tc_get_timecount;	/* [I] */
		/*
		 * This function reads the counter.  It is not required to
		 * mask any unimplemented bits out, as long as they are
		 * constant.
		 */
	timecounter_pps_t	*tc_poll_pps;		/* [I] */
		/*
		 * This function is optional.  It will be called whenever the
		 * timecounter is rewound, and is intended to check for PPS
		 * events.  Normal hardware does not need it but timecounters
		 * which latch PPS in hardware (like sys/pci/xrpu.c) do.
		 */
	u_int 			tc_counter_mask;	/* [I] */
		/* This mask should mask off any unimplemented bits. */
	u_int64_t		tc_frequency;		/* [I] */
		/* Frequency of the counter in Hz. */
	char			*tc_name;		/* [I] */
		/* Name of the timecounter. */
	int			tc_quality;		/* [I] */
		/*
		 * Used to determine if this timecounter is better than
		 * another timecounter higher means better.  Negative
		 * means "only use at explicit request".
		 */
	void			*tc_priv;		/* [I] */
		/* Pointer to the timecounter's private parts. */
	SLIST_ENTRY(timecounter) tc_next;		/* [I] */
		/* Pointer to the next timecounter. */
	int64_t			tc_freq_adj;		/* [tw] */
		/* Current frequency adjustment. */
};

struct rwlock;
extern struct rwlock tc_lock;

extern struct timecounter *timecounter;

u_int64_t tc_getfrequency(void);
void	tc_init(struct timecounter *tc);
void	tc_setclock(const struct timespec *ts);
void	tc_setrealtimeclock(const struct timespec *ts);
void	tc_ticktock(void);
void	inittimecounter(void);
int	sysctl_tc(int *, u_int, void *, size_t *, void *, size_t);
void	tc_adjfreq(int64_t *, int64_t *);
void	tc_adjtime(int64_t *, int64_t *);

#endif /* !_SYS_TIMETC_H_ */
