/*	$OpenBSD: elfrdsetroot.c,v 1.10 2005/01/14 22:47:06 deraadt Exp $	*/
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
#include <stdlib.h>
#include <unistd.h>
#include <nlist.h>

#ifdef __LP64__
#define ELFSIZE 64
#else
#define ELFSIZE 32
#endif

#include <sys/exec_elf.h>

char *file;

/* Virtual addresses of the symbols we frob. */
long	rd_root_image_va, rd_root_size_va;

/* Offsets relative to start of data segment. */
long	rd_root_image_off, rd_root_size_off;

/* value in the location at rd_root_size_off */
off_t	rd_root_size_val;

/* pointers to pieces of mapped file */
char *dataseg;

/* parameters to mmap digged out from program header */
off_t	mmap_offs;
size_t	mmap_size;

int	find_rd_root_image(char *, Elf_Ehdr *, Elf_Phdr *, int);
__dead void usage(void);

int
main(int argc, char **argv)
{
	int ch, fd, n, xflag = 0, found = 0, phsize;
	u_int32_t *ip;
	Elf_Ehdr eh;
	Elf_Phdr *ph;

	while ((ch = getopt(argc, argv, "x")) != -1) {
		switch (ch) {
		case 'x':
			xflag = 1;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 1)
		usage();
	file = argv[0];

	fd = open(file, O_RDWR, 0644);
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

	phsize = eh.e_phnum * sizeof(Elf_Phdr);
	ph = (Elf_Phdr *)malloc(phsize);
	lseek(fd, eh.e_phoff, 0);
	if (read(fd, (char *)ph, phsize) != phsize) {
		printf("%s: can't read phdr area\n", file);
		exit(1);
	}

	for (n = 0; n < eh.e_phnum && !found; n++) {
		if (ph[n].p_type == PT_LOAD)
			found = find_rd_root_image(file, &eh, &ph[n], n);
	}
	if (!found) {
		printf("%s: can't locate space for rd_root_image!\n", file);
		exit(1);
	}

	/*
	 * Map in the whole data segment.
	 * The file offset needs to be page aligned.
	 */
	dataseg = mmap(NULL, mmap_size, PROT_READ | PROT_WRITE,
	    MAP_SHARED, fd, mmap_offs);
	if (dataseg == MAP_FAILED) {
		printf("%s: can not map data seg\n", file);
		perror(file);
		exit(1);
	}

	/*
	 * Find value in the location: rd_root_size
	 */
	ip = (u_int32_t*) (dataseg + rd_root_size_off);
	rd_root_size_val = *ip;
#ifdef	DEBUG
	printf("rd_root_size  val: 0x%08X (%d blocks)\n",
	    (u_int32_t)rd_root_size_val,
	    (u_int32_t)(rd_root_size_val >> 9));
#endif

	/*
	 * Copy the symbol table and string table.
	 */
#ifdef	DEBUG
	printf("copying root image...\n");
#endif
	if (xflag) {
		n = write(STDOUT_FILENO, dataseg + rd_root_image_off,
		    rd_root_size_val);
		if (n != rd_root_size_val) {
			perror("write");
			exit(1);
		}
	} else {
		n = read(STDIN_FILENO, dataseg + rd_root_image_off,
		    rd_root_size_val);
		if (n < 0) {
			perror("read");
			exit(1);
		}

		msync(dataseg, mmap_size, 0);
	}

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
	{ NULL, 0 }
};

int
find_rd_root_image(char *file, Elf_Ehdr *eh, Elf_Phdr *ph, int segment)
{
	unsigned long kernel_start, kernel_size;

	if (nlist(file, wantsyms)) {
		printf("%s: no rd_root_image symbols?\n", file);
		exit(1);
	}
	kernel_start = ph->p_paddr;
	kernel_size = ph->p_filesz;

	rd_root_size_off	= wantsyms[0].n_value - kernel_start;
	rd_root_size_off	-= (ph->p_vaddr - ph->p_paddr);
	rd_root_image_off	= wantsyms[1].n_value - kernel_start;
	rd_root_image_off	-= (ph->p_vaddr - ph->p_paddr);

#ifdef DEBUG
	printf("segment %d rd_root_size_off = 0x%x\n", segment, rd_root_size_off);
	if ((ph->p_vaddr - ph->p_paddr) != 0)
		printf("root_off v %x p %x, diff %x altered %x\n",
		    ph->p_vaddr, ph->p_paddr,
		    (ph->p_vaddr - ph->p_paddr),
		    rd_root_size_off - (ph->p_vaddr - ph->p_paddr));
	printf("rd_root_image_off = 0x%x\n", rd_root_image_off);
#endif

	/*
	 * Sanity check locations of db_* symbols
	 */
	if (rd_root_image_off < 0 || rd_root_image_off >= kernel_size)
		return(0);
	if (rd_root_size_off < 0 || rd_root_size_off >= kernel_size) {
		printf("%s: rd_root_size not in data segment?\n", file);
		return(0);
	}
	mmap_offs = ph->p_offset;
	mmap_size = kernel_size;
	return(1);
}

__dead void
usage(void)
{
	extern char *__progname;

	fprintf(stderr, "usage: %s [-x] file_name\n", __progname);
	exit(1);
}
