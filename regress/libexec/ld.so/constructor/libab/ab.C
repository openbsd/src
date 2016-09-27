/*
 * Public Domain 2003 Dale Rahn
 *
 * $OpenBSD: ab.C,v 1.3 2016/09/27 06:52:50 kettenis Exp $
 */

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
