/*	$OpenBSD: strip.c,v 1.23 2004/10/09 20:36:05 mickey Exp $	*/

/*
 * Copyright (c) 1988 Regents of the University of California.
 * All rights reserved.
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

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1988 Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
/*static char sccsid[] = "from: @(#)strip.c	5.8 (Berkeley) 11/6/91";*/
static char rcsid[] = "$OpenBSD: strip.c,v 1.23 2004/10/09 20:36:05 mickey Exp $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include <a.out.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <ranlib.h>
#include "byte.c"

#ifdef MID_MACHINE_OVERRIDE
#undef MID_MACHINE
#define MID_MACHINE MID_MACHINE_OVERRIDE
#if MID_MACHINE_OVERRIDE == MID_M68K
#undef __LDPGSZ
#undef ELF_TARG_DATA
#undef ELF_TARG_MACH
#include "m68k/exec.h"
#elif MID_MACHINE_OVERRIDE == MID_M88K
#undef __LDPGSZ
#undef ELF_TARG_DATA
#undef ELF_TARG_MACH
#include "m88k/exec.h"
#endif
#endif

typedef struct exec EXEC;
typedef struct nlist NLIST;

#define	strx	n_un.n_strx

int s_stab(const char *, int, EXEC *, struct stat *, off_t *);
int s_sym(const char *, int, EXEC *, struct stat *, off_t *);
void usage(void);

int xflag = 0;
        
int
main(int argc, char *argv[])
{
	int fd;
	EXEC *ep;
	struct stat sb;
	int (*sfcn)(const char *, int, EXEC *, struct stat *, off_t *);
	int ch, errors;
	char *fn;
	off_t newsize;

	sfcn = s_sym;
	while ((ch = getopt(argc, argv, "dx")) != -1)
		switch(ch) {
                case 'x':
                        xflag = 1;
                        /*FALLTHROUGH*/
		case 'd':
			sfcn = s_stab;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	errors = 0;
#define	ERROR(x) errors |= 1; warnx("%s: %s", fn, strerror(x)); continue;
	while ((fn = *argv++)) {
		if ((fd = open(fn, O_RDWR)) < 0) {
			ERROR(errno);
		}
        	if (fstat(fd, &sb)) {
			(void)close(fd);
			ERROR(errno);
		}
		if (sb.st_size < sizeof(EXEC)) {
			(void)close(fd);
			ERROR(EFTYPE);
		}
		if ((ep = (EXEC *)mmap(NULL, sb.st_size, PROT_READ|PROT_WRITE,
		    MAP_SHARED, fd, (off_t)0)) == MAP_FAILED) {
			(void)close(fd);
			ERROR(errno);
		}
		if (BAD_OBJECT(*ep)) {
			munmap((caddr_t)ep, sb.st_size);
			(void)close(fd);
			ERROR(EFTYPE);
		}
		/* since we're dealing with an mmap there, we have to convert once
		   for dealing with data in memory, and a second time for out
		 */
		fix_header_order(ep);
		newsize = 0;
		errors |= sfcn(fn, fd, ep, &sb, &newsize);
		fix_header_order(ep);
		munmap((caddr_t)ep, sb.st_size);
		if (newsize  && ftruncate(fd, newsize)) {
			warn("%s", fn);
			errors = 1;
		}
		if (close(fd)) {
			ERROR(errno);
		}
	}
#undef ERROR
	exit(errors);
}

int
s_sym(const char *fn, int fd, EXEC *ep, struct stat *sp, off_t *sz)
{
	char *neweof;
#if	0
	char *mineof;
#endif
	int zmagic;

	zmagic = ep->a_data &&
		 (N_GETMAGIC(*ep) == ZMAGIC || N_GETMAGIC(*ep) == QMAGIC);

	/*
	 * If no symbols or data/text relocation info and
	 * the file data segment size is already minimized, quit.
	 */
	if (!ep->a_syms && !ep->a_trsize && !ep->a_drsize) {
#if 0
		if (!zmagic)
			return 0;
		if (sp->st_size < N_TRELOFF(*ep))
#endif
			return 0;
	}

	/*
	 * New file size is the header plus text and data segments; OMAGIC
	 * and NMAGIC formats have the text/data immediately following the
	 * header.  ZMAGIC format wastes the rest of of header page.
	 */
	neweof = (char *)ep + N_TRELOFF(*ep);

#if 0
	/*
	 * Unfortunately, this can't work correctly without changing the way
	 * the loader works.  We could cap it at one page, or even fiddle with
	 * a_data and a_bss, but this only works for CLBYTES == NBPG.  If we
	 * are on a system where, e.g., CLBYTES is 8k and NBPG is 4k, and we
	 * happen to remove 4.5k, we will lose.  And we really don't want to
	 * fiddle with pages, because that breaks binary compatibility.  Lose.
	 */

	if (zmagic) {
		/*
		 * Get rid of unneeded zeroes at the end of the data segment
		 * to reduce the file size even more.
		 */
		mineof = (char *)ep + N_DATOFF(*ep);
		while (neweof > mineof && neweof[-1] == '\0')
			neweof--;
	}
#endif

	/* Set symbol size and relocation info values to 0. */
	ep->a_syms = ep->a_trsize = ep->a_drsize = 0;

	/* Truncate the file. */
	*sz = neweof - (char *)ep;

	return 0;
}

int
s_stab(const char *fn, int fd, EXEC *ep, struct stat *sp, off_t *sz)
{
	int cnt, len;
	char *nstr, *nstrbase=0, *used=0, *p, *strbase;
	NLIST *sym, *nsym;
	u_long allocsize;
	int mid;
	NLIST *symbase;
	unsigned int *mapping=0;
	int error=1;
	unsigned int nsyms;
	struct relocation_info *reloc_base;
	unsigned int i, j;

	/* Quit if no symbols. */
	if (ep->a_syms == 0)
		return 0;

	if (N_SYMOFF(*ep) >= sp->st_size) {
		warnx("%s: bad symbol table", fn);
		return 1;
	}

	mid = N_GETMID(*ep);

	/*
	 * Initialize old and new symbol pointers.  They both point to the
	 * beginning of the symbol table in memory, since we're deleting
	 * entries.
	 */
	sym = nsym = symbase = (NLIST *)((char *)ep + N_SYMOFF(*ep));

	/*
	 * Allocate space for the new string table, initialize old and
	 * new string pointers.  Handle the extra long at the beginning
	 * of the string table.
	 */
	strbase = (char *)ep + N_STROFF(*ep);
	allocsize = fix_32_order(*(u_long *)strbase, mid);
	if ((nstrbase = malloc((u_int) allocsize)) == NULL) {
		warnx("%s", strerror(ENOMEM));
		goto end;
	}
	nstr = nstrbase + sizeof(u_long);

	/* okay, so we also need to keep symbol numbers for relocations. */
	nsyms = ep->a_syms/ sizeof(NLIST);
	used = calloc(nsyms, 1);
	if (!used) {
		warnx("%s", strerror(ENOMEM));
		goto end;
	}
	mapping = malloc(nsyms * sizeof(unsigned int));
	if (!mapping) {
		warnx("%s", strerror(ENOMEM));
		goto end;
	}

	if ((ep->a_trsize || ep->a_drsize) && byte_sex(mid) != BYTE_ORDER) {
		warnx("%s: cross-stripping not supported", fn);
		goto end;
	}

	/* first check the relocations for used symbols, and mark them */
	/* text */
	reloc_base = (struct relocation_info *) ((char *)ep + N_TRELOFF(*ep));
	if (N_TRELOFF(*ep) + ep->a_trsize > sp->st_size) {
		warnx("%s: bad text relocation", fn);
		goto end;
	}
	for (i = 0; i < ep->a_trsize / sizeof(struct relocation_info); i++) {
		if (!reloc_base[i].r_extern)
			continue;
		if (reloc_base[i].r_symbolnum > nsyms) {
			warnx("%s: bad symbol number in text relocation", fn);
			goto end;
		}
		used[reloc_base[i].r_symbolnum] = 1;
	}
	/* data */
	reloc_base = (struct relocation_info *) ((char *)ep + N_DRELOFF(*ep));
	if (N_DRELOFF(*ep) + ep->a_drsize > sp->st_size) {
		warnx("%s: bad data relocation", fn);
		goto end;
	}
	for (i = 0; i < ep->a_drsize / sizeof(struct relocation_info); i++) {
		if (!reloc_base[i].r_extern)
			continue;
		if (reloc_base[i].r_symbolnum > nsyms) {
			warnx("%s: bad symbol number in data relocation", fn);
			goto end;
		}
		used[reloc_base[i].r_symbolnum] = 1;
	}

	/*
	 * Read through the symbol table.  For each non-debugging symbol,
	 * copy it and save its string in the new string table.  Keep
	 * track of the number of symbols.
	 */
	for (cnt = nsyms, i = 0, j = 0; cnt--; ++sym, ++i) {
		fix_nlist_order(sym, mid);
		if (!(sym->n_type & N_STAB) && sym->strx) {
			*nsym = *sym;
			nsym->strx = nstr - nstrbase;
			p = strbase + sym->strx;
                        if (xflag && !used[i] &&
                            (!(sym->n_type & N_EXT) ||
                             (sym->n_type & ~N_EXT) == N_FN ||
                             strcmp(p, "gcc_compiled.") == 0 ||
                             strcmp(p, "gcc2_compiled.") == 0 ||
                             strncmp(p, "___gnu_compiled_", 16) == 0)) {
                                continue;
                        }
			len = strlen(p) + 1;
			mapping[i] = j++;
			if (N_STROFF(*ep) + sym->strx + len > sp->st_size) {
				warnx("%s: bad symbol table", fn);
				goto end;
			}
			bcopy(p, nstr, len);
			nstr += len;
			fix_nlist_order(nsym++, mid);
		}
	}

	/* renumber symbol relocations */
	/* text */
	reloc_base = (struct relocation_info *) ((char *)ep + N_TRELOFF(*ep));
	for (i = 0; i < ep->a_trsize / sizeof(struct relocation_info); i++) {
		if (!reloc_base[i].r_extern)
			continue;
		reloc_base[i].r_symbolnum = mapping[reloc_base[i].r_symbolnum];
	}
	/* data */
	reloc_base = (struct relocation_info *) ((char *)ep + N_DRELOFF(*ep));
	for (i = 0; i < ep->a_drsize / sizeof(struct relocation_info); i++) {
		if (!reloc_base[i].r_extern)
			continue;
		reloc_base[i].r_symbolnum = mapping[reloc_base[i].r_symbolnum];
	}

	/* Fill in new symbol table size. */
	ep->a_syms = (nsym - symbase) * sizeof(NLIST);

	/* Fill in the new size of the string table. */
	len = nstr - nstrbase;
	*(u_long *)nstrbase = fix_32_order(len, mid);

	/*
	 * Copy the new string table into place.  Nsym should be pointing
	 * at the address past the last symbol entry.
	 */
	bcopy(nstrbase, (void *)nsym, len);
	error = 0;
end:
	free(nstrbase);
	free(used);
	free(mapping);

	/* Truncate to the current length. */
	*sz = (char *)nsym + len - (char *)ep;

	return error;
}

void
usage(void)
{
	extern char *__progname;

	fprintf(stderr, "usage: %s [-dx] file ...\n", __progname);
	exit(1);
}

