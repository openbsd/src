/*	$OpenBSD: strdup.c,v 1.3 2001/04/02 23:11:21 drahn Exp $	*/

#include <string.h>

void * _dl_malloc(int);

char *
_dl_strdup(const char *orig)
{
	char *newstr;
	newstr = _dl_malloc(strlen(orig)+1);
	strcpy(newstr, orig);
	return (newstr);
}

