/* $OpenBSD: doas.h,v 1.8 2016/06/19 19:29:43 martijn Exp $ */
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

char **prepenv(struct rule *);

#define PERMIT	1
#define DENY	2

#define NOPASS		0x1
#define KEEPENV		0x2
