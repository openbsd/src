/* $OpenBSD: key.c,v 1.20 2004/12/28 11:19:47 hshoexer Exp $	 */
/*
 * The author of this code is Angelos D. Keromytis (angelos@cis.upenn.edu)
 *
 * Copyright (c) 2000-2001 Angelos D. Keromytis.
 *
 * Permission to use, copy, and modify this software with or without fee
 * is hereby granted, provided that this entire notice is included in
 * all copies of any software which is or includes a copy or
 * modification of this software.
 * You may use this code under the GNU public license if you so wish. Please
 * contribute changes back to the authors under this freer than GPL license
 * so that we may further the use of strong encryption without limitations to
 * all.
 *
 * THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTY. IN PARTICULAR, NONE OF THE AUTHORS MAKES ANY
 * REPRESENTATION OR WARRANTY OF ANY KIND CONCERNING THE
 * MERCHANTABILITY OF THIS SOFTWARE OR ITS FITNESS FOR ANY PARTICULAR
 * PURPOSE.
 */

#include <string.h>
#include <stdlib.h>

#include "sysdep.h"

#include "key.h"
#include "libcrypto.h"
#include "log.h"
#include "util.h"
#ifdef USE_X509
#include "x509.h"
#endif

void
key_free(int type, int private, void *key)
{
	switch (type) {
	case ISAKMP_KEY_PASSPHRASE:
		free(key);
		break;
	case ISAKMP_KEY_RSA:
#ifdef USE_X509
		RSA_free(key);
		break;
#endif
	case ISAKMP_KEY_NONE:
	default:
		log_error("key_free: unknown/unsupportedkey type %d", type);
		break;
	}
}

/* Convert from internal form to serialized */
void
key_serialize(int type, int private, void *key, u_int8_t **data,
    size_t *datalenp)
{
#ifdef USE_X509
	u_int8_t       *p;
	size_t		datalen;
#endif

	switch (type) {
	case ISAKMP_KEY_PASSPHRASE:
		*datalenp = strlen((char *)key);
		*data = (u_int8_t *)strdup((char *)key);
		break;
	case ISAKMP_KEY_RSA:
#ifdef USE_X509
		switch (private) {
		case ISAKMP_KEYTYPE_PUBLIC:
			datalen = i2d_RSAPublicKey((RSA *)key, NULL);
			*data = p = malloc(datalen);
			if (!p) {
				log_error("key_serialize: malloc (%lu) failed",
				    (unsigned long)datalen);
				return;
			}
			*datalenp = i2d_RSAPublicKey((RSA *) key, &p);
			break;

		case ISAKMP_KEYTYPE_PRIVATE:
			datalen = i2d_RSAPrivateKey((RSA *)key, NULL);
			*data = p = malloc(datalen);
			if (!p) {
				log_error("key_serialize: malloc (%lu) failed",
				    (unsigned long)datalen);
				return;
			}
			*datalenp = i2d_RSAPrivateKey((RSA *)key, &p);
			break;
		}
#endif
		break;
	default:
		log_error("key_serialize: unknown/unsupported key type %d",
		    type);
		break;
	}
}

/* Convert from serialized to printable */
char *
key_printable(int type, int private, u_int8_t *data, int datalen)
{
#ifdef USE_X509
	char	*s;
	int	 i;
#endif

	switch (type) {
	case ISAKMP_KEY_PASSPHRASE:
		return strdup((char *)data);

	case ISAKMP_KEY_RSA:
#ifdef USE_X509
		s = malloc(datalen * 2 + 1);
		if (!s) {
			log_error("key_printable: malloc (%d) failed",
			    datalen * 2 + 1);
			return 0;
		}
		for (i = 0; i < datalen; i++)
			snprintf(s + (2 * i), 2 * (datalen - i) + 1, "%02x",
			    data[i]);
		return s;
#endif

	default:
		log_error("key_printable: unknown/unsupported key type %d",
		    type);
		return 0;
	}
}

/* Convert from serialized to internal.  */
void *
key_internalize(int type, int private, u_int8_t *data, int datalen)
{
	switch (type) {
	case ISAKMP_KEY_PASSPHRASE:
		return strdup((char *)data);
	case ISAKMP_KEY_RSA:
#ifdef USE_X509
		switch (private) {
#if OPENSSL_VERSION_NUMBER >= 0x00907000L
		case ISAKMP_KEYTYPE_PUBLIC:
			return d2i_RSAPublicKey(NULL,
			    (const u_int8_t **)&data, datalen);
		case ISAKMP_KEYTYPE_PRIVATE:
			return d2i_RSAPrivateKey(NULL,
			    (const u_int8_t **)&data, datalen);
#else
		case ISAKMP_KEYTYPE_PUBLIC:
			return d2i_RSAPublicKey(NULL, &data, datalen);
		case ISAKMP_KEYTYPE_PRIVATE:
			return d2i_RSAPrivateKey(NULL, &data, datalen);
#endif
		default:
			log_error("key_internalize: not public or private "
			    "RSA key passed");
			return 0;
		}
		break;
#endif /* USE_X509 */
	default:
		log_error("key_internalize: unknown/unsupported key type %d",
		    type);
		break;
	}

	return 0;
}

/* Convert from printable to serialized */
void
key_from_printable(int type, int private, char *key, u_int8_t **data,
    u_int32_t *datalenp)
{
#ifdef USE_X509
	u_int32_t datalen;
#endif

	switch (type) {
	case ISAKMP_KEY_PASSPHRASE:
		*datalenp = strlen(key);
		*data = (u_int8_t *) strdup(key);
		break;

	case ISAKMP_KEY_RSA:
#ifdef USE_X509
		datalen = (strlen(key) + 1) / 2; /* Round up, just in case */
		*data = malloc(datalen);
		if (!*data) {
			log_error("key_from_printable: malloc (%d) failed",
			    datalen);
			*datalenp = 0;
			return;
		}
		if (hex2raw(key, *data, datalen)) {
			log_error("key_from_printable: invalid hex key");
			free(*data);
			*datalenp = 0;
			return;
		}
		*datalenp = datalen;
		break;
#endif

	default:
		log_error("key_from_printable: "
		    "unknown/unsupported key type %d", type);
		*data = NULL;
		*datalenp = 0;
		break;
	}
}
