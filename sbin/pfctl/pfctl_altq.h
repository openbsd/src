/*	$OpenBSD: pfctl_altq.h,v 1.1 2002/11/18 22:49:15 henning Exp $	*/
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

/*
 * misc defines needed by pfctl(8) for altq
 * (copied from altq headers until we find a better way...)
 */
#ifndef ALTQT_NONE
/* altq discipline type */
#define	ALTQT_NONE		0	/* reserved */
#define	ALTQT_CBQ		1	/* cbq */
#define	ALTQT_WFQ		2	/* wfq */
#define	ALTQT_AFMAP		3	/* afmap */
#define	ALTQT_FIFOQ		4	/* fifoq */
#define	ALTQT_RED		5	/* red */
#define	ALTQT_RIO		6	/* rio */
#define	ALTQT_LOCALQ		7	/* local use */
#define	ALTQT_HFSC		8	/* hfsc */
#define	ALTQT_CDNR		9	/* traffic conditioner */
#define	ALTQT_BLUE		10	/* blue */
#define	ALTQT_PRIQ		11	/* priority queue */
#define	ALTQT_MAX		12	/* should be max discipline type + 1 */
#endif

#ifndef CBQCLF_RED
/* class flags shoud be same as class flags in rm_class.h */
#define	CBQCLF_RED		0x0001	/* use RED */
#define	CBQCLF_ECN		0x0002  /* use RED/ECN */
#define	CBQCLF_RIO		0x0004  /* use RIO */
#define	CBQCLF_FLOWVALVE	0x0008	/* use flowvalve (aka penalty-box) */
#define	CBQCLF_CLEARDSCP	0x0010  /* clear diffserv codepoint */
#define	CBQCLF_BORROW		0x0020  /* borrow from parent */

/* class flags only for root class */
#define	CBQCLF_WRR		0x0100	/* weighted-round robin */
#define	CBQCLF_EFFICIENT	0x0200  /* work-conserving */

/* class flags for special classes */
#define	CBQCLF_ROOTCLASS	0x1000	/* root class */
#define	CBQCLF_DEFCLASS		0x2000	/* default class */
#define	CBQCLF_CTLCLASS		0x4000	/* control class */
#define	CBQCLF_CLASSMASK	0xf000	/* class mask */
#endif

#ifndef REDF_ECN4
/* red flags */
#define	REDF_ECN4	0x01	/* use packet marking for IPv4 packets */
#define	REDF_ECN6	0x02	/* use packet marking for IPv6 packets */
#define	REDF_ECN	(REDF_ECN4 | REDF_ECN6)
#define	REDF_FLOWVALVE	0x04	/* use flowvalve (aka penalty-box) */
#endif

struct pf_altq_node {
	struct pf_altq		 altq;
	struct pf_altq_node	*next;
	struct pf_altq_node	*children;
};

void			 pfctl_insert_altq_node(struct pf_altq_node **,
			    const struct pf_altq);
struct pf_altq_node	*pfctl_find_altq_node(struct pf_altq_node *,
			    const char *);
void			 pfctl_print_altq_node(const struct pf_altq_node *,
			    unsigned);
void			 pfctl_free_altq_node(struct pf_altq_node *);

int check_commit_altq(int, int);
void pfaltq_store(struct pf_altq *);
void pfaltq_free(struct pf_altq *);
struct pf_altq *pfaltq_lookup(const char *);
struct pf_altq *qname_to_pfaltq(const char *, const char *);
u_int32_t qname_to_qid(const char *, const char *);
char *qid_to_qname(u_int32_t, const char *);

void print_altq(const struct pf_altq *, unsigned);
void print_queue(const struct pf_altq *, unsigned);

int eval_pfaltq(struct pfctl *, struct pf_altq *);
int eval_pfqueue(struct pfctl *, struct pf_altq *, u_int32_t, u_int16_t);
