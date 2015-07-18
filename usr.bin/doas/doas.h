/* $OpenBSD: doas.h,v 1.2 2015/07/18 07:49:16 bcallah Exp $ */

struct rule {
	int action;
	int options;
	const char *ident;
	const char *target;
	const char *cmd;
	const char **envlist;
};

extern struct rule **rules;
extern int nrules, maxrules;

size_t arraylen(const char **);

#define PERMIT	1
#define DENY	2

#define NOPASS		0x1
#define KEEPENV		0x2
