/*	$OpenBSD: pfctl_parser.h,v 1.9 2001/09/06 18:05:46 jasoni Exp $ */

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

#ifndef _PFCTL_PARSER_H_
#define _PFCTL_PARSER_H_

struct pfctl {
	int dev;
	int opts;
	struct pfioc_rule *prule;
	struct pfioc_nat *pnat;
	struct pfioc_binat *pbinat;
	struct pfioc_rdr *prdr;
};

int	 pfctl_add_rule(struct pfctl *, struct pf_rule *);
int	 pfctl_add_nat(struct pfctl *, struct pf_nat *);
int	 pfctl_add_binat(struct pfctl *, struct pf_binat *);
int	 pfctl_add_rdr(struct pfctl *, struct pf_rdr *);

int	 parse_rules(FILE *, struct pfctl *);
int	 parse_nat(FILE *, struct pfctl *);
int	 parse_flags(char *);

void	 print_rule(struct pf_rule *);
void	 print_nat(struct pf_nat *);
void	 print_binat(struct pf_binat *);
void	 print_rdr(struct pf_rdr *);
void	 print_state(struct pf_state *);
void	 print_status(struct pf_status *);

struct icmptypeent {
	char *name;
	u_int8_t type;
};

struct icmpcodeent {
	char *name;
	u_int8_t type;
	u_int8_t code;
};

struct icmptypeent *geticmptypebynumber(u_int8_t);
struct icmptypeent *geticmptypebyname(char *);
struct icmpcodeent *geticmpcodebynumber(u_int8_t, u_int8_t);
struct icmpcodeent *geticmpcodebyname(u_long, char *);

#endif /* _PFCTL_PARSER_H_ */
