/* S/KEY v1.1b (skeysubr.c)
 *
 * Authors:
 *          Neil M. Haller <nmh@thumper.bellcore.com>
 *          Philip R. Karn <karn@chicago.qualcomm.com>
 *          John S. Walden <jsw@thumper.bellcore.com>
 *
 * Modifications: 
 *          Scott Chasin <chasin@crimelab.com>
 *
 * S/KEY misc routines.
 *
 * $Id: skeysubr.c,v 1.5 1996/09/29 23:35:07 millert Exp $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <termios.h>
#include <md4.h>
#include <md5.h>
#include <sha1.h>

#include "skey.h"

/* Default hash function to use (index into skey_hash_types array) */
#ifndef SKEY_HASH_DEFAULT
#define SKEY_HASH_DEFAULT	1
#endif

static void trapped __ARGS((int sig));
static void f_md4 __ARGS ((char *x));
static void f_md5 __ARGS ((char *x));
static void f_sha1 __ARGS ((char *x));
static void skey_echo __ARGS ((int action));
static int keycrunch_md4 __ARGS ((char *result, char *seed, char *passwd));
static int keycrunch_md5 __ARGS ((char *result, char *seed, char *passwd));
static int keycrunch_sha1 __ARGS ((char *result, char *seed, char *passwd));

/* Current hash type (index into skey_hash_types array) */
static int skey_hash_type = SKEY_HASH_DEFAULT;

/*
 * Hash types we support.
 * Each has an associated keycrunch() and f() function.
 */
#define SKEY_ALGORITH_MAX	3
struct skey_algorithm_table {
	const char *name;
	int (*keycrunch) __ARGS((char *, char *, char *));
	void (*f) __ARGS((char *));
};
static struct skey_algorithm_table skey_algorithm_table[] = {
	{ "md4", keycrunch_md4, f_md4 },
	{ "md5", keycrunch_md5, f_md5 },
	{ "sha1", keycrunch_sha1, f_sha1 }
};


/*
 * Crunch a key:
 * concatenate the seed and the password, run through MD4/5 and
 * collapse to 64 bits. This is defined as the user's starting key.
 */
int
keycrunch(result, seed, passwd)
	char *result;	/* 8-byte result */
	char *seed;	/* Seed, any length */
	char *passwd;	/* Password, any length */
{
	return(skey_algorithm_table[skey_hash_type].keycrunch(result, seed, passwd));
}

static int
keycrunch_md4(result, seed, passwd)
	char *result;	/* 8-byte result */
	char *seed;	/* Seed, any length */
	char *passwd;	/* Password, any length */
{
	char *buf;
	MD4_CTX md;
	u_int32_t results[4];
	unsigned int buflen;

	buflen = strlen(seed) + strlen(passwd);
	if ((buf = (char *)malloc(buflen+1)) == NULL)
		return -1;
	(void)strcpy(buf, seed);
	(void)strcat(buf, passwd);

	/* Crunch the key through MD4 */
	sevenbit(buf);
	MD4Init(&md);
	MD4Update(&md, (unsigned char *)buf, buflen);
	MD4Final((unsigned char *)results, &md);
	(void)free(buf);

	/* Fold result from 128 to 64 bits */
	results[0] ^= results[2];
	results[1] ^= results[3];

	(void)memcpy((void *)result, (void *)results, 8);

	return 0;
}

static int
keycrunch_md5(result, seed, passwd)
	char *result;	/* 8-byte result */
	char *seed;	/* Seed, any length */
	char *passwd;	/* Password, any length */
{
	char *buf;
	MD5_CTX md;
	u_int32_t results[4];
	unsigned int buflen;

	buflen = strlen(seed) + strlen(passwd);
	if ((buf = (char *)malloc(buflen+1)) == NULL)
		return -1;
	(void)strcpy(buf, seed);
	(void)strcat(buf, passwd);

	/* Crunch the key through MD5 */
	sevenbit(buf);
	MD5Init(&md);
	MD5Update(&md, (unsigned char *)buf, buflen);
	MD5Final((unsigned char *)results, &md);
	(void)free(buf);

	/* Fold result from 128 to 64 bits */
	results[0] ^= results[2];
	results[1] ^= results[3];

	(void)memcpy((void *)result, (void *)results, 8);

	return 0;
}

static int
keycrunch_sha1(result, seed, passwd)
	char *result;	/* 8-byte result */
	char *seed;	/* Seed, any length */
	char *passwd;	/* Password, any length */
{
	char *buf;
	SHA1_INFO sha;
	unsigned int buflen;

	buflen = strlen(seed) + strlen(passwd);
	if ((buf = (char *)malloc(buflen+1)) == NULL)
		return -1;
	(void)strcpy(buf, seed);
	(void)strcat(buf, passwd);

	/* Crunch the key through SHA1 */
	sevenbit(buf);
	sha1Init(&sha);
	sha1Update(&sha, (unsigned char *)buf, buflen);
	sha1Final(&sha);
	(void)free(buf);

	/* Fold 160 to 64 bits */
	sha.digest[0] ^= sha.digest[2];
	sha.digest[1] ^= sha.digest[3];
	sha.digest[0] ^= sha.digest[4];

	(void)memcpy((void *)result, (void *)sha.digest, 8);

	return 0;
}

/* The one-way function f(). Takes 8 bytes and returns 8 bytes in place */
void
f(x)
	char *x;
{
	skey_algorithm_table[skey_hash_type].f(x);
}

void
f_md4(x)
	char *x;
{
	MD4_CTX md;
	u_int32_t results[4];

	MD4Init(&md);
	MD4Update(&md, (unsigned char *)x, 8);
	MD4Final((unsigned char *)results, &md);

	/* Fold 128 to 64 bits */
	results[0] ^= results[2];
	results[1] ^= results[3];

	(void)memcpy((void *)x, (void *)results, 8);
}

void
f_md5(x)
	char *x;
{
	MD5_CTX md;
	u_int32_t results[4];

	MD5Init(&md);
	MD5Update(&md, (unsigned char *)x, 8);
	MD5Final((unsigned char *)results, &md);

	/* Fold 128 to 64 bits */
	results[0] ^= results[2];
	results[1] ^= results[3];

	(void)memcpy((void *)x, (void *)results, 8);
}

void
f_sha1(x)
	char *x;
{
	SHA1_INFO sha;

	sha1Init(&sha);
	sha1Update(&sha, (unsigned char *)x, 8);
	sha1Final(&sha);

	/* Fold 160 to 64 bits */
	sha.digest[0] ^= sha.digest[2];
	sha.digest[1] ^= sha.digest[3];
	sha.digest[0] ^= sha.digest[4];

	(void)memcpy((void *)x, (void *)sha.digest, 8);
}

/* Strip trailing cr/lf from a line of text */
void
rip(buf)
	char *buf;
{
	buf += strcspn(buf, "\r\n");

	if (*buf)
		*buf = '\0';
}

/* Read in secret password (turns off echo) */
char *
readpass(buf, n)
	char *buf;
	int n;
{
	void (*old_handler) __P(());

	/* Turn off echoing */
	skey_echo(0);

	/* Catch SIGINT and save old signal handler */
	old_handler = signal(SIGINT, trapped);

	(void)fgets(buf, n, stdin);
	rip(buf);

	(void)putc('\n', stderr);
	(void)fflush(stderr);

	/* Restore signal handler and turn echo back on */
	if (old_handler != SIG_ERR)
		(void)signal(SIGINT, old_handler);
	skey_echo(1);

	sevenbit(buf);

	return buf;
}

/* Read in an s/key OTP (does not turn off echo) */
char *
readskey(buf, n)
	char *buf;
	int n;
{
	(void)fgets(buf, n, stdin);
	rip(buf);

	(void)putc('\n', stderr);
	(void)fflush(stderr);

	sevenbit (buf);

	return buf;
}

/* Signal handler for trapping ^C */
static void
trapped(sig)
	int sig;
{
	(void)fputs("^C\n", stderr);
	(void)fflush(stderr);

	/* Turn on echo if necesary */
	skey_echo(1);

	exit(-1);
}

/*
 * Convert 8-byte hex-ascii string to binary array
 * Returns 0 on success, -1 on error
 */
int
atob8(out, in)
	register char *out, *in;
{
	register int i;
	register int val;

	if (in == NULL || out == NULL)
		return -1;

	for (i=0; i<8; i++) {
		if ((in = skipspace(in)) == NULL)
			return -1;
		if ((val = htoi(*in++)) == -1)
			return -1;
		*out = val << 4;

		if ((in = skipspace(in)) == NULL)
			return -1;
		if ((val = htoi(*in++)) == -1)
			return -1;
		*out++ |= val;
	}
	return 0;
}

/* Convert 8-byte binary array to hex-ascii string */
int
btoa8(out, in)
	register char *out, *in;
{
	register int i;

	if (in == NULL || out == NULL)
		return -1;

	for (i=0; i < 8; i++) {
		(void)sprintf(out, "%02x", *in++ & 0xff);
		out += 2;
	}
	return 0;
}

/* Convert hex digit to binary integer */
int
htoi(c)
	register char c;
{
	if ('0' <= c && c <= '9')
		return c - '0';
	if ('a' <= c && c <= 'f')
		return 10 + c - 'a';
	if ('A' <= c && c <= 'F')
		return 10 + c - 'A';
	return -1;
}

/* Skip leading spaces from the string */
char *
skipspace(cp)
	register char *cp;
{
	while (*cp == ' ' || *cp == '\t')
		cp++;

	if (*cp == '\0')
		return NULL;
	else
		return cp;
}

/* Remove backspaced over charaters from the string */
void
backspace(buf)
	char *buf;
{
	char bs = 0x8;
	char *cp = buf;
	char *out = buf;

	while (*cp) {
		if (*cp == bs) {
			if (out == buf) {
				cp++;
				continue;
			} else {
				cp++;
				out--;
			}
		} else {
			*out++ = *cp++;
		}

	}
	*out = '\0';
}

/* Make sure line is all seven bits */
void
sevenbit(s)
	char *s;
{
	while (*s)
		*s++ &= 0x7f;
}

/* Set hash type type */
char *
skey_set_algorithm(new)
	char *new;
{
	int i;

	for (i = 0; i < SKEY_ALGORITH_MAX; i++) {
		/* XXX - should be case *sensitive* but need to wait a bit. */
		if (strcasecmp(new, skey_algorithm_table[i].name) == 0) {
			skey_hash_type = i;
			return new;
		}
	}

	return NULL;
}

/* Get current hash type */
const char *
skey_get_algorithm()
{
	return(skey_algorithm_table[skey_hash_type].name);
}

/* Turn echo on/off */
static void
skey_echo(action)
	int action;
{
	static struct termios term;
	static int echo = 0;

	if (action == 0) {
		/* Turn echo off */
		(void) tcgetattr(fileno(stdin), &term);
		if ((echo = (term.c_lflag & ECHO))) {
			term.c_lflag &= ~ECHO;
			(void) tcsetattr(fileno(stdin), TCSAFLUSH|TCSASOFT, &term);
		}
	} else if (action && echo) {
		/* Turn echo on */
		term.c_lflag |= ECHO;
		(void) tcsetattr(fileno(stdin), TCSAFLUSH|TCSASOFT, &term);
		echo = 0;
	}
}
