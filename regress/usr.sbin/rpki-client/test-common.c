/*	$OpenBSD: test-common.c,v 1.1 2021/03/29 15:47:34 claudio Exp $ */
/*
 * Copyright (c) 2021 Claudio Jeker <claudio@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

const char *
pretty_key_id(char *hex)
{
	static char buf[128];	/* bigger than SHA_DIGEST_LENGTH * 3 */
	size_t i;

	for (i = 0; i < sizeof(buf) && *hex != '\0'; i++) {
		if  (i % 3 == 2 && *hex != '\0')
			buf[i] = ':';
		else
			buf[i] = *hex++;
	}
	if (i == sizeof(buf))
		memcpy(buf + sizeof(buf) - 4, "...", 4);
	return buf;
}
