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
 * $Id: skeysubr.c,v 1.3 1996/09/27 20:40:17 millert Exp $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <termios.h>
#include <md4.h>
#include <md5.h>

#include "skey.h"

/* Default MDX function to use (currently 4 or 5) */
#ifndef SKEY_MDX_DEFAULT
#define SKEY_MDX_DEFAULT	5
#endif

static void trapped __ARGS((int sig));
static void f_MD4 __ARGS ((char *x));
static void f_MD5 __ARGS ((char *x));
static void skey_echo __ARGS ((int));
static int keycrunch_MD4 __ARGS ((char *result, char *seed, char *passwd));
static int keycrunch_MD5 __ARGS ((char *result, char *seed, char *passwd));

static int skey_MDX = 0;

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
	switch (skey_get_MDX()) {
		/*
		 * Need a default case to appease gc even though 
		 * skey_set_MDX() guantaees we get back 4 or 5
		 */
		case 4  : return(keycrunch_MD4(result, seed, passwd));
		default : return(keycrunch_MD5(result, seed, passwd));
	}
	/* NOTREACHED */
}

static int
keycrunch_MD4(result, seed, passwd)
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
	MD4Final((unsigned char *)results ,&md);
	(void)free(buf);

	/* Fold result from 128 to 64 bits */
	results[0] ^= results[2];
	results[1] ^= results[3];

	(void)memcpy((void *)result, (void *)results, 8);

	return 0;
}

static int
keycrunch_MD5(result, seed, passwd)
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
	MD5Final((unsigned char *)results ,&md);
	(void)free(buf);

	/* Fold result from 128 to 64 bits */
	results[0] ^= results[2];
	results[1] ^= results[3];

	(void)memcpy((void *)result, (void *)results, 8);

	return 0;
}

/* The one-way function f(). Takes 8 bytes and returns 8 bytes in place */
void
f(x)
	char *x;
{
	switch (skey_get_MDX()) {
		/*
		 * Need a default case to appease gc even though 
		 * skey_set_MDX() guantaees we get back 4 or 5
		 */
		case 4  : return(f_MD4(x));
		default : return(f_MD5(x));
	}
	/* NOTREACHED */
}

void
f_MD4(x)
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
f_MD5(x)
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

/* Set MDX type (returns previous) */
int
skey_set_MDX(new)
	int new;
{
	int old;

	if (new != 4 && new != 5)
		return -1;

	old = skey_get_MDX();
	skey_MDX = new;
	return old;
}

/* Get current MDX type */
int
skey_get_MDX()
{
	if (skey_MDX == 0)
		skey_MDX = SKEY_MDX_DEFAULT;

	return skey_MDX;
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
