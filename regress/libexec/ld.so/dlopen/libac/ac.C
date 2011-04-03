/*
 * Public Domain 2003 Dale Rahn
 *
 * $OpenBSD: ac.C,v 1.3 2011/04/03 22:29:50 drahn Exp $
 */

#include <iostream>
#include <stdlib.h>
#include "ac.h"

extern int a;

extern "C" {
const char *libname = "libac";
};

extern "C" void
lib_entry()
{
	std::cout << "called into ac " << libname << " libname " << "\n";
}

AC::AC(const char *str)
{
	_name = str;
}

AC::~AC()
{
}
AC ac("local");
