/*	$OpenBSD: sysdep-os.h,v 1.8 2003/06/03 15:20:41 ho Exp $	*/

/*
 * Copyright (c) 1998, 1999 Niklas Hallqvist.  All rights reserved.
 * Copyright (c) 2003 Thomas Walpuski.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

#ifndef _SYSDEP_OS_H_
#define _SYSDEP_OS_H_

#include <netinet/in.h>
#include <time.h>
#include <sys/types.h>
#include <linux/ipsec.h>

#define KAME

#define LINUX_IPSEC

#define uh_sport source
#define uh_dport dest
#define uh_ulen len
#define uh_sum check

#ifndef CPI_RESERVED_MAX
#define CPI_RESERVED_MIN		1
#define CPI_RESERVED_MAX		255
#define CPI_PRIVATE_MIN			61440
#define CPI_PRIVATE_MAX			65536
#endif

#define SADB_X_EALG_AES			SADB_X_EALG_AESCBC
#define SADB_X_EALG_CAST		SADB_X_EALG_CASTCBC
#define SADB_X_EALG_BLF			SADB_X_EALG_BLOWFISHCBC

#define IP_IPSEC_POLICY			16
#define IPV6_IPSEC_POLICY		34

#define IPV6_VERSION			0x1

size_t strlcat(char *dst, const char *src, size_t siz);
size_t strlcpy(char *dst, const char *src, size_t siz);

#endif
