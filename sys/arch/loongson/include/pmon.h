/*	$OpenBSD: pmon.h,v 1.1.1.1 2009/08/04 20:29:35 miod Exp $	*/

/*
 * Copyright (c) 2009 Miodrag Vallat.
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

#ifndef	_MACHINE_PMON_H_
#define	_MACHINE_PMON_H_

#if defined(_KERNEL) || defined(STANDALONE)

/*
 * PMON2000 callvec definitions
 */

/* 32-bit compatible types */
typedef	uint32_t	pmon_size_t;
typedef	int32_t		pmon_ssize_t;
typedef int64_t		pmon_off_t;

int		pmon_open(const char *, int, ...);
int		pmon_close(int);
int		pmon_read(int, void *, pmon_size_t);
pmon_ssize_t	pmon_write(int, const void *, pmon_size_t);
pmon_off_t	pmon_lseek(int, quad_t /* off_t */, int);
int		pmon_printf(const char *, ...);
void		pmon_cacheflush(void);
char *		pmon_gets(char *);

#define	PMON_MAXLN	256	/* internal gets() size limit */

extern int32_t pmon_callvec;

const char	*pmon_getarg(const int);
const char	*pmon_getenv(const char *);
void		 pmon_init(int32_t, int32_t, int32_t, int32_t);

#endif	/* _KERNEL || STANDALONE */

#endif	/* _MACHINE_PMON_H_ */
