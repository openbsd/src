/*
 * Copyright (c) 2001 Sendmail, Inc. and its suppliers.
 *      All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 *	$Sendmail: ldap.h,v 1.4 2001/04/18 07:06:52 gshapiro Exp $
 */

#ifndef	SM_LDAP_H
# define SM_LDAP_H

# include <sm/conf.h>

# ifndef LDAPMAP_MAX_ATTR
#  define LDAPMAP_MAX_ATTR	64
# endif /* ! LDAPMAP_MAX_ATTR */
# ifndef LDAPMAP_MAX_FILTER
#  define LDAPMAP_MAX_FILTER	1024
# endif /* ! LDAPMAP_MAX_FILTER */
# ifndef LDAPMAP_MAX_PASSWD
#  define LDAPMAP_MAX_PASSWD	256
# endif /* ! LDAPMAP_MAX_PASSWD */

# if LDAPMAP
struct sm_ldap_struct
{
	/* needed for ldap_open or ldap_init */
	char		*ldap_host;
	int		ldap_port;
	pid_t		ldap_pid;

	/* options set in ld struct before ldap_bind_s */
	int		ldap_deref;
	time_t		ldap_timelimit;
	int		ldap_sizelimit;
	int		ldap_options;

	/* args for ldap_bind_s */
	LDAP		*ldap_ld;
	char		*ldap_binddn;
	char		*ldap_secret;
	int		ldap_method;

	/* args for ldap_search */
	char		*ldap_base;
	int		ldap_scope;
	char		*ldap_filter;
	char		*ldap_attr[LDAPMAP_MAX_ATTR + 1];
	bool		ldap_attrsonly;

	/* args for ldap_result */
	struct timeval	ldap_timeout;
	LDAPMessage	*ldap_res;

	/* ldapmap_lookup options */
	char		ldap_attrsep;

	/* Linked list of maps sharing the same LDAP binding */
	void		*ldap_next;
};

typedef struct sm_ldap_struct	SM_LDAP_STRUCT;

/* functions */
extern void	sm_ldap_clear __P((SM_LDAP_STRUCT *));
extern bool	sm_ldap_start __P((char *, SM_LDAP_STRUCT *));
extern int	sm_ldap_search __P((SM_LDAP_STRUCT *, char *));
extern void	sm_ldap_setopts __P((LDAP *, SM_LDAP_STRUCT *));
extern int	sm_ldap_geterrno __P((LDAP *));
extern void	sm_ldap_close __P((SM_LDAP_STRUCT *));
# endif /* LDAPMAP */

#endif /* ! SM_LDAP_H */
