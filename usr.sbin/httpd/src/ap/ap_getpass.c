/* $OpenBSD: ap_getpass.c,v 1.8 2005/03/28 21:03:33 niallo Exp $ */

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
 * ap_getpass.c: abstraction to provide for obtaining a password from the
 * command line in whatever way the OS supports.  In the best case, it's a
 * wrapper for the system library's getpass() routine; otherwise, we
 * use one we define ourselves.
 */

#include "ap_config.h"
#include <sys/types.h>
#include <errno.h>
#include "ap.h"

#define LF 10
#define CR 13

#define MAX_STRING_LEN 256

#define ERR_OVERFLOW 5

/*
 * Use the OS getpass() routine (or our own) to obtain a password from
 * the input stream.
 *
 * Exit values:
 *  0: Success
 *  5: Partial success; entered text truncated to the size of the
 *     destination buffer
 *
 * Restrictions: Truncation also occurs according to the host system's
 * getpass() semantics, or at position 255 if our own version is used,
 * but the caller is *not* made aware of it.
 */

API_EXPORT(int)
ap_getpass(const char *prompt, char *pwbuf, size_t bufsiz)
{
	char *pw_got;
	int result = 0;

	pw_got = getpass(prompt);
	if (strlen(pw_got) > (bufsiz - 1))
		result = ERR_OVERFLOW;
	ap_cpystrn(pwbuf, pw_got, bufsiz);
	return result;
}
