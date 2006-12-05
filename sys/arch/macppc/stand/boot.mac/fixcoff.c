/*	$OpenBSD: */
/*	$NetBSD: fixcoff.c,v 1.10 2006/04/07 02:34:55 gdamore Exp $ */

/*
 * Copyright (c) 1999 National Aeronautics & Space Administration
 * All rights reserved.
 *
 * This software was written by William Studenmund of the
 * Numerical Aerospace Similation Facility, NASA Ames Research Center.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the National Aeronautics & Space Administration
 *    nor the names of its contributors may be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NATIONAL AERONAUTICS & SPACE ADMINISTRATION
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE ADMINISTRATION OR CONTRIB-
 * UTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This program fixes up the extended xcoff headers generated when an elf
 * file is turned into an xcoff one with the current objcopy. It should
 * go away someday, when objcopy will correctly fix up the output xcoff
 *
 * Partially inspired by hack-coff, written by Paul Mackerras.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/endian.h>

struct filehdr {
#define U802WRMAGIC     0730
#define U802ROMAGIC     0735
#define U802TOCMAGIC    0737
	char f_magic[2];
	char f_nsect[2];
	char f_time[4];
	char f_symtab[4];
	char f_nsyms[4];
	char f_opthdr[2];
	char f_flags[2];
};

struct sectionhdr {
	char	s_name[8];
	char	s_paddr[4];
	char	s_vaddr[4];
	char	s_size[4];
	char	s_section[4];
	char	s_reloc[4];
	char	s_lineno[4];
	char	s_nreloc[2];
	char	s_nlineno[2];
	char	s_flags[4];
};

struct aouthdr {
	char	magic[2];
	char	vstamp[2];
	char	tsize[4];
	char	dsize[4];
	char	bsize[4];
	char	entry[4];
	char	text_start[4];
	char	data_start[4];
#define SMALL_AOUTSZ	28
	char	o_toc[4];
	char	o_snentry[2];
	char	o_sntext[2];
	char	o_sndata[2];
	char	o_sntoc[2];
	char	o_snloader[2];
	char	o_snbss[2];
	char	o_algntext[2];
	char	o_algndata[2];
	char	o_modtype[2];
	char	o_cputype[2];
	char	o_maxstack[4];
	char	o_maxdata[4];
	char	o_resv2[12];
};
#define RS6K_AOUTHDR_ZMAGIC     0x010B

char *progname;

void
usage(char *prog)
{
	fprintf(stderr, "Usage: %s [-h] | [<file to fix>]\n", prog);
}

void
help(char *prog)
{
	fprintf(stderr, "%s\tis designed to fix the xcoff headers in a\n",prog);
	fprintf(stderr,
"\tbinary generated using objcopy from a non-xcoff source.\n");
	usage(prog);
	exit(0);
}

main(int argc, char *argv[])
{
	int	fd, i, n, ch;
	struct	filehdr	fh;
	struct	aouthdr aoh;
	struct	sectionhdr sh;

	progname = argv[0];
	while ((ch = getopt(argc, argv, "h")) != -1)
	    switch (ch) {
		case 'h':
		help(progname);
	}

	argc -= optind;
	argv += optind;

	if (argc != 1) {
		usage(progname);
		exit(1);
	}

	if ((fd = open(argv[0], O_RDWR, 0)) == -1)
		err(i, "%s", argv[0]);

	/*
	 * Make sure it looks like an xcoff file..
	 */
	if (read(fd, &fh, sizeof(fh)) != sizeof(fh))
		err(1, "%s reading header", argv[0]);

	i = betoh16(*(uint16_t *)fh.f_magic);
	if ((i != U802WRMAGIC) && (i != U802ROMAGIC) && (i != U802TOCMAGIC))
		errx(1, "%s: not a valid xcoff file", argv[0]);

	/* Does the AOUT "Optional header" make sense? */
	i = betoh16(*(uint16_t *)fh.f_opthdr);

	if (i == SMALL_AOUTSZ)
		errx(1, "%s: file has small \"optional\" header, inappropriate for use with %s", argv[0], progname);
	else if (i != sizeof(aoh))
		errx(1, "%s: invalid \"optional\" header", argv[0]);

	if (read(fd, &aoh, i) != i)
		err(1, "%s reading \"optional\" header", argv[0]);

	/* Now start filing in the AOUT header */
	*(uint16_t *)aoh.magic = htobe16(RS6K_AOUTHDR_ZMAGIC);
	n = betoh16(*(uint16_t *)fh.f_nsect);

	for (i = 0; i < n; i++) {
		if (read(fd, &sh, sizeof(sh)) != sizeof(sh))
			err(1, "%s reading section headers", argv[0]);
		if (strcmp(sh.s_name, ".text") == 0) {
			*(uint16_t *)(aoh.o_snentry) = htobe16(i+1);
			*(uint16_t *)(aoh.o_sntext) = htobe16(i+1);
		} else if (strcmp(sh.s_name, ".data") == 0) {
			*(uint16_t *)(aoh.o_sndata) = htobe16(i+1);
		} else if (strcmp(sh.s_name, ".bss") == 0) {
			*(uint16_t *)(aoh.o_snbss) = htobe16(i+1);
		}
	}

	/* now write it out */
	if (pwrite(fd, &aoh, sizeof(aoh), sizeof(struct filehdr)) != 
	    sizeof(aoh))
		err(1, "%s writing modified header", argv[0]);
	close(fd);
	exit(0);
}
