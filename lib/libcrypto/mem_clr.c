/* $OpenBSD: mem_clr.c,v 1.5 2024/04/10 14:51:02 beck Exp $ */

/* Ted Unangst places this file in the public domain. */
#include <string.h>
#include <openssl/crypto.h>

void
OPENSSL_cleanse(void *ptr, size_t len)
{
	explicit_bzero(ptr, len);
}
LCRYPTO_ALIAS(OPENSSL_cleanse);
