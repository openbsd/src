/* $OpenBSD: opensslv.h,v 1.48 2018/09/23 17:52:40 bcook Exp $ */
#ifndef HEADER_OPENSSLV_H
#define HEADER_OPENSSLV_H

/* These will change with each release of LibreSSL-portable */
#define LIBRESSL_VERSION_NUMBER 0x2080100fL
/*                                    ^ Patch starts here   */
#define LIBRESSL_VERSION_TEXT   "LibreSSL 2.8.1"

/* These will never change */
#define OPENSSL_VERSION_NUMBER	0x20000000L
#define OPENSSL_VERSION_TEXT	LIBRESSL_VERSION_TEXT
#define OPENSSL_VERSION_PTEXT	" part of " OPENSSL_VERSION_TEXT

#define SHLIB_VERSION_HISTORY ""
#define SHLIB_VERSION_NUMBER "1.0.0"

#endif /* HEADER_OPENSSLV_H */
