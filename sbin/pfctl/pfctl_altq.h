/*	$OpenBSD: pfctl_altq.h,v 1.7 2002/12/17 11:29:04 henning Exp $	*/

/*
 * Copyright (C) 2002
 *	Sony Computer Science Laboratories Inc.  All rights reserved.
 * Copyright (C) 2002 Henning Brauer. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY SONY CSL AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL SONY CSL OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

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

int	 eval_pfaltq(struct pfctl *, struct pf_altq *, u_int32_t, u_int16_t);
int	 eval_pfqueue(struct pfctl *, struct pf_altq *, u_int32_t, u_int16_t);
