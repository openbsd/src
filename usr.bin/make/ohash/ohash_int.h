/*	$OpenBSD: ohash_int.h,v 1.2 2001/01/29 01:58:05 niklas Exp $	*/

#include <sys/types.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "ohash.h"

#define DELETED		((const char *)h)
#define NONE		(h->size)

/* Don't bother changing the hash table if the change is small enough.  */
#define MINSIZE		(1UL << 4)
#define MINDELETED	4

