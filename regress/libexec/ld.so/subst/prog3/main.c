/*	$OpenBSD: main.c,v 1.2 2026/09/17 16:53:28 kurt Exp $	*/

/*
 * written by Kurt Miller <kurt@openbsd.org> 2026
 * and placed in the public domain
 */

/*
 * prog3 depends on libyy and finds it via prog3's -rpath $ORIGIN/libyy.
 * libyy depends on libaa and finds it via libyy's -rpath 
 * '$ORIGIN/${OSNAME}/$OSREL/${PLATFORM}'. This tests the shared
 * lib $ORIGIN substitution is relative to the shared lib's location
 * and not the program's location.
 *
 * The files are organized as follows:
 *
 * prog3
 * libyy/
 * libyy/libyy.so.1.0
 * libyy/OpenBSD/`uname -r`/`uname -m`/
 * libyy/OpenBSD/`uname -r`/`uname -m`/libaa.so.1.0
 */

#include "yy.h"

int
main()
{
	return (yy());
}
