/*	$OpenBSD: rdsetroot.c,v 1.1 1998/11/09 06:16:05 millert Exp $	*/
/*	$NetBSD: rdsetroot.c,v 1.2 1995/10/13 16:38:39 gwr Exp $	*/

/*
 * Copyright (c) 1994 Gordon W. Ross
 * All rights reserved.
 *
 * ELF modifications Copyright (c) 1997 Per Fogelstrom.
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
#include <sys/file.h>
#include <sys/mman.h>

#include <stdio.h>
#include <nlist.h>
#include <sys/exec_elf.h>

extern off_t lseek();

char *file;

/* Virtual addresses of the symbols we frob. */
long rd_root_image_va, rd_root_size_va;

/* Offsets relative to start of data segment. */
long rd_root_image_off, rd_root_size_off;

/* value in the location at rd_root_size_off */
int rd_root_size_val;

/* pointers to pieces of mapped file */
char *dataseg;

/* parameters to mmap digged out from program header */
int mmap_offs;
int mmap_size;

main(argc,argv)
	char **argv;
{
	int fd, n;
	int found;
	int *ip;
	char *cp;
	Elf32_Ehdr eh;
	Elf32_Phdr *ph;
	int phsize;

	if (argc < 2) {
		printf("%s: missing file name\n", argv[0]);
		exit(1);
	}
	file = argv[1];

	fd = open(file, O_RDWR);
	if (fd < 0) {
		perror(file);
		exit(1);
	}

	n = read(fd, &eh, sizeof(eh));
	if (n < sizeof(eh)) {
		printf("%s: reading header\n", file);
		exit(1);
	}

	if (!IS_ELF(eh)) {
		printf("%s: not elf\n", file);
		exit(1);
	}

	phsize = eh.e_phnum * sizeof(Elf32_Phdr);
	ph = (Elf32_Phdr *)malloc(phsize);
	lseek(fd,  eh.e_phoff, 0);
	if(read(fd, (char *)ph, phsize) != phsize) {
		printf("%s: can't read phdr area\n", file);
		exit(1);
	}
	found = 0;
	for(n = 0; n < eh.e_phnum && !found; n++) {
		if(ph[n].p_type == PT_LOAD) {
			found = find_rd_root_image(file, &eh, &ph[n]);
		}
	}
	if(!found) {
		printf("%s: can't locate space for rd_root_image!", file);
		exit(1);
	}

	/*
	 * Map in the whole data segment.
	 * The file offset needs to be page aligned.
	 */
	dataseg = mmap(NULL,	/* any address is ok */
				   mmap_size, /* length */
				   PROT_READ | PROT_WRITE,
				   MAP_SHARED,
				   fd, mmap_offs);
	if ((long)dataseg == -1) {
		printf("%s: can not map data seg\n", file);
		perror(file);
		exit(1);
	}

	/*
	 * Find value in the location: rd_root_size
	 */
	ip = (int*) (dataseg + rd_root_size_off);
	rd_root_size_val = *ip;
#ifdef	DEBUG
	printf("rd_root_size  val: 0x%08X (%d blocks)\n",
		rd_root_size_val, (rd_root_size_val >> 9));
#endif

	/*
	 * Copy the symbol table and string table.
	 */
#ifdef	DEBUG
	printf("copying root image...\n");
#endif
	n = read(0, dataseg + rd_root_image_off,
			 rd_root_size_val);
	if (n < 0) {
		perror("read");
		exit(1);
	}

	msync(dataseg, mmap_size, 0);

#ifdef	DEBUG
	printf("...copied %d bytes\n", n);
#endif
	close(fd);
	exit(0);
}


/*
 * Find locations of the symbols to patch.
 */
struct nlist wantsyms[] = {
	{ "_rd_root_size", 0 },
	{ "_rd_root_image", 0 },
	{ NULL, 0 },
};

int
find_rd_root_image(file, eh, ph)
	char *file;
	Elf32_Ehdr *eh;
	Elf32_Phdr *ph;
{
	int data_va;
	int std_entry;
	int kernel_start;
	int kernel_size;

	if (nlist(file, wantsyms)) {
		printf("%s: no rd_root_image symbols?\n", file);
		exit(1);
	}
	kernel_start = ph->p_paddr;
	kernel_size = ph->p_filesz;

	rd_root_size_off = wantsyms[0].n_value - kernel_start;
	rd_root_image_off     = wantsyms[1].n_value - kernel_start;

#ifdef DEBUG
	printf("rd_root_size_off = 0x%x\n", rd_root_size_off);
	printf("rd_root_image_off = 0x%x\n", rd_root_image_off);
#endif

	/*
	 * Sanity check locations of db_* symbols
	 */
	if (rd_root_image_off < 0 || rd_root_image_off >= kernel_size) {
		return(0);
	}
	if (rd_root_size_off < 0 || rd_root_size_off >= kernel_size) {
		printf("%s: rd_root_size not in data segment?\n", file);
		return(0);
	}
	mmap_offs = ph->p_offset;
	mmap_size =  kernel_size;
	return(1);
}
