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

extern int tcfspwdbr_new (tcfspwdb **p);
extern int tcfspwdbr_edit (tcfspwdb **p, int i, ...);
extern int tcfspwdbr_read (tcfspwdb *p, int i, ...);
extern void tcfspwdbr_dispose (tcfspwdb *p);
extern int tcfsgpwdbr_new (tcfsgpwdb **p);
extern int tcfsgpwdbr_edit (tcfsgpwdb **p, int i, ...);
extern int tcfsgpwdbr_read (tcfsgpwdb *p, int i, ...);
extern void tcfsgpwdbr_dispose (tcfsgpwdb *p);
extern int tcfs_chgpwd (char *u, char *o, char *p);
extern int tcfs_group_chgpwd (char *u, gid_t gid, char *o, char *p);
extern int tcfs_chgpassword (char *u, char *o, char *p);
extern int tcfs_decrypt_key (char *pwd, u_char *t, u_char *tk, int tklen);
extern int tcfs_encrypt_key (char *pwd, u_char *key, int klen, u_char *ek, int eklen);
extern char *tcfs_decode (char *t, int *l);
extern char *tcfs_encode (char *t, int l);
extern char *gentcfskey (void);


