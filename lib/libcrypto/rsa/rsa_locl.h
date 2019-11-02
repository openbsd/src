/* $OpenBSD: rsa_locl.h,v 1.11 2019/11/02 13:47:41 jsing Exp $ */

__BEGIN_HIDDEN_DECLS

#define RSA_MIN_MODULUS_BITS	512

/* Macros to test if a pkey or ctx is for a PSS key */
#define pkey_is_pss(pkey) (pkey->ameth->pkey_id == EVP_PKEY_RSA_PSS)
#define pkey_ctx_is_pss(ctx) (ctx->pmeth->pkey_id == EVP_PKEY_RSA_PSS)

RSA_PSS_PARAMS *rsa_pss_params_create(const EVP_MD *sigmd, const EVP_MD *mgf1md,
    int saltlen);
int rsa_pss_get_param(const RSA_PSS_PARAMS *pss, const EVP_MD **pmd,
    const EVP_MD **pmgf1md, int *psaltlen);

extern int int_rsa_verify(int dtype, const unsigned char *m,
    unsigned int m_len, unsigned char *rm, size_t *prm_len,
    const unsigned char *sigbuf, size_t siglen, RSA *rsa);

__END_HIDDEN_DECLS
