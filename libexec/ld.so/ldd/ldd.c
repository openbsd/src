/*	$OpenBSD: ldd.c,v 1.1 2000/09/17 17:50:57 deraadt Exp $ */

/*
 * Copyright (c) 1993 Paul Kranenburg
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
 *      This product includes software developed by Paul Kranenburg.
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
/*
 * Copyright (c) 1996 Per Fogelstrom
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
 *	This product includes software developed under OpenBSD by
 *	Per Fogelstrom.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */


/* readsoname() adapted from Eric Youngdale's readelf program */


#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <err.h>
#include <fcntl.h>
#include <elf_abi.h>

int readsoneeded(FILE *f, int flag);

void
usage()
{
	extern char *__progname;

	fprintf(stderr, "Usage: %s <filename> ...\n", __progname);
	exit(1);
}

int
main(argc, argv)
	int argc;
	char **argv;
{
	FILE *fp;
	int lflag = 0;
	int rval;
	int c;

	while ((c = getopt(argc, argv, "l")) != EOF) {
		switch (c) {
		case 'l':
			lflag = 1;
			break;
		default:
			usage();
			/*NOTREACHED*/
		}
	}

	argc -= optind;
	argv += optind;

	if (argc <= 0) {
		usage();
		/*NOTREACHED*/
	}

	if (setenv("LD_TRACE_LOADED_OBJECTS", "1", 1) == -1)
		errx(1, "cannot setenv LD_TRACE_LOADED_OBJECTS");

	rval = 0;
	while (argc--) {
		int     fd;
		int     status;

		if (lflag) {
			if ((fp = fopen(*argv, "r")) == NULL) {
				warn("%s", *argv);
				rval |= 1;
				argv++;
				continue;
			}
			readsoneeded(fp, 0);
			fclose(fp);
			continue;
		}
		printf("%s:\n", *argv);
		fflush(stdout);

		switch (fork()) {
		case -1:
			err(1, "fork");
			break;
		default:
			if (wait(&status) <= 0) {
				warn("wait");
				rval |= 1;
			} else if (WIFSIGNALED(status)) {
				fprintf(stderr, "%s: signal %d\n",
						*argv, WTERMSIG(status));
				rval |= 1;
			} else if (WIFEXITED(status) && WEXITSTATUS(status)) {
				fprintf(stderr, "%s: exit status %d\n",
						*argv, WEXITSTATUS(status));
				rval |= 1;
			}
			break;
		case 0:
			rval |= execl(*argv, *argv, NULL) != 0;
			perror(*argv);
			_exit(1);
		}
		argv++;
	}

	return (rval ? 1 : 0);
}

int
readsoneeded(FILE *infile, int dyncheck)
{
	Elf32_Ehdr *epnt;
	Elf32_Phdr *ppnt;
	int i;
	int isdynamic = 0;
	char *header;
	unsigned int dynamic_addr = 0;
	unsigned int dynamic_size = 0;
	int strtab_val = 0;
	int soname_val = 0;
	int loadaddr = -1;
	int loadbase = 0;
	Elf32_Dyn *dpnt;
	struct stat st;
	char *res = NULL;

	if (fstat(fileno(infile), &st))
		return -1L;
	header = mmap(0, st.st_size, PROT_READ, MAP_SHARED, fileno(infile), 0);
	if (header == (caddr_t)-1)
		return -1;

	epnt = (Elf32_Ehdr *)header;
	if ((int)(epnt+1) > (int)(header + st.st_size))
		goto skip;

	ppnt = (Elf32_Phdr *)&header[epnt->e_phoff];
	if ((int)ppnt < (int)header ||
	    (int)(ppnt+epnt->e_phnum) > (int)(header + st.st_size))
		goto skip;

	for (i = 0; i < epnt->e_phnum; i++) {
		if (loadaddr == -1 && ppnt->p_vaddr != 0) 
			loadaddr = (ppnt->p_vaddr & 0xfffff000) -
			    (ppnt->p_offset & 0xfffff000);
		if (ppnt->p_type == 2) {
			dynamic_addr = ppnt->p_offset;
			dynamic_size = ppnt->p_filesz;
		}
		ppnt++;
	}
		
	dpnt = (Elf32_Dyn *) &header[dynamic_addr];
	dynamic_size = dynamic_size / sizeof(Elf32_Dyn);
	if ((int)dpnt < (int)header ||
	    (int)(dpnt+dynamic_size) > (int)(header + st.st_size))
		goto skip;
	
	while (dpnt->d_tag != DT_NULL) {
		if (dpnt->d_tag == DT_STRTAB)
			strtab_val = dpnt->d_un.d_val;
#define DT_MIPS_BASE_ADDRESS    0x70000006      /* XXX */
		if (dpnt->d_tag == DT_MIPS_BASE_ADDRESS)
			loadbase = dpnt->d_un.d_val;
		dpnt++;
	}

	if (!strtab_val)
		goto skip;

	dpnt = (Elf32_Dyn *) &header[dynamic_addr];
	while (dpnt->d_tag != DT_NULL) {
		if (dpnt->d_tag == DT_NEEDED) {
			isdynamic = 1;
			if (dyncheck)
				break;
			soname_val = dpnt->d_un.d_val;
			if (soname_val != 0 &&
			    soname_val + strtab_val - loadbase >= 0 &&
			    soname_val + strtab_val - loadbase < st.st_size)
				printf("%s\n",
				    header - loadbase + soname_val + strtab_val);
		}
		dpnt++;
	}

skip:
	munmap(header, st.st_size);

	return isdynamic;
}

