/*	$OpenBSD: cryptodev.h,v 1.14 2001/08/28 12:20:43 ben Exp $	*/

/*
 * The author of this code is Angelos D. Keromytis (angelos@cis.upenn.edu)
 *
 * This code was written by Angelos D. Keromytis in Athens, Greece, in
 * February 2000. Network Security Technologies Inc. (NSTI) kindly
 * supported the development of this code.
 *
 * Copyright (c) 2000 Angelos D. Keromytis
 *
 * Permission to use, copy, and modify this software without fee
 * is hereby granted, provided that this entire notice is included in
 * all source code copies of any software which is or includes a copy or
 * modification of this software. 
 *
 * THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTY. IN PARTICULAR, NONE OF THE AUTHORS MAKES ANY
 * REPRESENTATION OR WARRANTY OF ANY KIND CONCERNING THE
 * MERCHANTABILITY OF THIS SOFTWARE OR ITS FITNESS FOR ANY PARTICULAR
 * PURPOSE.
 *
 * Copyright (c) 2001 Theo de Raadt
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _CRYPTO_CRYPTO_H_
#define _CRYPTO_CRYPTO_H_

#include <sys/ioccom.h>

/* Some initial values */
#define CRYPTO_DRIVERS_INITIAL	4
#define CRYPTO_SW_SESSIONS	32

/* HMAC values */
#define HMAC_BLOCK_LEN		64
#define HMAC_IPAD_VAL		0x36
#define HMAC_OPAD_VAL		0x5C

/* Encryption algorithm block sizes */
#define DES_BLOCK_LEN		8
#define DES3_BLOCK_LEN		8
#define BLOWFISH_BLOCK_LEN	8
#define SKIPJACK_BLOCK_LEN	8
#define CAST128_BLOCK_LEN	8
#define RIJNDAEL128_BLOCK_LEN	16
#define EALG_MAX_BLOCK_LEN	16 /* Keep this updated */

/* Maximum hash algorithm result length */
#define AALG_MAX_RESULT_LEN	20 /* Keep this updated */

#define CRYPTO_DES_CBC		1
#define CRYPTO_3DES_CBC		2
#define CRYPTO_BLF_CBC		3
#define CRYPTO_CAST_CBC		4
#define CRYPTO_SKIPJACK_CBC	5
#define CRYPTO_MD5_HMAC		6
#define CRYPTO_SHA1_HMAC	7
#define CRYPTO_RIPEMD160_HMAC	8
#define CRYPTO_MD5_KPDK		9
#define CRYPTO_SHA1_KPDK	10
#define CRYPTO_RIJNDAEL128_CBC	11 /* 128 bit blocksize */
#define CRYPTO_AES_CBC		11 /* 128 bit blocksize -- the same as above */
#define CRYPTO_ARC4		19
#define CRYPTO_MD5		20
#define CRYPTO_SHA1		21

/* Begin public key additions */
#define CRYPTO_DH_SEND		12 /* Compute public value */
#define CRYPTO_DH_RECEIVE	13 /* Compute DH shared secret */
#define CRYPTO_RSA_ENCRYPT	14 /* RSA public key encryption */
#define CRYPTO_RSA_DECRYPT	15 /* RSA public key decryption */
#define CRYPTO_DSA_SIGN		16 /* DSA sign */
#define CRYPTO_DSA_VERIFY	17 /* DSA verify */

/* Compression */
#define CRYPTO_DEFLATE_COMP	18 /* Deflate compression algorithm */

#define CRYPTO_ALGORITHM_MAX	21 /* Keep updated - see below */

/* Algorithm flags */
#define	CRYPTO_ALG_FLAG_SUPPORTED	0x00000001 /* Algorithm is supported */
#define	CRYPTO_ALG_FLAG_RNG_ENABLE	0x00000002 /* Has HW RNG for DH/DSA */
#define	CRYPTO_ALG_FLAG_DSA_SHA		0x00000004 /* Can do SHA on msg */

#define SYMMETRIC		0
#define PUBLIC_KEY		1

/* 
 * Diffie-Hellman structure which defines fields needed to operate on the
 * input. Should be passed in the cryptoini->cri_key field.
 */
struct DH_key {
	/* 
	 * CRYPTO_DH_SEND - Enable or disable the random number generator.
	 * If disabled, private key and length should be stored in
	 * DH_buf; otherwise only the length is needed and the generated
	 * private key is stored in DH_buf->priv_key.
	 */
    
	/* Length of key-related variables */
	u_int16_t	dhk_gen_length;	/* SEND - generator length */
	u_int16_t	dhk_mod_length;	/* SEND/RECEIVE - modulus length */
 
	/* Input/output buffers for key generation */
	caddr_t		dhk_generator;	/* SEND - generator to use */
	caddr_t		dhk_modulus;	/* SEND/RECEIVE - modulus to use */   
};  

/*
 * These are inputs for DH processing - the private keys and public
 * keys are stored here because For DH-Send, if RNG_ENABLE, the
 * private key does not have to be provided.  Should be passed to the
 * cryptop->crp_buf.
 */
struct DH_buf {
	/* Length of variables */
	u_int16_t dh_public_key_length;	/* SEND/RECEIVE - public value len */
	u_int16_t dh_ss_key_length;	/* RECEIVE - shared secret key len */
	u_int16_t dh_priv_key_length;	/* SEND/RECEIVE - Private key length */
    
	/* Input/output buffers */
	caddr_t	dh_priv_key; /* 
			      * Buffer for private key the private key
			      * buffer is placed here because it can
			      * be both an input and an output. If this
			      * is left empty, the crypto framework or
			      * the underlying hardware will provide it for
			      * SEND. Must be present on RECEIVE.
			      */
	caddr_t	dh_pub_key; /* SEND/RECEIVE - I/O buffer for public key */
	caddr_t	dh_ss_key;  /* RECEIVE - output buffer for shared secret key */
};

/* 
 * RSA structure which defines fields needed to operate on the input.
 * Should be passed to the cryptoini->cri_key field.
 */
struct RSA_key {
	/* Length of variables (in bits) */
	u_int16_t 	rsak_exponent_length;  	/* Length of exponent (e) */
	u_int16_t	rsak_mod_length;	/* Length of modulus */
	u_int16_t	rsak_p_length;		/* Length of p */
	u_int16_t	rsak_q_length;		/* Length of q */
	u_int16_t 	rsak_dp_length;		/* Length of CRT dp */
	u_int16_t	rsak_dq_length;		/* Length of CRT dq */
	u_int16_t	rsak_qinv_length;	/* Length of CRT qinv */

	/* Input/output buffers */
	caddr_t		rsak_exponent;
	caddr_t		rsak_modulus;
	caddr_t		rsak_p;
	caddr_t		rsak_q;
	caddr_t		rsak_dp;
	caddr_t		rsak_dq;
	caddr_t		rsak_qinv;
};
  
/*
 * These are inputs for RSA processing - they are the data buffers for
 * the input and output message. Should be passed through cryptop->crp_buf.
 */
struct RSA_buf {
	u_int16_t	rsa_in_buf_length;	/* Length of input buffer */
	u_int16_t	rsa_out_buf_length;	/* Length of output buffer */

	caddr_t		rsa_in_buf;		/* Input message buffer */
	caddr_t		rsa_out_buf;		/* Output message buffer */
};

/*
 * DSA structure which defines fields needed to operate on the input.
 * Should be passed to the cyprtonini->cri_key field.
 */    
struct DSA_key {
	u_int16_t	dsak_p_length;	/* Length of modulus p */

	caddr_t		dsak_generator;	/* Generator to use, dsak_p_length */
	caddr_t		dsak_mod_q;	/* Modulus q to use, 160 bits */
	caddr_t		dsak_mod_p;	/* Modulus p to use, dsak_p_length */
	caddr_t		dsak_pub_key;	/* VERIFY - public key, dsak_p_length */
	caddr_t		dsak_priv_key;	/* SIGN - private key, 160 bits */
};

/*
 * DSA structure which defines the input and output buffers.
 * Should be passed to the cryptop->crp_buf field.
 */
struct DSA_buf {
	u_int16_t	dsa_msg_len;	/* Message length */

	/* r,s,v are all 160 bits */
	caddr_t		dsa_r_param;	/* Input for VERIFY; output for SIGN */
	caddr_t		dsa_s_param;	/* Input for VERIFY; output for SIGN */
	caddr_t		dsa_v_param;	/* Output for VERIFY; should be
					 * compared against r_param. */
	caddr_t		dsa_msg_buf;	/* Message buffer (hash or message) */
	caddr_t		dsa_rnd_num;	/* Random value from SW, 160 bits;
					 * if not provided, framework will
					 * provide one.
					 */
};

/* Standard initialization structure beginning */
struct cryptoini {
	int		cri_alg;	/* Algorithm to use */
	int		cri_klen;	/* Key length, in bits */
	int		cri_rnd;	/* Algorithm rounds, where relevant */
	caddr_t		cri_key;	/* key to use */
	u_int8_t	cri_iv[EALG_MAX_BLOCK_LEN];	/* IV to use */
	struct cryptoini *cri_next;
};

/* Describe boundaries of a single crypto operation */
struct cryptodesc {
	int		crd_skip;	/* How many bytes to ignore from start */
	int		crd_len;	/* How many bytes to process */
	int		crd_inject;	/* Where to inject results, if applicable */
	int		crd_flags;

#define	CRD_F_ENCRYPT		0x01	/* Set when doing encryption */
#define	CRD_F_IV_PRESENT	0x02	/* When encrypting, IV is already in
					   place, so don't copy. */
#define	CRD_F_IV_EXPLICIT	0x04	/* IV explicitly provided */
#define	CRD_F_DSA_SHA_NEEDED	0x08	/* Compute SHA-1 of buffer for DSA */
#define CRD_F_COMP		0x0f    /* Set when doing compression */

	struct cryptoini	CRD_INI; /* Initialization/context data */
#define crd_iv		CRD_INI.cri_iv
#define crd_key		CRD_INI.cri_key
#define crd_rnd		CRD_INI.cri_rnd
#define crd_alg		CRD_INI.cri_alg
#define crd_klen	CRD_INI.cri_klen

	struct cryptodesc *crd_next;
};

/* Structure describing complete operation */
struct cryptop {
	u_int64_t	crp_sid;	/* Session ID */
	int		crp_ilen;	/* Input data total length */
	int		crp_olen;	/* Result total length */
	int		crp_alloctype;	/* Type of buf to allocate if needed */

	int		crp_etype;	/*
					 * Error type (zero means no error).
					 * All error codes except EAGAIN
					 * indicate possible data corruption (as in,
					 * the data have been touched). On all
					 * errors, the crp_sid may have changed
					 * (reset to a new one), so the caller
					 * should always check and use the new
					 * value on future requests.
					 */
	int		crp_flags;

#define CRYPTO_F_IMBUF	0x0001	/* Input/output are mbuf chains, otherwise contig */
#define CRYPTO_F_IOV	0x0002	/* Input/output are uio */
#define CRYPTO_F_REL	0x0004	/* Must return data in same place */

	caddr_t		crp_buf;	/* Data to be processed */
	caddr_t		crp_opaque;	/* Opaque pointer, passed along */
	struct cryptodesc *crp_desc;	/* Linked list of processing descriptors */

	int (*crp_callback)(struct cryptop *); /* Callback function */

	struct cryptop	*crp_next;
	caddr_t		crp_mac;
};

#define CRYPTO_BUF_CONTIG	0x1
#define CRYPTO_BUF_MBUF		0x2

#define CRYPTO_OP_DECRYPT	0x0
#define CRYPTO_OP_ENCRYPT	0x1

/* Crypto capabilities structure */
struct cryptocap {
	u_int32_t	cc_sessions;

	/* 
	 * Largest possible operator length (in bits) for each type of
	 * encryption algorithm - especially important for public key
	 * operations.
	 */
	u_int16_t	cc_max_op_len[CRYPTO_ALGORITHM_MAX + 1]; 

	u_int8_t	cc_alg[CRYPTO_ALGORITHM_MAX + 1];

	u_int8_t	cc_flags;
#define CRYPTOCAP_F_CLEANUP   0x1
#define CRYPTOCAP_F_SOFTWARE  0x02

	int		(*cc_newsession) (u_int32_t *, struct cryptoini *);
	int		(*cc_process) (struct cryptop *);
	int		(*cc_freesession) (u_int64_t);
};

struct session_op {
	u_int32_t	cipher;		/* ie. CRYPTO_DES_CBC */
	u_int32_t	mac;		/* ie. CRYPTO_MD5_HMAC */

	u_int32_t	keylen;		/* cipher key */
	caddr_t		key;
	int		mackeylen;	/* mac key */
	caddr_t		mackey;

  	u_int32_t	ses;		/* returns: session # */ 
};

struct crypt_op {
	u_int32_t	ses;
	u_int16_t	op;
	u_int16_t	flags;		/* always 0 */

	u_int		len;
	caddr_t		src, dst;	/* become iov[] inside kernel */
	caddr_t		mac;		/* must be big enough for chosen MAC */
	caddr_t		iv;
};

#define CRYPTO_MAX_MAC_LEN	20

#define COP_ENCRYPT	1
#define COP_DECRYPT	2
/* #define COP_SETKEY	3 */
/* #define COP_GETKEY	4 */

#define	CRIOGET		_IOWR('c', 100, u_int32_t)

#define	CIOCGSESSION	_IOWR('c', 101, struct session_op)
#define	CIOCFSESSION	_IOW('c', 102, u_int32_t)
#define CIOCCRYPT	_IOWR('c', 103, struct crypt_op)

#ifdef _KERNEL
int	crypto_check_alg(struct cryptoini *);
int	crypto_newsession(u_int64_t *, struct cryptoini *, int);
int	crypto_freesession(u_int64_t);
int	crypto_dispatch(struct cryptop *);
int	crypto_register(u_int32_t, int, u_int16_t, u_int32_t, 
	    int (*)(u_int32_t *, struct cryptoini *), int (*)(u_int64_t),
	    int (*)(struct cryptop *));
int	crypto_unregister(u_int32_t, int);
int32_t	crypto_get_driverid(void);
void	crypto_thread(void);
int	crypto_invoke(struct cryptop *);
void	crypto_done(struct cryptop *);
int	crypto_check_alg(struct cryptoini *);

struct mbuf;
int	mbuf2pages __P((struct mbuf *, int *, long *, int *, int, int *));
int	iov2pages __P((struct uio *, int *, long *, int *, int, int *));
void	cuio_copydata __P((struct uio *, int, int, caddr_t));
void	cuio_copyback __P((struct uio *, int, int, caddr_t));

struct	cryptop *crypto_getreq(int);
void	crypto_freereq(struct cryptop *);
#endif /* _KERNEL */
#endif /* _CRYPTO_CRYPTO_H_ */
