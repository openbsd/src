/*
**	$Id: identd.h,v 1.4 1999/08/06 17:35:02 deraadt Exp $
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

extern char *version;

extern char *path_unix;
extern char *path_kmem;

extern int verbose_flag;
extern int debug_flag;
extern int syslog_flag;
extern int multi_flag;
extern int other_flag;
extern int unknown_flag;
extern int number_flag;
extern int noident_flag;
extern int token_flag;

extern char *charset_name;
extern char *indirect_host;
extern char *indirect_password;

extern int lport;
extern int fport;

int	parse __P((int, struct in_addr *, struct in_addr *));
char	*gethost __P((struct in_addr *));
int	k_getuid __P((struct in_addr *, int, struct in_addr *, int, uid_t *));

#endif
