/*	       DOS specific routines

 */

#include <mem.h>
#include <dos.h>


/* PUBLIC							HTDOS_wwwName()
**		CONVERTS DOS Name into WWW Name
** ON ENTRY:
**	dosname 	DOS file specification (NO NODE)
**
** ON EXIT:
**	returns 	WWW file specification
**
*/
char * HTDOS_wwwName (char *dosname)
{
	static char wwwname[1024];
	char *cp_url = wwwname;

	strcpy(wwwname,dosname);

	for ( ; *cp_url != '\0' ; cp_url++)
	  if(*cp_url == '\\') *cp_url = '/';   /* convert dos backslash to unix-style */

	if(strlen(wwwname) > 3 && *cp_url == '/')
		*cp_url = '\0';

	if(*cp_url == ':')
	{
		cp_url++;
		*cp_url = '/';
	}

/*
	if((strlen(wwwname)>2)&&(wwwname[1]==':')) wwwname[1]='|';
	printf("\n\nwww: %s\n\ndos: %s\n\n",wwwname,dosname);
	sleep(5);
*/
	return(wwwname);
}


/* PUBLIC							HTDOS_name()
**		CONVERTS WWW name into a DOS name
** ON ENTRY:
**	wwwname 	WWW file name
**
** ON EXIT:
**	returns 	DOS file specification
**
** Bug(?):	Returns pointer to input string, which is modified
*/
char * HTDOS_name(char *wwwname)
{
	static char cp_url[1024];
	int joe;

	memset(cp_url, 0, 1023);
	sprintf(cp_url, "%s",wwwname);

	for(joe = 0; cp_url[joe] != '\0'; joe++)	{
		if(cp_url[joe] == '/')	{
			cp_url[joe] = '\\';
		}
	}

/*	if(strlen(cp_url) < 4) cp_url[] = ':';
	if(strlen(cp_url) == 3) cp_url[3] = '\\';

	if(strlen(cp_url) == 4) cp_url[4] = '.'; */

	if((strlen(cp_url) > 2) && (cp_url[1] == '|'))
		cp_url[1] = ':';

	if((cp_url[1] == '\\') || (cp_url[0]  != '\\'))
	{
#if 0
		printf("\n\n%s = i%\n\n",cp_url,strlen(cp_url));
		sleep(5);
#endif
		strcpy(wwwname, cp_url);
		return(wwwname); /* return(cp_url); */
	} else {
#if 0
		printf("\n\n%s = %i\n\n",cp_url+1,strlen(cp_url));
		sleep(5);
#endif
		strcpy(wwwname, cp_url+1);
		return(wwwname); /* return(cp_url+1);  */
	}
}
