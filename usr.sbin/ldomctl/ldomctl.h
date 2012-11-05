/*	$OpenBSD: ldomctl.h,v 1.1 2012/11/05 19:50:54 kettenis Exp $	*/

/*
 * Copyright (c) 2012 Mark Kettenis
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

struct guest {
	const char *name;
	uint64_t gid;
	uint64_t mdpa;

	int num_cpus;

	TAILQ_ENTRY(guest) link;
};

extern TAILQ_HEAD(guest_head, guest) guests;

void add_guest(struct md_node *);

extern struct md *hvmd;
extern uint64_t hv_mdpa;
