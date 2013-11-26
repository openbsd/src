/*
 * iterated_hash.c -- nsec3 hash calculation.
 *
 * Copyright (c) 2001-2006, NLnet Labs. All rights reserved.
 *
 * See LICENSE for the license.
 *
 * With thanks to Ben Laurie.
 */
#include "config.h"
#ifdef NSEC3
#include <openssl/sha.h>
#include <stdio.h>
#include <assert.h>

#include "iterated_hash.h"

int
iterated_hash(unsigned char out[SHA_DIGEST_LENGTH],
	const unsigned char *salt, int saltlength,
	const unsigned char *in, int inlength, int iterations)
{
#if defined(NSEC3) && defined(HAVE_SSL)
	SHA_CTX ctx;
	int n;
	assert(in && inlength > 0 && iterations >= 0);
	for(n=0 ; n <= iterations ; ++n)
	{
		SHA1_Init(&ctx);
		SHA1_Update(&ctx, in, inlength);
		if(saltlength > 0)
			SHA1_Update(&ctx, salt, saltlength);
		SHA1_Final(out, &ctx);
		in=out;
		inlength=SHA_DIGEST_LENGTH;
	}
	return SHA_DIGEST_LENGTH;
#else
	(void)out; (void)salt; (void)saltlength;
	(void)in; (void)inlength; (void)iterations;
	return 0;
#endif
}

#endif /* NSEC3 */
