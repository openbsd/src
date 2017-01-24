/*	$OpenBSD: util.c,v 1.44 2017/01/24 07:48:37 guenther Exp $	*/

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
#include "archdep.h"
#include "resolve.h"

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
	_dl_strlcat(message, "stack overflow in function ", sizeof message);
	_dl_strlcat(message, func, sizeof message);

	_dl_sendsyslog(message, _dl_strlen(message), LOG_CONS);
	_dl_diedie();
}

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

void
_dl_arc4randombuf(void *v, size_t buflen)
{
	static char bytes[256];
	static u_int reserve;
	char *buf = v;
	size_t chunk;

	while (buflen != 0) {
		if (reserve == 0) {
			if (_dl_getentropy(bytes, sizeof(bytes)) != 0)
				_dl_die("no entropy");
			reserve = sizeof(bytes);
		}
		if (buflen > reserve)
			chunk = reserve;
		else
			chunk = buflen;
#if 0
		memcpy(buf, bytes + reserve - chunk, chunk);
		memset(bytes + reserve - chunk, 0, chunk);
#else
		{
			char *d = buf;
			char *s = bytes + reserve - chunk;
			u_int l;
			for (l = chunk; l > 0; l--, s++, d++) {
				*d = *s;
				*s = 0;
			}
		}
#endif
		reserve -= chunk;
		buflen -= chunk;
		buf += chunk;
	}
}

u_int32_t
_dl_arc4random(void)
{
	u_int32_t rnd;
	_dl_arc4randombuf(&rnd, sizeof(rnd));
	return (rnd);
}
