/*             DOS specific routines            */

#ifndef HTDOS_H
#define HTDOS_H

#ifndef HTUTILS_H
#include <HTUtils.h>
#endif /* HTUTILS_H */

/* PUBLIC                                                       HTDOS_wwwName()
**              CONVERTS DOS Name into WWW Name
** ON ENTRY:
**	dosname		DOS file specification (NO NODE)
**
** ON EXIT:
**	returns		WWW file specification
**
*/
char * HTDOS_wwwName PARAMS((CONST char * dosname));

/*
 * Converts Unix slashes to DOS
 */
char * HTDOS_slashes PARAMS((char * path));

/* PUBLIC                                                       HTDOS_name()
**              CONVERTS WWW name into a DOS name
** ON ENTRY:
**	wwwname		WWW file name
**
** ON EXIT:
**	returns		DOS file specification
**
** Bug:	Returns pointer to static -- non-reentrant
*/
char * HTDOS_name PARAMS((char * wwwname));

#ifdef WIN_EX 
char * HTDOS_short_name (char * fn);
#else 
#define HTDOS_short_name(fn)  fn 
#endif

#endif /*  HTDOS_H */
