/*	$OpenBSD: sysdep-os.h,v 1.4 1999/07/08 17:49:35 niklas Exp $	*/
/*	$EOM: sysdep-os.h,v 1.3 1999/07/08 16:48:40 niklas Exp $	*/

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

#ifdef SADB_EXT_X_SRC_MASK

/* Non-conformant PF_KEYv2 extensions, transform them into being conformant. */

#define SADB_X_EXT_SRC_MASK		SADB_EXT_X_SRC_MASK
#define SADB_X_EXT_DST_MASK		SADB_EXT_X_DST_MASK
#define SADB_X_EXT_PROTOCOL		SADB_EXT_X_PROTOCOL
#define SADB_X_EXT_SA2			SADB_EXT_X_SA2
#define SADB_X_EXT_SRC_FLOW		SADB_EXT_X_SRC_FLOW
#define SADB_X_EXT_DST_FLOW		SADB_EXT_X_DST_FLOW
#define SADB_X_EXT_DST2			SADB_EXT_X_DST2

#define SADB_X_SATYPE_AH_OLD		SADB_SATYPE_X_AH_OLD
#define SADB_X_SATYPE_ESP_OLD		SADB_SATYPE_X_ESP_OLD
#define SADB_X_SATYPE_IPIP		SADB_SATYPE_X_IPIP

#define SADB_X_AALG_RIPEMD160HMAC96	SADB_AALG_X_RIPEMD160HMAC96
#define SADB_X_AALG_MD5			SADB_AALG_X_MD5
#define SADB_X_AALG_SHA1		SADB_AALG_X_SHA1

#define SADB_X_EALG_BLF			SADB_EALG_X_BLF
#define SADB_X_EALG_CAST		SADB_EALG_X_CAST
#define SADB_X_EALG_SKIPJACK		SADB_EALG_X_SKIPJACK

#define SADB_X_SAFLAGS_HALFIV    	SADB_SAFLAGS_X_HALFIV
#define SADB_X_SAFLAGS_TUNNEL	 	SADB_SAFLAGS_X_TUNNEL
#define SADB_X_SAFLAGS_CHAINDEL  	SADB_SAFLAGS_X_CHAINDEL
#define SADB_X_SAFLAGS_LOCALFLOW 	SADB_SAFLAGS_X_LOCALFLOW
#define SADB_X_SAFLAGS_REPLACEFLOW	SADB_SAFLAGS_X_REPLACEFLOW

#endif	/* SADB_EXT_X_SRC_MASK */

#endif /* _SYSDEP_OS_H_ */
