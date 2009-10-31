/* $OpenBSD: ap_md5c.c,v 1.11 2009/10/31 13:29:07 sobrado Exp $ */

/* ====================================================================
 * The Apache Software License, Version 1.1
 *
 * Copyright (c) 2000-2003 The Apache Software Foundation.  All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. The end-user documentation included with the redistribution,
 *    if any, must include the following acknowledgment:
 *       "This product includes software developed by the
 *        Apache Software Foundation (http://www.apache.org/)."
 *    Alternately, this acknowledgment may appear in the software itself,
 *    if and wherever such third-party acknowledgments normally appear.
 *
 * 4. The names "Apache" and "Apache Software Foundation" must
 *    not be used to endorse or promote products derived from this
 *    software without prior written permission. For written
 *    permission, please contact apache@apache.org.
 *
 * 5. Products derived from this software may not be called "Apache",
 *    nor may "Apache" appear in their name, without prior written
 *    permission of the Apache Software Foundation.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE APACHE SOFTWARE FOUNDATION OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the Apache Software Foundation.  For more
 * information on the Apache Software Foundation, please see
 * <http://www.apache.org/>.
 *
 * Portions of this software are based upon public domain software
 * originally written at the National Center for Supercomputing Applications,
 * University of Illinois, Urbana-Champaign.
 */

/*
 * The ap_MD5Encode() routine uses much code obtained from the FreeBSD 3.0
 * MD5 crypt() function, which is licenced as follows:
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@login.dknet.dk> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 */

#include <string.h>

#include "ap_config.h"
#include "ap_md5.h"
#include "ap.h"

static void Encode(unsigned char *output, const UINT4 *input, unsigned int len);
static void Decode(UINT4 *output, const unsigned char *input, unsigned int len);

API_EXPORT(void)
ap_MD5Init(AP_MD5_CTX *context)
{
	MD5Init(context);
}

API_EXPORT(void)
ap_MD5Update(AP_MD5_CTX *context, const unsigned char *input,
    unsigned int inputLen)
{
	MD5Update(context, input, inputLen);
}

API_EXPORT(void)
ap_MD5Final(unsigned char digest[16], AP_MD5_CTX *context)
{
	MD5Final(digest, context);
}

/* Encodes input (UINT4) into output (unsigned char). Assumes len is
   a multiple of 4.
 */
static void
Encode(unsigned char *output, const UINT4 *input, unsigned int len)
{
	unsigned int i, j;
	UINT4 k;

	for (i = 0, j = 0; j < len; i++, j += 4) {
		k = input[i];
		output[j] = (unsigned char) (k & 0xff);
		output[j + 1] = (unsigned char) ((k >> 8) & 0xff);
		output[j + 2] = (unsigned char) ((k >> 16) & 0xff);
		output[j + 3] = (unsigned char) ((k >> 24) & 0xff);
	}
}

/* Decodes input (unsigned char) into output (UINT4). Assumes len is
 * a multiple of 4.
 */
static void
Decode(UINT4 *output, const unsigned char *input, unsigned int len)
{
	unsigned int i, j;

	for (i = 0, j = 0; j < len; i++, j += 4)
		output[i] = ((UINT4) input[j]) | (((UINT4) input[j + 1]) << 8) |
		    (((UINT4) input[j + 2]) << 16)
		    | (((UINT4) input[j + 3]) << 24);
}

/*
 * The following MD5 password encryption code was largely borrowed from
 * the FreeBSD 3.0 /usr/src/lib/libcrypt/crypt.c file, which is
 * licenced as stated at the top of this file.
 */
API_EXPORT(void)
ap_to64(char *s, unsigned long v, int n)
{
	static unsigned char itoa64[] =         /* 0 ... 63 => ASCII - 64 */
	    "./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

	while (--n >= 0) {
		*s++ = itoa64[v&0x3f];
		v >>= 6;
	}
}

API_EXPORT(void)
ap_MD5Encode(const unsigned char *pw, const unsigned char *salt, char *result,
    size_t nbytes)
{
	/*
	* Minimum size is 8 bytes for salt, plus 1 for the trailing NUL,
	* plus 4 for the '$' separators, plus the password hash itself.
	* Let's leave a goodly amount of leeway.
	*/

	char passwd[120], *p;
	const unsigned char *sp, *ep;
	unsigned char final[16];
	int i;
	unsigned int sl;
	int pl;
	unsigned int pwlen;
	MD5_CTX ctx, ctx1;
	unsigned long l;

	/* 
	* Refine the salt first.  It's possible we were given an already-hashed
	* string as the salt argument, so extract the actual salt value from it
	* if so.  Otherwise just use the string up to the first '$' as the salt.
	*/
	sp = salt;

	/*
	* If it starts with the magic string, then skip that.
	*/
	if (strncmp((char *)sp, AP_MD5PW_ID, AP_MD5PW_IDLEN) == 0)
		sp += AP_MD5PW_IDLEN;

	/*
	* It stops at the first '$' or 8 chars, whichever comes first
	*/
	for (ep = sp; (*ep != '\0') && (*ep != '$') && (ep < (sp + 8)); ep++)
		continue;

	/*
	* Get the length of the true salt
	*/
	sl = ep - sp;

	/*
	* 'Time to make the doughnuts..'
	*/
	MD5Init(&ctx);

	pwlen = strlen((char *)pw);
	/*
	* The password first, since that is what is most unknown
	*/
	MD5Update(&ctx, pw, pwlen);

	/*
	* Then our magic string
	*/
	MD5Update(&ctx, (const unsigned char *) AP_MD5PW_ID, AP_MD5PW_IDLEN);

	/*
	* Then the raw salt
	*/
	MD5Update(&ctx, sp, sl);

	/*
	* Then just as many characters of the MD5(pw, salt, pw)
	*/
	MD5Init(&ctx1);
	MD5Update(&ctx1, pw, pwlen);
	MD5Update(&ctx1, sp, sl);
	MD5Update(&ctx1, pw, pwlen);
	MD5Final(final, &ctx1);
	for(pl = pwlen; pl > 0; pl -= 16)
		MD5Update(&ctx, final, (pl > 16) ? 16 : (unsigned int) pl);

	/*
	* Don't leave anything around in vm they could use.
	*/
	memset(final, 0, sizeof(final));

	/*
	* Then something really weird...
	*/
	for (i = pwlen; i != 0; i >>= 1) {
		if (i & 1)
		    MD5Update(&ctx, final, 1);
		else
		    MD5Update(&ctx, pw, 1);
	}

	/*
	* Now make the output string.  We know our limitations, so we
	* can use the string routines without bounds checking.
	*/
	ap_cpystrn(passwd, AP_MD5PW_ID, AP_MD5PW_IDLEN + 1);
	ap_cpystrn(passwd + AP_MD5PW_IDLEN, (char *)sp, sl + 1);
	passwd[AP_MD5PW_IDLEN + sl]     = '$';
	passwd[AP_MD5PW_IDLEN + sl + 1] = '\0';

	MD5Final(final, &ctx);

	/*
	* And now, just to make sure things don't run too fast..
	* On a 60 MHz Pentium this takes 34 msec, so you would
	* need 30 seconds to build a 1000 entry dictionary...
	*/
	for (i = 0; i < 1000; i++) {
		MD5Init(&ctx1);
		if (i & 1)
		    MD5Update(&ctx1, pw, pwlen);
		else
		    MD5Update(&ctx1, final, 16);
		if (i % 3)
		    MD5Update(&ctx1, sp, sl);

		if (i % 7)
		    MD5Update(&ctx1, pw, pwlen);

		if (i & 1)
		    MD5Update(&ctx1, final, 16);
		else
		    MD5Update(&ctx1, pw, pwlen);
		MD5Final(final,&ctx1);
	}

	p = passwd + strlen(passwd);

	l = (final[ 0]<<16) | (final[ 6]<<8) | final[12]; ap_to64(p, l, 4);
	p += 4;
	l = (final[ 1]<<16) | (final[ 7]<<8) | final[13]; ap_to64(p, l, 4);
	p += 4;
	l = (final[ 2]<<16) | (final[ 8]<<8) | final[14]; ap_to64(p, l, 4);
	p += 4;
	l = (final[ 3]<<16) | (final[ 9]<<8) | final[15]; ap_to64(p, l, 4);
	p += 4;
	l = (final[ 4]<<16) | (final[10]<<8) | final[ 5]; ap_to64(p, l, 4);
	p += 4;
	l =                    final[11]                ; ap_to64(p, l, 2);
	p += 2;
	*p = '\0';

	/*
	* Don't leave anything around in vm they could use.
	*/
	memset(final, 0, sizeof(final));

	ap_cpystrn(result, passwd, nbytes - 1);
}
