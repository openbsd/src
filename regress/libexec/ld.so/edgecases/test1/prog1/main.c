/*
 * Public Domain 2003 Dale Rahn
 *
 * $OpenBSD: main.c,v 1.1.1.1 2005/09/22 22:31:27 drahn Exp $
 */
#include <stdio.h>
#include <dlfcn.h>


void ad(void);
extern int libglobal;

void (*ad_f)(void) = &ad;
int *a = &libglobal;
int
main()
{

	ad_f();

	return 1;
}
