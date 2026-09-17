/*	$OpenBSD: main.c,v 1.1 2026/09/17 17:00:04 kurt Exp $	*/

/*
 * written by Kurt Miller <kurt@openbsd.org> 2026
 * and placed in the public domain
 */

/*
 * prog4 dlopen's libyy and finds it via LD_LIBRARY_PATH.
 * libyy depends on libaa and finds it via libyy's -rpath
 * '$ORIGIN/${OSNAME}/$OSREL/${PLATFORM}'. This tests a
 * dlopen'ed shared lib $ORIGIN substitution is relative
 * to the its location and not the program's location. It
 * Also tests that a program not using $ORIGIN can dlopen()
 * a shared lib that does depend on it.
 *
 * The files are organized as follows:
 *
 * prog4
 * libyy/
 * libyy/libyy.so.1.0
 * libyy/OpenBSD/`uname -r`/`uname -m`/
 * libyy/OpenBSD/`uname -r`/`uname -m`/libaa.so.1.0
 */

#include <dlfcn.h>
#include <err.h>
#include <stdio.h>

#define LIBYY "libyy.so"

int
main()
{
	void *libyy;
	int (*yy)();

	libyy = dlopen(LIBYY, RTLD_LAZY);
	if (libyy == NULL)
		errx(1, "dlopen(%s, RTLD_LAZY) FAILED", LIBYY);

       	yy = (int (*)())dlsym(libyy, "yy");
	if (yy == NULL)
		errx(1, "dlsym(libyy, \"yy\") FAILED");

	return ((*yy)());
}
