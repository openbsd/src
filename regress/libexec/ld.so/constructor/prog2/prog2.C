/*
 * Public Domain 2003 Dale Rahn
 *
 * $OpenBSD: prog2.C,v 1.2 2016/09/27 06:52:50 kettenis Exp $
 */
#include <iostream>
#include "ab.h"

using namespace std;

BB BBmain("main");

int a;
int
main()
{
	cout << "main\n";
	return 0;
}
