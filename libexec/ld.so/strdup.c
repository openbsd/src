/*	$OpenBSD: strdup.c,v 1.4 2001/06/08 06:49:19 art Exp $	*/

#include <string.h>
#include <sys/types.h>
#include "archdep.h"

void * _dl_malloc(int);

char *
_dl_strdup(const char *orig)
{
	char *newstr;
	newstr = _dl_malloc(_dl_strlen(orig)+1);
	strcpy(newstr, orig);
	return (newstr);
}

