/*	$OpenBSD: util.c,v 1.37 2015/01/16 16:18:07 deraadt Exp $	*/

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
#include "archdep.h"

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
	extern const char *_dl_progname;
	char message[100];

	/* <10> indicates LOG_CRIT */
	_dl_strlcpy(message, "<10>ld.so:", sizeof message);
	_dl_strlcat(message, _dl_progname, sizeof message);
	_dl_strlcat(message, "stack overflow in function ", sizeof message);
	_dl_strlcat(message, func, sizeof message);

	_dl_sendsyslog(message, _dl_strlen(message));
	_dl_exit(127);
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
_dl_randombuf(void *v, size_t buflen)
{
	char *buf = v;
	size_t chunk;

	while (buflen != 0) {
		if (buflen > 256)
			chunk = 256;
		else
			chunk = buflen;
		if (_dl_getentropy(buf, chunk) != 0)
			_dl_exit(8);
		buflen -= chunk;
		buf += chunk;
	}
}

u_int32_t
_dl_random(void)
{
	u_int32_t rnd;
	_dl_randombuf(&rnd, sizeof(rnd));
	return (rnd);
}
