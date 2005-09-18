/*
 * Public Domain 2003 Dale Rahn
 *
 * $OpenBSD: aa.C,v 1.3 2005/09/18 19:58:49 drahn Exp $
 */

#include <iostream>
#include "aa.h"
char strbuf[512];

extern "C" { 
char *libname = "libaa";
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

AA::AA(char *arg)
{
	_name = arg;
}
AA::~AA()
{
}

AA foo("A");
