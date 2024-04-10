/* $OpenBSD: malloc-wrapper.c,v 1.11 2024/04/10 14:51:02 beck Exp $ */
/*
 * Copyright (c) 2014 Bob Beck
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <openssl/crypto.h>

int
CRYPTO_set_mem_functions(void *(*m)(size_t), void *(*r)(void *, size_t),
    void (*f)(void *))
{
	return 0;
}
LCRYPTO_ALIAS(CRYPTO_set_mem_functions);

int
CRYPTO_set_mem_ex_functions(void *(*m)(size_t, const char *, int),
    void *(*r)(void *, size_t, const char *, int), void (*f)(void *))
{
	return 0;
}
LCRYPTO_ALIAS(CRYPTO_set_mem_ex_functions);

void *
CRYPTO_malloc(size_t num, const char *file, int line)
{
	return malloc(num);
}
LCRYPTO_ALIAS(CRYPTO_malloc);

char *
CRYPTO_strdup(const char *str, const char *file, int line)
{
	return strdup(str);
}
LCRYPTO_ALIAS(CRYPTO_strdup);

void
CRYPTO_free(void *ptr, const char *file, int line)
{
	free(ptr);
}
LCRYPTO_ALIAS(CRYPTO_free);
