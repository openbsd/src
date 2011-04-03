/*
 * Public Domain 2003 Dale Rahn
 *
 * $OpenBSD: ab.C,v 1.5 2011/04/03 22:29:50 drahn Exp $
 */

#include <iostream>
#include <stdlib.h>
#include <string.h>
#include "ab.h"

char strbuf[512];

extern "C" {
const char *libname = "libab";
};

extern "C" char *
lib_entry()
{
	strlcpy(strbuf, libname, sizeof strbuf);
	strlcat(strbuf, ":", sizeof strbuf);
	strlcat(strbuf, "ab", sizeof strbuf);
	return strbuf;
	std::cout << "called into ab " << libname << " libname " << "\n";
}

BB::BB(const char *str)
{
	_name = str;
}

BB::~BB()
{
}
BB ab("local");
