/*	$OpenBSD: ohash_int.h,v 1.2 2004/06/22 20:00:16 espie Exp $	*/

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
