/*	$OpenBSD: policy.h,v 1.3 2000/02/20 16:30:20 niklas Exp $	*/
/*	$EOM: policy.h,v 1.5 2000/02/19 07:46:33 niklas Exp $ */

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

#if defined (USE_KEYNOTE)
#define LK(sym, args) sym args
#define LKV(sym) sym
#elif defined (HAVE_DLOPEN)
#define LK(sym, args) lk_ ## sym args
#define LKV(sym) *lk_ ## sym
#else
#define LK(sym, args) !!libkeynote called but no USE_KEYNOTE nor HAVE_DLOPEN!!
#define LKV(sym) !!libkeynote called but no USE_KEYNOTE nor HAVE_DLOPEN!!
#endif

#if defined(HAVE_DLOPEN) && !defined(USE_KEYNOTE)
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
#endif /* HAVE_DLOPEN && !USE_KEYNOTE */

extern int keynote_sessid;

extern void policy_init (void);

#endif /* _POLICY_H_ */
