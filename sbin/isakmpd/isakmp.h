/*	$Id: isakmp.h,v 1.1.1.1 1998/11/15 00:03:48 niklas Exp $	*/

/*
 * Copyright (c) 1998 Niklas Hallqvist.  All rights reserved.
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

#ifndef _ISAKMP_H_
#define _ISAKMP_H_

#include "isakmp_fld.h"
#include "isakmp_num.h"

/* IANA assigned port */
#define UDP_DEFAULT_PORT 500

/* ISAKMP header extras defines */
#define ISAKMP_HDR_COOKIES_OFF	ISAKMP_HDR_ICOOKIE_OFF
#define ISAKMP_HDR_COOKIES_LEN	(ISAKMP_HDR_ICOOKIE_LEN \
				 + ISAKMP_HDR_ICOOKIE_LEN)

/* ISAKMP attribute utilitiy macros.  */
#define ISAKMP_ATTR_FORMAT(x)		((x) >> 15)
#define ISAKMP_ATTR_TYPE(x)		((x) & 0x7fff)
#define ISAKMP_ATTR_MAKE(fmt, type)	(((fmt) << 15) | (type))

/* Version number handling.  */
#define ISAKMP_VERSION_MAJOR(x)		((x) >> 4)
#define ISAKMP_VERSION_MINOR(x)		((x) & 0xf)
#define ISAKMP_VERSION_MAKE(maj, min)	((maj) << 4 | (min))

#endif /* _ISAKMP_H_ */
