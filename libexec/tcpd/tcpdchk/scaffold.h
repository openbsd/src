/*	$OpenBSD: scaffold.h,v 1.2 2002/02/16 21:27:31 millert Exp $	*/

 /*
  * @(#) scaffold.h 1.3 94/12/31 18:19:19
  * 
  * Author: Wietse Venema, Eindhoven University of Technology, The Netherlands.
  */

#include <sys/cdefs.h>

__BEGIN_DECLS
extern struct hostent *find_inet_addr(char *);
extern int check_dns(char *);
extern int check_path(char *, struct stat *);
__END_DECLS
