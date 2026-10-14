/*	$OpenBSD: unistd.h,v 1.8 2026/10/14 06:42:35 tb Exp $ */
/*
 * Public domain
 * compatibility shim for OpenSSL 3
 * overloading unistd.h is a ugly guly hack for this issue but works here
 */

#ifndef RPKI_CLIENT_UNISTD_H
#define RPKI_CLIENT_UNISTD_H

#include_next <unistd.h>

#include <openssl/stack.h>
#include <openssl/x509.h>

#ifndef DECLARE_STACK_OF
#define DECLARE_STACK_OF DEFINE_STACK_OF
#endif

#if !HAVE_X509_CRL_GET0_TBS_SIGALG
static inline const X509_ALGOR *
X509_CRL_get0_tbs_sigalg(const X509_CRL *crl)
{
	const X509_ALGOR *alg = NULL;

	X509_CRL_get0_signature(crl, NULL, &alg);
	return alg;
}
#endif

#endif  /* ! RPKI_CLIENT_UNISTD_H */
