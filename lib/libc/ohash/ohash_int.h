/*	$OpenBSD: ohash_int.h,v 1.1 2001/03/02 13:27:07 espie Exp $	*/

#include <sys/types.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "ohash.h"

struct _ohash_record {
	u_int32_t	hv;
	const char 	*p;
};

#define DELETED		((const char *)h)
#define NONE		(h->size)

/* Don't bother changing the hash table if the change is small enough.  */
#define MINSIZE		(1UL << 4)
#define MINDELETED	4

