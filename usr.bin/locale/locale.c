/*	$OpenBSD: locale.c,v 1.8 2013/11/15 22:20:04 millert Exp $	*/
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
	"Pig.ISO8859-1",
	"Pig.UTF-8",
	"ar_SD.UTF-8",
	"ar_SY.UTF-8",
	"bg_BG.CP1251",
	"ca_ES.ISO8859-1",
	"ca_ES.ISO8859-15",
	"ca_ES.UTF-8",
	"cs_CZ.ISO8859-2",
	"cs_CZ.UTF-8",
	"da_DK.ISO8859-1",
	"da_DK.ISO8859-15",
	"da_DK.UTF-8",
	"de_AT.ISO8859-1",
	"de_AT.ISO8859-15",
	"de_AT.UTF-8",
	"de_CH.ISO8859-1",
	"de_CH.ISO8859-15",
	"de_CH.UTF-8",
	"de_DE.ISO8859-1",
	"de_DE.ISO8859-15",
	"de_DE.UTF-8",
	"el_GR.ISO8859-7",
	"el_GR.UTF-8",
	"en_AU.ISO8859-1",
	"en_AU.ISO8859-15",
	"en_AU.UTF-8",
	"en_CA.ISO8859-1",
	"en_CA.ISO8859-15",
	"en_CA.UTF-8",
	"en_GB.ISO8859-1",
	"en_GB.ISO8859-15",
	"en_GB.UTF-8",
	"en_US.ISO8859-1",
	"en_US.ISO8859-15",
	"en_US.UTF-8",
	"es_AR.ISO8859-1",
	"es_AR.ISO8859-15",
	"es_AR.UTF-8",
	"es_BO.ISO8859-1",
	"es_BO.ISO8859-15",
	"es_BO.UTF-8",
	"es_CH.ISO8859-1",
	"es_CH.ISO8859-15",
	"es_CH.UTF-8",
	"es_CO.ISO8859-1",
	"es_CO.ISO8859-15",
	"es_CO.UTF-8",
	"es_CR.ISO8859-1",
	"es_CR.ISO8859-15",
	"es_CR.UTF-8",
	"es_CU.ISO8859-1",
	"es_CU.ISO8859-15",
	"es_CU.UTF-8",
	"es_DO.ISO8859-1",
	"es_DO.ISO8859-15",
	"es_DO.UTF-8",
	"es_EC.ISO8859-1",
	"es_EC.ISO8859-15",
	"es_EC.UTF-8",
	"es_ES.ISO8859-1",
	"es_ES.ISO8859-15",
	"es_ES.UTF-8",
	"es_GQ.ISO8859-1",
	"es_GQ.ISO8859-15",
	"es_GQ.UTF-8",
	"es_GT.ISO8859-1",
	"es_GT.ISO8859-15",
	"es_GT.UTF-8",
	"es_HN.ISO8859-1",
	"es_HN.ISO8859-15",
	"es_HN.UTF-8",
	"es_MX.ISO8859-1",
	"es_MX.ISO8859-15",
	"es_MX.UTF-8",
	"es_NI.ISO8859-1",
	"es_NI.ISO8859-15",
	"es_NI.UTF-8",
	"es_PA.ISO8859-1",
	"es_PA.ISO8859-15",
	"es_PA.UTF-8",
	"es_PE.ISO8859-1",
	"es_PE.ISO8859-15",
	"es_PE.UTF-8",
	"es_PR.ISO8859-1",
	"es_PR.ISO8859-15",
	"es_PR.UTF-8",
	"es_PY.ISO8859-1",
	"es_PY.ISO8859-15",
	"es_PY.UTF-8",
	"es_SV.ISO8859-1",
	"es_SV.ISO8859-15",
	"es_SV.UTF-8",
	"es_US.ISO8859-1",
	"es_US.ISO8859-15",
	"es_US.UTF-8",
	"es_UY.ISO8859-1",
	"es_UY.ISO8859-15",
	"es_UY.UTF-8",
	"es_VE.ISO8859-1",
	"es_VE.ISO8859-15",
	"es_VE.UTF-8",
	"fa_IR.UTF-8",
	"fi_FI.ISO8859-1",
	"fi_FI.ISO8859-15",
	"fi_FI.UTF-8",
	"fr_BE.ISO8859-1",
	"fr_BE.ISO8859-15",
	"fr_BE.UTF-8",
	"fr_CA.ISO8859-1",
	"fr_CA.ISO8859-15",
	"fr_CA.UTF-8",
	"fr_CH.ISO8859-1",
	"fr_CH.ISO8859-15",
	"fr_CH.UTF-8",
	"fr_FR.ISO8859-1",
	"fr_FR.ISO8859-15",
	"fr_FR.UTF-8",
	"hr_HR.ISO8859-2",
	"hu_HU.ISO8859-2",
	"hu_HU.UTF-8",
	"hy_AM.ARMSCII-8",
	"hy_AM.UTF-8",
	"is_IS.ISO8859-1",
	"is_IS.ISO8859-15",
	"is_IS.UTF-8",
	"it_CH.ISO8859-1",
	"it_CH.ISO8859-15",
	"it_CH.UTF-8",
	"it_IT.ISO8859-1",
	"it_IT.ISO8859-15",
	"it_IT.UTF-8",
	"ja_JP.UTF-8",
	"ko_KR.UTF-8",
	"lt_LT.ISO8859-13",
	"lt_LT.ISO8859-4",
	"lt_LT.UTF-8",
	"nl_BE.ISO8859-1",
	"nl_BE.ISO8859-15",
	"nl_BE.UTF-8",
	"nl_NL.ISO8859-1",
	"nl_NL.ISO8859-15",
	"nl_NL.UTF-8",
	"no_NO.ISO8859-1",
	"no_NO.ISO8859-15",
	"no_NO.UTF-8",
	"pl_PL.ISO8859-2",
	"pl_PL.UTF-8",
	"pt_PT.ISO8859-1",
	"pt_PT.ISO8859-15",
	"pt_PT.UTF-8",
	"ro_RO.UTF-8",
	"ru_RU.CP866",
	"ru_RU.ISO8859-5",
	"ru_RU.KOI8-R",
	"ru_RU.UTF-8",
	"sk_SK.ISO8859-2",
	"sk_SK.UTF-8",
	"sl_SI.ISO8859-2",
	"sl_SI.UTF-8",
	"sv_SE.ISO8859-1",
	"sv_SE.ISO8859-15",
	"sv_SE.UTF-8",
	"tr_TR.ISO8859-9",
	"tr_TR.UTF-8",
	"uk_UA.KOI8-U",
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
show_charsets(void)
{
	char *charset;
	char charsets[sizeof(LOCALE_CHARSETS)];
	char *s = charsets;

	bcopy(LOCALE_CHARSETS, charsets, sizeof(charsets));
	do {
		charset = strsep(&s, " \t");
		if (charset && charset[0])
			printf("%s\n", charset);
	} while (charset);
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
		show_charsets();

	return 0;
}
