/*	$OpenBSD: unistd.h,v 1.5 2026/03/27 19:55:35 tb Exp $ */
/*
 * Public domain
 * compatibility shim for OpenSSL 3
 * overloading unistd.h is a ugly guly hack for this issue but works here
 */

#ifndef RPKI_CLIENT_UNISTD_H
#define RPKI_CLIENT_UNISTD_H

#include_next <unistd.h>

#include <openssl/cms.h>
#include <openssl/stack.h>

#ifndef DECLARE_STACK_OF
#define DECLARE_STACK_OF DEFINE_STACK_OF
#endif

static inline int
CMS_get_version(CMS_ContentInfo *cms, long *version)
{
	*version = 3;
	return 1;
}

static inline int
CMS_SignerInfo_get_version(CMS_SignerInfo *si, long *version)
{
	*version = 3;
	return 1;
}

#if !HAVE_X509_CRL_GET0_SIGALG
static inline const X509_ALGOR *
X509_CRL_get0_tbs_sigalg(const X509_CRL *crl)
{
	const X509_ALGOR *alg = NULL;

	X509_CRL_get0_signature(crl, NULL, &alg);
	return alg;
}
#endif

#endif  /* ! RPKI_CLIENT_UNISTD_H */
