#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include "tcfs_cipher.h"
#include "crypto/blf.h"


void *
BLOWFISH_init_key (char *key)
{
	blf_ctx *ks=NULL;

	ks=(blf_ctx *)malloc (sizeof (blf_ctx), M_FREE, M_NOWAIT);
	if (!ks)
		return NULL;

	blf_key (ks, key, BLOWFISH_KEYSIZE);

	return (void *)ks;
}

void
BLOWFISH_cleanup_key(void *k)
{
	free((blf_ctx *)k, M_FREE);
}

void
BLOWFISH_encrypt(char *block, int nb, void *key)
{
	char iv[] = {'\0','\0','\0','\0','\0','\0','\0','\0'};
	blf_cbc_encrypt((blf_ctx *)key, iv, block, nb);
}

void
BLOWFISH_decrypt(char *block, int nb, void *key)
{
	char iv[] = {'\0','\0','\0','\0','\0','\0','\0','\0'};
	blf_cbc_decrypt((blf_ctx *)key, iv, block, nb);
}
