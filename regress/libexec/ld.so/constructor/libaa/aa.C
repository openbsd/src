/*
 * Public Domain 2003 Dale Rahn
 *
 * $OpenBSD: aa.C,v 1.4 2017/08/07 16:33:52 bluhm Exp $
 */

#include "aa.h"
volatile int a;


AA::AA(const char *arg)
{
	a = 1;
}

AA foo("A");
