/*
 * strerror() - for those systems that don't have it yet.
 */

/* These are part of the C library. (See perror.3) */
extern char *sys_errlist[];
extern int sys_nerr;

static char errmsg[80];

char *strerror(en)
    int en;
{
    if ((0 <= en) && (en < sys_nerr))
	return sys_errlist[en];

    sprintf(errmsg, "Error %d", en);
    return errmsg;
}
