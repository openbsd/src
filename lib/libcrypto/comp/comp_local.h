/* $OpenBSD: comp_local.h,v 1.2 2022/01/14 08:21:12 tb Exp $ */

#ifndef HEADER_COMP_LOCAL_H
#define HEADER_COMP_LOCAL_H

__BEGIN_HIDDEN_DECLS

struct CMP_CTX;

struct comp_method_st {
	int type;		/* NID for compression library */
	const char *name;	/* A text string to identify the library */
	int (*init)(COMP_CTX *ctx);
	void (*finish)(COMP_CTX *ctx);
	int (*compress)(COMP_CTX *ctx, unsigned char *out, unsigned int olen,
	    unsigned char *in, unsigned int ilen);
	int (*expand)(COMP_CTX *ctx, unsigned char *out, unsigned int olen,
	    unsigned char *in, unsigned int ilen);
	/* The following two do NOTHING, but are kept for backward compatibility */
	long (*ctrl)(void);
	long (*callback_ctrl)(void);
} /* COMP_METHOD */;

struct comp_ctx_st {
	COMP_METHOD *meth;
	unsigned long compress_in;
	unsigned long compress_out;
	unsigned long expand_in;
	unsigned long expand_out;

	CRYPTO_EX_DATA	ex_data;
} /* COMP_CTX */;

__END_HIDDEN_DECLS

#endif /* !HEADER_COMP_LOCAL_H */
