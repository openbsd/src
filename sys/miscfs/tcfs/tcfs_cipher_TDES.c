#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include "tcfs_cipher.h"
#include <crypto/des_locl.h>
#include <crypto/des.h>

void *TDES_init_key (char *key)
{
        des_key_schedule *ks;

        ks=(des_key_schedule *)malloc (2*sizeof (des_key_schedule), M_FREE,M_NOWAIT);
        if (!ks) 
                return NULL;

        des_set_key ((des_cblock *)key, ks[0]);
        des_set_key ((des_cblock *)(key+8), ks[1]);

        return (void *)ks;
}

void TDES_cleanup_key(void *k)
{
/*	tcfs_keytab_dispnode does it
	free((des_key_schedule*)k,M_FREE);
*/
}

void TDES_encrypt(char *block, int nb, void *key)
{
        unsigned long * xi;
        int i;
        char *tmp;
        des_key_schedule *ks=(des_key_schedule *)key;
        xi=(long *)block;
        tmp=block;
        des_ecb3_encrypt((des_cblock *)tmp,(des_cblock *)tmp,ks[0],ks[1],ks[0],DES_ENCRYPT);
        tmp+=8;
        for (i=1;i<nb/8;i++) {
                *(xi+2)^=*xi;
                *(xi+3)^=*(xi+1);
                des_ecb3_encrypt((des_cblock *)tmp,(des_cblock *)tmp,ks[0],ks[1],ks[0],DES_ENCRYPT);
                tmp+=8;
                xi+=2;
        }
}

void TDES_decrypt(char *block, int nb, void *key)
{
        unsigned long * xi,xo[2],xa[2];
        int i;
        char *tmp;
        des_key_schedule *ks=(des_key_schedule *)key;

        xi=(long *)block;
        tmp=block;
        xo[0]=*xi; xo[1]=*(xi+1);
        des_ecb3_encrypt((des_cblock *)tmp,(des_cblock *)tmp,ks[0],ks[1],ks[0],DES_DECRYPT);
        tmp+=8;
        xi=(long *)tmp;
        for (i=1;i<nb/8;i++) {
                xa[0]=*xi; xa[1]=*(xi+1);
                des_ecb3_encrypt((des_cblock *)tmp,(des_cblock *)tmp,ks[0],ks[1],ks[0],DES_DECRYPT);
                *(xi)^=xo[0];
                *(xi+1)^=xo[1];
                xo[0]=xa[0];  
                xo[1]=xa[1];
                tmp+=8;
                xi+=2;
        }
}


