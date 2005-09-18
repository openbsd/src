/*
 * Public Domain 2003 Dale Rahn
 *
 * $OpenBSD: ac.C,v 1.2 2005/09/18 19:58:50 drahn Exp $
 */

#include <iostream>
#include <stdlib.h>
#include "ac.h"

extern int a;

extern "C" {
char *libname = "libac";
};

extern "C" void
lib_entry()
{
	std::cout << "called into ac " << libname << " libname " << "\n";
}

AC::AC(char *str)
{
	_name = str;
}

AC::~AC()
{
}
AC ac("local");
