/* $OpenBSD: doas.h,v 1.7 2016/06/16 17:40:30 tedu Exp $ */

#include <sys/tree.h>

struct envnode {
	RB_ENTRY(envnode) node;
	const char *key;
	const char *value;
};

struct env {
	RB_HEAD(envtree, envnode) root;
	u_int count;
};

RB_PROTOTYPE(envtree, envnode, node, envcmp)

struct rule {
	int action;
	int options;
	const char *ident;
	const char *target;
	const char *cmd;
	const char **cmdargs;
	const char **envlist;
};

extern struct rule **rules;
extern int nrules, maxrules;
extern int parse_errors;

size_t arraylen(const char **);

struct env *createenv(char **);
struct env *filterenv(struct env *, struct rule *);
char **flattenenv(struct env *);

#define PERMIT	1
#define DENY	2

#define NOPASS		0x1
#define KEEPENV		0x2
