/*	$OpenBSD: unistd.h,v 1.2 2024/09/04 07:52:45 tb Exp $ */
/*
 * Public domain
 * compatibility shim for OpenSSL 3
 * overloading unistd.h is a ugly guly hack for this issue but works here
 */

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
