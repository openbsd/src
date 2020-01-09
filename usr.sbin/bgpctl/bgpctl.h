/*
 * Copyright (c) 2019 Claudio Jeker <claudio@openbsd.org>
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

struct parse_result;
void	show_head(struct parse_result *);
void	show_neighbor(struct peer *, struct parse_result *);
void	show_timer(struct ctl_timer *);
void	show_fib(struct kroute_full *);
void	show_fib_table(struct ktable *);
void	show_nexthop(struct ctl_show_nexthop *);
void	show_interface(struct ctl_show_interface *);
void	show_rib(struct ctl_show_rib *, u_char *, size_t,
	    struct parse_result *);
void	show_rib_hash(struct rde_hashstats *);
void	show_rib_mem(struct rde_memstats *);
void	show_result(u_int);


#define EOL0(flag)	((flag & F_CTL_SSV) ? ';' : '\n')

void		 print_prefix(struct bgpd_addr *, u_int8_t, u_int8_t, u_int8_t);
void		 print_neighbor_capa_mp(struct peer *);
void		 print_neighbor_capa_restart(struct peer *);
void		 print_neighbor_msgstats(struct peer *);
void		 print_flags(u_int8_t, int);
void		 show_fib_flags(u_int16_t);

const char	*print_ovs(u_int8_t, int);
const char	*print_origin(u_int8_t, int);
const char	*print_auth_method(enum auth_method);
const char	*fmt_mem(long long);

const char	*fmt_timeframe(time_t);
const char	*fmt_monotime(time_t);
char		*fmt_peer(const char *, const struct bgpd_addr *, int);
const char	*get_errstr(u_int8_t, u_int8_t);

