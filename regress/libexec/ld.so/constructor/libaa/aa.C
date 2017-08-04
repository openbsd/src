/*
 * Public Domain 2003 Dale Rahn
 *
 * $OpenBSD: aa.C,v 1.3 2017/08/04 18:23:52 kettenis Exp $
 */

#include "aa.h"
volatile int a;


AA::AA(char *arg)
{
	a = 1;
}

AA foo("A");
