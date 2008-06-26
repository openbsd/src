/*	$NetBSD: mmap.c,v 1.12 2001/02/19 22:44:41 cgd Exp $	*/

/*-
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Test various memory mapping facilities.
 */

#include <sys/param.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>	/* for memset declaration (?) */

#include <machine/vmparam.h>	/* SHMMAXPGS */

int	main(int, char *[]);
void	usage(void);

int	check_residency(void *, int);

int	pgsize;
int	verbose;

#define	MAPPED_FILE	"mapped_file"
#define	TEST_PATTERN	0xa5

int
main(argc, argv)
	int argc;
	char *argv[];
{
	struct stat st;
	void *addr, *addr2, *addr3;
	int i, ch, ecode, fd, npgs, shmid;
	const char *filename;
	u_int8_t *cp;

	while ((ch = getopt(argc, argv, "v")) != -1) {
		switch (ch) {
		case 'v':
			verbose = 1;
			break;

		default:
			usage();
		}
	}
	argv += optind;
	argc -= optind;

	if (argc > 0)
		filename = argv[0];
	else
		filename = "/usr/share/dict/words";

	ecode = 0;

	pgsize = sysconf(_SC_PAGESIZE);

	/*
	 * TEST MLOCKING A FILE.
	 */

	printf(">>> MAPPING FILE <<<\n");

	fd = open(filename, O_RDONLY, 0666);
	if (fd == -1)
		err(1, "open %s", filename);

	if (fstat(fd, &st) == -1)
		err(1, "fstat %s", filename);

	addr = mmap(NULL, (size_t) st.st_size, PROT_READ, MAP_FILE|MAP_SHARED,
	    fd, (off_t) 0);
	if (addr == MAP_FAILED)
		err(1, "mmap %s", filename);

	(void) close(fd);

	npgs = (st.st_size / pgsize) + 1;

	printf("    CHECKING RESIDENCY\n");

	(void) check_residency(addr, npgs);

	printf("    LOCKING RANGE\n");

	if (mlock(addr, npgs * pgsize) == -1)
		err(1, "mlock %s", filename);

	printf("    CHECKING RESIDENCY\n");

	if (check_residency(addr, npgs) != npgs) {
		printf("    RESIDENCY CHECK FAILED!\n");
		ecode = 1;
	}

	printf("    UNLOCKING RANGE\n");

	if (munlock(addr, pgsize) == -1)
		err(1, "munlock %s", filename);

	(void) munmap(addr, st.st_size);

	/*
	 * TEST MLOCKALL'ING AN ANONYMOUS MEMORY RANGE.
	 */

	npgs = 128;
	if (npgs > SHMMAXPGS)
		npgs = SHMMAXPGS;

	printf(">>> MAPPING %d PAGE ANONYMOUS REGION <<<\n", npgs);

	addr = mmap(NULL, npgs * pgsize, PROT_READ|PROT_WRITE,
	    MAP_ANON|MAP_PRIVATE, -1, (off_t) 0);
	if (addr == MAP_FAILED)
		err(1, "mmap anon #1");

	printf("    CHECKING RESIDENCY\n");

	if (check_residency(addr, npgs) != 0) {
		printf("    RESIDENCY CHECK FAILED!\n");
		ecode = 1;
	}

	printf("    LOCKING ALL - CURRENT and FUTURE\n");

	if (mlockall(MCL_CURRENT|MCL_FUTURE) == -1)
		err(1, "mlockall current/future");

	printf("    CHECKING RESIDENCY\n");

	if (check_residency(addr, npgs) != npgs) {
		printf("    RESIDENCY CHECK FAILED!\n");
		ecode = 1;
	}

	printf(">>> MAPPING ANOTHER %d PAGE ANONYMOUS REGION <<<\n", npgs);

	addr2 = mmap(NULL, npgs * pgsize, PROT_READ, MAP_ANON, -1, (off_t) 0);
	if (addr2 == MAP_FAILED)
		err(1, "mmap anon #2");

	printf("    CHECKING RESIDENCY\n");

	if (check_residency(addr2, npgs) != npgs) {
		printf("    RESIDENCY CHECK FAILED!\n");
		ecode = 1;
	}

	printf(">>> MAPPING THIRD %d PAGE ANONYMOUS REGION, PROT_NONE <<<\n",
	    npgs);

	addr3 = mmap(NULL, npgs * pgsize, PROT_NONE, MAP_ANON, -1, (off_t) 0);
	if (addr3 == MAP_FAILED)
		err(1, "mmap anon #3");

	printf("    CHECKING RESIDENCY\n");

	if (check_residency(addr3, npgs) != 0) {
		printf("    RESIDENCY CHECK FAILED!\n");
		ecode = 1;
	}

	printf("    PROT_READ'ING MAPPING\n");

	if (mprotect(addr3, npgs * pgsize, PROT_READ) == -1)
		err(1, "mprotect");

	printf("    CHECKING RESIDENCY\n");

	if (check_residency(addr3, npgs) != npgs) {
		printf("    RESIDENCY CHECK FAILED!\n");
		ecode = 1;
	}

	printf("    UNLOCKING ALL\n");

	printf("    CHECKING RESIDENCY\n");

	if (check_residency(addr, npgs) != npgs ||
	    check_residency(addr2, npgs) != npgs) {
		printf("    RESIDENCY CHECK FAILED!\n");
		ecode = 1;
	}

	(void) munlockall();

	printf(">>> MADV_FREE'ING SECOND ANONYMOUS REGION <<<\n");

	if (madvise(addr2, npgs * pgsize, MADV_FREE) == -1)
		err(1, "madvise");

	printf("    CHECKING RESIDENCY\n");

	if (check_residency(addr2, npgs) != 0) {
		printf("    RESIDENCY CHECK FAILED!\n");
		ecode = 1;
	}

	printf(">>> MADV_FREE'ING FIRST ANONYMOUS REGION <<<\n");

	if (madvise(addr, npgs * pgsize, MADV_FREE) == -1)
		err(1, "madvise");

	printf("    CHECKING RESIDENCY\n");

	if (check_residency(addr, npgs) != 0) {
		printf("    RESIDENCY CHECK FAILED!\n");
		ecode = 1;
	}

	printf(">>> ZEROING FIRST ANONYMOUS REGION <<<\n");

	memset(addr, 0, npgs * pgsize);

	printf("    CHECKING RESIDENCY\n");

	if (check_residency(addr, npgs) != npgs) {
		printf("    RESIDENCY CHECK FAILED!\n");
		ecode = 1;
	}

	printf(">>> MADV_FREE'ING FIRST ANONYMOUS REGION AGAIN <<<\n");

	if (madvise(addr, npgs * pgsize, MADV_FREE) == -1)
		err(1, "madvise");

	printf("    CHECKING RESIDENCY\n");

	if (check_residency(addr2, npgs) != 0) {
		printf("    RESIDENCY CHECK FAILED!\n");
		ecode = 1;
	}

	printf(">>> UNMAPPING ANONYMOUS REGIONS <<<\n");

	(void) munmap(addr, npgs * pgsize);
	(void) munmap(addr2, npgs * pgsize);
	(void) munmap(addr3, npgs * pgsize);

	printf(">>> CREATING MAPPED FILE <<<\n");

	(void) unlink(MAPPED_FILE);

	if ((fd = open(MAPPED_FILE, O_RDWR|O_CREAT|O_TRUNC, 0666)) == -1)
		err(1, "open %s", MAPPED_FILE);

	if ((cp = malloc(npgs * pgsize)) == NULL)
		err(1, "malloc %d bytes", npgs * pgsize);

	memset(cp, 0x01, npgs * pgsize);

	if (write(fd, cp, npgs * pgsize) != npgs * pgsize)
		err(1, "write %s", MAPPED_FILE);

	addr = mmap(NULL, npgs * pgsize, PROT_READ|PROT_WRITE,
	    MAP_FILE|MAP_SHARED, fd, (off_t) 0);
	if (addr == MAP_FAILED)
		err(1, "mmap %s", MAPPED_FILE);

	(void) close(fd);

	printf("    WRITING TEST PATTERN\n");

	for (i = 0; i < npgs * pgsize; i++)
		((u_int8_t *)addr)[i] = TEST_PATTERN;

	printf("    SYNCING FILE\n");

	if (msync(addr, npgs * pgsize, MS_SYNC|MS_INVALIDATE) == -1)
		err(1, "msync %s", MAPPED_FILE);

	printf("    UNMAPPING FILE\n");

	(void) munmap(addr, npgs * pgsize);

	printf("    READING FILE\n");

	if ((fd = open(MAPPED_FILE, O_RDONLY, 0666)) == -1)
		err(1, "open %s", MAPPED_FILE);

	if (read(fd, cp, npgs * pgsize) != npgs * pgsize)
		err(1, "read %s", MAPPED_FILE);

	(void) close(fd);

	printf("    CHECKING TEST PATTERN\n");

	for (i = 0; i < npgs * pgsize; i++) {
		if (cp[i] != TEST_PATTERN) {
			printf("    INCORRECT BYTE AT OFFSET %d: "
			    "0x%02x should be 0x%02x\n", i, cp[i],
			    TEST_PATTERN);
			ecode = 1;
			break;
		}
	}

	printf(">>> CREATING SYSV SHM SEGMENT <<<\n");

	if ((shmid = shmget(IPC_PRIVATE, npgs * pgsize,
	    IPC_CREAT|S_IRUSR|S_IWUSR)) == -1)
		err(1, "shmget");

	if ((addr = shmat(shmid, NULL, 0)) == (void *) -1)
		err(1, "shmat");

	printf("    CHECKING RESIDENCY\n");

	if (check_residency(addr, npgs) != 0) {
		printf("    RESIDENCY CHECK FAILED!\n");
		ecode = 1;
	}

	printf("    ZEROING SEGMENT\n");

	memset(addr, 0xff, npgs * pgsize);

	printf("    CHECKING RESIDENCY\n");

	if (check_residency(addr, npgs) != npgs) {
		printf("    RESIDENCY CHECK FAILED!\n");
		ecode = 1;
	}

	printf("    MADV_FREE'ING SEGMENT\n");
	if (madvise(addr, npgs * pgsize, MADV_FREE) == -1)
		err(1, "madvise");

	printf("    CHECKING RESIDENCY\n");

	/*
	 * NOTE!  Even though we have MADV_FREE'd the range,
	 * there is another reference (the kernel's) to the
	 * object which owns the pages.  In this case, the
	 * kernel does not simply free the pages, as haphazardly
	 * freeing pages when there are still references to
	 * an object can cause data corruption (say, the other
	 * referencer doesn't expect the pages to be freed,
	 * and is surprised by the subsequent ZFOD).
	 *
	 * Because of this, we simply report the number of
	 * pages still resident, for information only.
	 */

	npgs = check_residency(addr, npgs);
	printf("    RESIDENCY CHECK: %d pages still resident\n", npgs);

	if (shmdt(addr) == -1)
		warn("shmdt");
	if (shmctl(shmid, IPC_RMID, NULL) == -1)
		err(1, "shmctl");

	exit(ecode);
}

int
check_residency(addr, npgs)
	void *addr;
	int npgs;
{
	char *vec;
	int i, resident;

	vec = malloc(npgs);
	if (vec == NULL)
		err(1, "malloc mincore vec");

	if (mincore(addr, npgs * pgsize, vec) == -1)
		err(1, "mincore");

	for (i = 0, resident = 0; i < npgs; i++) {
		if (vec[i] != 0)
			resident++;
		if (verbose)
			printf("page 0x%lx is %sresident\n",
			    addr + (i * pgsize), vec[i] ? "" : "not ");
	}

	free(vec);

	return (resident);
}

void
usage()
{

	fprintf(stderr, "usage: mmap [-v] filename\n");
	exit(1);
}
