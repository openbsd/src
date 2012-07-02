/* $OpenBSD: c.c,v 1.1 2012/07/02 12:02:36 espie Exp $ */
#include <stdio.h>

extern int g();

int main()
{
	printf("%d\n", g());
	return 0;
}
