/*	$OpenBSD: systrace.h,v 1.9 2002/07/14 22:34:55 provos Exp $	*/
/*
 * Copyright 2002 Niels Provos <provos@citi.umich.edu>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Niels Provos.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _SYSTRACE_H_
#define _SYSTRACE_H_
#include <sys/queue.h>

#define _PATH_XSYSTRACE	"/usr/X11R6/bin/xsystrace"

enum logicop { LOGIC_AND, LOGIC_OR, LOGIC_NOT, LOGIC_SINGLE };

struct logic {
	enum logicop op;
	struct logic *left;
	struct logic *right;
	char *type;
	int typeoff;
	void *filterdata;
	size_t filterlen;
	int (*filter_match)(struct intercept_translate *, struct logic *);
};

struct filter {
	TAILQ_ENTRY(filter) next;
	TAILQ_ENTRY(filter) policy_next;

	char *rule;
	char name[32];
	char emulation[16];
	struct logic *logicroot;
	short match_action;
	int match_error;
	int match_flags;
	int match_count;	/* Number of times this filter matched */
};

TAILQ_HEAD(filterq, filter);

struct policy_syscall {
	SPLAY_ENTRY(policy_syscall) node;

	char name[64];
	char emulation[16];

	struct filterq flq;
};

struct policy {
	SPLAY_ENTRY(policy) node;
	SPLAY_ENTRY(policy) nrnode;

	char *name;
	char emulation[16];

	SPLAY_HEAD(syscalltree, policy_syscall) pflqs;

	int policynr;
	int flags;

	struct filterq filters;
	int nfilters;
	struct filterq prefilters;
};

#define POLICY_PATH		"/etc/systrace"

#define POLICY_UNSUPERVISED	0x01	/* Auto-Pilot */
#define POLICY_DETACHED		0x02	/* Ignore this program */
#define POLICY_CHANGED		0x04

#define PROCESS_INHERIT_POLICY	0x01	/* Process inherits policy */

int systrace_initpolicy(char *);
void systrace_initcb(void);
struct policy *systrace_newpolicy(char *, char *);
int systrace_newpolicynr(int, struct policy *);
int systrace_modifypolicy(int, int, char *, short);
struct policy *systrace_findpolicy(char *);
struct policy *systrace_findpolnr(int);
int systrace_dumppolicy(void);
int systrace_readpolicy(char *);
int systrace_addpolicy(char *);
struct filterq *systrace_policyflq(struct policy *, char *, char *);

int systrace_error_translate(char *);

#define SYSTRACE_MAXALIAS	5

struct systrace_alias {
	SPLAY_ENTRY(systrace_alias) node;
	TAILQ_ENTRY(systrace_alias) next;

	char name[64];
	char emulation[16];

	char aname[64];
	char aemul[16];

	struct intercept_translate *arguments[SYSTRACE_MAXALIAS];
	int nargs;

	struct systrace_revalias *reverse;
};

int systrace_initalias(void);
struct systrace_alias *systrace_new_alias(char *, char *, char *, char *);
void systrace_switch_alias(char *, char *, char *, char *);
struct systrace_alias *systrace_find_alias(char *, char *);
void systrace_alias_add_trans(struct systrace_alias *,
    struct intercept_translate *);

struct systrace_revalias {
	SPLAY_ENTRY(systrace_revalias) node;

	char name[64];
	char emulation[16];

	TAILQ_HEAD(revaliasq, systrace_alias) revl;
};

struct systrace_revalias *systrace_reverse(char *, char *);
struct systrace_revalias *systrace_find_reverse(char *, char *);

short filter_evaluate(struct intercept_tlq *, struct filterq *, int *);
short filter_ask(struct intercept_tlq *, struct filterq *, int, char *,
    char *, char *, short *, int *);
void filter_free(struct filter *);
void filter_modifypolicy(int, int, char *, char *, short);

int filter_parse_simple(char *, short *, short *);
int filter_parse(char *, struct filter **);
int filter_prepolicy(int, struct policy *);
char *filter_expand(char *data);

int parse_filter(char *, struct filter **);

char *uid_to_name(uid_t);

char *strrpl(char *, size_t, char *, char *);

extern struct intercept_translate oflags;
extern struct intercept_translate modeflags;
extern struct intercept_translate fdt;
extern struct intercept_translate uidt;
extern struct intercept_translate uname;
extern struct intercept_translate gidt;
extern struct intercept_translate argv;

extern struct intercept_translate linux_oflags;
#endif /* _SYSTRACE_H_ */
