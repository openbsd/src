/*	       DOS specific routines

 */

#include <HTUtils.h>
#include <HTDOS.h>

#ifdef WIN_EX
#include <LYGlobalDefs.h>
#endif

/*
 * Make a copy of the source argument in the result, allowing some extra
 * space so we can append directly onto the result without reallocating.
 */
PRIVATE char * copy_plus ARGS2(char **, result, CONST char *, source)
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
char * HTDOS_wwwName ARGS1(CONST char *, dosname)
{
    static char *wwwname = NULL;
    char *cp_url = copy_plus(&wwwname, dosname);
    int wwwname_len;

#ifdef SH_EX
    char ch;

    while ((ch = *dosname) != '\0') {
	switch (ch) {
	case '\\':
	/* convert dos backslash to unix-style */
	    *cp_url++ = '/';
	    break;
	case ' ':
	    *cp_url++ = '%';
	    *cp_url++ = '2';
	    *cp_url++ = '0';
	    break;
      default:
	    *cp_url++ = ch;
	    break;
      }
      dosname++;
    }
    *cp_url = '\0';
#else
    for ( ; *cp_url != '\0' ; cp_url++)
	if(*cp_url == '\\')
	    *cp_url = '/';   /* convert dos backslash to unix-style */
#endif

    wwwname_len = strlen(wwwname);
    if (wwwname_len > 1)
	cp_url--;	/* point last char */

    if (wwwname_len > 3 && *cp_url == '/')
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
#ifdef _WINDOWS	/* 1998/04/02 (Thu) 08:47:20 */
    extern char windows_drive[];
    char temp_buff[LY_MAXPATH];
#endif
    static char *result = NULL;
    int joe;

    copy_plus(&result, wwwname);

    for (joe = 0; result[joe] != '\0'; joe++)	{
	if (result[joe] == '/')	{
	    result[joe] = '\\';	/* convert slashes to dos-style */
	}
    }

    /* pesky leading slash, rudiment from file://localhost/  */
    /* the rest of path may be with or without drive letter  */
    if((result[1] != '\\') && (result[0]  == '\\')) {
	for (joe = 0; (result[joe] = result[joe+1]) != 0; joe++)
	    ;
    }

#ifdef _WINDOWS	/* 1998/04/02 (Thu) 08:59:48 */
    if (strchr(result, '\\') != NULL
     && strchr(result, ':') == NULL) {
	sprintf(temp_buff, "%.3s\\%.*s", windows_drive,
		(int)(sizeof(temp_buff) - 5), result);
	StrAllocCopy(result, temp_buff);
    } else {
	char *p = strchr(result, ':');
	if (p && (strcmp(p, ":\\") == 0)) {
	    p[2] = '.';
	    p[3] = '\0';
	}
    }
#endif
    /*
     * If we have only a device, add a trailing slash.  Otherwise it just
     * refers to the current directory on the given device.
     */
    if (strchr(result, '\\') == 0
     && result[1] == ':')
	StrAllocCat(result, "\\");

    CTRACE((tfp, "HTDOS_name changed `%s' to `%s'\n", wwwname, result));
    return (result);
}

#if defined(DJGPP) && defined(DJGPP_KEYHANDLER)
/* PUBLIC       getxkey()
**              Replaces libc's getxkey() with polling of tcp/ip
**              library (WatTcp or Watt-32). This is required to
**              be able to finish off dead sockets, answer pings etc.
**
** ON EXIT:
**      returns extended keypress.
*/

/* Copyright (C) 1995 DJ Delorie, see COPYING.DJ for details */
#include <pc.h>
#include <dpmi.h>
#include <libc/farptrgs.h>
#include <go32.h>

int getxkey (void)
{
    __dpmi_regs r;

    /* poll tcp/ip lib and yield to DPMI-host while nothing in
     * keyboard buffer (head = tail) (simpler than kbhit).
     */
    while (_farpeekw(_dos_ds, 0x41a) == _farpeekw(_dos_ds, 0x41c))
    {
	tcp_tick (NULL);
	__dpmi_yield();
    }

    r.h.ah = 0x10;
    __dpmi_int(0x16, &r);

    if (r.h.al == 0x00)
	return 0x0100 | r.h.ah;
    if (r.h.al == 0xe0)
	return 0x0200 | r.h.ah;
    return r.h.al;
}
#endif /* DJGPP && DJGPP_KEYHANDLER */

