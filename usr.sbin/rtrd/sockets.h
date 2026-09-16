/*	$OpenBSD: sockets.h,v 1.1 2026/09/16 16:11:46 job Exp $ */
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

extern int listener;
extern int controller;
extern char *controller_filename;
extern int client_count;
extern int max_clients;
extern int max_sockets;
extern int max_sendq;

extern struct rtr_socket *rtr_socket_table;

extern struct pollfd *poll_table;
extern int poll_table_count;

extern uint8_t rtr_max_version;

extern ssize_t sendq_block_size;

extern struct rtr_socket *fd_to_socket(int);
extern int is_listener(int);
extern struct pollfd *poll_find(int);
extern int poll_index(int);
extern struct pollfd *poll_add(int, short);
extern void poll_remove(int);
extern int poll_isset_events(int, short);
extern int poll_isset_revents(int, short);
extern void init_socket(int, int, uint32_t, ssize_t, ssize_t, ssize_t, uint8_t);
extern int init_socket_table(FILE *, char *, uint16_t);
extern int rtr_errno_ignore(int);
extern ssize_t rtr_flush_write(struct rtr_socket *);
extern void rtr_flushall_write(void);
extern int rtr_sendq_add(struct rtr_socket *, void *, int);
extern void rtr_sendq_pop(struct rtr_socket *, int);
extern void rtr_sendq_popall(struct rtr_socket *);
extern int rtr_sendq_flush(struct rtr_socket *);
extern ssize_t writeto(struct rtr_socket *, void *);
extern void rtr_close(struct rtr_socket *);
extern void rtr_flushall_closed(void);
extern void rtr_shutdown(int);
extern void core_loop(void);
