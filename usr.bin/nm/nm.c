/*	$OpenBSD: nm.c,v 1.45 2015/05/17 20:19:08 guenther Exp $	*/
/*	$NetBSD: nm.c,v 1.7 1996/01/14 23:04:03 pk Exp $	*/

/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Hans Huebner.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/mman.h>
#include <a.out.h>
#include <elf_abi.h>
#include <ar.h>
#include <ranlib.h>
#include <unistd.h>
#include <err.h>
#include <errno.h>
#include <ctype.h>
#include <link.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include "elfuncs.h"
#include "util.h"

#define	SYMTABMAG	"/ "
#define	STRTABMAG	"//"
#define	SYM64MAG	"/SYM64/         "

union hdr {
	Elf32_Ehdr elf32;
	Elf64_Ehdr elf64;
};

int armap;
int demangle;
int non_object_warning;
int print_only_external_symbols;
int print_only_undefined_symbols;
int print_all_symbols;
int print_file_each_line;
int show_extensions;
int issize;
int usemmap = 1;
int dynamic_only;

/* size vars */
unsigned long total_text, total_data, total_bss, total_total;
int non_object_warning, print_totals;

int rev;
int fname(const void *, const void *);
int rname(const void *, const void *);
int value(const void *, const void *);
char *otherstring(struct nlist *);
int (*sfunc)(const void *, const void *) = fname;
char typeletter(struct nlist *);
int mmbr_name(struct ar_hdr *, char **, int, int *, FILE *);
int show_symtab(off_t, u_long, const char *, FILE *);
int show_symdef(off_t, u_long, const char *, FILE *);

/* some macros for symbol type (nlist.n_type) handling */
#define	IS_EXTERNAL(x)		((x) & N_EXT)
#define	SYMBOL_TYPE(x)		((x) & (N_TYPE | N_STAB))

void	 pipe2cppfilt(void);
void	 usage(void);
char	*symname(struct nlist *);
int	process_file(int, const char *);
int	show_archive(int, const char *, FILE *);
int	show_file(int, int, const char *, FILE *fp, off_t, union hdr *);
void	print_symbol(const char *, struct nlist *);

#define	OPTSTRING_NM	"aABCDegnoprsuvw"
const struct option longopts_nm[] = {
	{ "debug-syms",		no_argument,		0,	'a' },
	{ "demangle",		no_argument,		0,	'C' },
	{ "dynamic",		no_argument,		0,	'D' },
	{ "extern-only",	no_argument,		0,	'g' },
/*	{ "line-numbers",	no_argument,		0,	'l' }, */
	{ "no-sort",		no_argument,		0,	'p' },
	{ "numeric-sort",	no_argument,		0,	'n' },
	{ "print-armap",	no_argument,		0,	's' },
	{ "print-file-name",	no_argument,		0,	'o' },
	{ "reverse-sort",	no_argument,		0,	'r' },
/*	{ "size-sort",		no_argument,		&szval,	1 }, */
	{ "undefined-only",	no_argument,		0,	'u' },
	{ "help",		no_argument,		0,	'?' },
	{ NULL }
};

/*
 * main()
 *	parse command line, execute process_file() for each file
 *	specified on the command line.
 */
int
main(int argc, char *argv[])
{
	extern char *__progname;
	extern int optind;
	const char *optstr;
	const struct option *lopts;
	int ch, eval;

	optstr = OPTSTRING_NM;
	lopts = longopts_nm;
	if (!strcmp(__progname, "size")) {
		issize++;
		optstr = "tw";
		lopts = NULL;
	}

	while ((ch = getopt_long(argc, argv, optstr, lopts, NULL)) != -1) {
		switch (ch) {
		case 'a':
			print_all_symbols = 1;
			break;
		case 'B':
			/* no-op, compat with gnu-nm */
			break;
		case 'C':
			demangle = 1;
			break;
		case 'D':
			dynamic_only = 1;
			break;
		case 'e':
			show_extensions = 1;
			break;
		case 'g':
			print_only_external_symbols = 1;
			break;
		case 'n':
		case 'v':
			sfunc = value;
			break;
		case 'A':
		case 'o':
			print_file_each_line = 1;
			break;
		case 'p':
			sfunc = NULL;
			break;
		case 'r':
			rev = 1;
			break;
		case 's':
			armap = 1;
			break;
		case 'u':
			print_only_undefined_symbols = 1;
			break;
		case 'w':
			non_object_warning = 1;
			break;
		case 't':
			if (issize) {
				print_totals = 1;
				break;
			}
		case '?':
		default:
			usage();
		}
	}

	if (demangle)
		pipe2cppfilt();
	argv += optind;
	argc -= optind;

	if (rev && sfunc == fname)
		sfunc = rname;

	eval = 0;
	if (*argv)
		do {
			eval |= process_file(argc, *argv);
		} while (*++argv);
	else
		eval |= process_file(1, "a.out");

	if (issize && print_totals)
		printf("\n%lu\t%lu\t%lu\t%lu\t%lx\tTOTAL\n",
		    total_text, total_data, total_bss,
		    total_total, total_total);
	exit(eval);
}

/*
 * process_file()
 *	show symbols in the file given as an argument.  Accepts archive and
 *	object files as input.
 */
int
process_file(int count, const char *fname)
{
	union hdr exec_head;
	FILE *fp;
	int retval;
	size_t bytes;
	char magic[SARMAG];

	if (!(fp = fopen(fname, "r"))) {
		warn("cannot read %s", fname);
		return(1);
	}

	if (!issize && count > 1)
		(void)printf("\n%s:\n", fname);

	/*
	 * first check whether this is an object file - read a object
	 * header, and skip back to the beginning
	 */
	bzero(&exec_head, sizeof(exec_head));
	bytes = fread((char *)&exec_head, 1, sizeof(exec_head), fp);
	if (bytes < sizeof(exec_head)) {
		if (bytes < sizeof(exec_head.elf32) || IS_ELF(exec_head.elf32)) {
			warnx("%s: bad format", fname);
			(void)fclose(fp);
			return(1);
		}
	}
	rewind(fp);

	/* this could be an archive */
	if (!IS_ELF(exec_head.elf32)) {
		if (fread(magic, sizeof(magic), (size_t)1, fp) != 1 ||
		    strncmp(magic, ARMAG, SARMAG)) {
			warnx("%s: not object file or archive", fname);
			(void)fclose(fp);
			return(1);
		}
		retval = show_archive(count, fname, fp);
	} else
		retval = show_file(count, 1, fname, fp, 0, &exec_head);
	(void)fclose(fp);
	return(retval);
}

char *nametab;

/*
 *
 *	given the archive member header -- produce member name
 */
int
mmbr_name(struct ar_hdr *arh, char **name, int baselen, int *namelen, FILE *fp)
{
	char *p = *name + strlen(*name);
	long i;

	if (nametab && arh->ar_name[0] == '/') {
		int len;

		i = atol(&arh->ar_name[1]);
		len = strlen(&nametab[i]);
		if (len > *namelen) {
			p -= (long)*name;
			if ((*name = realloc(*name, baselen+len)) == NULL)
				err(1, NULL);
			*namelen = len;
			p += (long)*name;
		}
		strlcpy(p, &nametab[i], len);
		p += len;
	} else
#ifdef AR_EFMT1
	/*
	 * BSD 4.4 extended AR format: #1/<namelen>, with name as the
	 * first <namelen> bytes of the file
	 */
	if ((arh->ar_name[0] == '#') &&
	    (arh->ar_name[1] == '1') &&
	    (arh->ar_name[2] == '/') &&
	    (isdigit((unsigned char)arh->ar_name[3]))) {
		int len = atoi(&arh->ar_name[3]);

		if (len > *namelen) {
			p -= (long)*name;
			if ((*name = realloc(*name, baselen+len)) == NULL)
				err(1, NULL);
			*namelen = len;
			p += (long)*name;
		}
		if (fread(p, len, 1, fp) != 1) {
			warnx("%s: premature EOF", *name);
			free(*name);
			return(1);
		}
		p += len;
	} else
#endif
	for (i = 0; i < sizeof(arh->ar_name); ++i)
		if (arh->ar_name[i] && arh->ar_name[i] != ' ')
			*p++ = arh->ar_name[i];
	*p = '\0';
	if (p[-1] == '/')
		*--p = '\0';

	return (0);
}

/*
 * show_symtab()
 *	show archive ranlib index (fs5)
 */
int
show_symtab(off_t off, u_long len, const char *name, FILE *fp)
{
	struct ar_hdr ar_head;
	int *symtab, *ps;
	char *strtab, *p;
	int num, rval = 0;
	int namelen;
	off_t restore;

	restore = ftello(fp);

	MMAP(symtab, len, PROT_READ, MAP_PRIVATE|MAP_FILE, fileno(fp), off);
	if (symtab == MAP_FAILED)
		return (1);

	namelen = sizeof(ar_head.ar_name);
	if ((p = malloc(sizeof(ar_head.ar_name))) == NULL) {
		warn("%s: malloc", name);
		MUNMAP(symtab, len);
	}

	printf("\nArchive index:\n");
	num = betoh32(*symtab);
	strtab = (char *)(symtab + num + 1);
	for (ps = symtab + 1; num--; ps++, strtab += strlen(strtab) + 1) {
		if (fseeko(fp, betoh32(*ps), SEEK_SET)) {
			warn("%s: fseeko", name);
			rval = 1;
			break;
		}

		if (fread(&ar_head, sizeof(ar_head), 1, fp) != 1 ||
		    memcmp(ar_head.ar_fmag, ARFMAG, sizeof(ar_head.ar_fmag))) {
			warnx("%s: member fseeko", name);
			rval = 1;
			break;
		}

		*p = '\0';
		if (mmbr_name(&ar_head, &p, 0, &namelen, fp)) {
			rval = 1;
			break;
		}

		printf("%s in %s\n", strtab, p);
	}

	fseeko(fp, restore, SEEK_SET);

	free(p);
	MUNMAP(symtab, len);
	return (rval);
}

/*
 * show_symdef()
 *	show archive ranlib index (gob)
 */
int
show_symdef(off_t off, u_long len, const char *name, FILE *fp)
{
	struct ranlib *prn, *eprn;
	struct ar_hdr ar_head;
	char *symdef;
	char *strtab, *p;
	u_long size;
	int namelen, rval = 0;

	MMAP(symdef, len, PROT_READ, MAP_PRIVATE|MAP_FILE, fileno(fp), off);
	if (symdef == MAP_FAILED)
		return (1);
	if (usemmap)
		(void)madvise(symdef, len, MADV_SEQUENTIAL);

	namelen = sizeof(ar_head.ar_name);
	if ((p = malloc(sizeof(ar_head.ar_name))) == NULL) {
		warn("%s: malloc", name);
		MUNMAP(symdef, len);
		return (1);
	}

	size = *(u_long *)symdef;
	prn = (struct ranlib *)(symdef + sizeof(u_long));
	eprn = prn + size / sizeof(*prn);
	strtab = symdef + sizeof(u_long) + size + sizeof(u_long);

	printf("\nArchive index:\n");
	for (; prn < eprn; prn++) {
		if (fseeko(fp, prn->ran_off, SEEK_SET)) {
			warn("%s: fseeko", name);
			rval = 1;
			break;
		}

		if (fread(&ar_head, sizeof(ar_head), 1, fp) != 1 ||
		    memcmp(ar_head.ar_fmag, ARFMAG, sizeof(ar_head.ar_fmag))) {
			warnx("%s: member fseeko", name);
			rval = 1;
			break;
		}

		*p = '\0';
		if (mmbr_name(&ar_head, &p, 0, &namelen, fp)) {
			rval = 1;
			break;
		}

		printf("%s in %s\n", strtab + prn->ran_un.ran_strx, p);
	}

	free(p);
	MUNMAP(symdef, len);
	return (rval);
}

/*
 * show_archive()
 *	show symbols in the given archive file
 */
int
show_archive(int count, const char *fname, FILE *fp)
{
	struct ar_hdr ar_head;
	union hdr exec_head;
	int i, rval;
	off_t last_ar_off, foff, symtaboff;
	char *name;
	int baselen, namelen;
	u_long mmbrlen, symtablen;

	baselen = strlen(fname) + 3;
	namelen = sizeof(ar_head.ar_name);
	if ((name = malloc(baselen + namelen)) == NULL)
		err(1, NULL);

	rval = 0;
	nametab = NULL;
	symtaboff = 0;
	symtablen = 0;

	/* while there are more entries in the archive */
	while (fread(&ar_head, sizeof(ar_head), 1, fp) == 1) {
		/* bad archive entry - stop processing this archive */
		if (memcmp(ar_head.ar_fmag, ARFMAG, sizeof(ar_head.ar_fmag))) {
			warnx("%s: bad format archive header", fname);
			rval = 1;
			break;
		}

		/* remember start position of current archive object */
		last_ar_off = ftello(fp);
		mmbrlen = atol(ar_head.ar_size);

		if (strncmp(ar_head.ar_name, RANLIBMAG,
		    sizeof(RANLIBMAG) - 1) == 0) {
			if (!issize && armap &&
			    show_symdef(last_ar_off, mmbrlen, fname, fp)) {
				rval = 1;
				break;
			}
			goto skip;
		} else if (strncmp(ar_head.ar_name, SYMTABMAG,
		    sizeof(SYMTABMAG) - 1) == 0) {
			/* if nametab hasn't been seen yet -- doit later */
			if (!nametab) {
				symtablen = mmbrlen;
				symtaboff = last_ar_off;
				goto skip;
			}

			/* load the Sys5 long names table */
		} else if (strncmp(ar_head.ar_name, STRTABMAG,
		    sizeof(STRTABMAG) - 1) == 0) {
			char *p;

			if ((nametab = malloc(mmbrlen)) == NULL) {
				warn("%s: nametab", fname);
				rval = 1;
				break;
			}

			if (fread(nametab, mmbrlen, (size_t)1, fp) != 1) {
				warnx("%s: premature EOF", fname);
				rval = 1;
				break;
			}

			for (p = nametab, i = mmbrlen; i--; p++)
				if (*p == '\n')
					*p = '\0';

			if (issize || !armap || !symtablen || !symtaboff)
				goto skip;
		}
#ifdef __mips64
		else if (memcmp(ar_head.ar_name, SYM64MAG,
		    sizeof(ar_head.ar_name)) == 0) {
			/* IRIX6-compatible archive map */
			goto skip;
		}
#endif

		if (!issize && armap && symtablen && symtaboff) {
			if (show_symtab(symtaboff, symtablen, fname, fp)) {
				rval = 1;
				break;
			} else {
				symtaboff = 0;
				symtablen = 0;
			}
		}

		/*
		 * construct a name of the form "archive.a:obj.o:" for the
		 * current archive entry if the object name is to be printed
		 * on each output line
		 */
		*name = '\0';
		if (count > 1)
			snprintf(name, baselen - 1, "%s:", fname);

		if (mmbr_name(&ar_head, &name, baselen, &namelen, fp)) {
			rval = 1;
			break;
		}

		foff = ftello(fp);

		/* get and check current object's header */
		if (fread((char *)&exec_head, sizeof(exec_head),
		    (size_t)1, fp) != 1) {
			warnx("%s: premature EOF", fname);
			rval = 1;
			break;
		}

		rval |= show_file(2, non_object_warning, name, fp, foff, &exec_head);
		/*
		 * skip to next archive object - it starts at the next
		 * even byte boundary
		 */
#define even(x) (((x) + 1) & ~1)
skip:		if (fseeko(fp, last_ar_off + even(mmbrlen), SEEK_SET)) {
			warn("%s", fname);
			rval = 1;
			break;
		}
	}
	if (nametab) {
		free(nametab);
		nametab = NULL;
	}
	free(name);
	return(rval);
}

char *stab;

/*
 * show_file()
 *	show symbols from the object file pointed to by fp.  The current
 *	file pointer for fp is expected to be at the beginning of an object
 *	file header.
 */
int
show_file(int count, int warn_fmt, const char *name, FILE *fp, off_t foff, union hdr *head)
{
	u_long text, data, bss, total;
	struct nlist *np, *names, **snames;
	int i, nrawnames, nnames;
	size_t stabsize;

	if (IS_ELF(head->elf32) &&
	    head->elf32.e_ident[EI_CLASS] == ELFCLASS32 &&
	    head->elf32.e_ident[EI_VERSION] == ELF_TARG_VER) {
		void *shdr;

		if (!(shdr = elf32_load_shdrs(name, fp, foff, &head->elf32)))
			return (1);

		i = issize?
		    elf32_size(&head->elf32, shdr, &text, &data, &bss) :
		    elf32_symload(name, fp, foff, &head->elf32, shdr,
			&names, &snames, &stabsize, &nrawnames);
		free(shdr);
		if (i)
			return (i);

	} else if (IS_ELF(head->elf64) &&
	    head->elf64.e_ident[EI_CLASS] == ELFCLASS64 &&
	    head->elf64.e_ident[EI_VERSION] == ELF_TARG_VER) {
		void *shdr;

		if (!(shdr = elf64_load_shdrs(name, fp, foff, &head->elf64)))
			return (1);

		i = issize?
		    elf64_size(&head->elf64, shdr, &text, &data, &bss) :
		    elf64_symload(name, fp, foff, &head->elf64, shdr,
			&names, &snames, &stabsize, &nrawnames);
		free(shdr);
		if (i)
			return (i);
	} else {
		if (warn_fmt)
			warnx("%s: bad format", name);
		return (1);
	}

	if (issize) {
		static int first = 1;

		if (first) {
			first = 0;
			printf("text\tdata\tbss\tdec\thex\n");
		}

		total = text + data + bss;
		printf("%lu\t%lu\t%lu\t%lu\t%lx",
		    text, data, bss, total, total);
		if (count > 1)
			(void)printf("\t%s", name);

		total_text += text;
		total_data += data;
		total_bss += bss;
		total_total += total;

		printf("\n");
		return (0);
	}
	/* else we are nm */

	/*
	 * it seems that string table is sequential
	 * relative to the symbol table order
	 */
	if (sfunc == NULL && usemmap)
		(void)madvise(stab, stabsize, MADV_SEQUENTIAL);

	/*
	 * fix up the symbol table and filter out unwanted entries
	 *
	 * common symbols are characterized by a n_type of N_UNDF and a
	 * non-zero n_value -- change n_type to N_COMM for all such
	 * symbols to make life easier later.
	 *
	 * filter out all entries which we don't want to print anyway
	 */
	for (np = names, i = nnames = 0; i < nrawnames; np++, i++) {
		/*
		 * make n_un.n_name a character pointer by adding the string
		 * table's base to n_un.n_strx
		 *
		 * don't mess with zero offsets
		 */
		if (np->n_un.n_strx)
			np->n_un.n_name = stab + np->n_un.n_strx;
		else
			np->n_un.n_name = "";
		if (print_only_external_symbols && !IS_EXTERNAL(np->n_type))
			continue;
		if (print_only_undefined_symbols &&
		    SYMBOL_TYPE(np->n_type) != N_UNDF)
			continue;

		snames[nnames++] = np;
	}

	/* sort the symbol table if applicable */
	if (sfunc)
		qsort(snames, (size_t)nnames, sizeof(*snames), sfunc);

	if (count > 1)
		(void)printf("\n%s:\n", name);

	/* print out symbols */
	for (i = 0; i < nnames; i++)
		print_symbol(name, snames[i]);

	free(snames);
	free(names);
	MUNMAP(stab, stabsize);
	return(0);
}

char *
symname(struct nlist *sym)
{
	return sym->n_un.n_name;
}

/*
 * print_symbol()
 *	show one symbol
 */
void
print_symbol(const char *name, struct nlist *sym)
{
	if (print_file_each_line)
		(void)printf("%s:", name);

	/*
	 * handle undefined-only format especially (no space is
	 * left for symbol values, no type field is printed)
	 */
	if (!print_only_undefined_symbols) {
		/* print symbol's value */
		if (SYMBOL_TYPE(sym->n_type) == N_UNDF)
			(void)printf("        ");
		else
			(void)printf("%08lx", sym->n_value);

		/* print type information */
		if (show_extensions)
			(void)printf(" %c   ", typeletter(sym));
		else
			(void)printf(" %c ", typeletter(sym));
	}

	(void)puts(symname(sym));
}

/*
 * typeletter()
 *	return a description letter for the given basic type code of an
 *	symbol table entry.  The return value will be upper case for
 *	external, lower case for internal symbols.
 */
char
typeletter(struct nlist *np)
{
	int ext = IS_EXTERNAL(np->n_type);

	if (np->n_other)
		return np->n_other;

	switch(SYMBOL_TYPE(np->n_type)) {
	case N_ABS:
		return(ext? 'A' : 'a');
	case N_BSS:
		return(ext? 'B' : 'b');
	case N_COMM:
		return(ext? 'C' : 'c');
	case N_DATA:
		return(ext? 'D' : 'd');
	case N_FN:
		/* NOTE: N_FN == N_WARNING,
		 * in this case, the N_EXT bit is to considered as
		 * part of the symbol's type itself.
		 */
		return(ext? 'F' : 'W');
	case N_TEXT:
		return(ext? 'T' : 't');
	case N_SIZE:
		return(ext? 'S' : 's');
	case N_UNDF:
		return(ext? 'U' : 'u');
	}
	return('?');
}

int
fname(const void *a0, const void *b0)
{
	struct nlist * const *a = a0, * const *b = b0;

	return(strcmp((*a)->n_un.n_name, (*b)->n_un.n_name));
}

int
rname(const void *a0, const void *b0)
{
	struct nlist * const *a = a0, * const *b = b0;

	return(strcmp((*b)->n_un.n_name, (*a)->n_un.n_name));
}

int
value(const void *a0, const void *b0)
{
	struct nlist * const *a = a0, * const *b = b0;

	if (SYMBOL_TYPE((*a)->n_type) == N_UNDF)
		if (SYMBOL_TYPE((*b)->n_type) == N_UNDF)
			return(0);
		else
			return(-1);
	else if (SYMBOL_TYPE((*b)->n_type) == N_UNDF)
		return(1);
	if (rev) {
		if ((*a)->n_value == (*b)->n_value)
			return(rname(a0, b0));
		return((*b)->n_value > (*a)->n_value ? 1 : -1);
	} else {
		if ((*a)->n_value == (*b)->n_value)
			return(fname(a0, b0));
		return((*a)->n_value > (*b)->n_value ? 1 : -1);
	}
}

#define CPPFILT	"/usr/bin/c++filt"

void
pipe2cppfilt(void)
{
	int pip[2];
	char *argv[2];

	argv[0] = "c++filt";
	argv[1] = NULL;

	if (pipe(pip) == -1)
		err(1, "pipe");
	switch(fork()) {
	case -1:
		err(1, "fork");
	default:
		dup2(pip[0], 0);
		close(pip[0]);
		close(pip[1]);
		execve(CPPFILT, argv, NULL);
		err(1, "execve");
	case 0:
		dup2(pip[1], 1);
		close(pip[1]);
		close(pip[0]);
	}
}

void
usage(void)
{
	extern char *__progname;

	if (issize)
		fprintf(stderr, "usage: %s [-tw] [file ...]\n", __progname);
	else
		fprintf(stderr, "usage: %s [-aCegnoprsuw] [file ...]\n",
		    __progname);
	exit(1);
}
