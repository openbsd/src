/*	$OpenBSD: tcfslib.h,v 1.6 2000/06/19 20:35:48 fgsch Exp $	*/

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

extern int	tcfspwdbr_new __P((tcfspwdb **));
extern int	tcfspwdbr_edit __P((tcfspwdb **, int, ...));
extern int	tcfspwdbr_read __P((tcfspwdb *, int, ...));
extern void	tcfspwdbr_dispose __P((tcfspwdb *));
extern int	tcfsgpwdbr_new __P((tcfsgpwdb **));
extern int	tcfsgpwdbr_edit __P((tcfsgpwdb **, int, ...));
extern int	tcfsgpwdbr_read __P((tcfsgpwdb *, int, ...));
extern void	tcfsgpwdbr_dispose __P((tcfsgpwdb *));
extern int	tcfs_chgpwd __P((char *, char *, char *));
extern int	tcfs_group_chgpwd __P((char *, gid_t, char *, char *));
extern int	tcfs_chgpassword __P((char *, char *, char *));
extern int	tcfs_decrypt_key __P((char *, u_char *, u_char *, int));
extern int	tcfs_encrypt_key __P((char *, u_char *, int, u_char *, int));
extern char    *tcfs_decode __P((char *, int *));
extern char    *tcfs_encode __P((char *, int ));
extern char    *gentcfskey __P((void));

extern int	tcfs_getstatus __P((char *, struct tcfs_status *));
extern int	tcfs_getfspath __P((char *, char *));

extern int	tcfs_proc_enable __P((char *, uid_t, pid_t, char *));
extern int	tcfs_proc_disable __P((char *, uid_t, pid_t));
extern int	tcfs_user_enable __P((char *, uid_t, u_char *));
extern int	tcfs_user_disable __P((char *, uid_t));
extern int	tcfs_group_enable __P((char *, uid_t, gid_t, int, char *));
extern int	tcfs_group_disable __P((char *, uid_t, gid_t));

extern tcfspwdb *
		tcfs_getpwnam __P((char *, tcfspwdb **));
extern int	tcfs_putpwnam __P((char *, tcfspwdb *, int));

extern int	unix_auth __P((char **, char **, int));
