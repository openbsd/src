
void * _dl_malloc(int);

char *
_dl_strdup(const char *orig)
{
	char *newstr;
	newstr = _dl_malloc(strlen(orig)+1);
	strcpy(newstr, orig);
	return (newstr);
}

