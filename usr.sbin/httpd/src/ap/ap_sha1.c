/* $OpenBSD: ap_sha1.c,v 1.9 2005/03/28 21:03:33 niallo Exp $ */

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
 *
 * The exported function:
 *
 *       ap_sha1_base64(const char *clear, int len, char *out);
 *
 * provides a means to SHA1 crypt/encode a plaintext password in
 * a way which makes password files compatible with those commonly
 * used in netscape web and ldap installations. It was put together
 * by Clinton Wong <clintdw@netcom.com>, who also notes that:
 *
 * Note: SHA1 support is useful for migration purposes, but is less
 *     secure than Apache's password format, since Apache's (MD5)
 *     password format uses a random eight character salt to generate
 *     one of many possible hashes for the same password.  Netscape
 *     uses plain SHA1 without a salt, so the same password
 *     will always generate the same hash, making it easier
 *     to break since the search space is smaller.
 *
 * See also the documentation in support/SHA1 as to hints on how to
 * migrate an existing netscape installation and other supplied utitlites.
 *
 * This software also makes use of the following component:
 *
 * NIST Secure Hash Algorithm
 *      heavily modified by Uwe Hollerbach uh@alumni.caltech edu
 *      from Peter C. Gutmann's implementation as found in
 *      Applied Cryptography by Bruce Schneier
 *      This code is hereby placed in the public domain
 */

#include <string.h>

#include "ap_config.h"
#include "ap_sha1.h"
#include "ap.h"


API_EXPORT(void)
ap_SHA1Init(AP_SHA1_CTX *sha_info)
{
	SHA1Init(sha_info);
}

/* update the SHA digest */

API_EXPORT(void)
ap_SHA1Update_binary(AP_SHA1_CTX *sha_info, const unsigned char *buffer,
    unsigned int count)
{
	SHA1Update(sha_info, buffer, count);
}

API_EXPORT(void)
ap_SHA1Update(AP_SHA1_CTX *sha_info, const char *buf, unsigned int count)
{
	SHA1Update(sha_info, (const unsigned char *) buf, count);
}

/* finish computing the SHA digest */

API_EXPORT(void)
ap_SHA1Final(unsigned char digest[SHA_DIGESTSIZE], AP_SHA1_CTX *sha_info)
{
	SHA1Final(digest, sha_info);
}


API_EXPORT(void)
ap_sha1_base64(const char *clear, int len, char *out)
{
	int l;
	AP_SHA1_CTX context;
	unsigned char digest[SHA_DIGESTSIZE];

	if (strncmp(clear, AP_SHA1PW_ID, AP_SHA1PW_IDLEN) == 0)
		clear += AP_SHA1PW_IDLEN;

	ap_SHA1Init(&context);
	ap_SHA1Update(&context, clear, len);
	ap_SHA1Final(digest, &context);

	/* private marker. */
	ap_cpystrn(out, AP_SHA1PW_ID, AP_SHA1PW_IDLEN + 1);

	/* SHA1 hash is always 20 chars */
	l = ap_base64encode_binary(out + AP_SHA1PW_IDLEN, digest,
	    sizeof(digest));
	out[l + AP_SHA1PW_IDLEN] = '\0';

	/*
	* output of base64 encoded SHA1 is always 28 chars + AP_SHA1PW_IDLEN
	*/
}
