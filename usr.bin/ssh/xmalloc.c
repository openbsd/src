/*

xmalloc.c

Author: Tatu Ylonen <ylo@cs.hut.fi>

Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
                   All rights reserved

Created: Mon Mar 20 21:23:10 1995 ylo

Versions of malloc and friends that check their results, and never return
failure (they call fatal if they encounter an error).

*/

#include "includes.h"
RCSID("$Id: xmalloc.c,v 1.1 1999/09/26 20:53:38 deraadt Exp $");

#include "ssh.h"

#if 0
void *malloc(size_t size);
void *realloc(void *ptr, size_t size);
void free(void *ptr);
#endif

void *xmalloc(size_t size)
{
  void *ptr = malloc(size);
  if (ptr == NULL)
    fatal("xmalloc: out of memory (allocating %d bytes)", (int)size);
  return ptr;
}

void *xrealloc(void *ptr, size_t new_size)
{
  void *new_ptr;

  if (ptr == NULL)
    fatal("xrealloc: NULL pointer given as argument");
  new_ptr = realloc(ptr, new_size);
  if (new_ptr == NULL)
    fatal("xrealloc: out of memory (new_size %d bytes)", (int)new_size);
  return new_ptr;
}

void xfree(void *ptr)
{
  if (ptr == NULL)
    fatal("xfree: NULL pointer given as argument");
  free(ptr);
}

char *xstrdup(const char *str)
{
  char *cp = xmalloc(strlen(str) + 1);
  strcpy(cp, str);
  return cp;
}
