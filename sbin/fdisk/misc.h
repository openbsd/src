/*	$OpenBSD: misc.h,v 1.35 2021/06/10 15:21:19 krw Exp $	*/

/*
 * Copyright (c) 1997 Tobias Weingartner
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

#ifndef _MISC_H
#define _MISC_H

struct unit_type {
	char	*abbr;
	int64_t	 conversion;
	char	*lname;
};
extern struct unit_type unit_types[];
#define SECTORS	1	/* units are bytes/sectors/kbytes/mbytes/gbytes */

/* Prototypes */
int unit_lookup(char *);
int string_from_line(char *, size_t);
void ask_cmd(char **, char **);
int ask_num(const char *, int, int, int);
int ask_pid(int, struct uuid *);
char *ask_string(const char *, const char *);
int ask_yn(const char *);
uint64_t getuint64(char *, uint64_t, uint64_t, uint64_t);
uint32_t crc32(const u_char *, const uint32_t);
char *utf16le_to_string(const uint16_t *);
uint16_t *string_to_utf16le(const char *);

#endif /* _MISC_H */
