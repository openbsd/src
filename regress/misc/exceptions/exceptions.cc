/*	$OpenBSD: exceptions.cc,v 1.1 2002/12/04 05:02:15 art Exp $	*/
/*
 *	Written by Artur Grabowski <art@openbsd.org> 2002 Public Domain
 */

#include <cstring>

int
main()
{
	try {
		throw("foo");
        }
	catch(const char *p) {
		if (!strcmp(p, "foo"))
			return (0);
	}
	return (1);
}
