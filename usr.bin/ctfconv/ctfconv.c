/*	$OpenBSD: ctfconv.c,v 1.17 2018/08/08 20:15:17 mestre Exp $ */

/*
 * Copyright (c) 2016-2017 Martin Pieuchot
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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/queue.h>
#include <sys/tree.h>
#include <sys/ctf.h>

#include <assert.h>
#include <elf.h>
#include <err.h>
#include <fcntl.h>
#include <locale.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "itype.h"
#include "xmalloc.h"

#ifndef nitems
#define nitems(_a)	(sizeof((_a)) / sizeof((_a)[0]))
#endif

#define DEBUG_ABBREV	".debug_abbrev"
#define DEBUG_INFO	".debug_info"
#define DEBUG_LINE	".debug_line"
#define DEBUG_STR	".debug_str"

__dead void	 usage(void);
int		 convert(const char *);
int		 generate(const char *, const char *, int);
int		 elf_convert(char *, size_t);
void		 elf_sort(void);
struct itype	*find_symb(struct itype *, size_t);
void		 dump_type(struct itype *);
void		 dump_func(struct itype *, int *);
void		 dump_obj(struct itype *, int *);

/* elf.c */
int		 iself(const char *, size_t);
int		 elf_getshstab(const char *, size_t, const char **, size_t *);
ssize_t		 elf_getsymtab(const char *, size_t, const char *, size_t,
		     const Elf_Sym **, size_t *, const char **, size_t *);
ssize_t		 elf_getsection(char *, size_t, const char *, const char *,
		     size_t, const char **, size_t *);

/* parse.c */
void		 dwarf_parse(const char *, size_t, const char *, size_t);

const char	*ctf_enc2name(unsigned short);

/* lists of parsed types and functions */
struct itype_queue itypeq = TAILQ_HEAD_INITIALIZER(itypeq);
struct itype_queue ifuncq = TAILQ_HEAD_INITIALIZER(ifuncq);
struct itype_queue iobjq = TAILQ_HEAD_INITIALIZER(iobjq);

__dead void
usage(void)
{
	fprintf(stderr, "usage: %s [-d] -l label -o outfile file\n",
	    getprogname());
	exit(1);
}

int
main(int argc, char *argv[])
{
	const char *filename, *label = NULL, *outfile = NULL;
	int dump = 0;
	int ch, error = 0;
	struct itype *it;

	setlocale(LC_ALL, "");

	while ((ch = getopt(argc, argv, "dl:o:")) != -1) {
		switch (ch) {
		case 'd':
			dump = 1;	/* ctfdump(1)-like SUNW_ctf sections */
			break;
		case 'l':
			if (label != NULL)
				usage();
			label = optarg;
			break;
		case 'o':
			if (outfile != NULL)
				usage();
			outfile = optarg;
			break;
		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;

	if (argc != 1)
		usage();

	/* Either dump the sections, or write it out. */
	if ((dump && (outfile != NULL || label != NULL)) ||
	    (!dump && (outfile == NULL || label == NULL)))
		usage();

	filename = *argv;

	if (unveil(filename, "r") == -1)
		err(1, "unveil");

	if (outfile != NULL) {
		if (unveil(outfile, "wc") == -1)
			err(1, "unveil");
	}

	if (pledge("stdio rpath wpath cpath", NULL) == -1)
		err(1, "pledge");

	error = convert(filename);
	if (error != 0)
		return error;

	if (outfile != NULL) {
		if (pledge("stdio wpath cpath", NULL) == -1)
			err(1, "pledge");

		error = generate(outfile, label, 1);
		if (error != 0)
			return error;
	}

	if (dump) {
		if (pledge("stdio", NULL) == -1)
			err(1, "pledge");

		int fidx = -1, oidx = -1;

		TAILQ_FOREACH(it, &iobjq, it_symb)
			dump_obj(it, &oidx);
		printf("\n");

		TAILQ_FOREACH(it, &ifuncq, it_symb)
			dump_func(it, &fidx);
		printf("\n");

		TAILQ_FOREACH(it, &itypeq, it_next) {
			if (it->it_flags & (ITF_FUNC|ITF_OBJ))
				continue;

			dump_type(it);
		}

		return 0;
	}

	return 0;
}

int
convert(const char *path)
{
	struct stat		 st;
	int			 fd, error = 1;
	char			*p;

	fd = open(path, O_RDONLY);
	if (fd == -1) {
		warn("open %s", path);
		return 1;
	}
	if (fstat(fd, &st) == -1) {
		warn("fstat %s", path);
		close(fd);
		return 1;
	}
	if ((uintmax_t)st.st_size > SIZE_MAX) {
		warnx("file too big to fit memory");
		close(fd);
		return 1;
	}

	p = mmap(NULL, st.st_size, PROT_READ|PROT_WRITE, MAP_PRIVATE, fd, 0);
	if (p == MAP_FAILED)
		err(1, "mmap");

	if (iself(p, st.st_size))
		error = elf_convert(p, st.st_size);

	munmap(p, st.st_size);
	close(fd);

	return error;
}

const char		*dstrbuf;
size_t			 dstrlen;
const char		*strtab;
const Elf_Sym		*symtab;
size_t			 strtabsz, nsymb;

int
elf_convert(char *p, size_t filesize)
{
	const char		*shstab;
	const char		*infobuf, *abbuf;
	size_t			 infolen, ablen;
	size_t			 shstabsz;

	/* Find section header string table location and size. */
	if (elf_getshstab(p, filesize, &shstab, &shstabsz))
		return 1;

	/* Find symbol table and associated string table. */
	if (elf_getsymtab(p, filesize, shstab, shstabsz, &symtab, &nsymb,
	    &strtab, &strtabsz) == -1)
		warnx("symbol table not found");

	/* Find abbreviation location and size. */
	if (elf_getsection(p, filesize, DEBUG_ABBREV, shstab, shstabsz, &abbuf,
	    &ablen) == -1) {
		warnx("%s section not found", DEBUG_ABBREV);
		return 1;
	}

	if (elf_getsection(p, filesize, DEBUG_INFO, shstab, shstabsz, &infobuf,
	    &infolen) == -1) {
		warnx("%s section not found", DEBUG_INFO);
		return 1;
	}

	/* Find string table location and size. */
	if (elf_getsection(p, filesize, DEBUG_STR, shstab, shstabsz, &dstrbuf,
	    &dstrlen) == -1)
		warnx("%s section not found", DEBUG_STR);

	dwarf_parse(infobuf, infolen, abbuf, ablen);

	/* Sort functions */
	elf_sort();

	return 0;
}

struct itype *
find_symb(struct itype *tmp, size_t stroff)
{
	struct itype		*it;
	char 			*sname, *p;

	if (strtab == NULL || stroff >= strtabsz)
		return NULL;

	/*
	 * Skip local suffix
	 *
	 * FIXME: only skip local copies.
	 */
	sname = xstrdup(strtab + stroff);
	if ((p = strtok(sname, ".")) == NULL) {
		free(sname);
		return NULL;
	}

	strlcpy(tmp->it_name, p, ITNAME_MAX);
	free(sname);
	it = RB_FIND(isymb_tree, &isymbt, tmp);

	/* Restore original name */
	if (it == NULL)
		strlcpy(tmp->it_name, (strtab + stroff), ITNAME_MAX);

	return it;
}

void
elf_sort(void)
{
	struct itype		*it, tmp;
	size_t			 i;

	memset(&tmp, 0, sizeof(tmp));
	for (i = 0; i < nsymb; i++) {
		const Elf_Sym	*st = &symtab[i];

		if (st->st_shndx == SHN_UNDEF || st->st_shndx == SHN_COMMON)
			continue;

		switch (ELF_ST_TYPE(st->st_info)) {
		case STT_FUNC:
			tmp.it_flags = ITF_FUNC;
			break;
		case STT_OBJECT:
			tmp.it_flags = ITF_OBJ;
			break;
		default:
			continue;
		}

		it = find_symb(&tmp, st->st_name);
		if (it == NULL) {
			/* Insert 'unknown' entry to match symbol order. */
			it = it_dup(&tmp);
			it->it_refp = it;
#ifdef DEBUG
			warnx("symbol not found: %s", it_name(it));
#endif
		}

		if (it->it_flags & ITF_INSERTED) {
#ifdef DEBUG
			warnx("%s: already inserted", it_name(it));
#endif
			it = it_dup(it);
		}

		/* Save symbol index for dump. */
		it->it_ref = i;

		it->it_flags |= ITF_INSERTED;
		if (it->it_flags & ITF_FUNC)
			TAILQ_INSERT_TAIL(&ifuncq, it, it_symb);
		else
			TAILQ_INSERT_TAIL(&iobjq, it, it_symb);
	}
}

const char *
type_name(struct itype *it)
{
	const char *name;

	name = it_name(it);
	if (name == NULL)
		return "(anon)";

	return name;
}

/* Display parsed types a la ctfdump(1) */
void
dump_type(struct itype *it)
{
	struct imember *im;

#ifdef DEBUG
	switch (it->it_type) {
	case CTF_K_POINTER:
	case CTF_K_TYPEDEF:
	case CTF_K_VOLATILE:
	case CTF_K_CONST:
	case CTF_K_RESTRICT:
	case CTF_K_ARRAY:
	case CTF_K_FUNCTION:
		if (it->it_refp == NULL) {
			printf("unresolved: %s type=%d\n", it_name(it),
			    it->it_type);
			return;
		}
	default:
		break;
	}
#endif

	switch (it->it_type) {
	case CTF_K_FLOAT:
	case CTF_K_INTEGER:
		printf("  [%u] %s %s encoding=%s offset=0 bits=%u\n",
		    it->it_idx,
		    (it->it_type == CTF_K_INTEGER) ? "INTEGER" : "FLOAT",
		    it_name(it), ctf_enc2name(it->it_enc), it->it_size);
		break;
	case CTF_K_POINTER:
		printf("  <%u> POINTER %s refers to %u\n", it->it_idx,
		    type_name(it), it->it_refp->it_idx);
		break;
	case CTF_K_TYPEDEF:
		printf("  <%u> TYPEDEF %s refers to %u\n",
		    it->it_idx, it_name(it), it->it_refp->it_idx);
		break;
	case CTF_K_VOLATILE:
		printf("  <%u> VOLATILE %s refers to %u\n", it->it_idx,
		    type_name(it), it->it_refp->it_idx);
		break;
	case CTF_K_CONST:
		printf("  <%u> CONST %s refers to %u\n", it->it_idx,
		    type_name(it), it->it_refp->it_idx);
		break;
	case CTF_K_RESTRICT:
		printf("  <%u> RESTRICT %s refers to %u\n", it->it_idx,
		    it_name(it), it->it_refp->it_idx);
		break;
	case CTF_K_ARRAY:
		printf("  [%u] ARRAY %s content: %u index: %u nelems: %u\n",
		    it->it_idx, type_name(it), it->it_refp->it_idx, long_tidx,
		    it->it_nelems);
		printf("\n");
		break;
	case CTF_K_STRUCT:
	case CTF_K_UNION:
		printf("  [%u] %s %s (%u bytes)\n", it->it_idx,
		    (it->it_type == CTF_K_STRUCT) ? "STRUCT" : "UNION",
		    type_name(it), it->it_size);
		TAILQ_FOREACH(im, &it->it_members, im_next) {
			printf("\t%s type=%u off=%zu\n",
			    (im_name(im) == NULL) ? "unknown" : im_name(im),
			    im->im_refp ? im->im_refp->it_idx : 0, im->im_off);
		}
		printf("\n");
		break;
	case CTF_K_ENUM:
		printf("  [%u] ENUM %s\n", it->it_idx, type_name(it));
		TAILQ_FOREACH(im, &it->it_members, im_next) {
			printf("\t%s = %zu\n", im_name(im), im->im_ref);
		}
		printf("\n");
		break;
	case CTF_K_FUNCTION:
		printf("  [%u] FUNCTION (%s) returns: %u args: (",
		    it->it_idx, (it_name(it) != NULL) ? it_name(it) : "anon",
		    it->it_refp->it_idx);
		TAILQ_FOREACH(im, &it->it_members, im_next) {
			printf("%u%s", im->im_refp->it_idx,
			    TAILQ_NEXT(im, im_next) ? ", " : "");
		}
		printf(")\n");
		break;
	default:
		assert(0 == 1);
	}
}

void
dump_func(struct itype *it, int *idx)
{
	struct imember *im;

	(*idx)++;

	if (it->it_type == CTF_K_UNKNOWN && it->it_nelems == 0)
		return;

	printf("  [%u] FUNC (%s) returns: %u args: (", (*idx),
	    (it_name(it) != NULL) ? it_name(it) : "unknown",
	    it->it_refp->it_idx);
	TAILQ_FOREACH(im, &it->it_members, im_next) {
		printf("%u%s", im->im_refp->it_idx,
		    TAILQ_NEXT(im, im_next) ? ", " : "");
	}
	printf(")\n");
}

void
dump_obj(struct itype *it, int *idx)
{
	int l;

	(*idx)++;

	l = printf("  [%u] %u", (*idx), it->it_refp->it_idx);
	printf("%*s %s (%llu)\n", 14 - l, "", it_name(it), it->it_ref);
}

const char *
ctf_enc2name(unsigned short enc)
{
	static const char *enc_name[] = { "SIGNED", "CHAR", "SIGNED CHAR",
	    "BOOL", "SIGNED BOOL" };
	static char invalid[7];

	if (enc == CTF_INT_VARARGS)
		return "VARARGS";

	if (enc > 0 && enc < nitems(enc_name))
		return enc_name[enc - 1];

	snprintf(invalid, sizeof(invalid), "0x%x", enc);
	return invalid;
}
