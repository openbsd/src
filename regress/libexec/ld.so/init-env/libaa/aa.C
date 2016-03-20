/*
 * Public Domain 2016 Philip Guenther <guenther@openbsd.org>
 *
 * $OpenBSD: aa.C,v 1.1 2016/03/20 05:13:22 guenther Exp $
 */

#include <iostream>
#include <cstdlib>

extern char *__progname;

class AA {
public:
	AA(const char *);
};

AA::AA(const char *arg)
{
	int fail = 0;

	if (getenv("PATH") != NULL)
		std::cout << "OK: PATH is set\n";
	else {
		std::cout << "FAILED: PATH not set\n";
		fail = 1;
	}
	if (__progname != NULL && __progname[0] != '\0')
		std::cout << "OK: __progname is set\n";
	else {
		std::cout << "FAILED: __progname not set\n";
		fail = 1;
	}
	setenv(arg, "foo", 1);
//	if (fail)
//		exit(1);
}

AA foo("INIT_ENV_REGRESS_TEST");

