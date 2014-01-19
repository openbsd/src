/*	$OpenBSD: a2coff.c,v 1.10 2014/01/19 15:39:51 miod Exp $	*/
/*
 * Copyright (c) 2006, 2013, Miodrag Vallat
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Quick and dirty ELF to 88K BCS ECOFF converter. Will only work for
 * standalone binaries with no relocations, and will drop symbols.
 * Also, bss is merged into the data section to cope with PROMs which
 * do not zero-fill the bss upon loading (sad but true).
 *
 * This should really only be used to build a BSD/aviion bootloader.
 */

#include <unistd.h>
#include <stdlib.h>
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#include <a.out.h>	/* ZMAGIC */
#define	ELFSIZE		32
#include <sys/exec_elf.h>

#define	MINIMAL_ALIGN	8
#define	ECOFF_ALIGN	0x200

/*
 * We can't use the standard ecoff defines, first, because the system
 * we are building this tool on might not have ecoff support at all (thus
 * no <machine/ecoff_machdep.h> file), second, because the common defines
 * do not know about the scnhdr changes for 88K BCS.
 * So we'll provide our own, working, definitions.
 */
struct ecoff_filehdr {
	u_short f_magic;	/* magic number */
	u_short f_nscns;	/* # of sections */
	u_int   f_timdat;	/* time and date stamp */
	u_long  f_symptr;	/* file offset of symbol table */
	u_int   f_nsyms;	/* # of symbol table entries */
	u_short f_opthdr;	/* sizeof the optional header */
	u_short f_flags;	/* flags??? */
};

struct ecoff_aouthdr {
	u_short magic;
	u_short vstamp;
#if 0
	ECOFF_PAD
#endif
	u_long  tsize;
	u_long  dsize;
	u_long  bsize;
	u_long  entry;
	u_long  text_start;
	u_long  data_start;
#if 0	/* not on m88k */
	u_long  bss_start;
	ECOFF_MACHDEP;
#endif
};

struct ecoff_scnhdr {		/* needed for size info */
	char	s_name[8];	/* name */
	u_long  s_paddr;	/* physical addr? for ROMing?*/
	u_long  s_vaddr;	/* virtual addr? */
	u_long  s_size;		/* size */
	u_long  s_scnptr;	/* file offset of raw data */
	u_long  s_relptr;	/* file offset of reloc data */
	u_long  s_lnnoptr;	/* file offset of line data */
#if 0
	u_short s_nreloc;	/* # of relocation entries */
	u_short s_nlnno;	/* # of line entries */
#else
	/* m88k specific changes */
	u_long  s_nreloc;
	union {
	 u_long _s_nlnno;
	 u_long _s_vendor;
	} _s_s;
#define	s_nlnno   _s_s._s_nlnno
#define	s_vendor _s_s._s_vendor 
#endif
	u_long	s_flags;
};

struct ecoff_exechdr {
	struct ecoff_filehdr f;
	struct ecoff_aouthdr a;
};

#define	round(qty, pow2)	(((qty) + (pow2 - 1)) & ~(pow2 - 1UL))

void	convert_elf(const char *, int, int, Elf_Ehdr *);
void	copybits(int, int, u_int32_t);
void	usage(void);
void	zerobits(int, u_int32_t);

int
main(int argc, char *argv[])
{
	Elf_Ehdr head;
	int infd, outfd;
	int n;

	if (argc != 3)
		usage();

	infd = open(argv[1], O_RDONLY);
	if (infd < 0)
		err(1, argv[1]);

	n = read(infd, &head, sizeof(head));
	if (n < sizeof(head))
		err(1, "read");

	if (!IS_ELF(head))
		err(1, "%s: bad magic", argv[1]);

	outfd = open(argv[2], O_WRONLY | O_TRUNC | O_CREAT, 0644);
	if (outfd < 0)
		err(1, argv[2]);

	convert_elf(argv[1], infd, outfd, &head);

	close(infd);
	close(outfd);

	exit(0);
}

char buf[4096];
#define	min(a ,b)	((a) < (b) ? (a) : (b))

void
copybits(int from, int to, u_int32_t count)
{
	int chunk;

	while (count != 0) {
		chunk = min(count, sizeof buf);
		if (read(from, buf, chunk) != chunk)
			err(1, "read");
		if (write(to, buf, chunk) != chunk)
			err(1, "write");
		count -= chunk;
	}
}

void
zerobits(int to, u_int32_t count)
{
	int chunk;

	memset(buf, 0, sizeof buf);
	while (count != 0) {
		chunk = min(count, sizeof buf);
		if (write(to, buf, chunk) != chunk)
			err(1, "write");
		count -= chunk;
	}
}

__dead void
usage(void)
{
	extern char *__progname;

	fprintf(stderr, "usage: %s infile outfile\n", __progname);
	exit(1);
}

/*
 * Convert an ELF binary into BCS ECOFF format.
 * We merge all program headers into a single ECOFF section for simplicity.
 * However, PROM version 01.14 on AV4300 will fail to load a BCS binary
 * unless it has a non-empty data section, so we put the smallest possible
 * bunch of zeroes as a second section to appease it.
 */
void
convert_elf(const char *infile, int infd, int outfd, Elf_Ehdr *ehdr)
{
	struct ecoff_exechdr ehead;
	struct ecoff_scnhdr escn[2];
	Elf_Phdr *phdr;
	off_t outpos;
	uint delta, ptload;
	Elf_Addr minaddr, maxaddr;
	int n, last;

	phdr = (Elf_Phdr *)malloc(ehdr->e_phnum * sizeof(Elf_Phdr));
	if (phdr == NULL)
		err(1, "malloc");

	memset(phdr, 0, sizeof phdr);
	for (n = 0; n < ehdr->e_phnum; n++) {
		if (lseek(infd, ehdr->e_phoff + n * ehdr->e_phentsize,
		    SEEK_SET) == (off_t) -1)
			err(1, "seek");
		if (read(infd, phdr + n, sizeof *phdr) != sizeof(*phdr))
			err(1, "read");
	}

	ptload = 0;
	for (n = 0; n < ehdr->e_phnum; n++)
		if (phdr[n].p_type == PT_LOAD)
			ptload++;
	if (ptload > 3)
		errx(1, "%s: too many PT_LOAD program headers", infile);

	maxaddr = 0;
	minaddr = (Elf_Addr)-1;

	for (n = 0; n < ehdr->e_phnum; n++) {
		if (phdr[n].p_type != PT_LOAD)
			continue;
		if (phdr[n].p_paddr < minaddr)
			minaddr = phdr[n].p_paddr;
		if (phdr[n].p_paddr + phdr[n].p_memsz > maxaddr)
			maxaddr = phdr[n].p_paddr + phdr[n].p_memsz;
	}
	maxaddr = round(maxaddr, MINIMAL_ALIGN);

	/*
	 * Header
	 */

	memset(&ehead, 0, sizeof ehead);
	memset(&escn, 0, sizeof escn);

	ehead.f.f_magic = 0x016d;		/* MC88OMAGIC */
	ehead.f.f_nscns = 2;
	ehead.f.f_opthdr = sizeof ehead.a;
	ehead.f.f_flags = 0x020f;
		/* F_RELFLG | F_EXEC | F_LNNO | 8 | F_AR16WR */

	ehead.a.magic = ZMAGIC;
	ehead.a.tsize = maxaddr - minaddr;			/* ignored */
	ehead.a.dsize = MINIMAL_ALIGN;				/* ignored */
	ehead.a.bsize = 0;					/* ignored */
	ehead.a.entry = ehdr->e_entry;
	ehead.a.text_start = minaddr;				/* ignored */
	ehead.a.data_start = maxaddr;				/* ignored */

	n = write(outfd, &ehead, sizeof(ehead));
	if (n != sizeof(ehead))
		err(1, "write");

	/*
	 * Sections
	 */

	strncpy(escn[0].s_name, ".text", sizeof escn[0].s_name);
	escn[0].s_paddr = minaddr;		/* ignored, 1:1 mapping */
	escn[0].s_size = maxaddr - minaddr;
	escn[0].s_scnptr = round(sizeof(ehead) + sizeof(escn), MINIMAL_ALIGN);
	escn[0].s_flags = 0x20;	/* STYP_TEXT */

	strncpy(escn[1].s_name, ".data", sizeof escn[1].s_name);
	escn[1].s_paddr = escn[0].s_paddr + escn[0].s_size;
	escn[1].s_size = MINIMAL_ALIGN;
	escn[1].s_scnptr = escn[0].s_scnptr + escn[0].s_size;
	escn[1].s_flags = 0x40;	/* STYP_DATA */

	/* adjust load addresses */
	escn[0].s_vaddr = escn[0].s_paddr;
	escn[1].s_vaddr = escn[1].s_paddr;

	n = write(outfd, &escn, sizeof(escn));
	if (n != sizeof(escn))
		err(1, "write");

	/*
	 * Copy ``text'' section (all PT_LOAD program headers).
	 */

	outpos = escn[0].s_scnptr;
	if (lseek(outfd, outpos, SEEK_SET) == (off_t) -1)
		err(1, "seek");
	for (n = 0, last = -1; n < ehdr->e_phnum; n++) {
		if (phdr[n].p_type != PT_LOAD)
			continue;
		if (last >= 0) {
			delta = (phdr[n].p_paddr - phdr[last].p_paddr) -
			    phdr[last].p_memsz;
			if (delta != 0) {
				zerobits(outfd, delta);
				outpos += delta;
			}
		}
#ifdef DEBUG
		printf("copying %s: source %x dest %llx size %x\n",
		    escn[0].s_name, phdr[n].p_offset, outpos, phdr[n].p_filesz);
#endif
		if (lseek(infd, phdr[n].p_offset, SEEK_SET) == (off_t) -1)
			err(1, "seek");
		copybits(infd, outfd, phdr[n].p_filesz);
		delta = phdr[n].p_memsz - phdr[n].p_filesz;
		if (delta != 0)
			zerobits(outfd, delta);
		outpos += phdr[n].p_memsz;
		last = n;
	}

	free(phdr);

	/*
	 * Fill ``data'' section.
	 */

	zerobits(outfd, escn[1].s_size);
	outpos += escn[1].s_size;

	/*
	 * Round file to a multiple of 512 bytes, since ``recent'' PROM
	 * (such as rev 1.20 on AV530) will reject files not being properly
	 * rounded to a multiple of 512 bytes.
	 */

	if ((outpos % ECOFF_ALIGN) != 0)
		zerobits(outfd, ECOFF_ALIGN - (outpos % ECOFF_ALIGN));
}
