/*	$OpenBSD: pfctl.h,v 1.1 2003/01/04 00:01:34 deraadt Exp $ */

/*
 * Copyright (c) 2001 Daniel Hartmeier
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *    - Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    - Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef _PFCTL_H_
#define _PFCTL_H_

void	 pfr_set_fd(int);
int	 pfr_get_fd(void);
int	 pfr_clr_tables(int *ndel, int);
int	 pfr_add_tables(struct pfr_table *, int, int *, int);
int	 pfr_del_tables(struct pfr_table *, int, int *, int);
int	 pfr_get_tables(struct pfr_table *, int *, int);
int	 pfr_get_tstats(struct pfr_tstats *, int *, int);
int	 pfr_clr_tstats(struct pfr_table *, int, int *, int);
int	 pfr_clr_addrs(struct pfr_table *, int *, int);
int	 pfr_add_addrs(struct pfr_table *, struct pfr_addr *, int, int *, int);
int	 pfr_del_addrs(struct pfr_table *, struct pfr_addr *, int, int *, int);
int	 pfr_set_addrs(struct pfr_table *, struct pfr_addr *, int, int *,
	     int *, int *, int *, int);
int	 pfr_get_addrs(struct pfr_table *, struct pfr_addr *, int *, int);
int	 pfr_get_astats(struct pfr_table *, struct pfr_astats *, int *, int);
int	 pfr_clr_astats(struct pfr_table *, struct pfr_addr *, int, int *, int);
int	 pfr_tst_addrs(struct pfr_table *, struct pfr_addr *, int, int *, int);
int	 pfr_wrap_table(struct pfr_table *, struct pf_addr_wrap *, int *, int);
int	 pfr_unwrap_table(struct pfr_table *, struct pf_addr_wrap *, int);
int	 pfctl_clear_tables(int);
int	 pfctl_show_tables(int);
int	 pfctl_command_tables(int, char *[], char *, char *, char *, int);

#ifndef DEFAULT_PRIORITY
#define DEFAULT_PRIORITY	1
#define DEFAULT_QLIMIT		50
#endif

/*
 * generalized service curve used for admission control
 */
struct segment {
	LIST_ENTRY(segment)	_next;
	double			x, y, d, m;
};

struct pf_altq_node {
	struct pf_altq		 altq;
	struct pf_altq_node	*next;
	struct pf_altq_node	*children;
};

void			 pfctl_insert_altq_node(struct pf_altq_node **,
			    const struct pf_altq);
struct pf_altq_node	*pfctl_find_altq_node(struct pf_altq_node *,
			    const char *, const char *);
void			 pfctl_print_altq_node(const struct pf_altq_node *,
			    unsigned);
void			 pfctl_free_altq_node(struct pf_altq_node *);

int		 check_commit_altq(int, int);
void		 pfaltq_store(struct pf_altq *);
void		 pfaltq_free(struct pf_altq *);
struct pf_altq	*pfaltq_lookup(const char *);
struct pf_altq	*qname_to_pfaltq(const char *, const char *);
u_int32_t	 qname_to_qid(const char *, const char *);
char		*qid_to_qname(u_int32_t, const char *);

void	 print_altq(const struct pf_altq *, unsigned);
void	 print_queue(const struct pf_altq *, unsigned);

void	print_addr(struct pf_addr_wrap *, sa_family_t);
void	print_host(struct pf_state_host *, sa_family_t, int);
void	print_seq(struct pf_state_peer *);
void	print_state(struct pf_state *s, int);
int	unmask(struct pf_addr *, sa_family_t);

#endif /* _PFCTL_H_ */
