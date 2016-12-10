/*	$OpenBSD: ieee80211_mira.h,v 1.2 2016/12/10 14:46:04 stsp Exp $	*/

/*
 * Copyright (c) 2016 Stefan Sperling <stsp@openbsd.org>
 * Copyright (c) 2016 Theo Buehler <tb@openbsd.org>
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

#ifndef _NET80211_IEEE80211_MIRA_H_
#define _NET80211_IEEE80211_MIRA_H_

/* 
 * MiRA - "MIMO Rate Adaptation in 802.11n Wireless Networks"
 * Ioannis Pefkianakis, Yun Hu, Starsky H.Y. Wong, Hao Yang, Songwu Lu
 * http://www.cs.ucla.edu/wing/publication/papers/Pefkianakis.MOBICOM10.pdf
 */

/* The number of data rates MiRA can choose from. */
#define IEEE80211_MIRA_NUM_RATES 32 /* XXX limited to MCS 0-31 */

/* 
 * Goodput statistics struct. Measures the effective data rate of an MCS
 * index and contains data related to time-based probing to a new rate.
 * All uint64_t numbers in this struct use fixed-point arithmetic.
 */
struct ieee80211_mira_goodput_stats {
	uint64_t measured;	/* Most recently measured goodput. */
	uint64_t average;	/* Average measured goodput. */
	uint64_t average_agg;	/* Average number of subframes per frame. */
	uint64_t stddeviation;	/* Goodput standard deviation. */

	/* These fields are used while calculating probe intervals: */
	uint64_t loss;		/* This rate's loss percentage SFER. */
	uint32_t nprobes;	/* Number of probe attempts. */
	uint32_t nprobe_bytes;	/* Number of bytes sent while probing. */
	int probe_interval;	/* Probe interval for this rate. */
	int probe_timeout_triggered; /* It is time to probe this rate. */
};

/*
 * Rate control state.
 */
struct ieee80211_mira_node {
	/*
	 * Fields set by drivers before calling ieee80211_mira_choose().
	 */
	uint32_t frames;	/* Increment per (sub-)frame transmitted. */
	uint32_t retries;	/* Increment per Tx retry (frame not ACKed). */
	uint32_t txfail;	/* Increment per Tx failure (also not ACKed). */
	uint32_t ampdu_size;	/* Length of last (aggregated) frame sent. */
	uint32_t agglen;	/* Number of subframes in last frame (1-64). */

	/* 
	 * Private fields for use by the rate control algorithm.
	 */

	/* Bitmaps MCS 0-31. */
	uint32_t valid_rates;
	uint32_t candidate_rates;
	uint32_t probed_rates;

	/* Timeouts which trigger time-driven probing. */
	struct timeout probe_to[2];
#define IEEE80211_MIRA_PROBE_TO_INVALID	-1
#define IEEE80211_MIRA_PROBE_TO_UP	0
#define IEEE80211_MIRA_PROBE_TO_DOWN	1
	int probe_timer_expired[2];

	/* Probing state. */
	int probing;
#define IEEE80211_MIRA_NOT_PROBING	0x0
#define IEEE80211_MIRA_PROBING_DOWN	0x1
#define IEEE80211_MIRA_PROBING_UP	0x2
#define IEEE80211_MIRA_PROBING_INTER	0x4 /* combined with UP or DOWN */

	/* The current best MCS found by probing. */
	int best_mcs;

	/* Goodput statistics for each MCS. */
	struct ieee80211_mira_goodput_stats g[IEEE80211_MIRA_NUM_RATES];
};

/* Set up and tear down rate control. */
void	ieee80211_mira_node_init(struct ieee80211_mira_node *);
void	ieee80211_mira_node_destroy(struct ieee80211_mira_node *);

/* Called by drivers from the Tx completion interrupt handler. */
void	ieee80211_mira_choose(struct ieee80211_mira_node *,
	    struct ieee80211com *, struct ieee80211_node *);

#endif /* _NET80211_IEEE80211_MIRA_H_ */
