/*
 * Public Domain 2003 Dale Rahn
 *
 * $OpenBSD: aa.C,v 1.5 2011/04/03 22:29:50 drahn Exp $
 */

#include <iostream>
#include <string.h>
#include "aa.h"
char strbuf[512];

extern "C" { 
const char *libname = "libaa";
};

extern "C" char *
lib_entry()
{
	strlcpy(strbuf, libname, sizeof strbuf);
	strlcat(strbuf, ":", sizeof strbuf);
	strlcat(strbuf, "aa", sizeof strbuf);
	return strbuf;
	std::cout << "called into aa " << libname << " libname " << "\n";
}

AA::AA(const char *arg)
{
	_name = arg;
}
AA::~AA()
{
}

AA foo("A");
