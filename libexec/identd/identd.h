/*	$OpenBSD: identd.h,v 1.13 2002/07/16 10:36:10 deraadt Exp $*/

/*
**
** identd.h		    Common variables for the Pidentd daemon
**
** This program is in the public domain and may be used freely by anyone
** who wants to. 
**
** Last update: 6 Dec 1992
**
** Please send bug fixes/bug reports to: Peter Eriksson <pen@lysator.liu.se>
*/

#ifndef __IDENTD_H__
#define __IDENTD_H__

extern int verbose_flag;
extern int debug_flag;
extern int syslog_flag;
extern int multi_flag;
extern int unknown_flag;
extern int number_flag;
extern int noident_flag;
extern int token_flag;
extern int userident_flag;

extern const char *opsys_name;
extern const char *charset_sep;
extern char *charset_name;

extern int lport;
extern int fport;

int	parse(int, struct in_addr *, struct in_addr *);
int	parse6(int, struct sockaddr_in6 *, struct sockaddr_in6 *);
char	*gethost4(struct sockaddr_in *);
char	*gethost4_addr(struct in_addr *);
char	*gethost6(struct sockaddr_in6 *);
int	k_getuid(struct in_addr *, int, struct in_addr *, int, uid_t *);
int	k_getuid6(struct sockaddr_in6 *, int, struct sockaddr_in6 *,
	    int, uid_t *);

#endif
