/*	$OpenBSD: policy.h,v 1.6 2000/06/08 20:50:52 niklas Exp $	*/
/*	$EOM: policy.h,v 1.11 2000/05/21 04:24:54 angelos Exp $ */

/*
 * Copyright (c) 1999 Angelos D. Keromytis.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Ericsson Radio Systems.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This code was written under funding by Ericsson Radio Systems.
 */

#ifndef _POLICY_H_
#define _POLICY_H_

#ifndef POLICY_FILE_DEFAULT
#define POLICY_FILE_DEFAULT "/etc/isakmpd/isakmpd.policy"
#endif /* POLICY_FILE_DEFAULT */

#if defined (USE_KEYNOTE)
#define CREDENTIAL_FILE "credentials"
#define PRIVATE_KEY_FILE "private_key"

#define LK(sym, args) sym args
#define LKV(sym) sym
#elif defined (HAVE_DLOPEN) && 0
#define LK(sym, args) lk_ ## sym args
#define LKV(sym) *lk_ ## sym
#else
#define LK(sym, args) !!libkeynote called but no USE_KEYNOTE nor HAVE_DLOPEN!!
#define LKV(sym) !!libkeynote called but no USE_KEYNOTE nor HAVE_DLOPEN!!
#endif

#if defined(HAVE_DLOPEN) && !defined(USE_KEYNOTE) && 0
struct keynote_deckey;

extern void *libkeynote;

/*
 * These prototypes matches OpenBSD keynote.h 1.6.  If you use
 * a different version than that, you are on your own.
 */
extern int *lk_keynote_errno;
extern int (*lk_kn_add_action) (int, char *, char *, int);
extern int (*lk_kn_add_assertion) (int, char *, int, int);
extern int (*lk_kn_add_authorizer) (int, char *);
extern int (*lk_kn_close) (int);
extern int (*lk_kn_do_query) (int, char **, int);
extern char *(*lk_kn_encode_key) (struct keynote_deckey *, int, int, int);
extern int (*lk_kn_init) (void);
extern char **(*lk_kn_read_asserts) (char *, int, int *);
extern int (*lk_kn_remove_authorizer) (int, char *);
extern void (*lk_kn_free_key) (struct keynote_deckey *);
extern void *(*lk_kn_get_authorizer) (int, int, int*);
#endif /* HAVE_DLOPEN && !USE_KEYNOTE */

extern int keynote_sessid;
extern int keynote_policy_asserts_num;
extern int x509_policy_asserts_num;
extern int x509_policy_asserts_num_alloc;
extern char **keynote_policy_asserts;
extern char **x509_policy_asserts;
extern struct exchange *policy_exchange;
extern struct sa *policy_sa;
extern struct sa *policy_isakmp_sa;

extern void policy_init (void);
extern char *policy_callback (char *);
extern int keynote_cert_init (void);
extern void *keynote_cert_get (u_int8_t *, u_int32_t);
extern int keynote_cert_validate (void *);
extern int keynote_cert_insert (int, void *);
extern void keynote_cert_free (void *);
extern int keynote_certreq_validate (u_int8_t *, u_int32_t);
extern void *keynote_certreq_decode (u_int8_t *, u_int32_t);
extern void keynote_free_aca (void *);
extern int keynote_cert_obtain (u_int8_t *, size_t, void *,
				u_int8_t **, u_int32_t *);
extern int keynote_cert_get_subject (void *, u_int8_t **, u_int32_t *);
extern int keynote_cert_get_key (void *, void *);
#endif /* _POLICY_H_ */
