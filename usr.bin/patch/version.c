/*	$OpenBSD: version.c,v 1.3 1997/09/22 05:45:28 millert Exp $	*/

#ifndef lint
static char rcsid[] = "$OpenBSD: version.c,v 1.3 1997/09/22 05:45:28 millert Exp $";
#endif /* not lint */

#include "EXTERN.h"
#include "common.h"
#include "util.h"
#include "INTERN.h"
#include "patchlevel.h"
#include "version.h"

#ifdef __GNUC__
void my_exit() __attribute__((noreturn));
#else
void my_exit();
#endif

/* Print out the version number and die. */

void
version()
{
    fprintf(stderr, "Patch version 2.0, patch level %s\n", PATCHLEVEL);
    my_exit(0);
}
