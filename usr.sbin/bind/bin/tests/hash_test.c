/*
 * Copyright (C) 2000, 2001  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
 * INTERNET SOFTWARE CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
 * FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* $ISC: hash_test.c,v 1.8 2001/01/09 21:41:07 bwelling Exp $ */

#include <config.h>

#include <stdio.h>
#include <string.h>

#include <isc/hmacmd5.h>
#include <isc/md5.h>
#include <isc/sha1.h>
#include <isc/util.h>
#include <isc/string.h>

static void
print_digest(char *s, const char *hash, unsigned char *d,
	     unsigned int words)
{
	unsigned int i, j;

	printf("hash (%s) %s:\n\t", hash, s);
	for (i = 0 ; i < words ; i++) {
		printf(" ");
		for (j = 0 ; j < 4 ; j++)
			printf("%02x", d[i * 4 + j]);
	}
	printf("\n");
}

int
main(int argc, char **argv) {
	isc_sha1_t sha1;
	isc_md5_t md5;
	isc_hmacmd5_t hmacmd5;
	unsigned char digest[20];
	unsigned char buffer[1024];
	const unsigned char *s;
	unsigned char key[20];

	UNUSED(argc);
	UNUSED(argv);

	s = "abc";
	isc_sha1_init(&sha1);
	strlcpy(buffer, s, sizeof(buffer));
	isc_sha1_update(&sha1, buffer, strlen(s));
	isc_sha1_final(&sha1, digest);
	print_digest(buffer, "sha1", digest, 5);

	s = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
	isc_sha1_init(&sha1);
	strlcpy(buffer, s, sizeof(buffer));
	isc_sha1_update(&sha1, buffer, strlen(s));
	isc_sha1_final(&sha1, digest);
	print_digest(buffer, "sha1", digest, 5);

	s = "abc";
	isc_md5_init(&md5);
	strlcpy(buffer, s, sizeof(buffer));
	isc_md5_update(&md5, buffer, strlen(s));
	isc_md5_final(&md5, digest);
	print_digest(buffer, "md5", digest, 4);

	/*
	 * The 3 HMAC-MD5 examples from RFC 2104
	 */
	s = "Hi There";
	memset(key, 0x0b, 16);
	isc_hmacmd5_init(&hmacmd5, key, 16);
	strlcpy(buffer, s, sizeof(buffer));
	isc_hmacmd5_update(&hmacmd5, buffer, strlen(s));
	isc_hmacmd5_sign(&hmacmd5, digest);
	print_digest(buffer, "hmacmd5", digest, 4);

	s = "what do ya want for nothing?";
	strlcpy(key, "Jefe", sizeof(key));
	isc_hmacmd5_init(&hmacmd5, key, 4);
	strlcpy(buffer, s, sizeof(buffer));
	isc_hmacmd5_update(&hmacmd5, buffer, strlen(s));
	isc_hmacmd5_sign(&hmacmd5, digest);
	print_digest(buffer, "hmacmd5", digest, 4);

	s = "\335\335\335\335\335\335\335\335\335\335"
	    "\335\335\335\335\335\335\335\335\335\335"
	    "\335\335\335\335\335\335\335\335\335\335"
	    "\335\335\335\335\335\335\335\335\335\335"
	    "\335\335\335\335\335\335\335\335\335\335";
	memset(key, 0xaa, 16);
	isc_hmacmd5_init(&hmacmd5, key, 16);
	strlcpy(buffer, s, sizeof(buffer));
	isc_hmacmd5_update(&hmacmd5, buffer, strlen(s));
	isc_hmacmd5_sign(&hmacmd5, digest);
	print_digest(buffer, "hmacmd5", digest, 4);

	return (0);
}
