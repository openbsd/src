/*	$OpenBSD: tcfslib.h,v 1.9 2002/02/16 21:27:54 millert Exp $	*/

/*
 *	Transparent Cryptographic File System (TCFS) for NetBSD 
 *	Author and mantainer: 	Luigi Catuogno [luicat@tcfs.unisa.it]
 *	
 *	references:		http://tcfs.dia.unisa.it
 *				tcfs-bsd@tcfs.unisa.it
 */

/*
 *	Base utility set v0.1
 */

#include <unistd.h>
#include "tcfsdefines.h"
#include "tcfspwdb.h"

extern int	tcfspwdbr_new(tcfspwdb **);
extern int	tcfspwdbr_edit(tcfspwdb **, int, ...);
extern int	tcfspwdbr_read(tcfspwdb *, int, ...);
extern void	tcfspwdbr_dispose(tcfspwdb *);
extern int	tcfsgpwdbr_new(tcfsgpwdb **);
extern int	tcfsgpwdbr_edit(tcfsgpwdb **, int, ...);
extern int	tcfsgpwdbr_read(tcfsgpwdb *, int, ...);
extern void	tcfsgpwdbr_dispose(tcfsgpwdb *);
extern int	tcfs_chgpwd(char *, char *, char *);
extern int	tcfs_group_chgpwd(char *, gid_t, char *, char *);
extern int	tcfs_chgpassword(char *, char *, char *);
extern int	tcfs_decrypt_key(char *, u_char *, u_char *, int);
extern int	tcfs_encrypt_key(char *, u_char *, int, u_char *, int);
extern char    *tcfs_decode(char *, int *);
extern char    *tcfs_encode(char *, int );
extern char    *gentcfskey(void);

extern int	tcfs_getstatus(char *, struct tcfs_status *);
extern int	tcfs_getfspath(char *, char *);

extern int	tcfs_proc_enable(char *, uid_t, pid_t, char *);
extern int	tcfs_proc_disable(char *, uid_t, pid_t);
extern int	tcfs_user_enable(char *, uid_t, u_char *);
extern int	tcfs_user_disable(char *, uid_t);
extern int	tcfs_group_enable(char *, uid_t, gid_t, int, char *);
extern int	tcfs_group_disable(char *, uid_t, gid_t);

extern tcfspwdb *
		tcfs_getpwnam(char *, tcfspwdb **);
extern int	tcfs_putpwnam(char *, tcfspwdb *, int);

extern int	unix_auth(char **, char **, int);
extern tcfsgpwdb *
		tcfs_ggetpwnam(char *, gid_t, tcfsgpwdb **);
extern int	tcfs_gputpwnam(char *, tcfsgpwdb *, int);
extern int	tcfs_get_label(char *, char *, int *);
extern int	tcfs_verify_fs(char *);
extern int	tcfs_callfunction(char *, struct tcfs_args *);
