/*	$OpenBSD: headersize.c,v 1.9 2008/01/29 13:02:31 krw Exp $	*/
/*	$NetBSD: headersize.c,v 1.5 1996/09/23 04:32:59 cgd Exp $	*/

/*
 * Copyright (c) 1995, 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#define	ELFSIZE		64

#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/exec.h>
#include <sys/exec_ecoff.h>
#include <sys/exec_elf.h>

#include <unistd.h>
#include <stdio.h>
#include <err.h>

#define	HDR_BUFSIZE	512

int
main(argc, argv)
	int argc;
	char *argv[];
{
	char buf[HDR_BUFSIZE], *fname;
#ifdef ALPHA_BOOT_ELF
	Elf64_Ehdr *elfp;
#endif
	int fd;
	unsigned long loadaddr;

	if (argc != 3)
		errx(1, "must be given two arguments (load addr, file name)");
	if (sscanf(argv[1], "%lx", &loadaddr) != 1)
		errx(1, "load addr argument (%s) not valid", argv[1]);
	fname = argv[2];

	if ((fd = open(fname, O_RDONLY, 0)) == -1)
		err(1, "%s: open failed", fname);

	if (read(fd, &buf, HDR_BUFSIZE) != HDR_BUFSIZE)
		err(1, "%s: read failed", fname);
#ifdef ALPHA_BOOT_ELF
	elfp = (Elf64_Ehdr *)buf;
#endif

#ifdef ALPHA_BOOT_ELF
	if (memcmp(ELFMAG, elfp->e_ident, SELFMAG) == 0) {
		Elf64_Phdr phdr;

		/* XXX assume the first segment is the one we want */
		if (lseek(fd, elfp->e_phoff, SEEK_SET) == -1)
			err(1, "%s: lseek phdr failed", fname);
		if (read(fd, (void *)&phdr, sizeof(phdr)) != sizeof(phdr))
			err(1, "%s: read phdr failed", fname);

		printf("%ld\n", phdr.p_offset + (loadaddr - phdr.p_vaddr));
	}
	else
#endif
		errx(1, "%s: bad magic number", fname);

	close(fd);
	exit(0);
}
