/*	$OpenBSD: cryptodev.h,v 1.3 2001/06/01 23:51:27 deraadt Exp $	*/

/*
 * Copyright (c) 2001 Theo de Raadt
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
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

#include <sys/ioccom.h>

struct session_op {
	u_int32_t	cipher;		/* ie. CRYPTO_DES_CBC */
	u_int32_t	mac;		/* ie. CRYPTO_MD5_HMAC */

	u_int32_t	keylen;		/* cipher key */
	caddr_t		key;
	int		mackeylen;	/* mac key */
	caddr_t		mackey;

	u_int32_t	ses;		/* returns: session # */
};

struct crypt_op {
	u_int32_t	ses;
	u_int16_t	op;
	u_int16_t	flags;		/* always 0 */

	u_int		len;
	caddr_t		src, dst;	/* become iov[] inside kernel */
	caddr_t		mac;
	caddr_t		iv;
};

#define COP_ENCRYPT	1
#define COP_DECRYPT	2
/* #define COP_SETKEY	3 */
/* #define COP_GETKEY	4 */

#define	CRIOGET		_IOWR('c', 100, u_int32_t)

#define	CIOCGSESSION	_IOWR('c', 101, struct session_op)
#define	CIOCFSESSION	_IOW('c', 102, u_int32_t)
#define CIOCCRYPT	_IOWR('c', 103, struct crypt_op)
