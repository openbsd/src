/*	$OpenBSD: strdup.c,v 1.2 2001/01/28 19:34:29 niklas Exp $	*/


void * _dl_malloc(int);

char *
_dl_strdup(const char *orig)
{
	char *newstr;
	newstr = _dl_malloc(strlen(orig)+1);
	strcpy(newstr, orig);
	return (newstr);
}

