/*	$OpenBSD: inject.c,v 1.4 2000/03/01 22:10:07 todd Exp $	*/
/*
 * Copyright (c) 1995 Matthias Pfaller.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Matthias Pfaller.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <bm.h>

/*
 * Map a file
 */

void *map(char *file, int mode, int *len)
{
	int fd;
	struct stat sb;
	void *p;

	/* Open the file we'd like map */
	fd = open(file, mode);
	if (fd < 0) {
		perror(file);
		exit(1);
	}

	/* Get the length of the file */
	if (fstat(fd, &sb) < 0) {
		perror("fstat");
		exit(1);
	}

	/* Return the length of file in len */
	*len = sb.st_size;

	/* Now map the file */
	p = mmap(NULL, *len, PROT_READ | (mode == O_RDWR ? PROT_WRITE : 0),
			MAP_SHARED, fd, 0);
	if (p == NULL) {
		perror("mmap");
		exit(1);
	}

	/*
	 * We will access this mostly sequential.
	 * So let's tell it to the vm system.
	 */
	madvise(p, *len, MADV_SEQUENTIAL);
	close(fd);
	return(p);
}

main(int argc, char **argv)
{
	void *kern, *filesys, *ramdisk;
	int kernlen, filesyslen;
	bm_pat *bm;
	static char pattern[] = "Ramdiskorigin";

	if (argc != 3) {
		fprintf(stderr, "usage: %s kernel filesystem", argv[0]);
		exit(1);
	}

	/* Map the kernel image read/write */
	kern = map(argv[1], O_RDWR, &kernlen);

	/* Map the filesystem image read only */
	filesys = map(argv[2], O_RDONLY, &filesyslen);

	/* Search the kernel image for the ramdisk signature */
	bm = bm_comp(pattern, sizeof(pattern), NULL);
	ramdisk = bm_exec(bm, kern, kernlen);
	if (!ramdisk) {
		fprintf(stderr, "Origin of ramdisk not found in kernel\n");
		exit(1);
	}

	/* Does the filesystem image fit into the kernel image? */
	if ((kernlen - (ramdisk - kern)) < filesyslen) {
		fprintf(stderr, "Kernel image to small\n");
		exit(1);
	}

	/* Copy the filesystem image into the kernel image */
	memcpy(ramdisk, filesys, filesyslen);

	/* Sync vm/fs and unmap the images */
	msync(kern, kernlen, 0);
	munmap(kern, kernlen);
	munmap(filesys, filesyslen);
	exit(0);
}
