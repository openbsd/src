#include "tcfs_cipher.h"

void *cnone_init_key (char *key)
{
        return (void *)key;
}

void cnone_cleanup_key(void *k)
{
}

void cnone_encrypt(char *block, int nb, void *key)
{
}

void cnone_decrypt(char *block, int nb, void *key)
{
}
