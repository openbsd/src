/* $OpenBSD: signature.c,v 1.18 2006/12/16 06:18:35 ray Exp $ */
/*
 * The author of this code is Angelos D. Keromytis (angelos@dsl.cis.upenn.edu)
 *
 * This code was written by Angelos D. Keromytis in Philadelphia, PA, USA,
 * in April-May 1998
 *
 * Copyright (C) 1998, 1999 by Angelos D. Keromytis.
 *	
 * Permission to use, copy, and modify this software with or without fee
 * is hereby granted, provided that this entire notice is included in
 * all copies of any software which is or includes a copy or
 * modification of this software. 
 *
 * THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTY. IN PARTICULAR, THE AUTHORS MAKES NO
 * REPRESENTATION OR WARRANTY OF ANY KIND CONCERNING THE
 * MERCHANTABILITY OF THIS SOFTWARE OR ITS FITNESS FOR ANY PARTICULAR
 * PURPOSE.
 */

/*
 * Support for X509 keys and signing added by Ben Laurie <ben@algroup.co.uk>
 * 3 May 1999
 */

#include <sys/types.h>

#include <limits.h>
#include <regex.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <openssl/dsa.h>
#include <openssl/md5.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/sha.h>
#include <openssl/x509.h>

#include "keynote.h"
#include "assertion.h"
#include "signature.h"

static const char hextab[] = {
     '0', '1', '2', '3', '4', '5', '6', '7',
     '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'
}; 

/*
 * Actual conversion to hex.
 */   
static void
bin2hex(unsigned char *data, unsigned char *buffer, int len)
{
    int off = 0;
     
    while(len > 0) 
    {
	buffer[off++] = hextab[*data >> 4];
	buffer[off++] = hextab[*data & 0xF];
	data++;
	len--;
    }
}

/*
 * Encode a binary string with hex encoding. Return 0 on success.
 */
int
kn_encode_hex(unsigned char *buf, char **dest, int len)
{
    keynote_errno = 0;
    if (dest == (char **) NULL)
    {
	keynote_errno = ERROR_SYNTAX;
	return -1;
    }

    *dest = (char *) calloc(2 * len + 1, sizeof(char));
    if (*dest == (char *) NULL)
    {
	keynote_errno = ERROR_MEMORY;
	return -1;
    }

    bin2hex(buf, *dest, len);
    return 0;
}

/*
 * Decode a hex encoding. Return 0 on success. The second argument
 * will be half as large as the first.
 */
int
kn_decode_hex(char *hex, char **dest)
{
    int i, decodedlen;
    char ptr[3];

    keynote_errno = 0;
    if (dest == (char **) NULL)
    {
	keynote_errno = ERROR_SYNTAX;
	return -1;
    }

    if (strlen(hex) % 2)			/* Should be even */
    {
	keynote_errno = ERROR_SYNTAX;
	return -1;
    }

    decodedlen = strlen(hex) / 2;
    *dest = (char *) calloc(decodedlen, sizeof(char));
    if (*dest == (char *) NULL)
    {
	keynote_errno = ERROR_MEMORY;
	return -1;
    }

    ptr[2] = '\0';
    for (i = 0; i < decodedlen; i++)
    {
	ptr[0] = hex[2 * i];
	ptr[1] = hex[(2 * i) + 1];
      	(*dest)[i] = (unsigned char) strtoul(ptr, (char **) NULL, 16);
    }

    return 0;
}

void
keynote_free_key(void *key, int type)
{
    if (key == (void *) NULL)
      return;

    /* DSA keys */
    if (type == KEYNOTE_ALGORITHM_DSA)
    {
	DSA_free(key);
	return;
    }

    /* RSA keys */
    if (type == KEYNOTE_ALGORITHM_RSA)
    {
	RSA_free(key);
	return;
    }

    /* X509 keys */
    if (type == KEYNOTE_ALGORITHM_X509)
    {
	RSA_free(key); /* RSA-specific */
	return;
    }

    /* BINARY keys */
    if (type == KEYNOTE_ALGORITHM_BINARY)
    {
	free(((struct keynote_binary *) key)->bn_key);
	free(key);
	return;
    }

    /* Catch-all case */
    if (type == KEYNOTE_ALGORITHM_NONE)
      free(key);
}

/*
 * Map a signature to an algorithm. Return algorithm number (defined in
 * keynote.h), or KEYNOTE_ALGORITHM_NONE if unknown.
 * Also return in the second, third and fourth arguments the digest
 * algorithm, ASCII and internal encodings respectively.
 */
static int
keynote_get_sig_algorithm(char *sig, int *hash, int *enc, int *internal)
{
    if (sig == (char *) NULL)
      return KEYNOTE_ALGORITHM_NONE;

    if (!strncasecmp(SIG_DSA_SHA1_HEX, sig, SIG_DSA_SHA1_HEX_LEN))
    {
	*hash = KEYNOTE_HASH_SHA1;
	*enc = ENCODING_HEX;
	*internal = INTERNAL_ENC_ASN1;
	return KEYNOTE_ALGORITHM_DSA;
    }

    if (!strncasecmp(SIG_DSA_SHA1_BASE64, sig, SIG_DSA_SHA1_BASE64_LEN))
    {
	*hash = KEYNOTE_HASH_SHA1;
	*enc = ENCODING_BASE64;
	*internal = INTERNAL_ENC_ASN1;
	return KEYNOTE_ALGORITHM_DSA;
    }

    if (!strncasecmp(SIG_RSA_MD5_PKCS1_HEX, sig, SIG_RSA_MD5_PKCS1_HEX_LEN))
    {
	*hash = KEYNOTE_HASH_MD5;
	*enc = ENCODING_HEX;
	*internal = INTERNAL_ENC_PKCS1;
	return KEYNOTE_ALGORITHM_RSA;
    }

    if (!strncasecmp(SIG_RSA_SHA1_PKCS1_HEX, sig, SIG_RSA_SHA1_PKCS1_HEX_LEN))
    {
	*hash = KEYNOTE_HASH_SHA1;
	*enc = ENCODING_HEX;
	*internal = INTERNAL_ENC_PKCS1;
	return KEYNOTE_ALGORITHM_RSA;
    }

    if (!strncasecmp(SIG_RSA_MD5_PKCS1_BASE64, sig,
                     SIG_RSA_MD5_PKCS1_BASE64_LEN))
    {
	*hash = KEYNOTE_HASH_MD5;
	*enc = ENCODING_BASE64;
	*internal = INTERNAL_ENC_PKCS1;
	return KEYNOTE_ALGORITHM_RSA;
    }

    if (!strncasecmp(SIG_RSA_SHA1_PKCS1_BASE64, sig,
                     SIG_RSA_SHA1_PKCS1_BASE64_LEN))
    {
	*hash = KEYNOTE_HASH_SHA1;
	*enc = ENCODING_BASE64;
	*internal = INTERNAL_ENC_PKCS1;
	return KEYNOTE_ALGORITHM_RSA;
    }

    if (!strncasecmp(SIG_X509_SHA1_BASE64, sig, SIG_X509_SHA1_BASE64_LEN))
    {
	*hash = KEYNOTE_HASH_SHA1;
	*enc = ENCODING_BASE64;
	*internal = INTERNAL_ENC_ASN1;
	return KEYNOTE_ALGORITHM_X509;
    }

    if (!strncasecmp(SIG_X509_SHA1_HEX, sig, SIG_X509_SHA1_HEX_LEN))
    {
	*hash = KEYNOTE_HASH_SHA1;
	*enc = ENCODING_HEX;
	*internal = INTERNAL_ENC_ASN1;
	return KEYNOTE_ALGORITHM_X509;
    }

#if 0 /* Not supported yet */
    if (!strncasecmp(SIG_ELGAMAL_SHA1_HEX, sig, SIG_ELGAMAL_SHA1_HEX_LEN))
    {
	*hash = KEYNOTE_HASH_SHA1;
	*enc = ENCODING_HEX;
	*internal = INTERNAL_ENC_ASN1;
	return KEYNOTE_ALGORITHM_ELGAMAL;
    }

    if (!strncasecmp(SIG_ELGAMAL_SHA1_BASE64, sig,
		     SIG_ELGAMAL_SHA1_BASE64_LEN))
    {
	*hash = KEYNOTE_HASH_SHA1;
	*enc = ENCODING_BASE64;
	*internal = INTERNAL_ENC_ASN1;
	return KEYNOTE_ALGORITHM_ELGAMAL;
    }
#endif /* 0 */

    *hash = KEYNOTE_HASH_NONE;
    *enc = ENCODING_NONE;
    *internal = INTERNAL_ENC_NONE;
    return KEYNOTE_ALGORITHM_NONE;
}

/*
 * Map a key to an algorithm. Return algorithm number (defined in
 * keynote.h), or KEYNOTE_ALGORITHM_NONE if unknown. 
 * This latter is also a valid algorithm (for logical tags). Also return
 * in the second and third arguments the ASCII and internal encodings.
 */
int
keynote_get_key_algorithm(char *key, int *encoding, int *internalencoding)
{
    if (!strncasecmp(DSA_HEX, key, DSA_HEX_LEN))
    {
	*internalencoding = INTERNAL_ENC_ASN1;
	*encoding = ENCODING_HEX;
	return KEYNOTE_ALGORITHM_DSA;
    }

    if (!strncasecmp(DSA_BASE64, key, DSA_BASE64_LEN))
    {
	*internalencoding = INTERNAL_ENC_ASN1;
	*encoding = ENCODING_BASE64;
	return KEYNOTE_ALGORITHM_DSA;
    }

    if (!strncasecmp(RSA_PKCS1_HEX, key, RSA_PKCS1_HEX_LEN))
    {
	*internalencoding = INTERNAL_ENC_PKCS1;
	*encoding = ENCODING_HEX;
	return KEYNOTE_ALGORITHM_RSA;
    }

    if (!strncasecmp(RSA_PKCS1_BASE64, key, RSA_PKCS1_BASE64_LEN))
    {
	*internalencoding = INTERNAL_ENC_PKCS1;
	*encoding = ENCODING_BASE64;
	return KEYNOTE_ALGORITHM_RSA;
    }

    if (!strncasecmp(X509_BASE64, key, X509_BASE64_LEN))
    {
	*internalencoding = INTERNAL_ENC_ASN1;
	*encoding = ENCODING_BASE64;
	return KEYNOTE_ALGORITHM_X509;
    }

    if (!strncasecmp(X509_HEX, key, X509_HEX_LEN))
    {
	*internalencoding = INTERNAL_ENC_ASN1;
	*encoding = ENCODING_HEX;
	return KEYNOTE_ALGORITHM_X509;
    }

#if 0 /* Not supported yet */
    if (!strncasecmp(ELGAMAL_HEX, key, ELGAMAL_HEX_LEN))
    {
	*internalencoding = INTERNAL_ENC_ASN1;
	*encoding = ENCODING_HEX;
	return KEYNOTE_ALGORITHM_ELGAMAL;
    }

    if (!strncasecmp(ELGAMAL_BASE64, key, ELGAMAL_BASE64_LEN))
    {
	*internalencoding = INTERNAL_ENC_ASN1;
	*encoding = ENCODING_BASE64;
	return KEYNOTE_ALGORITHM_ELGAMAL;
    }
#endif /* 0 */

    if (!strncasecmp(BINARY_HEX, key, BINARY_HEX_LEN))
    {
	*internalencoding = INTERNAL_ENC_NONE;
	*encoding = ENCODING_HEX;
	return KEYNOTE_ALGORITHM_BINARY;
    }
    
    if (!strncasecmp(BINARY_BASE64, key, BINARY_BASE64_LEN))
    {
	*internalencoding = INTERNAL_ENC_NONE;
	*encoding = ENCODING_BASE64;
	return KEYNOTE_ALGORITHM_BINARY;
    }
    
    *internalencoding = INTERNAL_ENC_NONE;
    *encoding = ENCODING_NONE;
    return KEYNOTE_ALGORITHM_NONE;
}

/*
 * Same as keynote_get_key_algorithm(), only verify that this is
 * a private key (just look at the prefix).
 */
static int
keynote_get_private_key_algorithm(char *key, int *encoding,
				  int *internalencoding)
{
    if (strncasecmp(KEYNOTE_PRIVATE_KEY_PREFIX, key, 
		    KEYNOTE_PRIVATE_KEY_PREFIX_LEN))
    {
	*internalencoding = INTERNAL_ENC_NONE;
	*encoding = ENCODING_NONE;
	return KEYNOTE_ALGORITHM_NONE;
    }

    return keynote_get_key_algorithm(key + KEYNOTE_PRIVATE_KEY_PREFIX_LEN,
				     encoding, internalencoding);
}

/*
 * Decode a string to a key. Return 0 on success.
 */
int
kn_decode_key(struct keynote_deckey *dc, char *key, int keytype)
{
    void *kk = (void *) NULL;
    X509 *px509Cert;
    EVP_PKEY *pPublicKey;
    unsigned char *ptr = (char *) NULL, *decoded = (char *) NULL;
    int encoding, internalencoding, len = 0;

    keynote_errno = 0;
    if (keytype == KEYNOTE_PRIVATE_KEY)
      dc->dec_algorithm = keynote_get_private_key_algorithm(key, &encoding,
							    &internalencoding);
    else
      dc->dec_algorithm = keynote_get_key_algorithm(key, &encoding,
						    &internalencoding);
    if (dc->dec_algorithm == KEYNOTE_ALGORITHM_NONE)
    {
	dc->dec_key = (void *) strdup(key);
	if (dc->dec_key == (void *) NULL)
	{
	    keynote_errno = ERROR_MEMORY;
	    return -1;
	}

	return 0;
    }

    key = strchr(key, ':'); /* Move forward, to the Encoding. We're guaranteed
			    * to have a ':' character, since this is a key */
    key++;

    /* Remove ASCII encoding */
    switch (encoding)
    {
	case ENCODING_NONE:
	    break;

	case ENCODING_HEX:
            len = strlen(key) / 2;
	    if (kn_decode_hex(key, (char **) &decoded) != 0)
	      return -1;
	    ptr = decoded;
	    break;

	case ENCODING_BASE64:
	    len = strlen(key);
	    if (len % 4)  /* Base64 encoding must be a multiple of 4 */
	    {
		keynote_errno = ERROR_SYNTAX;
		return -1;
	    }

	    len = 3 * (len / 4);
	    decoded = (unsigned char *) calloc(len, sizeof(unsigned char));
	    ptr = decoded;
	    if (decoded == (unsigned char *) NULL)
	    {
		keynote_errno = ERROR_MEMORY;
		return -1;
	    }

	    if ((len = kn_decode_base64(key, decoded, len)) == -1)
	      return -1;
	    break;

	case ENCODING_NATIVE:
	    decoded = strdup(key);
	    if (decoded == (unsigned char *) NULL)
	    {
		keynote_errno = ERROR_MEMORY;
		return -1;
	    }
	    len = strlen(key);
	    ptr = decoded;
	    break;

	default:
	    keynote_errno = ERROR_SYNTAX;
	    return -1;
    }

    /* DSA-HEX */
    if ((dc->dec_algorithm == KEYNOTE_ALGORITHM_DSA) &&
	(internalencoding == INTERNAL_ENC_ASN1))
    {
	dc->dec_key = DSA_new();
	if (dc->dec_key == (DSA *) NULL)
	{
	    keynote_errno = ERROR_MEMORY;
	    return -1;
	}

	kk = dc->dec_key;
	if (keytype == KEYNOTE_PRIVATE_KEY)
	{
	    if (d2i_DSAPrivateKey((DSA **) &kk,(const unsigned char **) &decoded, len) == (DSA *) NULL)
	    {
		if (ptr != (unsigned char *) NULL)
		  free(ptr);
		DSA_free(kk);
		keynote_errno = ERROR_SYNTAX; /* Could be a memory error */
		return -1;
	    }
	}
	else
	{
	    if (d2i_DSAPublicKey((DSA **) &kk, (const unsigned char **) &decoded, len) == (DSA *) NULL)
	    {
		if (ptr != (unsigned char *) NULL)
		  free(ptr);
		DSA_free(kk);
		keynote_errno = ERROR_SYNTAX; /* Could be a memory error */
		return -1;
	    }
	}

	if (ptr != (unsigned char *) NULL)
	  free(ptr);

	return 0;
    }

    /* RSA-PKCS1-HEX */
    if ((dc->dec_algorithm == KEYNOTE_ALGORITHM_RSA) &&
        (internalencoding == INTERNAL_ENC_PKCS1))
    {
        dc->dec_key = RSA_new();
        if (dc->dec_key == (RSA *) NULL)
        {
            keynote_errno = ERROR_MEMORY;
            return -1;
        }

        kk = dc->dec_key;
        if (keytype == KEYNOTE_PRIVATE_KEY)
        {
            if (d2i_RSAPrivateKey((RSA **) &kk, (const unsigned char **) &decoded, len) == (RSA *) NULL)
            {
                if (ptr != (unsigned char *) NULL)
                  free(ptr);
                RSA_free(kk);
                keynote_errno = ERROR_SYNTAX; /* Could be a memory error */
                return -1;
            }
	    if (RSA_blinding_on ((RSA *) kk, NULL) != 1)
	    {
                if (ptr != (unsigned char *) NULL)
                  free(ptr);
                RSA_free(kk);
                keynote_errno = ERROR_MEMORY;
                return -1;
	    }		
        }
        else
        {
            if (d2i_RSAPublicKey((RSA **) &kk, (const unsigned char **) &decoded, len) == (RSA *) NULL)
            {
                if (ptr != (unsigned char *) NULL)
                  free(ptr);
                RSA_free(kk);
                keynote_errno = ERROR_SYNTAX; /* Could be a memory error */
                return -1;
            }
        }

        if (ptr != (unsigned char *) NULL)
          free(ptr);

        return 0;
    }

    /* X509 Cert */
    if ((dc->dec_algorithm == KEYNOTE_ALGORITHM_X509) &&
	(internalencoding == INTERNAL_ENC_ASN1) &&
	(keytype == KEYNOTE_PUBLIC_KEY))
    {
	if ((px509Cert = X509_new()) == (X509 *) NULL)
	{
	    if (ptr)
	      free(ptr);
	    keynote_errno = ERROR_MEMORY;
	    return -1;
	}

	if(d2i_X509(&px509Cert, &decoded, len) == NULL)
	{
	    if (ptr)
	      free(ptr);
	    X509_free(px509Cert);
	    keynote_errno = ERROR_SYNTAX;
	    return -1;
	}

	if ((pPublicKey = X509_get_pubkey(px509Cert)) == (EVP_PKEY *) NULL)
	{
	    if (ptr)
	      free(ptr);
	    X509_free(px509Cert);
	    keynote_errno = ERROR_SYNTAX;
	    return -1;
	}

	/* RSA-specific */
	dc->dec_key = pPublicKey->pkey.rsa;

	if(ptr)
	  free(ptr);
	return 0;
    }    

    /* BINARY keys */
    if ((dc->dec_algorithm == KEYNOTE_ALGORITHM_BINARY) &&
	(internalencoding == INTERNAL_ENC_NONE))
    {
	dc->dec_key = (void *) calloc(1, sizeof(struct keynote_binary));
	if (dc->dec_key == (struct keynote_binary *) NULL)
	{
	    keynote_errno = ERROR_MEMORY;
	    return -1;
	}

	((struct keynote_binary *) dc->dec_key)->bn_key = decoded;
	((struct keynote_binary *) dc->dec_key)->bn_len = len;
	return RESULT_TRUE;
    }

    /* Add support for more algorithms here */

    if (ptr != (unsigned char *) NULL)
      free(ptr);

    /* This shouldn't ever be reached really */
    keynote_errno = ERROR_SYNTAX;
    return -1;
}

/*
 * Compare two keys for equality. Return RESULT_TRUE if equal,
 * RESULT_FALSE otherwise.
 */
int
kn_keycompare(void *key1, void *key2, int algorithm)
{
    DSA *p1, *p2;
    RSA *p3, *p4;
    struct keynote_binary *bn1, *bn2;

    if ((key1 == (void *) NULL) ||
	(key2 == (void *) NULL))
      return RESULT_FALSE;

    switch (algorithm)
    {
	case KEYNOTE_ALGORITHM_NONE:
	    if  (!strcmp((char *) key1, (char *) key2))
	      return RESULT_TRUE;
	    else
	      return RESULT_FALSE;
	    
	case KEYNOTE_ALGORITHM_DSA:
	    p1 = (DSA *) key1;
	    p2 = (DSA *) key2;
	    if (!BN_cmp(p1->p, p2->p) &&
		!BN_cmp(p1->q, p2->q) &&
		!BN_cmp(p1->g, p2->g) &&
		!BN_cmp(p1->pub_key, p2->pub_key))
	      return RESULT_TRUE;
	    else
	      return RESULT_FALSE;

	case KEYNOTE_ALGORITHM_X509:
            p3 = (RSA *) key1;
            p4 = (RSA *) key2;
            if (!BN_cmp(p3->n, p4->n) &&
                !BN_cmp(p3->e, p4->e))
              return RESULT_TRUE;
            else
	      return RESULT_FALSE;

	case KEYNOTE_ALGORITHM_RSA:
            p3 = (RSA *) key1;
            p4 = (RSA *) key2;
            if (!BN_cmp(p3->n, p4->n) &&
                !BN_cmp(p3->e, p4->e))
              return RESULT_TRUE;
            else
	      return RESULT_FALSE;

	case KEYNOTE_ALGORITHM_ELGAMAL:
	    /* Not supported yet */
	    return RESULT_FALSE;

	case KEYNOTE_ALGORITHM_PGP:
	    /* Not supported yet */
	    return RESULT_FALSE;

	case KEYNOTE_ALGORITHM_BINARY:
	    bn1 = (struct keynote_binary *) key1;
	    bn2 = (struct keynote_binary *) key2;
	    if ((bn1->bn_len == bn2->bn_len) &&
		!memcmp(bn1->bn_key, bn2->bn_key, bn1->bn_len))
	      return RESULT_TRUE;
	    else
	      return RESULT_FALSE;

	default:
	    return RESULT_FALSE;
    }
}

/*
 * Verify the signature on an assertion; return SIGRESULT_TRUE is
 * success, SIGRESULT_FALSE otherwise.
 */
int
keynote_sigverify_assertion(struct assertion *as)
{
    int hashtype, enc, intenc, alg = KEYNOTE_ALGORITHM_NONE, hashlen = 0;
    unsigned char *sig, *decoded = (char *) NULL, *ptr;
    unsigned char res2[20];
    SHA_CTX shscontext;
    MD5_CTX md5context;
    int len = 0;
    DSA *dsa;
    RSA *rsa;
    if ((as->as_signature == (char *) NULL) ||
	(as->as_startofsignature == (char *) NULL) ||
	(as->as_allbutsignature == (char *) NULL) ||
	(as->as_allbutsignature - as->as_startofsignature <= 0))
      return SIGRESULT_FALSE;

    alg = keynote_get_sig_algorithm(as->as_signature, &hashtype, &enc,
				    &intenc);
    if (alg == KEYNOTE_ALGORITHM_NONE)
      return SIGRESULT_FALSE;

    /* Check for matching algorithms */
    if ((alg != as->as_signeralgorithm) &&
	!((alg == KEYNOTE_ALGORITHM_RSA) &&
	  (as->as_signeralgorithm == KEYNOTE_ALGORITHM_X509)) &&
	!((alg == KEYNOTE_ALGORITHM_X509) &&
	  (as->as_signeralgorithm == KEYNOTE_ALGORITHM_RSA)))
      return SIGRESULT_FALSE;

    sig = strchr(as->as_signature, ':');   /* Move forward to the Encoding. We
					   * are guaranteed to have a ':'
					   * character, since this is a valid
					   * signature */
    sig++;

    switch (hashtype)
    {
	case KEYNOTE_HASH_SHA1:
	    hashlen = 20;
	    memset(res2, 0, hashlen);
	    SHA1_Init(&shscontext);
	    SHA1_Update(&shscontext, as->as_startofsignature,
			as->as_allbutsignature - as->as_startofsignature);
	    SHA1_Update(&shscontext, as->as_signature, 
			(char *) sig - as->as_signature);
	    SHA1_Final(res2, &shscontext);
	    break;
	    
	case KEYNOTE_HASH_MD5:
	    hashlen = 16;
	    memset(res2, 0, hashlen);
	    MD5_Init(&md5context);
	    MD5_Update(&md5context, as->as_startofsignature,
		       as->as_allbutsignature - as->as_startofsignature);
	    MD5_Update(&md5context, as->as_signature,
		       (char *) sig - as->as_signature);
	    MD5_Final(res2, &md5context);
	    break;

	case KEYNOTE_HASH_NONE:
	    break;
    }

    /* Remove ASCII encoding */
    switch (enc)
    {
	case ENCODING_NONE:
	    ptr = (char *) NULL;
	    break;

	case ENCODING_HEX:
	    len = strlen(sig) / 2;
	    if (kn_decode_hex(sig, (char **) &decoded) != 0)
	      return -1;
	    ptr = decoded;
	    break;

	case ENCODING_BASE64:
	    len = strlen(sig);
	    if (len % 4)  /* Base64 encoding must be a multiple of 4 */
	    {
		keynote_errno = ERROR_SYNTAX;
		return -1;
	    }

	    len = 3 * (len / 4);
	    decoded = (unsigned char *) calloc(len, sizeof(unsigned char));
	    ptr = decoded;
	    if (decoded == (unsigned char *) NULL)
	    {
		keynote_errno = ERROR_MEMORY;
		return -1;
	    }

	    len = kn_decode_base64(sig, decoded, len);
	    if ((len == -1) || (len == 0) || (len == 1))
	      return -1;
	    break;

	case ENCODING_NATIVE:
	    decoded = (unsigned char *) strdup(sig);
	    if (decoded == (unsigned char *) NULL)
	    {
		keynote_errno = ERROR_MEMORY;
		return -1;
	    }
	    len = strlen(sig);
	    ptr = decoded;
	    break;

	default:
	    keynote_errno = ERROR_SYNTAX;
	    return -1;
    }

    /* DSA */
    if ((alg == KEYNOTE_ALGORITHM_DSA) && (intenc == INTERNAL_ENC_ASN1))
    {
	dsa = (DSA *) as->as_authorizer;
	if (DSA_verify(0, res2, hashlen, decoded, len, dsa) == 1)
	{
	    if (ptr != (unsigned char *) NULL)
	      free(ptr);
	    return SIGRESULT_TRUE;
	}
    }
    else /* RSA */
      if ((alg == KEYNOTE_ALGORITHM_RSA) && (intenc == INTERNAL_ENC_PKCS1))
      {
          rsa = (RSA *) as->as_authorizer;
          if (RSA_verify_ASN1_OCTET_STRING(RSA_PKCS1_PADDING, res2, hashlen,
					   decoded, len, rsa) == 1)
          {
              if (ptr != (unsigned char *) NULL)
                free(ptr);
              return SIGRESULT_TRUE;
          }
      }
      else
	if ((alg == KEYNOTE_ALGORITHM_X509) && (intenc == INTERNAL_ENC_ASN1))
	{
	    /* RSA-specific */
	    rsa = (RSA *) as->as_authorizer;
	    if (RSA_verify(NID_shaWithRSAEncryption, res2, hashlen, decoded,
			   len, rsa) == 1)
	    {
		if (ptr != (unsigned char *) NULL)
		  free(ptr);
		return SIGRESULT_TRUE;
	    }
	}
    
    /* Handle more algorithms here */
    
    if (ptr != (unsigned char *) NULL)
      free(ptr);

    return SIGRESULT_FALSE;
}

/*
 * Sign an assertion.
 */
static char *
keynote_sign_assertion(struct assertion *as, char *sigalg, void *key,
		       int keyalg, int verifyflag)
{
    int slen, i, hashlen = 0, hashtype, alg, encoding, internalenc;
    unsigned char *sig = (char *) NULL, *finalbuf = (char *) NULL;
    unsigned char res2[LARGEST_HASH_SIZE], *sbuf = (char *) NULL;
    BIO *biokey = (BIO *) NULL;
    DSA *dsa = (DSA *) NULL;
    RSA *rsa = (RSA *) NULL;
    SHA_CTX shscontext;
    MD5_CTX md5context;
    int len;

    if ((as->as_signature_string_s == (char *) NULL) ||
	(as->as_startofsignature == (char *) NULL) ||
	(as->as_allbutsignature == (char *) NULL) ||
	(as->as_allbutsignature - as->as_startofsignature <= 0) ||
	(as->as_authorizer == (void *) NULL) ||
	(key == (void *) NULL) ||
	(as->as_signeralgorithm == KEYNOTE_ALGORITHM_NONE))
    {
	keynote_errno = ERROR_SYNTAX;
	return (char *) NULL;
    }

    alg = keynote_get_sig_algorithm(sigalg, &hashtype, &encoding,
				    &internalenc);
    if (((alg != as->as_signeralgorithm) &&
	 !((alg == KEYNOTE_ALGORITHM_RSA) &&
	   (as->as_signeralgorithm == KEYNOTE_ALGORITHM_X509)) &&
	 !((alg == KEYNOTE_ALGORITHM_X509) &&
	   (as->as_signeralgorithm == KEYNOTE_ALGORITHM_RSA))) ||
        ((alg != keyalg) &&
	 !((alg == KEYNOTE_ALGORITHM_RSA) &&
	   (keyalg == KEYNOTE_ALGORITHM_X509)) &&
	 !((alg == KEYNOTE_ALGORITHM_X509) &&
	   (keyalg == KEYNOTE_ALGORITHM_RSA))))
    {
	keynote_errno = ERROR_SYNTAX;
	return (char *) NULL;
    }

    sig = strchr(sigalg, ':');
    if (sig == (unsigned char *) NULL)
    {
	keynote_errno = ERROR_SYNTAX;
	return (char *) NULL;
    }

    sig++;

    switch (hashtype)
    {
	case KEYNOTE_HASH_SHA1:
    	    hashlen = 20;
	    memset(res2, 0, hashlen);
	    SHA1_Init(&shscontext);
	    SHA1_Update(&shscontext, as->as_startofsignature,
			as->as_allbutsignature - as->as_startofsignature);
	    SHA1_Update(&shscontext, sigalg, (char *) sig - sigalg);
	    SHA1_Final(res2, &shscontext);
	    break;
   
	case KEYNOTE_HASH_MD5:
	    hashlen = 16;
	    memset(res2, 0, hashlen);
	    MD5_Init(&md5context);
	    MD5_Update(&md5context, as->as_startofsignature,
		       as->as_allbutsignature - as->as_startofsignature);
	    MD5_Update(&md5context, sigalg, (char *) sig - sigalg);
	    MD5_Final(res2, &md5context);
	    break;

	case KEYNOTE_HASH_NONE:
	    break;
    }

    if ((alg == KEYNOTE_ALGORITHM_DSA) &&
	(hashtype == KEYNOTE_HASH_SHA1) &&
	(internalenc == INTERNAL_ENC_ASN1) &&
	((encoding == ENCODING_HEX) || (encoding == ENCODING_BASE64)))
    {
	dsa = (DSA *) key;
	sbuf = (unsigned char *) calloc(DSA_size(dsa), sizeof(unsigned char));
	if (sbuf == (unsigned char *) NULL)
	{
	    keynote_errno = ERROR_MEMORY;
	    return (char *) NULL;
	}

	if (DSA_sign(0, res2, hashlen, sbuf, &slen, dsa) <= 0)
	{
	    free(sbuf);
	    keynote_errno = ERROR_SYNTAX;
	    return (char *) NULL;
	}
    }
    else
      if ((alg == KEYNOTE_ALGORITHM_RSA) &&
          ((hashtype == KEYNOTE_HASH_SHA1) ||
           (hashtype == KEYNOTE_HASH_MD5)) &&
          (internalenc == INTERNAL_ENC_PKCS1) &&
          ((encoding == ENCODING_HEX) || (encoding == ENCODING_BASE64)))
      {
          rsa = (RSA *) key;
          sbuf = (unsigned char *) calloc(RSA_size(rsa),
                                          sizeof(unsigned char));
          if (sbuf == (unsigned char *) NULL)
          {
              keynote_errno = ERROR_MEMORY;
              return (char *) NULL;
          }

          if (RSA_sign_ASN1_OCTET_STRING(RSA_PKCS1_PADDING, res2, hashlen,
					 sbuf, &slen, rsa) <= 0)
          {
              free(sbuf);
              keynote_errno = ERROR_SYNTAX;
              return (char *) NULL;
          }
      }
    else
      if ((alg == KEYNOTE_ALGORITHM_X509) &&
	  (hashtype == KEYNOTE_HASH_SHA1) &&
	  (internalenc == INTERNAL_ENC_ASN1))
      {
	  if ((biokey = BIO_new(BIO_s_mem())) == (BIO *) NULL)
	  {
	      keynote_errno = ERROR_SYNTAX;
	      return (char *) NULL;
	  }
	  
	  if (BIO_write(biokey, key, strlen(key) + 1) <= 0)
	  {
	      BIO_free(biokey);
	      keynote_errno = ERROR_SYNTAX;
	      return (char *) NULL;
	  }

	  /* RSA-specific */
	  rsa = (RSA *) PEM_read_bio_RSAPrivateKey(biokey, NULL, NULL, NULL);
	  if (rsa == (RSA *) NULL)
	  {
	      BIO_free(biokey);
	      keynote_errno = ERROR_SYNTAX;
	      return (char *) NULL;
	  }

	  sbuf = calloc(RSA_size(rsa), sizeof(char));
	  if (sbuf == (unsigned char *) NULL)
	  {
	      BIO_free(biokey);
	      RSA_free(rsa);
	      keynote_errno = ERROR_MEMORY;
	      return (char *) NULL;
	  }

	  if (RSA_sign(NID_shaWithRSAEncryption, res2, hashlen, sbuf, &slen,
		       rsa) <= 0)
          {
	      BIO_free(biokey);
	      RSA_free(rsa);
	      free(sbuf);
	      keynote_errno = ERROR_SIGN_FAILURE;
	      return NULL;
	  }

	  BIO_free(biokey);
	  RSA_free(rsa);
      }
      else /* Other algorithms here */
      {
	  keynote_errno = ERROR_SYNTAX;
	  return (char *) NULL;
      }

    /* ASCII encoding */
    switch (encoding)
    {
	case ENCODING_HEX:
	    i = kn_encode_hex(sbuf, (char **) &finalbuf, slen);
	    free(sbuf);
	    if (i != 0)
	      return (char *) NULL;
	    break;

	case ENCODING_BASE64:
	    finalbuf = (unsigned char *) calloc(2 * slen,
						sizeof(unsigned char));
	    if (finalbuf == (unsigned char *) NULL)
	    {
		keynote_errno = ERROR_MEMORY;
		free(sbuf);
		return (char *) NULL;
	    }

	    if ((slen = kn_encode_base64(sbuf, slen, finalbuf, 
					 2 * slen)) == -1)
	    {
		free(sbuf);
		return (char *) NULL;
	    }
	    break;

	default:
	    free(sbuf);
	    keynote_errno = ERROR_SYNTAX;
	    return (char *) NULL;
    }

    /* Replace as->as_signature */
    len = strlen(sigalg) + strlen(finalbuf) + 1;
    as->as_signature = (char *) calloc(len, sizeof(char));
    if (as->as_signature == (char *) NULL)
    {
	free(finalbuf);
	keynote_errno = ERROR_MEMORY;
	return (char *) NULL;
    }

    /* Concatenate algorithm name and signature value */
    snprintf(as->as_signature, len, "%s%s", sigalg, finalbuf);
    free(finalbuf);
    finalbuf = as->as_signature;

    /* Verify the newly-created signature if requested */
    if (verifyflag)
    {
	/* Do the signature verification */
	if (keynote_sigverify_assertion(as) != SIGRESULT_TRUE)
	{
	    as->as_signature = (char *) NULL;
	    free(finalbuf);
	    if (keynote_errno == 0)
	      keynote_errno = ERROR_SYNTAX;
	    return (char *) NULL;
	}

	as->as_signature = (char *) NULL;
    }
    else
      as->as_signature = (char *) NULL;

    /* Everything ok */
    return (char *) finalbuf;
}

/*
 * Verify the signature on an assertion.
 */
int
kn_verify_assertion(char *buf, int len)
{
    struct assertion *as;
    int res;

    keynote_errno = 0;
    as = keynote_parse_assertion(buf, len, ASSERT_FLAG_SIGVER);
    if (as == (struct assertion *) NULL)
      return -1;

    res = keynote_sigverify_assertion(as);
    keynote_free_assertion(as);
    return res;
}

/*
 * Produce the signature for an assertion.
 */
char *
kn_sign_assertion(char *buf, int buflen, char *key, char *sigalg, int vflag)
{
    int i, alg, hashtype, encoding, internalenc;
    struct keynote_deckey dc;
    struct assertion *as;
    char *s, *sig;

    keynote_errno = 0;
    s = (char *) NULL;

    if ((sigalg == (char *) NULL) || (buf == (char *) NULL) ||
	(key == (char *) NULL))
    {
	keynote_errno = ERROR_NOTFOUND;
	return (char *) NULL;
    }

    if (sigalg[0] == '\0' || sigalg[strlen(sigalg) - 1] != ':')
    {
	keynote_errno = ERROR_SYNTAX;
	return (char *) NULL;
    }

    /* We're using a different format for X509 private keys, so... */
    alg = keynote_get_sig_algorithm(sigalg, &hashtype, &encoding,
				    &internalenc);
    if (alg != KEYNOTE_ALGORITHM_X509)
    {
	/* Parse the private key */
	s = keynote_get_private_key(key);
	if (s == (char *) NULL)
	  return (char *) NULL;

	/* Decode private key */
	i = kn_decode_key(&dc, s, KEYNOTE_PRIVATE_KEY);
	if (i == -1)
	{
	    free(s);
	    return (char *) NULL;
	}
    }
    else /* X509 private key */
    {
	dc.dec_key = key;
	dc.dec_algorithm = alg;
    }

    as = keynote_parse_assertion(buf, buflen, ASSERT_FLAG_SIGGEN);
    if (as == (struct assertion *) NULL)
    {
	if (alg != KEYNOTE_ALGORITHM_X509)
	{
	    keynote_free_key(dc.dec_key, dc.dec_algorithm);
	    free(s);
	}
	return (char *) NULL;
    }

    sig = keynote_sign_assertion(as, sigalg, dc.dec_key, dc.dec_algorithm,
				 vflag);
    if (alg != KEYNOTE_ALGORITHM_X509)
      keynote_free_key(dc.dec_key, dc.dec_algorithm);
    keynote_free_assertion(as);
    if (s != (char *) NULL)
      free(s);
    return sig;
}

/*
 * ASCII-encode a key.
 */
char *
kn_encode_key(struct keynote_deckey *dc, int iencoding,
	      int encoding, int keytype)
{
    char *foo, *ptr;
    DSA *dsa;
    RSA *rsa;
    int i;
    struct keynote_binary *bn;
    char *s;

    keynote_errno = 0;
    if ((dc == (struct keynote_deckey *) NULL) ||
	(dc->dec_key == (void *) NULL))
    {
	keynote_errno = ERROR_NOTFOUND;
	return (char *) NULL;
    }

    /* DSA keys */
    if ((dc->dec_algorithm == KEYNOTE_ALGORITHM_DSA) &&
	(iencoding == INTERNAL_ENC_ASN1) &&
	((encoding == ENCODING_HEX) || (encoding == ENCODING_BASE64)))
    {
	dsa = (DSA *) dc->dec_key;
	if (keytype == KEYNOTE_PUBLIC_KEY)
	  i = i2d_DSAPublicKey(dsa, NULL);
	else
	  i = i2d_DSAPrivateKey(dsa, NULL);

	if (i <= 0)
	{
	    keynote_errno = ERROR_SYNTAX;
	    return (char *) NULL;
	}

	ptr = foo = (char *) calloc(i, sizeof(char));
	if (foo == (char *) NULL)
	{
	    keynote_errno = ERROR_MEMORY;
	    return (char *) NULL;
	}

	dsa->write_params = 1;
	if (keytype == KEYNOTE_PUBLIC_KEY)
	  i2d_DSAPublicKey(dsa, (unsigned char **) &foo);
	else
	  i2d_DSAPrivateKey(dsa, (unsigned char **) &foo);

	if (encoding == ENCODING_HEX)
	{
	    if (kn_encode_hex(ptr, &s, i) != 0)
	    {
		free(ptr);
		return (char *) NULL;
	    }

	    free(ptr);
	    return s;
	}
	else
	  if (encoding == ENCODING_BASE64)
	  {
	      s = (char *) calloc(2 * i, sizeof(char));
	      if (s == (char *) NULL)
	      {
		  free(ptr);
		  keynote_errno = ERROR_MEMORY;
		  return (char *) NULL;
	      }

	      if (kn_encode_base64(ptr, i, s, 2 * i) == -1)
	      {
		  free(s);
		  free(ptr);
		  return (char *) NULL;
	      }

	      free(ptr);
	      return s;
	  }
    }

    /* RSA keys */
    if ((dc->dec_algorithm == KEYNOTE_ALGORITHM_RSA) &&
	(iencoding == INTERNAL_ENC_PKCS1) &&
	((encoding == ENCODING_HEX) || (encoding == ENCODING_BASE64)))
    {
	rsa = (RSA *) dc->dec_key;
	if (keytype == KEYNOTE_PUBLIC_KEY)
	  i = i2d_RSAPublicKey(rsa, NULL);
	else
	  i = i2d_RSAPrivateKey(rsa, NULL);

	if (i <= 0)
	{
	    keynote_errno = ERROR_SYNTAX;
	    return (char *) NULL;
	}

	ptr = foo = (char *) calloc(i, sizeof(char));
	if (foo == (char *) NULL)
	{
	    keynote_errno = ERROR_MEMORY;
	    return (char *) NULL;
	}

	if (keytype == KEYNOTE_PUBLIC_KEY)
	  i2d_RSAPublicKey(rsa, (unsigned char **) &foo);
	else
	  i2d_RSAPrivateKey(rsa, (unsigned char **) &foo);

	if (encoding == ENCODING_HEX)
	{
	    if (kn_encode_hex(ptr, &s, i) != 0)
	    {
		free(ptr);
		return (char *) NULL;
	    }

	    free(ptr);
	    return s;
	}
	else
	  if (encoding == ENCODING_BASE64)
	  {
	      s = (char *) calloc(2 * i, sizeof(char));
	      if (s == (char *) NULL)
	      {
		  free(ptr);
		  keynote_errno = ERROR_MEMORY;
		  return (char *) NULL;
	      }

	      if (kn_encode_base64(ptr, i, s, 2 * i) == -1)
	      {
		  free(s);
		  free(ptr);
		  return (char *) NULL;
	      }

	      free(ptr);
	      return s;
	  }
    }

    /* BINARY keys */
    if ((dc->dec_algorithm == KEYNOTE_ALGORITHM_BINARY) &&
	(iencoding == INTERNAL_ENC_NONE) &&
	((encoding == ENCODING_HEX) || (encoding == ENCODING_BASE64)))
    {
	bn = (struct keynote_binary *) dc->dec_key;

	if (encoding == ENCODING_HEX)
	{
	    if (kn_encode_hex(bn->bn_key, &s, bn->bn_len) != 0)
	      return (char *) NULL;

	    return s;
	}
	else
	  if (encoding == ENCODING_BASE64)
	  {
	      s = (char *) calloc(2 * bn->bn_len, sizeof(char));
	      if (s == (char *) NULL)
	      {
		  keynote_errno = ERROR_MEMORY;
		  return (char *) NULL;
	      }

	      if (kn_encode_base64(bn->bn_key, bn->bn_len, s,
				   2 * bn->bn_len) == -1)
	      {
		  free(s);
		  return (char *) NULL;
	      }

	      return s;
	  }
    }

    keynote_errno = ERROR_NOTFOUND;
    return (char *) NULL;
}
