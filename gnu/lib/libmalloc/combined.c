/*
 * this file (combined.c) is malloc.c, free.c, and realloc.c, combined into
 * one file, because the malloc.o in libc defined malloc, realloc, and free,
 * and libc sometimes invokes realloc, which can greatly confuse things
 * in the linking process...
 *
 *	$Id: combined.c,v 1.1.1.1 1995/10/18 08:38:21 deraadt Exp $
 */

#include "malloc.c"
#include "free.c"
#include "realloc.c"
