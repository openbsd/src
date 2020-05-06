/* $OpenBSD: opensslv.h,v 1.57 2020/05/06 15:45:22 tb Exp $ */
#ifndef HEADER_OPENSSLV_H
#define HEADER_OPENSSLV_H

/* These will change with each release of LibreSSL-portable */
#define LIBRESSL_VERSION_NUMBER 0x3010100fL
/*                                    ^ Patch starts here   */
#define LIBRESSL_VERSION_TEXT   "LibreSSL 3.1.1"

/* These will never change */
#define OPENSSL_VERSION_NUMBER	0x20000000L
#define OPENSSL_VERSION_TEXT	LIBRESSL_VERSION_TEXT
#define OPENSSL_VERSION_PTEXT	" part of " OPENSSL_VERSION_TEXT

#define SHLIB_VERSION_HISTORY ""
#define SHLIB_VERSION_NUMBER "1.0.0"

#endif /* HEADER_OPENSSLV_H */
