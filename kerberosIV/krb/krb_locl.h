/*	$OpenBSD: krb_locl.h,v 1.7 1998/11/28 23:41:02 art Exp $	*/
/*	$KTH: krb_locl.h,v 1.48 1998/04/04 17:56:49 assar Exp $		*/

/*
 * Copyright (c) 1995, 1996, 1997 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the Kungliga Tekniska
 *      Högskolan and its contributors.
 * 
 * 4. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef __krb_locl_h
#define __krb_locl_h

#include <sys/cdefs.h>
#include <kerberosIV/site.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <stdarg.h>

#include <errno.h>

#include <pwd.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include <errno.h>

#include <kerberosIV/krb.h>
#include <kerberosIV/prot.h>

#include "resolve.h"
#include "krb_log.h"

/* --- */

/* Utils */
int
krb_name_to_name __P((
	const char *host,
	char *phost,
	size_t phost_size));

void
encrypt_ktext __P((
	KTEXT cip,
	des_cblock *key,
	int encrypt));

int
kdc_reply_cipher __P((
	KTEXT reply,
	KTEXT cip));

int
kdc_reply_cred __P((
	KTEXT cip,
	CREDENTIALS *cred));

void
k_ricercar __P((char *name));

/* used in rd_safe.c and mk_safe.c */
void
fixup_quad_cksum __P((
	void *start,
	size_t len,
	des_cblock *key,
	void *new_checksum,
	void *old_checksum,
	int little));

void
krb_kdctimeofday __P((struct timeval *tv));

/* stuff from libroken*/

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

char *strtok_r(char *s1, const char *s2, char **lasts);

int k_concat(char *, size_t, ...);
int k_vconcat(char *, size_t, va_list);
size_t k_vmconcat(char **, size_t, va_list);
size_t k_mconcat(char **, size_t, ...);

/* Temporary fixes for krb_{rd,mk}_safe */
#define DES_QUAD_GUESS 0
#define DES_QUAD_NEW 1
#define DES_QUAD_OLD 2

/* Set this to one of the constants above to specify default checksum
   type to emit */
#define DES_QUAD_DEFAULT DES_QUAD_GUESS

#endif /*  __krb_locl_h */
