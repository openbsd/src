/* $OpenBSD: rsa_locl.h,v 1.9 2019/11/01 03:45:13 jsing Exp $ */

__BEGIN_HIDDEN_DECLS

#define RSA_MIN_MODULUS_BITS	512

/* Macros to test if a pkey or ctx is for a PSS key */
#define pkey_is_pss(pkey) (pkey->ameth->pkey_id == EVP_PKEY_RSA_PSS)
#define pkey_ctx_is_pss(ctx) (ctx->pmeth->pkey_id == EVP_PKEY_RSA_PSS)

RSA_PSS_PARAMS *rsa_pss_params_create(const EVP_MD *sigmd, const EVP_MD *mgf1md,
    int saltlen);
int rsa_pss_get_param(const RSA_PSS_PARAMS *pss, const EVP_MD **pmd,
    const EVP_MD **pmgf1md, int *psaltlen);

typedef struct rsa_oaep_params_st {
	X509_ALGOR *hashFunc;
	X509_ALGOR *maskGenFunc;
	X509_ALGOR *pSourceFunc;

	/* Hash algorithm decoded from maskGenFunc. */
	X509_ALGOR *maskHash;
} RSA_OAEP_PARAMS;

RSA_OAEP_PARAMS *RSA_OAEP_PARAMS_new(void);
void RSA_OAEP_PARAMS_free(RSA_OAEP_PARAMS *a);
RSA_OAEP_PARAMS *d2i_RSA_OAEP_PARAMS(RSA_OAEP_PARAMS **a, const unsigned char **in, long len);
int i2d_RSA_OAEP_PARAMS(RSA_OAEP_PARAMS *a, unsigned char **out);
extern const ASN1_ITEM RSA_OAEP_PARAMS_it;

extern int int_rsa_verify(int dtype, const unsigned char *m,
    unsigned int m_len, unsigned char *rm, size_t *prm_len,
    const unsigned char *sigbuf, size_t siglen, RSA *rsa);

int RSA_padding_add_PKCS1_OAEP_mgf1(unsigned char *to, int tlen,
    const unsigned char *from, int flen, const unsigned char *param, int plen,
    const EVP_MD *md, const EVP_MD *mgf1md);
int RSA_padding_check_PKCS1_OAEP_mgf1(unsigned char *to, int tlen,
    const unsigned char *from, int flen, int num, const unsigned char *param,
    int plen, const EVP_MD *md, const EVP_MD *mgf1md);

__END_HIDDEN_DECLS
