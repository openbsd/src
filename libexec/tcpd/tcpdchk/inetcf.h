/*	$OpenBSD: inetcf.h,v 1.1 1997/02/26 06:17:07 downsj Exp $	*/

 /*
  * @(#) inetcf.h 1.1 94/12/28 17:42:30
  * 
  * Author: Wietse Venema, Eindhoven University of Technology, The Netherlands.
  */

#include <sys/cdefs.h>

extern char *inet_cfg __P((char *));
extern void inet_set __P((char *, int));
extern int inet_get __P((char *));

#define WR_UNKNOWN	(-1)		/* service unknown */
#define WR_NOT		1		/* may not be wrapped */
#define WR_MAYBE	2		/* may be wrapped */
#define	WR_YES		3		/* service is wrapped */
