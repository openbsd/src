/* $OpenBSD: uid.c,v 1.8 2014/06/12 15:49:27 deraadt Exp $ */
/*
 * Written by Theo de Raadt.  Public domain.
 */

#include <unistd.h>

int
OPENSSL_issetugid(void)
{
	return issetugid();
}
