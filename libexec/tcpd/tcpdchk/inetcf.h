/*	$OpenBSD: inetcf.h,v 1.2 2002/02/16 21:27:31 millert Exp $	*/

 /*
  * @(#) inetcf.h 1.1 94/12/28 17:42:30
  * 
  * Author: Wietse Venema, Eindhoven University of Technology, The Netherlands.
  */

#include <sys/cdefs.h>

extern char *inet_cfg(char *);
extern void inet_set(char *, int);
extern int inet_get(char *);

#define WR_UNKNOWN	(-1)		/* service unknown */
#define WR_NOT		1		/* may not be wrapped */
#define WR_MAYBE	2		/* may be wrapped */
#define	WR_YES		3		/* service is wrapped */
