/*	$OpenBSD: udf_subr.c,v 1.1 2006/01/14 19:04:17 miod Exp $	*/

/*
 * Copyright (c) 2006, Miodrag Vallat
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
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/mutex.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/dirent.h>
#include <sys/unistd.h>

#include <isofs/udf/ecma167-udf.h>
#include <isofs/udf/udf.h>
#include <isofs/udf/udf_extern.h>

/*
 * Convert a CS0 dstring to a 16-bit Unicode string.
 * Returns the length of the Unicode string, in unicode characters (not
 * bytes!), or -1 if an error arises.
 * Note that the transname destination buffer is expected to be large
 * enough to hold the result, and will not be terminated in any way.
 */
int
udf_rawnametounicode(u_int len, char *cs0string, unicode_t *transname)
{
	unicode_t *origname = transname;

	if (len-- == 0)
		return (-1);

	switch (*cs0string++) {
	case 8:		/* bytes string */
		while (len-- != 0)
			*transname++ = (unicode_t)*cs0string++;
		break;
	case 16:	/* 16 bit unicode string */
		if (len & 1)
			return (-1);
		len >>= 1;
		while (len-- != 0) {
			unicode_t tmpchar;

			tmpchar = (unicode_t)*cs0string++;
			tmpchar = (tmpchar << 8) | (unicode_t)*cs0string++;
			*transname++ = tmpchar;
		}
		break;
	default:
		return (-1);
	}

	return (transname - origname);
}
