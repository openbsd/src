/*
 * iterated_hash.h -- nsec3 hash calculation.
 *
 * Copyright (c) 2001-2006, NLnet Labs. All rights reserved.
 *
 * See LICENSE for the license.
 *
 * With thanks to Ben Laurie.
 */
#ifndef ITERATED_HASH_H
#define ITERATED_HASH_H

#include <config.h>
#ifdef NSEC3
#include <openssl/sha.h>

int iterated_hash(unsigned char out[SHA_DIGEST_LENGTH],
	const unsigned char *salt,int saltlength,
	const unsigned char *in,int inlength,int iterations);

#endif /* NSEC3 */
#endif /* ITERATED_HASH_H */
