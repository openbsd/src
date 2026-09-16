/*	$OpenBSD: send.h,v 1.1 2026/09/16 16:11:46 job Exp $ */
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

extern ssize_t sendto_one(struct rtr_socket *, void *);
extern void sendto_allclients(void *);
extern void sendto_allregisteredclients(void *);
extern void sendto_allregisteredclientsversion(void *, uint8_t);

extern ssize_t sendserialnotifyto_one(struct rtr_socket *, uint32_t);
extern ssize_t sendcacheresponseto_one(struct rtr_socket *);
extern ssize_t sendvrp4to_one(struct rtr_socket *, struct vrp4 *, uint8_t);
extern ssize_t sendvrp6to_one(struct rtr_socket *, struct vrp6 *, uint8_t);
extern ssize_t sendbrkto_one(struct rtr_socket *, struct brk *, uint8_t);
extern ssize_t sendvapto_one(struct rtr_socket *, struct vap *, uint8_t);
extern ssize_t sendendofdatato_one(struct rtr_socket *, uint32_t);
extern ssize_t senderrorto_one(struct rtr_socket *, uint16_t, void *, char *,
    ...);
extern ssize_t sendcacheresetto_one(struct rtr_socket *);

extern ssize_t sendstartofstatsto_one(struct rtr_socket *);
extern ssize_t sendglobalstatsto_one(struct rtr_socket *);
extern void sendclientstatsto_one(struct rtr_socket *);
extern void sendcacheframestatsto_one(struct rtr_socket *);
extern ssize_t sendendofstatsto_one(struct rtr_socket *);
