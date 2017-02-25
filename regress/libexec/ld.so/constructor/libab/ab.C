/*
 * Public Domain 2003 Dale Rahn
 *
 * $OpenBSD: ab.C,v 1.4 2017/02/25 07:28:32 jsg Exp $
 */

#include <cstdlib>
#include <iostream>
#include "aa.h"
#include "ab.h"

using namespace std;

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
