/*
 * Copyright (C) 2001  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
 * INTERNET SOFTWARE CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
 * FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* $ISC: platform.h,v 1.5 2001/07/26 03:15:15 mayer Exp $ */

#ifndef ISC_PLATFORM_H
#define ISC_PLATFORM_H 1

/*****
 ***** Platform-dependent defines.
 *****/

#define ISC_PLATFORM_USETHREADS

/***
 *** Network.
 ***/

#define ISC_PLATFORM_HAVEIPV6
#define ISC_PLATFORM_NEEDPORTT
#undef MSG_TRUNC
#define ISC_PLATFORM_NEEDNTOP
#define ISC_PLATFORM_NEEDPTON
#define ISC_PLATFORM_NEEDATON

#define ISC_PLATFORM_QUADFORMAT "I64"

#define ISC_PLATFORM_NEEDSTRSEP

/*
 * Used to control how extern data is linked; needed for Win32 platforms.
 */
#define ISC_PLATFORM_USEDECLSPEC 1

/*
 * Define this here for now as winsock2.h defines h_errno
 * and we don't want to redeclare it.
 */
#define ISC_PLATFORM_NONSTDHERRNO
 /*
 * Set up a macro for importing and exporting from the DLL
 */

#ifdef LIBISC_EXPORTS
#define LIBISC_EXTERNAL_DATA __declspec( dllexport )
#else
#define LIBISC_EXTERNAL_DATA __declspec( dllimport ) 
#endif

#ifdef LIBISCCFG_EXPORTS
#define LIBISCCFG_EXTERNAL_DATA __declspec( dllexport )
#else
#define LIBISCCFG_EXTERNAL_DATA __declspec( dllimport ) 
#endif

#ifdef LIBISCCC_EXPORTS
#define LIBISCCC_EXTERNAL_DATA __declspec( dllexport )
#else
#define LIBISCCC_EXTERNAL_DATA __declspec( dllimport ) 
#endif

#ifdef LIBDNS_EXPORTS
#define LIBDNS_EXTERNAL_DATA __declspec( dllexport )
#else
#define LIBDNS_EXTERNAL_DATA __declspec( dllimport )
#endif

#endif /* ISC_PLATFORM_H */
