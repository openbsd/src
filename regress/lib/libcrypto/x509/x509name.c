/* $OpenBSD: x509name.c,v 1.3 2021/10/31 08:27:15 tb Exp $ */
/*
 * Copyright (c) 2018 Ingo Schwarze <schwarze@openbsd.org>
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

#include <err.h>
#include <stdio.h>

#include <openssl/x509.h>

static void	 debug_print(X509_NAME *);

static void
debug_print(X509_NAME *name)
{
	int loc;

	for (loc = 0; loc < X509_NAME_entry_count(name); loc++)
		printf("%d:",
		    X509_NAME_ENTRY_set(X509_NAME_get_entry(name, loc)));
	putchar(' ');
	X509_NAME_print_ex_fp(stdout, name, 0, XN_FLAG_SEP_CPLUS_SPC);
	putchar('\n');
}

int
main(void)
{
	X509_NAME *name;

	if ((name = X509_NAME_new()) == NULL)
		err(1, NULL);
	X509_NAME_add_entry_by_txt(name, "ST", MBSTRING_ASC,
	    "BaWue", -1, -1, 0);
	X509_NAME_add_entry_by_txt(name, "O", MBSTRING_ASC,
	    "KIT", -1, -1, 0);
	debug_print(name);

	X509_NAME_add_entry_by_txt(name, "L", MBSTRING_ASC,
	    "Karlsruhe", -1, 1, 0);
	debug_print(name);

	X509_NAME_add_entry_by_txt(name, "C", MBSTRING_ASC,
	    "DE", -1, 0, 1);
	debug_print(name);

	X509_NAME_free(name);

	return 0;
}
