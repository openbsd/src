/*
 * Public Domain 2003 Dale Rahn
 *
 * $OpenBSD: ab.C,v 1.5 2017/08/07 16:33:52 bluhm Exp $
 */

#include <cstdlib>
#include <iostream>
#include "aa.h"
#include "ab.h"

using namespace std;

extern int a;

BB::BB(const char *str)
{
	if (a == 0) {
		cout << "A not initialized in B constructors " << a << "\n";
		exit(1);
	}
}

BB ab("local");
AA aa("B");
