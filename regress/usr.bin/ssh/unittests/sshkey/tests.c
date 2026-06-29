/* 	$OpenBSD: tests.c,v 1.3 2026/06/29 07:46:22 djm Exp $ */
/*
 * Regress test for sshbuf.h buffer API
 *
 * Placed in the public domain
 */

#include <openssl/evp.h>

#include "test_helper.h"

void sshkey_tests(void);
void sshkey_file_tests(void);
void sshkey_fuzz_tests(void);
void sshkey_benchmarks(void);

void
tests(void)
{
	OpenSSL_add_all_algorithms();
	OPENSSL_init_crypto(OPENSSL_INIT_LOAD_CRYPTO_STRINGS, NULL);

	sshkey_tests();
	sshkey_file_tests();
	sshkey_fuzz_tests();
}

void
benchmarks(void)
{
	printf("\n");
	sshkey_benchmarks();
}
