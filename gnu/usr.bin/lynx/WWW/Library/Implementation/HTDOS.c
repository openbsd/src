/*	       DOS specific routines

 */

#include <HTUtils.h>
#include <HTDOS.h>

/*
 * Make a copy of the source argument in the result, allowing some extra
 * space so we can append directly onto the result without reallocating.
 */
PRIVATE char * copy_plus ARGS2(char **, result, char *, source)
{
    int length = strlen(source);
    HTSprintf0(result, "%-*s", length+10, source);
    (*result)[length] = 0;
    return (*result);
}

/* PUBLIC							HTDOS_wwwName()
**		CONVERTS DOS Name into WWW Name
** ON ENTRY:
**	dosname 	DOS file specification (NO NODE)
**
** ON EXIT:
**	returns 	WWW file specification
**
*/
char * HTDOS_wwwName ARGS1(char *, dosname)
{
    static char *wwwname;
    char *cp_url = copy_plus(&wwwname, dosname);

    for ( ; *cp_url != '\0' ; cp_url++)
	if(*cp_url == '\\')
	    *cp_url = '/';   /* convert dos backslash to unix-style */

    if(strlen(wwwname) > 3 && *cp_url == '/')
	*cp_url = '\0';

#ifdef NOTUSED
    if(*cp_url == ':') {
	cp_url++;
	*cp_url = '/';	/* terminate drive letter to survive */
    }
#endif

    return(wwwname);
}


/* PUBLIC							HTDOS_name()
**		CONVERTS WWW name into a DOS name
** ON ENTRY:
**	wwwname 	WWW file name
**
** ON EXIT:
**	returns 	DOS file specification
*/
char * HTDOS_name ARGS1(char *, wwwname)
{
    static char *cp_url;
    char *result;
    int joe;

    copy_plus(&cp_url, wwwname);

    for (joe = 0; cp_url[joe] != '\0'; joe++)	{
	if (cp_url[joe] == '/')	{
	    cp_url[joe] = '\\';	/* convert slashes to dos-style */
	}
    }

    /* pesky leading slash, rudiment from file://localhost/  */
    /* the rest of path may be with or without drive letter  */
    if((cp_url[1] == '\\') || (cp_url[0]  != '\\')) {
	result = cp_url;
    } else {
	result = cp_url+1;
    }

    CTRACE(tfp, "HTDOS_name changed `%s' to `%s'\n", wwwname, result);
    return (result);
}
