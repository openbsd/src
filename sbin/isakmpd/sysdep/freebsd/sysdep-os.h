/*	$OpenBSD: sysdep-os.h,v 1.4 2002/01/25 13:55:21 ho Exp $	*/

/*
 * Copyright (c) 1998, 1999 Niklas Hallqvist.  All rights reserved.
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

#ifndef _SYSDEP_OS_H_

#define _SYSDEP_OS_H_

#define KAME

#include <netinet6/ipsec.h>

#if ( __FreeBSD_cc_version < 440000 )
/* We need in_addr_t & in_port_t */
typedef u_int32_t in_addr_t;
typedef u_int16_t in_port_t;
#endif
#if ( __FreeBSD__ < 4 )
/* We need socklen_t too. */
typedef u_int32_t socklen_t;
#endif

/* Map extensions to values from /usr/include/net/pfkeyv2.h */
#if ( SADB_EALG_MAX == 7 )
/* FreeBSD 4.2 */
#define SADB_X_EALG_BLF			SADB_EALG_BLOWFISHCBC
#define SADB_X_EALG_CAST		SADB_EALG_CAST128CBC
#else if ( SADB_EALG_MAX == 12 )
/* FreeBSD 4.4 */
#define SADB_X_EALG_BLF			SADB_X_EALG_BLOWFISHCBC
#define SADB_X_EALG_CAST		SADB_X_EALG_CAST128CBC
#endif

#if 0
/* OpenSSL differs from OpenBSD very slightly... */

#define SHA1Init SHA1_Init
#define SHA1Update SHA1_Update
#define SHA1Final SHA1_Final

#define cast_key CAST_KEY
#define cast_setkey(k, d, l) CAST_set_key ((k), (l), (d))
#define cast_encrypt(k, i, o) do { \
  memcpy ((o), (i), BLOCKSIZE); \
  CAST_encrypt ((CAST_LONG *)(o), (k)); \
}
#define cast_decrypt(k, i, o) do { \
  memcpy ((o), (i), BLOCKSIZE); \
  CAST_decrypt ((CAST_LONG *)(o), (k)); \
}
#endif

#endif /* _SYSDEP_OS_H_ */
