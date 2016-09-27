/*
 * Public Domain 2003 Dale Rahn
 *
 * $OpenBSD: aa.C,v 1.2 2016/09/27 06:52:50 kettenis Exp $
 */

#include "aa.h"
int a;


AA::AA(char *arg)
{
	a = 1;
}

AA foo("A");
