/*	$OpenBSD: gcvt.c,v 1.1 2002/12/02 15:38:54 millert Exp $	*/

/*
 * Copyright (c) 2002 Todd C. Miller <Todd.Miller@courtesan.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: gcvt.c,v 1.1 2002/12/02 15:38:54 millert Exp $";
#endif /* LIBC_SCCS and not lint */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern char *__dtoa(double, int, int, int *, int *, char **);

char *
gcvt(double value, int ndigit, char *buf)
{
	char *digits, *dst, *src;
	int i, decpt, sign;

	if (ndigit == 0) {
		buf[0] = '\0';
		return (buf);
	}

	digits = __dtoa(value, 2, ndigit, &decpt, &sign, NULL);
	if (decpt == 9999)
		return (strcpy(buf, digits));

	dst = buf;
	if (sign)
		*dst++ = '-';

	if (decpt < 0 || decpt > ndigit) {
		/* exponential format */
		if (--decpt < 0) {
			sign = 1;
			decpt = -decpt;
		} else
			sign = 0;
		for (src = digits; *src != '\0'; )
			*dst++ = *src++;
		*dst++ = 'e';
		if (sign)
			*dst++ = '-';
		else
			*dst++ = '+';
		if (decpt < 10) {
			*dst++ = '0';
			*dst++ = '0' + decpt;
			*dst = '\0';
		} else {
			/* XXX - optimize */
			for (sign = decpt, i = 0; (sign /= 10) != 0; i++)
				sign /= 10;
			while (decpt != 0) {
				dst[i--] = '0' + decpt % 10;
				decpt /= 10;
			}
		}
	} else {
		/* standard format */
		for (i = 0, src = digits; i < decpt; i++) {
			if (*src != '\0')
				*dst++ = *src++;
			else
				*dst++ = '0';
		}
		if (*src != '\0') {
			*dst++ = '.';		/* XXX - locale-specific (LC_NUMERIC) */
			for (i = decpt; digits[i] != '\0'; i++) {
				*dst++ = digits[i];
			}
		}
		*dst = '\0';
	}
	return (buf);
}
