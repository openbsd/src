/*	$OpenBSD: sched.h,v 1.1 2026/09/16 16:11:46 job Exp $ */
/*
 * Copyright (c) 2025-2026 Ralph Covelli <rcovelli@he.net>
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

extern struct timeval now;

extern void rtr_gettime(void);
extern RB_PROTOTYPE(sched_tree, sched, entry, schedcmp)
extern int insert_sched(struct sched_tree *, struct sched *);
extern int remove_sched(struct sched_tree *, struct sched *);
extern void remove_sched_type(struct sched_tree *, uint32_t);
extern void remove_sched_socket(struct sched_tree *, struct rtr_socket *);
extern void remove_sched_type_and_socket(
    struct sched_tree *,
    uint32_t,
    struct rtr_socket *);
extern void free_sched_tree(struct sched_tree *);
extern void sched_init(void);
extern int time_until_next_event(struct timeval);
extern int schedule_cleanup(time_t);
extern int schedule_handshake_timeout(time_t, struct rtr_socket *);
extern int schedule_idle_timeout(time_t, struct rtr_socket *);
extern void process_cleanup(time_t);
extern void process_handshake_timeout(struct rtr_socket *);
extern void process_idle_timeout(struct rtr_socket *);
extern void process_scheduler_events(time_t);

extern struct sched_tree scheduler;
