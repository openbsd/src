/*	$OpenBSD: scaffold.h,v 1.3 2002/06/07 03:32:04 itojun Exp $	*/

 /*
  * @(#) scaffold.h 1.3 94/12/31 18:19:19
  * 
  * Author: Wietse Venema, Eindhoven University of Technology, The Netherlands.
  */

#include <sys/cdefs.h>

__BEGIN_DECLS
extern struct addrinfo *find_inet_addr(char *, int);
extern int check_dns(char *);
extern int check_path(char *, struct stat *);
__END_DECLS
