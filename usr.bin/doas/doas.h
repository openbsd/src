/* $OpenBSD: doas.h,v 1.5 2016/06/05 00:46:34 djm Exp $ */

struct rule {
	int action;
	int options;
	const char *ident;
	const char *target;
	const char *cmd;
	const char **cmdargs;
	const char **envlist;
	const char **setenvlist;
};

extern struct rule **rules;
extern int nrules, maxrules;
extern int parse_errors;

size_t arraylen(const char **);

#define PERMIT	1
#define DENY	2

#define NOPASS		0x1
#define KEEPENV		0x2
#define SETENV		0x4
