/*
 * Public Domain 2003 Dale Rahn
 *
 * $OpenBSD: aa.C,v 1.1 2003/02/01 19:56:17 drahn Exp $
 */

#include "iostream.h"
#include "aa.h"
int a;


AA::AA(char *arg)
{
	a = 1;
}

AA foo("A");
