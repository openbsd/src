/*
 * Copyright (C) 1998-2001  Internet Software Consortium.
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

/* $ISC: sym_test.c,v 1.22 2001/01/09 21:41:42 bwelling Exp $ */

#include <config.h>

#include <string.h>

#include <isc/commandline.h>
#include <isc/mem.h>
#include <isc/symtab.h>
#include <isc/util.h>

isc_mem_t *mctx;
isc_symtab_t *st;

static void
undefine_action(char *key, unsigned int type, isc_symvalue_t value, void *arg)
{
	UNUSED(arg);

	INSIST(type == 1);
	isc_mem_free(mctx, key);
	isc_mem_free(mctx, value.as_pointer);
}

int
main(int argc, char *argv[]) {
	char s[1000], *cp, *key;
	size_t len;
	isc_result_t result;
	isc_symvalue_t value;
	int trace = 0;
	int c;
	isc_symexists_t exists_policy = isc_symexists_reject;
	isc_boolean_t case_sensitive = ISC_FALSE;

	while ((c = isc_commandline_parse(argc, argv, "tarc")) != -1) {
		switch (c) {
		case 't':
			trace = 1;
			break;
		case 'a':
			exists_policy = isc_symexists_add;
			break;
		case 'r':
			exists_policy = isc_symexists_replace;
			break;
		case 'c':
			case_sensitive = ISC_TRUE;
			break;
		}
	}

	RUNTIME_CHECK(isc_mem_create(0, 0, &mctx) == ISC_R_SUCCESS);
	RUNTIME_CHECK(isc_symtab_create(mctx, 691, undefine_action, NULL,
					case_sensitive, &st) == ISC_R_SUCCESS);

	while (fgets(s, sizeof s, stdin) != NULL) {
		len = strlen(s);
		if (len > 0 && s[len - 1] == '\n') {
			s[len - 1] = '\0';
			len--;
		}

		cp = s;

		if (cp[0] == '!') {
			cp++;
			result = isc_symtab_undefine(st, cp, 1);
			if (trace || result != ISC_R_SUCCESS)
				printf("undefine('%s'): %s\n", cp,
				       isc_result_totext(result));
		} else {
			key = cp;
			while (*cp != '\0' && *cp != ' ' && *cp != '\t')
				cp++;
			if (*cp == '\0') {
				result = isc_symtab_lookup(st, key, 0, &value);
				if (trace || result != ISC_R_SUCCESS) {
					printf("lookup('%s'): %s", key,
					       isc_result_totext(result));
					if (result == ISC_R_SUCCESS) {
						cp = value.as_pointer;
						printf(", value == '%s'", cp);
					}
					printf("\n");
				}
			} else {
				*cp++ = '\0';
				key = isc_mem_strdup(mctx, key);
				value.as_pointer = isc_mem_strdup(mctx, cp);
				result = isc_symtab_define(st, key, 1, value,
							   exists_policy);
				if (trace || result != ISC_R_SUCCESS) {
					printf("define('%s', '%s'): %s\n",
					       key, cp,
					       isc_result_totext(result));
					if (result != ISC_R_SUCCESS)
						undefine_action(key, 1,
							value, NULL);
				}
			}
		}
	}

	isc_symtab_destroy(&st);
	isc_mem_stats(mctx, stdout);
	isc_mem_destroy(&mctx);

	return (0);
}
