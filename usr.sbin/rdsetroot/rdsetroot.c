/*	$OpenBSD: rdsetroot.c,v 1.1 2019/04/05 21:07:11 deraadt Exp $	*/

/*
 * Copyright (c) 1994 Gordon W. Ross
 * Copyright (c) 1997 Per Fogelstrom. (ELF modifications)
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copy a ramdisk image into the space reserved for it.
 * Kernel variables: rd_root_size, rd_root_image
 */

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <elf.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <err.h>

#include "rdsetroot.h"

struct elfhdr head;

/* Offsets relative to start of data segment. */
long	rd_root_image_off, rd_root_size_off;

/* value in the location at rd_root_size_off */
off_t	rd_root_size_val;

/* pointers to pieces of mapped file */
char	*dataseg;

/* parameters to mmap digged out from program header */
off_t	mmap_off;
size_t	mmap_size;

__dead void usage(void);

int	debug;

struct elf_fn *elf_fn;

int
main(int argc, char *argv[])
{
	int ch, kfd, n, xflag = 0, fsfd;
	char *fs = NULL;
	char *kernel;
	u_int32_t *ip;

	while ((ch = getopt(argc, argv, "dx")) != -1) {
		switch (ch) {
		case 'd':
			debug = 1;
			break;
		case 'x':
			xflag = 1;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc == 1)
		kernel = argv[0];
	else if (argc == 2) {
		kernel = argv[0];
		fs = argv[1];
	} else
		usage();

	kfd = open(kernel, xflag ? O_RDONLY : O_RDWR, 0644);
	if (kfd < 0)
		err(1, "%s", kernel);

	if (fs) {
		if (xflag)
			fsfd = open(fs, O_RDWR | O_CREAT | O_TRUNC, 0644);
		else
			fsfd = open(fs, O_RDONLY, 0644);
	} else {
		if (xflag)
			fsfd = dup(STDOUT_FILENO);
		else
			fsfd = dup(STDIN_FILENO);
	}
	if (fsfd < 0)
		err(1, "%s", fs);

	if (pledge("stdio", NULL) == -1)
		err(1, "pledge");

	n = read(kfd, &head, sizeof(head));
	if (n < sizeof(head))
		err(1, "%s: reading header", kernel);

	if (!IS_ELF(head))
		err(1, "%s: bad magic number", kernel);

	if (head.e_ident[EI_CLASS] == ELFCLASS32) {
		elf_fn = &ELF32_fn;
	} else if (head.e_ident[EI_CLASS] == ELFCLASS64) {
		elf_fn = &ELF64_fn;
	} else {
		fprintf(stderr, "%s: invalid elf, not 32 or 64 bit", kernel);
		exit(1);
	}

	elf_fn->locate_image(kfd, &head, kernel, &rd_root_size_off,
	    &rd_root_image_off, &mmap_off, &mmap_size);

	/*
	 * Map in the whole data segment.
	 * The file offset needs to be page aligned.
	 */
	dataseg = mmap(NULL, mmap_size,
	    xflag ? PROT_READ : PROT_READ | PROT_WRITE,
	    MAP_SHARED, kfd, mmap_off);
	if (dataseg == MAP_FAILED)
		err(1, "%s: can not map data seg", kernel);

	/*
	 * Find value in the location: rd_root_size
	 */
	ip = (u_int32_t *) (dataseg + rd_root_size_off);
	rd_root_size_val = *ip;
	if (debug)
		fprintf(stderr, "rd_root_size  val: 0x%llx (%lld blocks)\n",
		    (unsigned long long)rd_root_size_val,
		    (unsigned long long)rd_root_size_val >> 9);

	/*
	 * Copy the symbol table and string table.
	 */
	if (debug)
		fprintf(stderr, "copying root image...\n");

	if (xflag) {
		n = write(fsfd, dataseg + rd_root_image_off,
		    (size_t)rd_root_size_val);
		if (n != rd_root_size_val)
			err(1, "write");
	} else {
		struct stat sstat;

		if (fstat(fsfd, &sstat) == -1)
			err(1, "fstat");
		if (S_ISREG(sstat.st_mode) &&
		    sstat.st_size > rd_root_size_val) {
			fprintf(stderr, "ramdisk too small 0x%llx 0x%llx\n",
			    (unsigned long long)sstat.st_size,
			    (unsigned long long)rd_root_size_val);
			exit(1);
		}
		n = read(fsfd, dataseg + rd_root_image_off,
		    (size_t)rd_root_size_val);
		if (n < 0)
			err(1, "read");

		msync(dataseg, mmap_size, 0);
	}

	if (debug)
		fprintf(stderr, "...copied %d bytes\n", n);
	exit(0);
}

__dead void
usage(void)
{
	extern char *__progname;

	fprintf(stderr, "usage: %s [-dx] bsd [fs]\n", __progname);
	exit(1);
}
