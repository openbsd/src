/* $OpenBSD: modes_local.h,v 1.4 2025/04/23 14:15:19 jsing Exp $ */
/* ====================================================================
 * Copyright (c) 2010 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use is governed by OpenSSL license.
 * ====================================================================
 */

#include <endian.h>

#include <openssl/opensslconf.h>

#include <openssl/modes.h>

__BEGIN_HIDDEN_DECLS

#if defined(_LP64)
typedef long i64;
typedef unsigned long u64;
#define U64(C) C##UL
#else
typedef long long i64;
typedef unsigned long long u64;
#define U64(C) C##ULL
#endif

typedef unsigned int u32;
typedef unsigned char u8;

/* GCM definitions */

typedef struct {
	u64 hi, lo;
} u128;

#ifdef	TABLE_BITS
#undef	TABLE_BITS
#endif
/*
 * Even though permitted values for TABLE_BITS are 8, 4 and 1, it should
 * never be set to 8 [or 1]. For further information see gcm128.c.
 */
#define	TABLE_BITS 4

struct gcm128_context {
	/* Following 6 names follow names in GCM specification */
	union {
		u64 u[2];
		u32 d[4];
		u8 c[16];
		size_t t[16/sizeof(size_t)];
	} Yi, EKi, EK0, len, Xi, H;
	/* Relative position of Xi, H and pre-computed Htable is used
	 * in some assembler modules, i.e. don't change the order! */
#if TABLE_BITS==8
	u128 Htable[256];
#else
	u128 Htable[16];
	void (*gmult)(u64 Xi[2], const u128 Htable[16]);
	void (*ghash)(u64 Xi[2], const u128 Htable[16], const u8 *inp,
	    size_t len);
#endif
	unsigned int mres, ares;
	block128_f block;
	void *key;
};

struct xts128_context {
	void      *key1, *key2;
	block128_f block1, block2;
};

struct ccm128_context {
	union {
		u64 u[2];
		u8 c[16];
	} nonce, cmac;
	u64 blocks;
	block128_f block;
	void *key;
};

__END_HIDDEN_DECLS
