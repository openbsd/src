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
 * $Id: skeysubr.c,v 1.1.1.1 1995/10/18 08:43:11 deraadt Exp $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <termios.h>

#include "md4.h"
#include "skey.h"

struct termios newtty;
struct termios oldtty;

static void trapped __ARGS((int sig));
static void set_term __ARGS((void));
static void unset_term __ARGS((void));
static void echo_off __ARGS((void));

/* Crunch a key:
 * concatenate the seed and the password, run through MD4 and
 * collapse to 64 bits. This is defined as the user's starting key.
 */
int
keycrunch(result,seed,passwd)
char *result;	/* 8-byte result */
char *seed;	/* Seed, any length */
char *passwd;	/* Password, any length */
{
	char *buf;
	MDstruct md;
	unsigned int buflen;
	int i;
	register long tmp;
	
	buflen = strlen(seed) + strlen(passwd);
	if ((buf = (char *)malloc(buflen+1)) == NULL)
		return -1;
	strcpy(buf,seed);
	strcat(buf,passwd);

	/* Crunch the key through MD4 */
	sevenbit(buf);
	MDbegin(&md);
	MDupdate(&md,(unsigned char *)buf,8*buflen);

	free(buf);

	/* Fold result from 128 to 64 bits */
	md.buffer[0] ^= md.buffer[2];
	md.buffer[1] ^= md.buffer[3];

	/* Default (but slow) code that will convert to
	 * little-endian byte ordering on any machine
	 */
	for (i=0; i<2; i++) {
		tmp = md.buffer[i];
		*result++ = tmp;
		tmp >>= 8;
		*result++ = tmp;
		tmp >>= 8;
		*result++ = tmp;
		tmp >>= 8;
		*result++ = tmp;
	}

	return 0;
}

/* The one-way function f(). Takes 8 bytes and returns 8 bytes in place */
void
f(x)
	char *x;
{
	MDstruct md;
	register long tmp;

	MDbegin(&md);
	MDupdate(&md,(unsigned char *)x,64);

	/* Fold 128 to 64 bits */
	md.buffer[0] ^= md.buffer[2];
	md.buffer[1] ^= md.buffer[3];

	/* Default (but slow) code that will convert to
	 * little-endian byte ordering on any machine
	 */
	tmp = md.buffer[0];
	*x++ = tmp;
	tmp >>= 8;
	*x++ = tmp;
	tmp >>= 8;
	*x++ = tmp;
	tmp >>= 8;
	*x++ = tmp;

	tmp = md.buffer[1];
	*x++ = tmp;
	tmp >>= 8;
	*x++ = tmp;
	tmp >>= 8;
	*x++ = tmp;
	tmp >>= 8;
	*x = tmp;
}

/* Strip trailing cr/lf from a line of text */
void
rip(buf)
	char *buf;
{
	char *cp;

	if ((cp = strchr(buf,'\r')) != NULL)
		*cp = '\0';

	if ((cp = strchr(buf,'\n')) != NULL)
		*cp = '\0';
}

char *
readpass (buf,n)
	char *buf;
	int n;
{
	set_term();
	echo_off();

	fgets(buf, n, stdin);

	rip(buf);
	printf("\n");

	sevenbit(buf);

	unset_term();
	return buf;
}

char *
readskey(buf, n)
	char *buf;
	int n;
{
	fgets (buf, n, stdin);

	rip(buf);
	printf ("\n");

	sevenbit (buf);

	return buf;
}

static void
set_term() 
{
	fflush(stdout);
	tcgetattr(fileno(stdin), &newtty);
	tcgetattr(fileno(stdin), &oldtty);
 
	signal (SIGINT, trapped);
}

static void
echo_off()
{
	newtty.c_lflag &= ~(ICANON | ECHO | ECHONL);
	newtty.c_cc[VMIN] = 1;
	newtty.c_cc[VTIME] = 0;
	newtty.c_cc[VINTR] = 3;

	tcsetattr(fileno(stdin), TCSADRAIN, &newtty);
}

static void
unset_term()
{
	tcsetattr(fileno(stdin), TCSADRAIN, &oldtty);
}

static void
trapped(sig)
	int sig;
{
	signal(SIGINT, trapped);
	printf("^C\n");
	unset_term();
	exit(-1);
}

/* Convert 8-byte hex-ascii string to binary array
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

	for (i=0;i<8;i++) {
		sprintf(out,"%02x",*in++ & 0xff);
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

/* removebackspaced over charaters from the string */
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
			}
			else {
			  cp++;
			  out--;
			}
		} else {
			*out++ = *cp++;
		}

	}
	*out = '\0';
}

/* sevenbit ()
 *
 * Make sure line is all seven bits.
 */
 
void
sevenbit(s)
	char *s;
{
	while (*s)
		*s++ &= 0x7f;
}
