/*	$OpenBSD: util.c,v 1.49 2022/01/08 06:49:41 guenther Exp $	*/

/*
 * Copyright (c) 1998 Per Fogelstrom, Opsycon AB
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/types.h>
#include <syslog.h>

#include "syscall.h"
#include "util.h"
#include "resolve.h"
#define KEYSTREAM_ONLY
#include "chacha_private.h"

#ifndef _RET_PROTECTOR
/*
 * Stack protector dummies.
 * Ideally, a scheme to compile these stubs from libc should be used, but
 * this would end up dragging too much code from libc here.
 */
long __guard_local __dso_hidden __attribute__((section(".openbsd.randomdata")));

void __stack_smash_handler(char [], int);

void
__stack_smash_handler(char func[], int damaged)
{
	char message[256];

	/* <10> indicates LOG_CRIT */
	_dl_strlcpy(message, "<10>ld.so:", sizeof message);
	_dl_strlcat(message, __progname, sizeof message);
	if (_dl_strlen(message) > sizeof(message)/2)
		_dl_strlcpy(message + sizeof(message)/2, "...",
		    sizeof(message) - sizeof(message)/2);
	_dl_strlcat(message, " stack overflow in function ", sizeof message);
	_dl_strlcat(message, func, sizeof message);

	_dl_sendsyslog(message, _dl_strlen(message), LOG_CONS);
	_dl_diedie();
}
#endif /* _RET_PROTECTOR */

char *
_dl_strdup(const char *orig)
{
	char *newstr;
	size_t len;

	len = _dl_strlen(orig)+1;
	newstr = _dl_malloc(len);
	if (newstr != NULL)
		_dl_strlcpy(newstr, orig, len);
	return (newstr);
}

#define KEYSZ 32
#define IVSZ  8
#define REKEY_AFTER_BYTES (1 << 31)
static chacha_ctx chacha;
static size_t chacha_bytes;

void
_dl_arc4randombuf(void *buf, size_t buflen)
{
	if (chacha_bytes == 0) {
		char bytes[KEYSZ + IVSZ];

		if (_dl_getentropy(bytes, KEYSZ + IVSZ) != 0)
			_dl_die("no entropy");
		chacha_keysetup(&chacha, bytes, KEYSZ * 8);
		chacha_ivsetup(&chacha, bytes + KEYSZ);
		if (_dl_getentropy(bytes, KEYSZ + IVSZ) != 0)
			_dl_die("could not clobber rng key");
	}

	chacha_encrypt_bytes(&chacha, buf, buf, buflen);

	if (REKEY_AFTER_BYTES - chacha_bytes < buflen)
		chacha_bytes = 0;
	else
		chacha_bytes += buflen;
}

u_int32_t
_dl_arc4random(void)
{
	u_int32_t rnd;

	_dl_arc4randombuf(&rnd, sizeof(rnd));
	return (rnd);
}
