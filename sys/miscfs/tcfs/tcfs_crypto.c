#include "tcfs_cipher.h"

#define	BLOCKSIZE	1024
#define SBLOCKSIZE	8
#define MIN(a,b) ((a)<(b)?(a):(b))
#define D_NOBLK(o)      ((o)/BLOCKSIZE+(o%BLOCKSIZE?1:0))


void  mkencrypt(struct tcfs_mount *mp,char *block,int nb, void* ks)
{
	int i,r;
	char *tmp;
	
	tmp=block;
	r=nb;
	for(i=0;i<D_NOBLK(nb)&&r>0;i++)
	{
		TCFS_ENCRYPT(mp,tmp,MIN(BLOCKSIZE,r),ks);
		tmp+=BLOCKSIZE;
		r-=BLOCKSIZE;
	}
}

void  mkdecrypt(struct tcfs_mount *mp,char *block,int nb,void* ks)
{
	int i,r;
	char *tmp;
	
	tmp=block;
	r=nb;
	for(i=0;i<D_NOBLK(nb)&&r>0;i++)
	{
		TCFS_DECRYPT(mp, tmp,MIN(BLOCKSIZE,r),ks);
		tmp+=BLOCKSIZE;
		r-=BLOCKSIZE;
	}
}
		
