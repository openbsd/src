/*
 * sha64bit.h: placeholder values for 64-bit data and routines
 *
 * Ref: NIST FIPS PUB 180-4 Secure Hash Standard
 *
 * Copyright (C) 2003-2015 Mark Shelor, All Rights Reserved
 *
 * Version: 5.95
 * Sat Jan 10 12:15:36 MST 2015
 *
 * The following macros supply placeholder values that enable the
 * sha.c module to successfully compile when 64-bit integer types
 * aren't present.
 *
 * They are appropriately redefined in sha64bit.c if the compiler
 * provides a 64-bit type (i.e. when SHA_384_512 is defined).
 *
 */

#define sha_384_512		0
#define W64			SHA64
#define sha512			NULL
#define H0384			H01
#define H0512			H01
#define H0512224		H01
#define H0512256		H01
