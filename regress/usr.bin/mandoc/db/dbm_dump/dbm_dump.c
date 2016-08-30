/*	$OpenBSD: dbm_dump.c,v 1.2 2016/08/30 22:20:03 schwarze Exp $ */
/*
 * Copyright (c) 2016 Ingo Schwarze <schwarze@openbsd.org>
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
 *
 * Function to dump an on-disk read-only mandoc database
 * in diff(1)able format for debugging purposes.
 */
#include <err.h>
#include <regex.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "mansearch.h"
#include "dbm_map.h"
#include "dbm.h"

union ptr {
	const char	*c;
	const int32_t	*i;
};

static void		 dump(void);
static const char	*dump_macro(union ptr, int32_t);
static const char	*dump_macros(union ptr);
static const char	*dump_pages(union ptr);
static void		 dump_str(const char **);
static void		 dump_lst(const char **);
static void		 pchk(const char *, const char **, const char *, int);


int
main(int argc, char *argv[])
{
	if (argc != 2)
		errx(1, "usage: dump filename");
	if (dbm_open(argv[1]) == -1)
		err(1, "%s", argv[1]);
	dump();
	dbm_close();
	return 0;
}

static void
dump(void)
{
	union ptr	 p, macros, end;

	p.i = dbm_getint(0);
	printf("initial magic 0x%08x\n", be32toh(*p.i++));
	printf("version       0x%08x\n", be32toh(*p.i++));
	printf("macros offset 0x%08x\n", be32toh(*p.i));
	macros.i = dbm_get(*p.i++);
	printf("end offset    0x%08x\n", be32toh(*p.i));
	end.i = dbm_get(*p.i++);
	p.c = dump_pages(p);
	pchk(macros.c, &p.c, "macros", 3);
	p.c = dump_macros(p);
	pchk(end.c, &p.c, "end", 0);
	printf("final magic   0x%08x\n", be32toh(*p.i));
}

static const char *
dump_pages(union ptr p)
{
	const char	*name0, *sect0, *arch0, *desc0, *file0;
	const char	*namep, *sectp, *archp, *descp, *filep;
	int32_t		 i, npages;

	npages = be32toh(*p.i++);
	printf("page count    %d\n", npages);
	if (npages == 0)
		return p.c;
	namep = name0 = dbm_get(p.i[0]);
	sectp = sect0 = dbm_get(p.i[1]);
	archp = arch0 = p.i[2] == 0 ? NULL : dbm_get(p.i[2]);
	descp = desc0 = dbm_get(p.i[3]);
	filep = file0 = dbm_get(p.i[4]);
	printf("=== PAGES ===\n");
	for (i = 0; i < npages; i++) {
		pchk(dbm_get(*p.i++), &namep, "name", 0);
		printf("page name ");
		dump_lst(&namep);
		pchk(dbm_get(*p.i++), &sectp, "sect", 0);
		printf("page sect ");
		dump_lst(&sectp);
		if (*p.i++) {
			if (arch0 == NULL)
				archp = arch0 = dbm_get(p.i[-1]);
			else
				pchk(dbm_get(p.i[-1]), &archp, "arch", 0);
			printf("page arch ");
			dump_lst(&archp);
		}
		pchk(dbm_get(*p.i++), &descp, "desc", 0);
		printf("page desc # ");
		dump_str(&descp);
		printf("\npage file ");
		pchk(dbm_get(*p.i++), &filep, "file", 0);
		if (filep == NULL) {
			printf("# (NULL)\n");
			continue;
		}
		switch(*filep++) {
		case 1:
			printf("src ");
			break;
		case 2:
			printf("cat ");
			break;
		default:
			printf("UNKNOWN FORMAT %d ", filep[-1]);
			break;
		}
		dump_lst(&filep);
	}
	printf("=== END OF PAGES ===\n");
	pchk(name0, &p.c, "name0", 0);
	pchk(sect0, &namep, "sect0", 0);
	if (arch0 != NULL) {
		pchk(arch0, &sectp, "arch0", 0);
		pchk(desc0, &archp, "desc0", 0);
	} else
		pchk(desc0, &sectp, "desc0", 0);
	pchk(file0, &descp, "file0", 0);
	return filep;
}

static const char *
dump_macros(union ptr p)
{
	union ptr	 macro0, macrop;
	int32_t		 i, nmacros;

	nmacros = be32toh(*p.i++);
	printf("macros count  %d\n", nmacros);
	if (nmacros == 0)
		return p.c;
	macrop.i = macro0.i = dbm_get(*p.i);
	printf("=== MACROS ===\n");
	for (i = 0; i < nmacros; i++) {
		pchk(dbm_get(*p.i++), &macrop.c, "macro", 0);
		macrop.c = dump_macro(macrop, i);
	}
	printf("=== END OF MACROS ===\n");
	pchk(macro0.c, &p.c, "macro0", 0);
	return macrop.c;
}

static const char *
dump_macro(union ptr p, int32_t im)
{
	union ptr	 page0, pagep;
	const char	*val0, *valp;
	int32_t		 i, nentries;

	nentries = be32toh(*p.i++);
	printf("macro %02d entry count %d\n", im, nentries);
	if (nentries == 0)
		return p.c;
	valp = val0 = dbm_get(p.i[0]);
	pagep.i = page0.i = dbm_get(p.i[1]);
	printf("=== MACRO %02d ===\n", im);
	for (i = 0; i < nentries; i++) {
		pchk(dbm_get(*p.i++), &valp, "value", 0);
		printf("macro %02d # ", im);
		dump_str(&valp);
		pchk(dbm_get(*p.i++), &pagep.c, "pages", 0);
		while (*pagep.i++ != 0)
			printf("# %s ", (char *)dbm_get(
			    *(int32_t *)dbm_get(pagep.i[-1])) + 1);
		printf("\n");
	}
	printf("=== END OF MACRO %02d ===\n", im);
	pchk(val0, &p.c, "value0", 0);
	pchk(page0.c, &valp, "page0", 3);
	return pagep.c;
}

static void
dump_str(const char **cp)
{
	if (*cp == NULL) {
		printf("(NULL)");
		return;
	}
	if (**cp <= (char)NAME_MASK) {
		putchar('[');
		if (**cp & NAME_FILE)
			putchar('f');
		if (**cp & NAME_HEAD)
			putchar('h');
		if (**cp & NAME_FIRST)
			putchar('1');
		if (**cp & NAME_TITLE)
			putchar('t');
		if (**cp & NAME_SYN)
			putchar('s');
		putchar(']');
		(*cp)++;
	}
	while (**cp != '\0')
		putchar(*(*cp)++);
	putchar(' ');
	(*cp)++;
}

static void
dump_lst(const char **cp)
{
	if (*cp == NULL) {
		printf("# (NULL)\n");
		return;
	}
	while (**cp != '\0') {
		printf("# ");
		dump_str(cp);
	}
	(*cp)++;
	printf("\n");
}

static void
pchk(const char *want, const char **got, const char *name, int fuzz)
{
	if (want == NULL) {
		warnx("%s wants (NULL), ignoring", name);
		return;
	}
	if (*got == NULL)
		warnx("%s jumps from (NULL) to 0x%x", name,
		    be32toh(dbm_addr(want)));
	else if (*got > want || *got + fuzz < want)
		warnx("%s jumps from 0x%x to 0x%x", name,
		    be32toh(dbm_addr(*got)), be32toh(dbm_addr(want)));
	*got = want;
}
