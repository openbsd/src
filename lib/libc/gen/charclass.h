/*
 * Public domain, 2008, Todd C. Miller <millert@openbsd.org>
 *
 * $OpenBSD: charclass.h,v 1.2 2019/01/25 00:19:25 millert Exp $
 */

/*
 * POSIX character class support for fnmatch() and glob().
 */
static struct cclass {
	const char *name;
	int (*isctype)(int);
} cclasses[] = {
	{ "alnum",	isalnum },
	{ "alpha",	isalpha },
	{ "blank",	isblank },
	{ "cntrl",	iscntrl },
	{ "digit",	isdigit },
	{ "graph",	isgraph },
	{ "lower",	islower },
	{ "print",	isprint },
	{ "punct",	ispunct },
	{ "space",	isspace },
	{ "upper",	isupper },
	{ "xdigit",	isxdigit },
	{ NULL,		NULL }
};

#define NCCLASSES	(sizeof(cclasses) / sizeof(cclasses[0]) - 1)
