/*	$OpenBSD: size.c,v 1.20 2003/09/30 19:00:14 mickey Exp $	*/
/*	$NetBSD: size.c,v 1.7 1996/01/14 23:07:12 pk Exp $	*/

/*
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
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
static const char copyright[] =
"@(#) Copyright (c) 1988, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)size.c	8.2 (Berkeley) 12/9/93";
#endif
static const char rcsid[] = "$OpenBSD: size.c,v 1.20 2003/09/30 19:00:14 mickey Exp $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/file.h>
#include <elf_abi.h>
#include <a.out.h>
#include <ar.h>
#include <ranlib.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <err.h>
#include "byte.c"

#ifdef MID_MACHINE_OVERRIDE
#undef MID_MACHINE
#define MID_MACHINE MID_MACHINE_OVERRIDE
#endif

#define	STRTABMAG	"//"

union hdr {
	struct exec aout;
	Elf_Ehdr elf;
};

unsigned long total_text, total_data, total_bss, total_total;
int non_object_warning, print_totals;

int	process_file(int, char *);
int	show_archive(int, char *, FILE *);
int	show_file(int, int, char *, FILE *, off_t, union hdr *);
void	usage(void);

int
main(int argc, char *argv[])
{
	int ch, eval;

	while ((ch = getopt(argc, argv, "wt")) != -1)
		switch(ch) {
		case 'w':
			non_object_warning = 1;
			break;
		case 't':
			print_totals = 1;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	eval = 0;
	if (*argv)
		do {
			eval |= process_file(argc, *argv);
		} while (*++argv);
	else
		eval |= process_file(1, "a.out");

	if (print_totals)
		(void)printf("\n%lu\t%lu\t%lu\t%lu\t%lx\tTOTAL\n",
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
process_file(int count, char *fname)
{
	union hdr exec_head;
	FILE *fp;
	int retval;
	char magic[SARMAG];

	if (!(fp = fopen(fname, "r"))) {
		warnx("cannot read %s", fname);
		return(1);
	}

	/*
	 * first check whether this is an object file - read a object
	 * header, and skip back to the beginning
	 */
	if (fread((char *)&exec_head, sizeof(exec_head), (size_t)1, fp) != 1) {
		warnx("%s: bad format", fname);
		(void)fclose(fp);
		return(1);
	}
	rewind(fp);

	/* this could be an archive */
	if (!IS_ELF(exec_head.elf) && N_BADMAG(exec_head.aout)) {
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

/*
 * show_archive()
 *	show symbols in the given archive file
 */
int
show_archive(int count, char *fname, FILE *fp)
{
	struct ar_hdr ar_head;
	union hdr exec_head;
	int i, rval;
	off_t last_ar_off, foff;
	char *p, *name, *strtab;
	int baselen, namelen;

	baselen = strlen(fname) + 3;
	namelen = sizeof(ar_head.ar_name);
	if ((name = malloc(baselen + namelen)) == NULL)
		err(1, NULL);

	rval = 0;
	strtab = NULL;

	/* while there are more entries in the archive */
	while (fread((char *)&ar_head, sizeof(ar_head), (size_t)1, fp) == 1) {
		/* bad archive entry - stop processing this archive */
		if (strncmp(ar_head.ar_fmag, ARFMAG, sizeof(ar_head.ar_fmag))) {
			warnx("%s: bad format archive header", fname);
			free(name);
			free(strtab);
			return(1);
		}

		/* remember start position of current archive object */
		last_ar_off = ftello(fp);

		/* skip ranlib entries */
		if (!strncmp(ar_head.ar_name, RANLIBMAG, sizeof(RANLIBMAG) - 1))
			goto skip;

		/* load the Sys5 long names table */
		if (!strncmp(ar_head.ar_name, STRTABMAG,
		    sizeof(STRTABMAG) - 1)) {

			i = atol(ar_head.ar_size);
			if ((strtab = malloc(i)) == NULL) {
				warn("%s: strtab", name);
				free(name);
				return(1);
			}

			if (fread(strtab, i, (size_t)1, fp) != 1) {
				warnx("%s: premature EOF", name);
				free(strtab);
				free(name);
				return(1);
			}

			for (p = strtab; i--; p++)
				if (*p == '\n')
					*p = '\0';
			goto skip;
		}

		/*
		 * construct a name of the form "archive.a:obj.o:" for the
		 * current archive entry if the object name is to be printed
		 * on each output line
		 */
		p = name;
		if (count > 1) {
			snprintf(name, baselen - 1, "%s:", fname);
			p += strlen(name);
		}

		if (strtab && ar_head.ar_name[0] == '/') {
			int len;

			i = atol(&ar_head.ar_name[1]);
			len = strlen(&strtab[i]);
			if (len > namelen) {
				p -= (long)name;
				if ((name = realloc(name, baselen+len)) == NULL)
					err(1, NULL);
				namelen = len;
				p += (long)name;
			}
			strlcpy(p, &strtab[i], len);
			p += len;
		} else
#ifdef AR_EFMT1
		/*
		 * BSD 4.4 extended AR format: #1/<namelen>, with name as the
		 * first <namelen> bytes of the file
		 */
		if ((ar_head.ar_name[0] == '#') &&
		    (ar_head.ar_name[1] == '1') &&
		    (ar_head.ar_name[2] == '/') && 
		    (isdigit(ar_head.ar_name[3]))) {
			int len = atoi(&ar_head.ar_name[3]);

			if (len > namelen) {
				p -= (long)name;
				if ((name = realloc(name, baselen+len)) == NULL)
					err(1, NULL);
				namelen = len;
				p += (long)name;
			}
			if (fread(p, len, 1, fp) != 1) {
				warnx("%s: premature EOF", name);
				free(name);
				return(1);
			}
			p += len;
		} else
#endif
		for (i = 0; i < sizeof(ar_head.ar_name); ++i)
			if (ar_head.ar_name[i] && ar_head.ar_name[i] != ' ')
				*p++ = ar_head.ar_name[i];
		*p = '\0';
		if (p[-1] == '/')
			*--p = '\0';

		foff = ftello(fp);

		/* get and check current object's header */
		if (fread((char *)&exec_head, sizeof(exec_head),
		    (size_t)1, fp) != 1) {
			warnx("%s: premature EOF", name);
			free(name);
			return(1);
		}

		rval |= show_file(2, non_object_warning, name, fp, foff, &exec_head);
		/*
		 * skip to next archive object - it starts at the next
	 	 * even byte boundary
		 */
#define even(x) (((x) + 1) & ~1)
skip:		if (fseeko(fp, last_ar_off + even(atol(ar_head.ar_size)),
		    SEEK_SET)) {
			warn("%s", fname);
			free(name);
			return(1);
		}
	}
	free(name);
	return(rval);
}

int
show_file(int count, int warn_fmt, char *name, FILE *fp, off_t foff, union hdr *head)
{
	static int first = 1;
	Elf_Shdr *shdr;
	u_long text, data, bss, total;
	int i;

	if (IS_ELF(head->elf) &&
	    head->elf.e_ident[EI_CLASS] == ELF_TARG_CLASS &&
	    head->elf.e_ident[EI_DATA] == ELF_TARG_DATA &&
	    head->elf.e_ident[EI_VERSION] == ELF_TARG_VER &&
	    head->elf.e_machine == ELF_TARG_MACH &&
	    head->elf.e_version == ELF_TARG_VER) {

		if ((shdr = malloc(head->elf.e_shentsize *
		    head->elf.e_shnum)) == NULL) {
			warn("%s: malloc shdr", name);
			return (1);
		}

		if (fseeko(fp, foff + head->elf.e_shoff, SEEK_SET)) {
			warn("%s: fseeko", name);
			free(shdr);
			return (1);
		}

		if (fread(shdr, head->elf.e_shentsize, head->elf.e_shnum,
		    fp) != head->elf.e_shnum) {
			warnx("%s: premature EOF", name);
			free(shdr);
			return(1);
		}

		text = data = bss = 0;
		for (i = 0; i < head->elf.e_shnum; i++) {
			if (!(shdr[i].sh_flags & SHF_ALLOC))
				;
			else if (shdr[i].sh_flags & SHF_EXECINSTR ||
			    !(shdr[i].sh_flags & SHF_WRITE))
				text += shdr[i].sh_size;
			else if (shdr[i].sh_type == SHT_NOBITS)
				bss += shdr[i].sh_size;
			else
				data += shdr[i].sh_size;
		}
		free(shdr);

	} else if (BAD_OBJECT(head->aout)) {
		if (warn_fmt)
			warnx("%s: bad format", name);
		return (1);
	} else {
		fix_header_order(&head->aout);

		text = head->aout.a_text;
		data = head->aout.a_data;
		bss = head->aout.a_bss;
	}

	if (first) {
		first = 0;
		(void)printf("text\tdata\tbss\tdec\thex\n");
	}

	total = text + data + bss;
	(void)printf("%lu\t%lu\t%lu\t%lu\t%lx", text, data, bss, total, total);
	if (count > 1)
		(void)printf("\t%s", name);

	total_text += text;
	total_data += data;
	total_bss += bss;
	total_total += total;

	(void)printf("\n");
	return (0);
}

void
usage(void)
{
	(void)fprintf(stderr, "usage: size [-tw] [file ...]\n");
	exit(1);
}
