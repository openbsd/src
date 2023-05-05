/* $OpenBSD: rsa_local.h,v 1.2 2023/05/05 12:21:44 tb Exp $ */

__BEGIN_HIDDEN_DECLS

#define RSA_MIN_MODULUS_BITS	512

/* Macros to test if a pkey or ctx is for a PSS key */
#define pkey_is_pss(pkey) (pkey->ameth->pkey_id == EVP_PKEY_RSA_PSS)
#define pkey_ctx_is_pss(ctx) (ctx->pmeth->pkey_id == EVP_PKEY_RSA_PSS)

struct rsa_meth_st {
	char *name;
	int (*rsa_pub_enc)(int flen, const unsigned char *from,
	    unsigned char *to, RSA *rsa, int padding);
	int (*rsa_pub_dec)(int flen, const unsigned char *from,
	    unsigned char *to, RSA *rsa, int padding);
	int (*rsa_priv_enc)(int flen, const unsigned char *from,
	    unsigned char *to, RSA *rsa, int padding);
	int (*rsa_priv_dec)(int flen, const unsigned char *from,
	    unsigned char *to, RSA *rsa, int padding);
	int (*rsa_mod_exp)(BIGNUM *r0, const BIGNUM *I, RSA *rsa,
	    BN_CTX *ctx); /* Can be null */
	int (*bn_mod_exp)(BIGNUM *r, const BIGNUM *a, const BIGNUM *p,
	    const BIGNUM *m, BN_CTX *ctx, BN_MONT_CTX *m_ctx); /* Can be null */
	int (*init)(RSA *rsa);		/* called at new */
	int (*finish)(RSA *rsa);	/* called at free */
	int flags;			/* RSA_METHOD_FLAG_* things */
	char *app_data;			/* may be needed! */
/* New sign and verify functions: some libraries don't allow arbitrary data
 * to be signed/verified: this allows them to be used. Note: for this to work
 * the RSA_public_decrypt() and RSA_private_encrypt() should *NOT* be used
 * RSA_sign(), RSA_verify() should be used instead. Note: for backwards
 * compatibility this functionality is only enabled if the RSA_FLAG_SIGN_VER
 * option is set in 'flags'.
 */
	int (*rsa_sign)(int type, const unsigned char *m, unsigned int m_length,
	    unsigned char *sigret, unsigned int *siglen, const RSA *rsa);
	int (*rsa_verify)(int dtype, const unsigned char *m,
	    unsigned int m_length, const unsigned char *sigbuf,
	    unsigned int siglen, const RSA *rsa);
/* If this callback is NULL, the builtin software RSA key-gen will be used. This
 * is for behavioural compatibility whilst the code gets rewired, but one day
 * it would be nice to assume there are no such things as "builtin software"
 * implementations. */
	int (*rsa_keygen)(RSA *rsa, int bits, BIGNUM *e, BN_GENCB *cb);
};

struct rsa_st {
	/* The first parameter is used to pickup errors where
	 * this is passed instead of aEVP_PKEY, it is set to 0 */
	int pad;
	long version;
	const RSA_METHOD *meth;

	/* functional reference if 'meth' is ENGINE-provided */
	ENGINE *engine;
	BIGNUM *n;
	BIGNUM *e;
	BIGNUM *d;
	BIGNUM *p;
	BIGNUM *q;
	BIGNUM *dmp1;
	BIGNUM *dmq1;
	BIGNUM *iqmp;

	/* Parameter restrictions for PSS only keys. */
	RSA_PSS_PARAMS *pss;

	/* be careful using this if the RSA structure is shared */
	CRYPTO_EX_DATA ex_data;
	int references;
	int flags;

	/* Used to cache montgomery values */
	BN_MONT_CTX *_method_mod_n;
	BN_MONT_CTX *_method_mod_p;
	BN_MONT_CTX *_method_mod_q;

	/* all BIGNUM values are actually in the following data, if it is not
	 * NULL */
	BN_BLINDING *blinding;
	BN_BLINDING *mt_blinding;
};

RSA_PSS_PARAMS *rsa_pss_params_create(const EVP_MD *sigmd, const EVP_MD *mgf1md,
    int saltlen);
int rsa_pss_get_param(const RSA_PSS_PARAMS *pss, const EVP_MD **pmd,
    const EVP_MD **pmgf1md, int *psaltlen);

extern int int_rsa_verify(int dtype, const unsigned char *m,
    unsigned int m_len, unsigned char *rm, size_t *prm_len,
    const unsigned char *sigbuf, size_t siglen, RSA *rsa);

int RSA_padding_add_X931(unsigned char *to, int tlen,
    const unsigned char *f, int fl);
int RSA_padding_check_X931(unsigned char *to, int tlen,
    const unsigned char *f, int fl, int rsa_len);
int RSA_X931_hash_id(int nid);

__END_HIDDEN_DECLS
