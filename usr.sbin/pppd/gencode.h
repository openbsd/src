/*	$OpenBSD: gencode.h,v 1.1 1996/03/25 15:55:41 niklas Exp $	*/
/*	$NetBSD: gencode.h,v 1.1 1996/03/15 03:09:12 paulus Exp $	*/

/*
 * Copyright (c) 1990, 1991, 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * @(#) Header: gencode.h,v 1.20 94/06/12 14:29:30 leres Exp (LBL)
 */

/*
 * filter.h must be included before this file.
 */

/* Address qualifers. */

#define Q_HOST		1
#define Q_NET		2
#define Q_PORT		3
#define Q_PROTO		4

/* Protocol qualifiers. */

#define Q_LINK		1
#define Q_IP		2
#define Q_TCP		3
#define Q_UDP		4
#define Q_ICMP		5

/* Directional qualifers. */

#define Q_SRC		1
#define Q_DST		2
#define Q_OR		3
#define Q_AND		4

#define Q_DEFAULT	0
#define Q_UNDEF		255

struct stmt {
	int code;
	long k;
};

struct slist {
	struct stmt s;
	struct slist *next;
};

/* 
 * A bit vector to represent definition sets.  We assume TOT_REGISTERS
 * is smaller than 8*sizeof(atomset).
 */
typedef unsigned long atomset;
#define ATOMMASK(n) (1 << (n))
#define ATOMELEM(d, n) (d & ATOMMASK(n))

/*
 * An unbounded set.
 */
typedef unsigned long *uset;

/*
 * Total number of atomic entities, including accumulator (A) and index (X).
 * We treat all these guys similarly during flow analysis.
 */
#define N_ATOMS (BPF_MEMWORDS+2)

struct edge {
	int id;
	int code;
	uset edom;
	struct block *succ;
	struct block *pred;
	struct edge *next;	/* link list of incoming edges for a node */
};

struct block {
	int id;
	struct slist *stmts;	/* side effect stmts */
	struct stmt s;		/* branch stmt */
	int mark;
	int level;
	int offset;
	int sense;
	struct edge et;
	struct edge ef;
	struct block *head;
	struct block *link;	/* link field used by optimizer */
	uset dom;
	uset closure;
	struct edge *in_edges;
	atomset def, kill;
	atomset in_use;
	atomset out_use;
	long oval;
	long val[N_ATOMS];
};

struct arth {
	struct block *b;	/* protocol checks */
	struct slist *s;	/* stmt list */
	int regno;		/* virtual register number of result */
};

struct qual {
	unsigned char addr;
	unsigned char proto;
	unsigned char dir;
	unsigned char pad;
};

#ifndef __GNUC__
#define volatile
#endif

struct arth *gen_loadi __P((int));
struct arth *gen_load __P((int, struct arth *, int));
struct arth *gen_loadlen __P((void));
struct arth *gen_neg __P((struct arth *));
struct arth *gen_arth __P((int, struct arth *, struct arth *));

void gen_and __P((struct block *, struct block *));
void gen_or __P((struct block *, struct block *));
void gen_not __P((struct block *));

struct block *gen_scode __P((char *, struct qual));
struct block *gen_ecode __P((unsigned char *, struct qual));
struct block *gen_ncode __P((unsigned long, struct qual));
struct block *gen_proto_abbrev __P((int));
struct block *gen_relation __P((int, struct arth *, struct arth *, int));
struct block *gen_less __P((int));
struct block *gen_greater __P((int));
struct block *gen_byteop __P((int, int, int));
struct block *gen_broadcast __P((int));
struct block *gen_multicast __P((int));
struct block *gen_inbound __P((int));

void bpf_optimize __P((struct block **));
volatile void bpf_error __P((char *, ...));

void finish_parse __P((struct block *));
char *sdup __P((char *));

struct bpf_insn *icode_to_fcode __P((struct block *, int *));
int pcap_parse __P((void));
void lex_init __P((char *));
void sappend __P((struct slist *, struct slist *));

/* XXX */
#define JT(b)  ((b)->et.succ)
#define JF(b)  ((b)->ef.succ)
