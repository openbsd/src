/*	$OpenBSD: version.c,v 1.2 1996/06/10 11:21:35 niklas Exp $	*/

#ifndef lint
static char rcsid[] = "$OpenBSD: version.c,v 1.2 1996/06/10 11:21:35 niklas Exp $";
#endif /* not lint */

#include "EXTERN.h"
#include "common.h"
#include "util.h"
#include "INTERN.h"
#include "patchlevel.h"
#include "version.h"

void my_exit();

/* Print out the version number and die. */

void
version()
{
    fprintf(stderr, "Patch version 2.0, patch level %s\n", PATCHLEVEL);
    my_exit(0);
}
