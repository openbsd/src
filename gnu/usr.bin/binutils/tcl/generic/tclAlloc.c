/*
 * tclAlloc.c --
 *
 *      This file provides default memory allocation primitives used
 *      by the Tcl library.  Tcl calls these instead of malloc(),
 *      free(), etc, so that applications can override these calls
 *      if necessary.
 *
 */
 

#include "tclInt.h"

char *
Tcl_Malloc(size)
	unsigned int size;
{
	return malloc(size);
}

char *
Tcl_Realloc(ptr, size)
	char *ptr;
	unsigned int size;
{
	return realloc(ptr, size);
}

void
Tcl_Free(ptr)
	char *ptr;
{
	free(ptr);
}
