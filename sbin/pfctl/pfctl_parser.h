/*	$OpenBSD: pfctl_parser.h,v 1.28 2002/11/23 05:22:24 mcbride Exp $ */

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

#define PF_OPT_DISABLE		0x0001
#define PF_OPT_ENABLE		0x0002
#define PF_OPT_VERBOSE		0x0004
#define PF_OPT_NOACTION		0x0008
#define PF_OPT_QUIET		0x0010
#define PF_OPT_CLRRULECTRS	0x0020
#define PF_OPT_USEDNS		0x0040

#define PF_TH_ALL		0xFF

#define PF_NAT_PROXY_PORT_LOW	50001
#define PF_NAT_PROXY_PORT_HIGH	65535

#define FCNT_NAMES { \
	"searches", \
	"inserts", \
	"removals", \
	NULL \
}

struct pfctl {
	int dev;
	int opts;
	u_int16_t rule_nr;
	struct pfioc_pooladdr paddr;
	struct pfioc_rule *prule;
	struct pfioc_nat *pnat;
	struct pfioc_binat *pbinat;
	struct pfioc_rdr *prdr;
	struct pfioc_altq *paltq;
	struct pfioc_queue *pqueue;
};

int	pfctl_add_rule(struct pfctl *, struct pf_rule *);
int	pfctl_add_nat(struct pfctl *, struct pf_nat *);
int	pfctl_add_binat(struct pfctl *, struct pf_binat *);
int	pfctl_add_rdr(struct pfctl *, struct pf_rdr *);
int	pfctl_add_altq(struct pfctl *, struct pf_altq *);

int	pfctl_set_timeout(struct pfctl *, const char *, int);
int	pfctl_set_optimization(struct pfctl *, const char *);
int	pfctl_set_limit(struct pfctl *, const char *, unsigned int);
int	pfctl_set_logif(struct pfctl *, char *);

int	parse_rules(FILE *, struct pfctl *);
int	parse_flags(char *);

void	print_rule(struct pf_rule *);
void	print_nat(struct pf_nat *);
void	print_binat(struct pf_binat *);
void	print_rdr(struct pf_rdr *);
void	print_status(struct pf_status *);

int	unmask(struct pf_addr *, sa_family_t);

struct icmptypeent {
	char *name;
	u_int8_t type;
};

struct icmpcodeent {
	char *name;
	u_int8_t type;
	u_int8_t code;
};

const struct icmptypeent *geticmptypebynumber(u_int8_t, u_int8_t);
const struct icmptypeent *geticmptypebyname(char *, u_int8_t);
const struct icmpcodeent *geticmpcodebynumber(u_int8_t, u_int8_t, u_int8_t);
const struct icmpcodeent *geticmpcodebyname(u_long, char *, u_int8_t);

struct pf_timeout {
	const char	*name;
	int		 timeout;
};

#define PFCTL_FLAG_ALL		0x01
#define PFCTL_FLAG_FILTER	0x02
#define PFCTL_FLAG_NAT		0x04
#define PFCTL_FLAG_OPTION	0x08
#define PFCTL_FLAG_ALTQ		0x10

extern const struct pf_timeout pf_timeouts[];

#endif /* _PFCTL_PARSER_H_ */
