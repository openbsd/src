/*
 * Copyright (C) 1999-2001  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
 * INTERNET SOFTWARE CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
 * FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* $ISC: t_api.h,v 1.14 2001/08/08 22:54:55 gson Exp $ */

#ifndef TESTS_T_API_H
#define TESTS_T_API_H 1

#include <stdio.h>

#include <isc/lang.h>
#include <isc/result.h>
#include <isc/formatcheck.h>

/*
 *
 * Result codes.
 *
 */

#define	T_PASS		0x1
#define	T_FAIL		0x2
#define	T_UNRESOLVED	0x3
#define	T_UNSUPPORTED	0x4
#define	T_UNTESTED	0x5

/*
 *
 * Assertion class codes.
 *
 */

#define	T_OPTIONAL	0x0
#define	T_REQUIRED	0x1

/*
 * Misc
 */

#define	T_MAXTOKS	16
#define	T_ARG(n)	(*(av + (n)))

typedef	void (*PFV)(void);

typedef struct {
	PFV		pfv;
	const char	*func_name;
} testspec_t;

extern	int	T_debug;
extern	testspec_t T_testlist[];

ISC_LANG_BEGINDECLS

void
t_assert(const char *component, int anum, int class, const char *what, ...)
     ISC_FORMAT_PRINTF(4, 5);

void
t_info(const char *format, ...) ISC_FORMAT_PRINTF(1, 2);

void
t_result(int result);

char *
t_getenv(const char *name);

char *
t_fgetbs(FILE *fp);

isc_result_t
t_dns_result_fromtext(char *result);

unsigned int
t_dc_method_fromtext(char *dc_method);

int
t_bustline(char *line, char **toks);

int
t_eval(const char *filename, int (*func)(char **), int nargs);

ISC_LANG_ENDDECLS

#endif /* TESTS_T_API_H */

