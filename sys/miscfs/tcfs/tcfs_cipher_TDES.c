/*	$OpenBSD: tcfs_cipher_TDES.c,v 1.4 2000/06/18 16:23:10 provos Exp $	*/
/*
 * Copyright 2000 The TCFS Project at http://tcfs.dia.unisa.it/
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the authors may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/md5k.h>

#include <miscfs/tcfs/tcfs_cipher.h>

#include <crypto/des_locl.h>
#include <crypto/des.h>

/* EDE Triple-DES with K1, K2 and K3 */

void *
TDES_init_key (char *key)
{
        des_key_schedule *ks;
	char dkey[TDES_KEYSIZE];
	char digest[16];
	MD5_CTX ctx;
	int i;

	
	/* Fold the bigger key into a Triple-DES suitable one */
	bcopy (key, dkey, sizeof(dkey));
	MD5Init(&ctx);
	MD5Update(&ctx, key, KEYSIZE);
	MD5Final(digest, &ctx);

	for (i = 0; i < sizeof(dkey); i++)
	  dkey[i] ^= digest[i % 16];

        ks = (des_key_schedule *)malloc (3 * sizeof (des_key_schedule),
					 M_FREE, M_NOWAIT);
        if (!ks) 
                return NULL;

        des_set_key ((des_cblock *) dkey,       ks[0]);
        des_set_key ((des_cblock *)(dkey + 8),  ks[1]);
        des_set_key ((des_cblock *)(dkey + 16), ks[2]);

	bzero(dkey, sizeof(dkey));
	bzero(digest, sizeof(digest));

        return (void *)ks;
}

void
TDES_cleanup_key(void *k)
{
/*	tcfs_keytab_dispnode does it
	free((des_key_schedule*)k,M_FREE);
*/
}

void
TDES_encrypt(char *block, int nb, void *key)
{
        u_int32_t * xi;
        int i;
        char *tmp;
        des_key_schedule *ks = (des_key_schedule *)key;

        xi = (u_int32_t *)block;
        tmp = block;
        des_ecb3_encrypt((des_cblock *)tmp, (des_cblock *)tmp,
			 ks[0], ks[1], ks[2],DES_ENCRYPT);
        tmp += 8;
        for (i = 1;i < nb/8;i++) {
                *(xi+2) ^= *xi;
                *(xi+3) ^= *(xi + 1);
                des_ecb3_encrypt((des_cblock *)tmp, (des_cblock *)tmp,
				 ks[0], ks[1], ks[2], DES_ENCRYPT);
                tmp += 8;
                xi += 2;
        }
}

void
TDES_decrypt(char *block, int nb, void *key)
{
        u_int32_t * xi, xo[2], xa[2];
        int i;
        char *tmp;
        des_key_schedule *ks = (des_key_schedule *)key;

        xi = (u_int32_t *)block;
        tmp = block;
        xo[0] = *xi; xo[1] = *(xi+1);
        des_ecb3_encrypt((des_cblock *)tmp, (des_cblock *)tmp,
			 ks[0], ks[1], ks[2], DES_DECRYPT);
        tmp += 8;
        xi = (u_int32_t *)tmp;
        for (i = 1;i < nb/8; i++) {
                xa[0] = *xi; xa[1] = *(xi+1);
                des_ecb3_encrypt((des_cblock *)tmp, (des_cblock *)tmp,
				 ks[0], ks[1], ks[2], DES_DECRYPT);
                *(xi) ^= xo[0];
                *(xi+1)^= xo[1];
                xo[0] = xa[0];  
                xo[1] = xa[1];
                tmp += 8;
                xi += 2;
        }
}
