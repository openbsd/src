/*
 * Public Domain 2003 Dale Rahn
 *
 * $OpenBSD: ab.C,v 1.2 2005/09/13 21:03:46 drahn Exp $
 */

#include <iostream>
#include <stdlib.h>
#include "ab.h"

extern int a;

extern "C" {
char *libname = "libab";
};

extern "C" void
lib_entry()
{
	std::cout << "called into ab " << libname << " libname " << "\n";
}

BB::BB(char *str)
{
	_name = str;
}

BB::~BB()
{
	std::cout << "dtors BB " << _name << "\n";
}
BB ab("local");
