/* $OpenBSD: header.h,v 1.5 1999/10/26 22:31:38 angelos Exp $ */
/*
 * The author of this code is Angelos D. Keromytis (angelos@dsl.cis.upenn.edu)
 *
 * This code was written by Angelos D. Keromytis in Philadelphia, PA, USA,
 * in April-May 1998
 *
 * Copyright (C) 1998, 1999 by Angelos D. Keromytis.
 *	
 * Permission to use, copy, and modify this software without fee
 * is hereby granted, provided that this entire notice is included in
 * all copies of any software which is or includes a copy or
 * modification of this software. 
 *
 * THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTY. IN PARTICULAR, THE AUTHORS MAKES NO
 * REPRESENTATION OR WARRANTY OF ANY KIND CONCERNING THE
 * MERCHANTABILITY OF THIS SOFTWARE OR ITS FITNESS FOR ANY PARTICULAR
 * PURPOSE.
 */

#ifndef _HEADER_H_
#define _HEADER_H_

/* Functions */
extern void keynote_sign(int, char **), keynote_sigver(int, char **);
extern void keynote_verify(int, char **), keynote_keygen(int, char **);
extern void print_key(FILE *, char *, char *, int, int);
extern void print_space(FILE *, int);
extern int read_environment(char *);
extern void parse_key(char *);
extern int kvparse(), kvlex();
extern void kverror(char *);

/* Variables */
int sessid;

/* Defines */
#define SEED_LEN        40
#define RND_BYTES       1024
#define DEFAULT_PUBLIC  0x10001

#define KEY_PRINT_OFFSET      12
#define KEY_PRINT_LENGTH      50

#define SIG_PRINT_OFFSET      12
#define SIG_PRINT_LENGTH      50

#if !defined(HAVE_STRCASECMP) && defined(HAVE_STRICMP)
#define strcasecmp stricmp
#endif /* !HAVE_STRCASECMP && HAVE_STRICMP */

#if !defined(HAVE_STRNCASECMP) && defined(HAVE_STRNICMP)
#define strncasecmp strnicmp
#endif /* !HAVE_STRNCASECMP && HAVE_STRNICMP */

#if !defined(HAVE_OPEN) && defined(HAVE__OPEN)
#define open _open
#endif /* !HAVE_OPEN && HAVE__OPEN */

#if !defined(HAVE_READ) && defined(HAVE__READ)
#define read _read
#endif /* !HAVE_READ && HAVE__OPEN */

#if !defined(HAVE_CLOSE) && defined(HAVE__CLOSE)
#define close _close
#endif /* !HAVE_CLOSE && HAVE__CLOSE */

#if defined(CRYPTO)
#if HAVE__DEV_URANDOM
#define KEYNOTERNDFILENAME "/dev/urandom"
#else /* HAVE__DEV_URANDOM */
#error "You need a random device!"
#endif /* HAVE__DEV_URANDOM */
#endif /* CRYPTO */

/* Includes */
#if HAVE_REGEX_H
#include <sys/types.h>
#include <regex.h>
#endif /* HAVE_REGEX_H */

#if defined(CRYPTO)
#if defined(HAVE_OPENSSL_CRYPTO_H)
#include <openssl/crypto.h>
#include <openssl/dsa.h>
#include <openssl/rsa.h>
#include <openssl/sha.h>
#include <openssl/md5.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/x509.h>
#include <openssl/pem.h>
#elif defined(HAVE_SSL_CRYPTO_H)
#include <ssl/crypto.h>
#include <ssl/dsa.h>
#include <ssl/rsa.h>
#include <ssl/sha.h>
#include <ssl/md5.h>
#include <ssl/err.h>
#include <ssl/rand.h>
#include <ssl/x509.h>
#include <ssl/pem.h>
#elif defined(HAVE_CRYPTO_H)
#include <crypto.h>
#include <dsa.h>
#include <rsa.h>
#include <sha.h>
#include <md5.h>
#include <err.h>
#include <rand.h>
#include <x509.h>
#include <pem.h>
#else /* HAVE_OPENSSL_CRYPTO_H */
#error "SSLeay or OpenSSL not detected!"
#endif /* HAVE_OPENSSL_CRYPTO_H */
#endif /* CRYPTO */

#endif /* _HEADER_H_ */
