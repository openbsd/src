/*
 * Public Domain 2003 Dale Rahn
 *
 * $OpenBSD: ab.C,v 1.2 2003/02/18 13:14:42 jmc Exp $
 */

#include "iostream.h"
#include "aa.h"
#include "ab.h"

extern int a;

BB::BB(char *str)
{
	if (a == 0) {
		cout << "A not initialized in B constructors " << a << "\n";
		exit(1);
	}
}

BB ab("local");
AA aa("B");
