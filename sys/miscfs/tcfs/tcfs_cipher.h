#ifndef _TCFS_MOUNT_H_
#include "tcfs_mount.h"
#endif

#define _TCFS_CIPHER_H_
#define MaxNumOfCipher	 8
#define MaxCipherNameLen 8

enum {
	 C_TDES=0,C_BLOW=2
	} ;

struct tcfs_cipher
	{
	 char cipher_desc[MaxCipherNameLen];
	 int  cipher_version;
	 int cipher_keysize;
	 void *(*init_key)(char*);
	 void (*cleanup_key)(void*);
	 void (*encrypt)(char*, int, void*);
	 void (*decrypt)(char*, int, void*);
	};

extern struct tcfs_cipher tcfs_cipher_vect[MaxNumOfCipher];

#define TCFS_MP_CIPHER(mp) (((struct tcfs_mount*)(mp))->tcfs_cipher_num)

#define TCFS_CIPHER_KEYSIZE(mp)\
	 (tcfs_cipher_vect[TCFS_MP_CIPHER((mp))].cipher_keysize)

#define TCFS_CIPHER_VERSION(mp)\
	 (tcfs_cipher_vect[TCFS_MP_CIPHER((mp))].cipher_version)

#define TCFS_CIPHER_DESC(mp)\
	 (tcfs_cipher_vect[TCFS_MP_CIPHER((mp))].cipher_desc)

static __inline void *TCFS_INIT_KEY(struct tcfs_mount *,char *);
static __inline void *TCFS_INIT_KEY(struct tcfs_mount *mp, char *tok)
{
	 return (tcfs_cipher_vect[TCFS_MP_CIPHER((mp))].init_key((tok)));
}

static __inline void  TCFS_CLEANUP_KEY(struct tcfs_mount*,void*);
static __inline void  TCFS_CLEANUP_KEY(struct tcfs_mount* mp,void* tok)
{
	 (tcfs_cipher_vect[TCFS_MP_CIPHER((mp))].cleanup_key((tok)));
	 return;
}
static __inline void  TCFS_ENCRYPT(struct tcfs_mount*,char*,int,void*);
static __inline void  TCFS_ENCRYPT(struct tcfs_mount *mp,char *blk,int len,void *key)
{
	 (tcfs_cipher_vect[TCFS_MP_CIPHER((mp))].encrypt((blk),(len),(key)));
	return;
}
static __inline void  TCFS_DECRYPT(struct tcfs_mount*,char*,int,void*);
static __inline void  TCFS_DECRYPT(struct tcfs_mount *mp,char *blk,int len,void *key)
{
	 (tcfs_cipher_vect[TCFS_MP_CIPHER((mp))].decrypt((blk),(len),(key)));
	 return;
}

void mkencrypt (struct tcfs_mount *, char *, int, void*);
void mkdecrypt (struct tcfs_mount *, char *, int, void*);

/* prototipi funzioni */

void *cnone_init_key(char *);
void cnone_cleanup_key(void*);
void cnone_encrypt(char *, int , void*);
void cnone_decrypt(char *, int , void*);
#define NONE_KEYSIZE	0

void *TDES_init_key(char *);
void TDES_cleanup_key(void*);
void TDES_encrypt(char *, int , void*);
void TDES_decrypt(char *, int , void*);
#define TDES_KEYSIZE	8

void *BLOWFISH_init_key(char *);
void BLOWFISH_cleanup_key(void*);
void BLOWFISH_encrypt(char *, int , void*);
void BLOWFISH_decrypt(char *, int , void*);
#define BLOWFISH_KEYSIZE	8

