/* e_os2.h */
/* ====================================================================
 * Copyright (c) 1998-2000 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.openssl.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    openssl-core@openssl.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.openssl.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com).
 *
 */

#include <openssl/opensslconf.h>

#ifndef HEADER_E_OS2_H
#define HEADER_E_OS2_H

#ifdef  __cplusplus
extern "C" {
#endif

/******************************************************************************
 * Detect operating systems.  This probably needs completing.
 * The result is that at least one OPENSSL_SYS_os macro should be defined.
 * However, if none is defined, Unix is assumed.
 **/

#define OPENSSL_SYS_UNIX

/* --------------------------------- Unix ---------------------------------- */
#ifdef OPENSSL_SYS_UNIX
# if defined(linux) || defined(__linux__) || defined(OPENSSL_SYSNAME_LINUX)
#  define OPENSSL_SYS_LINUX
# endif
# ifdef OPENSSL_SYSNAME_MPE
#  define OPENSSL_SYS_MPE
# endif
# ifdef OPENSSL_SYSNAME_SNI
#  define OPENSSL_SYS_SNI
# endif
# ifdef OPENSSL_SYSNAME_ULTRASPARC
#  define OPENSSL_SYS_ULTRASPARC
# endif
# ifdef OPENSSL_SYSNAME_NEWS4
#  define OPENSSL_SYS_NEWS4
# endif
# ifdef OPENSSL_SYSNAME_MACOSX
#  define OPENSSL_SYS_MACOSX
# endif
# ifdef OPENSSL_SYSNAME_MACOSX_RHAPSODY
#  define OPENSSL_SYS_MACOSX_RHAPSODY
#  define OPENSSL_SYS_MACOSX
# endif
# ifdef OPENSSL_SYSNAME_SUNOS
#  define OPENSSL_SYS_SUNOS
#endif
# if defined(_CRAY) || defined(OPENSSL_SYSNAME_CRAY)
#  define OPENSSL_SYS_CRAY
# endif
# if defined(_AIX) || defined(OPENSSL_SYSNAME_AIX)
#  define OPENSSL_SYS_AIX
# endif
#endif

/**
 * That's it for OS-specific stuff
 *****************************************************************************/


/* Specials for I/O an exit */
# define OPENSSL_UNISTD_IO OPENSSL_UNISTD
# define OPENSSL_DECLARE_EXIT /* declared in unistd.h */
# define OPENSSL_EXPORT extern
# define OPENSSL_IMPORT extern
# define OPENSSL_GLOBAL
#define OPENSSL_EXTERN OPENSSL_IMPORT

/* Macros to allow global variables to be reached through function calls when
   required (if a shared library version requires it, for example.
   The way it's done allows definitions like this:

	// in foobar.c
	OPENSSL_IMPLEMENT_GLOBAL(int,foobar,0)
	// in foobar.h
	OPENSSL_DECLARE_GLOBAL(int,foobar);
	#define foobar OPENSSL_GLOBAL_REF(foobar)
*/
#ifdef OPENSSL_EXPORT_VAR_AS_FUNCTION
# define OPENSSL_IMPLEMENT_GLOBAL(type,name,value)			\
	type *_shadow_##name(void)					\
	{ static type _hide_##name=value; return &_hide_##name; }
# define OPENSSL_DECLARE_GLOBAL(type,name) type *_shadow_##name(void)
# define OPENSSL_GLOBAL_REF(name) (*(_shadow_##name()))
#else
# define OPENSSL_IMPLEMENT_GLOBAL(type,name,value) OPENSSL_GLOBAL type _shadow_##name=value;
# define OPENSSL_DECLARE_GLOBAL(type,name) OPENSSL_EXPORT type _shadow_##name
# define OPENSSL_GLOBAL_REF(name) _shadow_##name
#endif
#  define ossl_ssize_t ssize_t
#ifdef  __cplusplus
}
#endif
#endif
