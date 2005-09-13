/*
 * Public Domain 2003 Dale Rahn
 *
 * $OpenBSD: aa.C,v 1.2 2005/09/13 21:03:46 drahn Exp $
 */

#include <iostream>
#include "aa.h"
int a;

extern "C" { 
char *libname = "libaa";
};

extern "C" void
lib_entry()
{
	std::cout << "called into aa " << libname << " libname " << "\n";
}

AA::AA(char *arg)
{
	a = 1;
	_name = arg;
}
AA::~AA()
{
	std::cout << "dtors AA " << libname << " " << _name << "\n";
}

AA foo("A");
