/* $OpenBSD: key.c,v 1.15 2004/04/15 18:39:26 deraadt Exp $	 */
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

#include "sysdep.h"

#include "key.h"
#include "libcrypto.h"
#include "log.h"
#include "util.h"
#include "x509.h"

void
key_free(int type, int private, void *key)
{
	switch (type) {
		case ISAKMP_KEY_PASSPHRASE:
		free(key);
		break;
	case ISAKMP_KEY_RSA:
		RSA_free(key);
		break;
	case ISAKMP_KEY_NONE:
	default:
		log_error("key_free: unknown/unsupportedkey type %d", type);
		break;
	}
}

/* Convert from internal form to serialized */
void
key_serialize(int type, int private, void *key, u_int8_t **data, size_t *datalen)
{
	u_int8_t       *p;

	switch (type) {
	case ISAKMP_KEY_PASSPHRASE:
		*datalen = strlen((char *) key);
		*data = (u_int8_t *) strdup((char *) key);
		break;
	case ISAKMP_KEY_RSA:
		switch (private) {
		case ISAKMP_KEYTYPE_PUBLIC:
			*datalen = i2d_RSAPublicKey((RSA *) key, NULL);
			*data = p = malloc(*datalen);
			if (!p) {
				log_error("key_serialize: malloc (%lu) failed",
				    (unsigned long) *datalen);
				return;
			}
			*datalen = i2d_RSAPublicKey((RSA *) key, &p);
			break;

		case ISAKMP_KEYTYPE_PRIVATE:
			*datalen = i2d_RSAPrivateKey((RSA *) key, NULL);
			*data = p = malloc(*datalen);
			if (!p) {
				log_error("key_serialize: malloc (%lu) failed",
				    (unsigned long) *datalen);
				return;
			}
			*datalen = i2d_RSAPrivateKey((RSA *) key, &p);
			break;
		}
		break;
	default:
		log_error("key_serialize: unknown/unsupported key type %d", type);
		break;
	}
}

/* Convert from serialized to printable */
char *
key_printable(int type, int private, u_int8_t *data, int datalen)
{
	char           *s;
	int             i;

	switch (type) {
	case ISAKMP_KEY_PASSPHRASE:
		return strdup((char *) data);

	case ISAKMP_KEY_RSA:
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

	default:
		log_error("key_printable: unknown/unsupported key type %d", type);
		return 0;
	}
}

/* Convert from serialized to internal.  */
void *
key_internalize(int type, int private, u_int8_t *data, int datalen)
{
	switch (type) {
	case ISAKMP_KEY_PASSPHRASE:
		return strdup((char *) data);
	case ISAKMP_KEY_RSA:
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
    u_int32_t *datalen)
{
	switch (type) {
	case ISAKMP_KEY_PASSPHRASE:
		*datalen = strlen(key);
		*data = (u_int8_t *) strdup(key);
		break;

	case ISAKMP_KEY_RSA:
		*datalen = (strlen(key) + 1) / 2;	/* Round up, just in case */
		*data = malloc(*datalen);
		if (!*data) {
			log_error("key_from_printable: malloc (%d) failed", *datalen);
			return;
		}
		*datalen = hex2raw(key, *data, *datalen);
		break;

	default:
		log_error("key_from_printable: unknown/unsupported key type %d", type);
		*data = 0;
		break;
	}
}
