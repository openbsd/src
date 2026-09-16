/*	$OpenBSD: packets.h,v 1.1 2026/09/16 16:11:46 job Exp $ */
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

extern char *error_code_to_str[];
extern char *pdu_type_to_str[];

extern void pdu_hton(struct rtr_socket *, void *);
extern void pdu_ntoh(struct rtr_socket *, void *);
extern void pdu_hton_rtr(void *);
extern void pdu_ntoh_rtr(void *);
extern void pdu_hton_controller(void *);
extern void pdu_ntoh_controller(void *);
extern int check_pdu_ipv4_prefix_controller(struct pdu_ipv4_prefix_import *);
extern int check_pdu_ipv6_prefix_controller(struct pdu_ipv6_prefix_import *);
extern int check_pdu_router_key_controller(struct pdu_router_key_import *);
extern int check_pdu_aspa_controller(struct pdu_aspa_import *);
extern int check_error_code(uint16_t, uint8_t);
