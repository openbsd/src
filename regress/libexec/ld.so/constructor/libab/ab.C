/*
 * Public Domain 2003 Dale Rahn
 *
 * $OpenBSD: ab.C,v 1.1 2003/02/01 19:56:17 drahn Exp $
 */

#include "iostream.h"
#include "aa.h"
#include "ab.h"

extern int a;

BB::BB(char *str)
{
	if (a == 0) {
		cout << "A not intialized in B constructors " << a << "\n";
		exit(1);
	}
}

BB ab("local");
AA aa("B");
