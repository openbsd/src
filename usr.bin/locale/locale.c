/*	$OpenBSD: locale.c,v 1.9 2015/08/14 14:31:49 stsp Exp $	*/
/*
 * Copyright (c) 2013 Stefan Sperling <stsp@openbsd.org>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <unistd.h>

extern char		*__progname;

struct category_name {
	int category;
	const char *name;
} categories[] = {
	{ LC_COLLATE,	"LC_COLLATE" },
	{ LC_CTYPE,	"LC_CTYPE" },
	{ LC_MONETARY,	"LC_MONETARY" },
	{ LC_NUMERIC,	"LC_NUMERIC" },
	{ LC_TIME,	"LC_TIME" },
	{ LC_MESSAGES,	"LC_MESSAGES" },
	{ 0, 		NULL},
};

static void
put_assignment(const char *name, const char *value, int double_quoted)
{
	char c;

	fputs(name, stdout);
	putchar('=');
	if (double_quoted)
		putchar('"');
	if (value != NULL)
		while ((c = *value++) != '\0')
			switch (c) {
			case ' ': case '\t': case '\n': case '\'':
			case '(': case ')': case '<': case '>':
			case '&': case ';': case '|': case '~':
				if (!double_quoted)
			case '"': case '\\': case '$': case '`': 
					putchar('\\');
			default:
				putchar(c);
				break;
			}
	if (double_quoted)
		putchar('"');
	putchar('\n');
}

static void
show_current_locale(void)
{
	char *lang, *lc_all;
	int i;

	lang = getenv("LANG");
	lc_all = getenv("LC_ALL");

	put_assignment("LANG", lang, 0);
	for (i = 0; categories[i].name != NULL; i++) {
		if (lc_all == NULL && getenv(categories[i].name))
			put_assignment(categories[i].name,
			    getenv(categories[i].name), 0);
		else
			put_assignment(categories[i].name,
			    setlocale(categories[i].category, NULL), 1);
	}
	put_assignment("LC_ALL", lc_all, 0);
}

const char * const some_locales[] = {
	"C",
	"C.UTF-8",
	"POSIX",
	"POSIX.UTF-8",
	"Pig.UTF-8",
	"ar_SD.UTF-8",
	"ar_SY.UTF-8",
	"ca_ES.UTF-8",
	"cs_CZ.UTF-8",
	"da_DK.UTF-8",
	"de_AT.UTF-8",
	"de_CH.UTF-8",
	"de_DE.UTF-8",
	"el_GR.UTF-8",
	"en_AU.UTF-8",
	"en_CA.UTF-8",
	"en_GB.UTF-8",
	"en_US.UTF-8",
	"es_AR.UTF-8",
	"es_BO.UTF-8",
	"es_CH.UTF-8",
	"es_CO.UTF-8",
	"es_CR.UTF-8",
	"es_CU.UTF-8",
	"es_DO.UTF-8",
	"es_EC.UTF-8",
	"es_ES.UTF-8",
	"es_GQ.UTF-8",
	"es_GT.UTF-8",
	"es_HN.UTF-8",
	"es_MX.UTF-8",
	"es_NI.UTF-8",
	"es_PA.UTF-8",
	"es_PE.UTF-8",
	"es_PR.UTF-8",
	"es_PY.UTF-8",
	"es_SV.UTF-8",
	"es_US.UTF-8",
	"es_UY.UTF-8",
	"es_VE.UTF-8",
	"fa_IR.UTF-8",
	"fi_FI.UTF-8",
	"fr_BE.UTF-8",
	"fr_CA.UTF-8",
	"fr_CH.UTF-8",
	"fr_FR.UTF-8",
	"hu_HU.UTF-8",
	"hy_AM.UTF-8",
	"is_IS.UTF-8",
	"it_CH.UTF-8",
	"it_IT.UTF-8",
	"ja_JP.UTF-8",
	"ko_KR.UTF-8",
	"lt_LT.UTF-8",
	"nl_BE.UTF-8",
	"nl_NL.UTF-8",
	"no_NO.UTF-8",
	"pl_PL.UTF-8",
	"pt_PT.UTF-8",
	"ro_RO.UTF-8",
	"ru_RU.UTF-8",
	"sk_SK.UTF-8",
	"sl_SI.UTF-8",
	"sv_SE.UTF-8",
	"tr_TR.UTF-8",
	"uk_UA.UTF-8",
	"zh_CN.UTF-8",
	"zh_TW.UTF-8",
	NULL
};

static void
show_locales(void)
{
	int i = 0;

	while (some_locales[i])
		puts(some_locales[i++]);
}

static void
usage(void)
{
	fprintf(stderr, "usage: %s [-a | -m]\n", __progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	int opt, aflag = 0, mflag = 0;

	setlocale(LC_ALL, "");

	if (argc == 1) {
		show_current_locale();
		return 0;
	}

	while ((opt = getopt(argc, argv, "am")) != -1) {
		switch (opt) {
		case 'a':
			aflag = 1;
			break;
		case 'm':
			mflag = 1;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 0 || (aflag && mflag))
		usage();
	else if (aflag)
		show_locales();
	else if (mflag)
		printf("UTF-8\n");

	return 0;
}
