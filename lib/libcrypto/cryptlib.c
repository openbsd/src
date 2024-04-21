/* $OpenBSD: cryptlib.c,v 1.51 2024/04/21 13:41:14 tb Exp $ */
/* ====================================================================
 * Copyright (c) 1998-2006 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.openssl.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    openssl-core@openssl.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.openssl.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com).
 *
 */
/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 *
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 *
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 *
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */
/* ====================================================================
 * Copyright 2002 Sun Microsystems, Inc. ALL RIGHTS RESERVED.
 * ECDH support in OpenSSL originally developed by
 * SUN MICROSYSTEMS, INC., and contributed to the OpenSSL project.
 */

#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include <openssl/opensslconf.h>
#include <openssl/crypto.h>

#include "crypto_local.h"

static void (*locking_callback)(int mode, int type,
    const char *file, int line) = NULL;
static int (*add_lock_callback)(int *pointer, int amount,
    int type, const char *file, int line) = NULL;

int
CRYPTO_num_locks(void)
{
	return 1;
}
LCRYPTO_ALIAS(CRYPTO_num_locks);

unsigned long
(*CRYPTO_get_id_callback(void))(void)
{
	return NULL;
}
LCRYPTO_ALIAS(CRYPTO_get_id_callback);

void
CRYPTO_set_id_callback(unsigned long (*func)(void))
{
	return;
}
LCRYPTO_ALIAS(CRYPTO_set_id_callback);

unsigned long
CRYPTO_thread_id(void)
{
	return (unsigned long)pthread_self();
}
LCRYPTO_ALIAS(CRYPTO_thread_id);

void
CRYPTO_set_locking_callback(void (*func)(int mode, int lock_num,
    const char *file, int line))
{
	locking_callback = func;
}
LCRYPTO_ALIAS(CRYPTO_set_locking_callback);

void
(*CRYPTO_get_locking_callback(void))(int mode, int lock_num,
	const char *file, int line)
{
	return locking_callback;
}
LCRYPTO_ALIAS(CRYPTO_get_locking_callback);

void
CRYPTO_set_add_lock_callback(int (*func)(int *num, int mount, int lock_num,
	const char *file, int line))
{
	add_lock_callback = func;
}
LCRYPTO_ALIAS(CRYPTO_set_add_lock_callback);

int
(*CRYPTO_get_add_lock_callback(void))(int *num, int mount, int type,
    const char *file, int line)
{
	return add_lock_callback;
}
LCRYPTO_ALIAS(CRYPTO_get_add_lock_callback);

const char *
CRYPTO_get_lock_name(int lock_num)
{
	return "";
}
LCRYPTO_ALIAS(CRYPTO_get_lock_name);

struct CRYPTO_dynlock_value *
CRYPTO_get_dynlock_value(int i)
{
	return NULL;
}
LCRYPTO_ALIAS(CRYPTO_get_dynlock_value);

int CRYPTO_get_new_dynlockid(void)
{
	return 0;
}
LCRYPTO_ALIAS(CRYPTO_get_new_dynlockid);

void
CRYPTO_destroy_dynlockid(int i)
{
	return;
}
LCRYPTO_ALIAS(CRYPTO_destroy_dynlockid);

int CRYPTO_get_new_lockid(char *name)
{
	return 0;
}
LCRYPTO_ALIAS(CRYPTO_get_new_lockid);

int
CRYPTO_THREADID_set_callback(void (*func)(CRYPTO_THREADID *))
{
	return 1;
}
LCRYPTO_ALIAS(CRYPTO_THREADID_set_callback);

void
(*CRYPTO_THREADID_get_callback(void))(CRYPTO_THREADID *)
{
	return NULL;
}
LCRYPTO_ALIAS(CRYPTO_THREADID_get_callback);

void
CRYPTO_THREADID_set_numeric(CRYPTO_THREADID *id, unsigned long val)
{
	return;
}
LCRYPTO_ALIAS(CRYPTO_THREADID_set_numeric);

void
CRYPTO_THREADID_set_pointer(CRYPTO_THREADID *id, void *ptr)
{
	return;
}
LCRYPTO_ALIAS(CRYPTO_THREADID_set_pointer);

void
CRYPTO_set_dynlock_create_callback(struct CRYPTO_dynlock_value *(
    *dyn_create_function)(const char *file, int line))
{
	return;
}
LCRYPTO_ALIAS(CRYPTO_set_dynlock_create_callback);

void
CRYPTO_set_dynlock_lock_callback(void (*dyn_lock_function)(
    int mode, struct CRYPTO_dynlock_value *l, const char *file, int line))
{
	return;
}
LCRYPTO_ALIAS(CRYPTO_set_dynlock_lock_callback);

void
CRYPTO_set_dynlock_destroy_callback(void (*dyn_destroy_function)(
    struct CRYPTO_dynlock_value *l, const char *file, int line))
{
	return;
}
LCRYPTO_ALIAS(CRYPTO_set_dynlock_destroy_callback);

struct CRYPTO_dynlock_value *
(*CRYPTO_get_dynlock_create_callback(void))(const char *file, int line)
{
	return NULL;
}

void
(*CRYPTO_get_dynlock_lock_callback(void))(int mode,
    struct CRYPTO_dynlock_value *l, const char *file, int line)
{
	return NULL;
}
LCRYPTO_ALIAS(CRYPTO_get_dynlock_lock_callback);

void
(*CRYPTO_get_dynlock_destroy_callback(void))(
    struct CRYPTO_dynlock_value *l, const char *file, int line)
{
	return NULL;
}
LCRYPTO_ALIAS(CRYPTO_get_dynlock_destroy_callback);

void
CRYPTO_THREADID_current(CRYPTO_THREADID *id)
{
	memset(id, 0, sizeof(*id));
	id->val = (unsigned long)pthread_self();
}

int
CRYPTO_THREADID_cmp(const CRYPTO_THREADID *a, const CRYPTO_THREADID *b)
{
	return memcmp(a, b, sizeof(*a));
}

void
CRYPTO_THREADID_cpy(CRYPTO_THREADID *dest, const CRYPTO_THREADID *src)
{
	memcpy(dest, src, sizeof(*src));
}

unsigned long
CRYPTO_THREADID_hash(const CRYPTO_THREADID *id)
{
	return id->val;
}

#if	defined(__i386)   || defined(__i386__)   || defined(_M_IX86) || \
	defined(__INTEL__) || \
	defined(__x86_64) || defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)

uint64_t OPENSSL_ia32cap_P;

uint64_t
OPENSSL_cpu_caps(void)
{
	return OPENSSL_ia32cap_P;
}
LCRYPTO_ALIAS(OPENSSL_cpu_caps);

#if defined(OPENSSL_CPUID_OBJ) && !defined(OPENSSL_NO_ASM)
#define OPENSSL_CPUID_SETUP
void
OPENSSL_cpuid_setup(void)
{
	static int trigger = 0;
	uint64_t OPENSSL_ia32_cpuid(void);

	if (trigger)
		return;
	trigger = 1;
	OPENSSL_ia32cap_P = OPENSSL_ia32_cpuid();
}
#endif

#else
uint64_t
OPENSSL_cpu_caps(void)
{
	return 0;
}
LCRYPTO_ALIAS(OPENSSL_cpu_caps);
#endif

#if !defined(OPENSSL_CPUID_SETUP) && !defined(OPENSSL_CPUID_OBJ)
void
OPENSSL_cpuid_setup(void)
{
}
#endif

static void
OPENSSL_showfatal(const char *fmta, ...)
{
	struct syslog_data sdata = SYSLOG_DATA_INIT;
	va_list ap;

	va_start(ap, fmta);
	vsyslog_r(LOG_CONS|LOG_LOCAL2, &sdata, fmta, ap);
	va_end(ap);
}

void
OpenSSLDie(const char *file, int line, const char *assertion)
{
	OPENSSL_showfatal(
	    "uid %u cmd %s %s(%d): OpenSSL internal error, assertion failed: %s\n",
	    getuid(), getprogname(), file, line, assertion);
	_exit(1);
}
LCRYPTO_ALIAS(OpenSSLDie);

int
CRYPTO_memcmp(const void *in_a, const void *in_b, size_t len)
{
	size_t i;
	const unsigned char *a = in_a;
	const unsigned char *b = in_b;
	unsigned char x = 0;

	for (i = 0; i < len; i++)
		x |= a[i] ^ b[i];

	return x;
}
LCRYPTO_ALIAS(CRYPTO_memcmp);
