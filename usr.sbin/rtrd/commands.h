/*	$OpenBSD: commands.h,v 1.1 2026/09/16 16:11:46 job Exp $ */
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

extern int m_serial_notify(struct rtr_socket *, struct pdu_header *);
extern int m_serial_query(struct rtr_socket *, struct pdu_header *);
extern int m_reset_query(struct rtr_socket *, struct pdu_header *);
extern int m_cache_response(struct rtr_socket *, struct pdu_header *);
extern int m_ipv4_prefix(struct rtr_socket *, struct pdu_header *);
extern int m_reserved(struct rtr_socket *, struct pdu_header *);
extern int m_ipv6_prefix(struct rtr_socket *, struct pdu_header *);
extern int m_end_of_data(struct rtr_socket *, struct pdu_header *);
extern int m_cache_reset(struct rtr_socket *, struct pdu_header *);
extern int m_router_key(struct rtr_socket *, struct pdu_header *);
extern int m_error(struct rtr_socket *, struct pdu_header *);
extern int m_aspa_pdu(struct rtr_socket *, struct pdu_header *);

extern int m_open_controller(struct rtr_socket *, struct pdu_header *);
extern int m_close_controller(struct rtr_socket *, struct pdu_header *);
extern int m_start_of_import(struct rtr_socket *, struct pdu_header *);
extern int m_ipv4_prefix_import(struct rtr_socket *, struct pdu_header *);
extern int m_ipv6_prefix_import(struct rtr_socket *, struct pdu_header *);
extern int m_router_key_import(struct rtr_socket *, struct pdu_header *);
extern int m_aspa_pdu_import(struct rtr_socket *, struct pdu_header *);
extern int m_end_of_import(struct rtr_socket *, struct pdu_header *);
extern int m_push_import(struct rtr_socket *, struct pdu_header *);
extern int m_query_stats(struct rtr_socket *, struct pdu_header *);
extern int m_start_of_stats(struct rtr_socket *, struct pdu_header *);
extern int m_global_stats(struct rtr_socket *, struct pdu_header *);
extern int m_client_stats(struct rtr_socket *, struct pdu_header *);
extern int m_cache_frame_stats(struct rtr_socket *, struct pdu_header *);
extern int m_end_of_stats(struct rtr_socket *, struct pdu_header *);

extern int (*command_lookup(uint8_t, uint8_t, int))
    (struct rtr_socket *, struct pdu_header *);
