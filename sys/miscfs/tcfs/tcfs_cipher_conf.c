#include "tcfs_cipher.h"

struct tcfs_cipher tcfs_cipher_vect[]={
	{"3des",0,TDES_KEYSIZE,TDES_init_key,TDES_cleanup_key,
					TDES_encrypt,TDES_decrypt},
	{"none",0,0,cnone_init_key,cnone_cleanup_key,
					cnone_encrypt,cnone_decrypt},
	{"bfish",0,BLOWFISH_KEYSIZE,BLOWFISH_init_key,BLOWFISH_cleanup_key,
					BLOWFISH_encrypt,BLOWFISH_decrypt},
	{"none",0,0,cnone_init_key,cnone_cleanup_key,
					cnone_encrypt,cnone_decrypt},
	{"none",0,0,cnone_init_key,cnone_cleanup_key,
					cnone_encrypt,cnone_decrypt},
	{"none",0,0,cnone_init_key,cnone_cleanup_key,
					cnone_encrypt,cnone_decrypt},
	{"none",0,0,cnone_init_key,cnone_cleanup_key,
					cnone_encrypt,cnone_decrypt},
	{"none",0,0,cnone_init_key,cnone_cleanup_key,
					cnone_encrypt,cnone_decrypt},
};

