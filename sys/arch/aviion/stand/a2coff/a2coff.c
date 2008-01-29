/*	$OpenBSD: a2coff.c,v 1.3 2008/01/29 13:02:31 krw Exp $	*/
/*
 * Copyright (c) 2006, Miodrag Vallat
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
 * Quick and dirty a.out to 88K BCS ECOFF converter. Will only work for
 * standalone binaries with no relocations, and will drop symbols.
 * Also, bss is merged into the data section to cope with PROMs which
 * do not zero-fill the bss upon loading (sad but true).
 *
 * This should really only be used to build a BSD/dg88k bootloader.
 */

#include <unistd.h>
#include <stdlib.h>
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#include <a.out.h>
/* overwrite __LDPGSZ if not a native binary */
#ifndef	m88k
#undef	__LDPGSZ
#define	__LDPGSZ	0x1000
#endif	/* m88k */

/*
 * We can't use the standard ecoff defines, first, because the system
 * we are building this tool on might not have ecoff support at all (thus
 * no <machine/ecoff_machdep.h> file), second, because the common defines
 * do not know about the scnhdr changes for 88K BCS.
 * So we'll provide our own, working, definitions.
 */
#if 0 /* defined(_KERN_DO_ECOFF) */
#include <sys/exec_ecoff.h>
#else
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
#endif

#define	round(qty, pow2)	(((qty) + (pow2 - 1)) & ~(pow2 - 1UL))

void	copybits(int, int, u_int32_t);
void	usage(void);
void	zerobits(int, u_int32_t);

int
main(int argc, char *argv[])
{
	struct exec head;
	struct ecoff_exechdr ehead;
	struct ecoff_scnhdr escn[3];
	int infd, outfd;
	int n;

	if (argc != 3)
		usage();

	infd = open(argv[1], O_RDONLY);
	if (infd < 0)
		err(1, argv[1]);

	outfd = open(argv[2], O_WRONLY | O_TRUNC | O_CREAT, 0644);
	if (outfd < 0)
		err(1, argv[2]);

	n = read(infd, &head, sizeof(head));
	if (n < sizeof(head))
		err(1, "read");

	if (N_BADMAG(head)) {
		printf("%s: bad magic number\n", argv[1]);
		exit(1);
	}

	if (head.a_trsize || head.a_drsize) {
		printf("%s: has relocations\n", argv[1]);
		exit(1);
	}

	/*
	 * Header
	 */

	ehead.f.f_magic = 0x016d;		/* MC88OMAGIC */
	ehead.f.f_nscns = 3;
	ehead.f.f_timdat = 0;			/* ignored */
	ehead.f.f_symptr = 0;			/* ignored */
	ehead.f.f_nsyms = 0;			/* ignored */
	ehead.f.f_opthdr = sizeof ehead.a;
	ehead.f.f_flags = 0x020f;
		/* F_RELFLG | F_EXEC | F_LNNO | 8 | F_AR16WR */

	ehead.a.magic = N_GETMAGIC(head);
	ehead.a.vstamp = 0;			/* ignored */
	ehead.a.tsize = head.a_text;		/* ignored */
	ehead.a.dsize = head.a_data;		/* ignored */
	ehead.a.bsize = head.a_bss;		/* ignored */
	ehead.a.entry = head.a_entry;
	ehead.a.text_start = N_TXTADDR(head);	/* ignored */
	ehead.a.data_start = N_DATADDR(head);	/* ignored */

	n = write(outfd, &ehead, sizeof(ehead));
	if (n != sizeof(ehead))
		err(1, "write");

	/*
	 * Sections.
	 * Note that we merge .bss into .data since the PROM will not
	 * clear it and locore does not do this either.
	 */

	strncpy(escn[0].s_name, ".text", sizeof escn[0].s_name);
	escn[0].s_paddr = N_TXTADDR(head);	/* ignored, 1:1 mapping */
	escn[0].s_size = round(head.a_text, 8);
	escn[0].s_scnptr = round(sizeof(ehead) + sizeof(escn), 0x10);
	escn[0].s_relptr = 0;
	escn[0].s_lnnoptr = 0;
	escn[0].s_nlnno = 0;
	escn[0].s_flags = 0x20;	/* STYP_TEXT */

	strncpy(escn[1].s_name, ".data", sizeof escn[1].s_name);
	escn[1].s_paddr = N_DATADDR(head);		/* ignored, 1:1 mapping */
	escn[1].s_scnptr = escn[0].s_scnptr + escn[0].s_size;
	escn[1].s_size = round(head.a_data + head.a_bss, 8);
	escn[1].s_relptr = 0;
	escn[1].s_lnnoptr = 0;
	escn[1].s_nlnno = 0;
	escn[1].s_flags = 0x40;	/* STYP_DATA */

	strncpy(escn[2].s_name, ".bss", sizeof escn[2].s_name);
	escn[2].s_paddr = N_BSSADDR(head) + head.a_bss;	/* ignored, 1:1 mapping */
	escn[2].s_scnptr = 0;		/* nothing in the file */
	escn[2].s_size = 0;
	escn[2].s_relptr = 0;
	escn[2].s_lnnoptr = 0;
	escn[2].s_nlnno = 0;
	escn[2].s_flags = 0x80;	/* STYP_BSS */

	/* adjust load addresses */
	escn[0].s_paddr += (head.a_entry & ~(__LDPGSZ - 1)) - __LDPGSZ;
	escn[1].s_paddr += (head.a_entry & ~(__LDPGSZ - 1)) - __LDPGSZ;
	escn[2].s_paddr += (head.a_entry & ~(__LDPGSZ - 1)) - __LDPGSZ;
	escn[0].s_vaddr = escn[0].s_paddr;
	escn[1].s_vaddr = escn[1].s_paddr;
	escn[2].s_vaddr = escn[2].s_paddr;

	n = write(outfd, &escn, sizeof(escn));
	if (n != sizeof(escn))
		err(1, "write");

	/*
	 * Copy text section
	 */

#ifdef DEBUG
	printf("copying %s: source %lx dest %lx size %x\n",
	    escn[0].s_name, N_TXTOFF(head), escn[0].s_scnptr, head.a_text);
#endif
	lseek(outfd, escn[0].s_scnptr, SEEK_SET);
	lseek(infd, N_TXTOFF(head), SEEK_SET);
	copybits(infd, outfd, head.a_text);

	/*
	 * Copy data section
	 */

#ifdef DEBUG
	printf("copying %s: source %lx dest %lx size %x\n",
	    escn[1].s_name, N_DATOFF(head), escn[1].s_scnptr, head.a_data);
#endif
	lseek(outfd, escn[1].s_scnptr, SEEK_SET);
	lseek(infd, N_DATOFF(head), SEEK_SET);
	copybits(infd, outfd, head.a_data);

	/*
	 * ``Copy'' bss section
	 */

#ifdef DEBUG
	printf("copying %s: size %lx\n",
	    escn[2].s_name, round(head.a_data + head.a_bss, 8) - head.a_data);
#endif
	zerobits(outfd, round(head.a_data + head.a_bss, 8) - head.a_data);

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

	bzero(buf, sizeof buf);
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
