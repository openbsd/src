/*	$Id: acl.h,v 1.2 1995/12/14 08:43:41 tholo Exp $	*/

#ifndef __ACL_H
#define __ACL_H

#include <sys/cdefs.h>

int acl_canonicalize_principal __P((char *principal, char *canon));
int acl_initialize __P((char *acl_file, int perm));
int acl_exact_match __P((char *acl, char *principal));
int acl_check __P((char *acl, char *principal));
int acl_add __P((char *acl, char *principal));
int acl_delete __P((char *acl, char *principal));

#endif /* __ACL_H */
