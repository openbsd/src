/*	$NetBSD: rdsetroot.c,v 1.2 1995/10/13 16:38:39 gwr Exp $	*/

/*
 * Copyright (c) 1997 Dale Rahn
 * Copyright (c) 1994 Gordon W. Ross
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
 *
 *	$Id: elfrdsetroot.c,v 1.1 1997/01/31 05:04:44 rahnds Exp $
 */

/*
 * Copy a ramdisk image into the space reserved for it.
 * Kernel variables: rd_root_size, rd_root_image
 */

/*
 * ELF version by Dale Rahn
 */

#include <sys/types.h>
#include <sys/file.h>
#include <sys/mman.h>

#include <sys/stat.h>
#include <stdio.h>
#include <sys/exec_elf.h>
#include <nlist.h>

extern off_t lseek();

char *file;

/* Virtual addresses of the symbols we frob. */
long rd_root_image_va, rd_root_size_va;

/* Offsets relative to start of data segment. */
long rd_root_image_off, rd_root_size_off;

/* value in the location at rd_root_size_off */
int rd_root_size_val;

/* pointers to pieces of mapped file */
char *baseptr;

/* and lengths */
int data_len;
int data_off;
int data_pgoff;



char *pexe;

main(argc,argv)
	char **argv;
{
	int fd, n;
	int *ip;
	char *cp;
	struct stat sb;
	Elf32_Ehdr *pehdr;                    
	Elf32_Shdr *pshdr;                                     
	Elf32_Phdr *pphdr;
	int i;


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

	fstat(fd, &sb);
	pexe = mmap(0, sb.st_size, PROT_READ|PROT_WRITE,
		MAP_FILE | MAP_SHARED, fd, 0);

	pehdr = (Elf32_Ehdr *)pexe;

#ifdef	DEBUG
	printf("elf header\n");
	printf("e_type %x\n", pehdr->e_type);
	printf("e_machine %x\n", pehdr->e_machine);
	printf("e_version %x\n", pehdr->e_version);
	printf("e_entry %x\n", pehdr->e_entry);
	printf("e_phoff %x\n", pehdr->e_phoff);
	printf("e_shoff %x\n", pehdr->e_shoff);
	printf("e_flags %x\n", pehdr->e_flags);
	printf("e_ehsize %x\n", pehdr->e_ehsize);
	printf("e_phentsize %x\n", pehdr->e_phentsize);
	printf("e_phnum %x\n", pehdr->e_phnum);
	printf("e_shentsize %x\n", pehdr->e_shentsize);
	printf("e_shnum %x\n", pehdr->e_shnum);
	printf("e_shstrndx %x\n", pehdr->e_shstrndx);
#endif


	find_rd_root_image(file);


	baseptr = pexe;

	/*
	 * Find value in the location: rd_root_size
	 */
	ip = (int*) (baseptr + rd_root_size_off);
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
	n = read(0, baseptr + rd_root_image_off,
			 rd_root_size_val);
	if (n < 0) {
		perror("read");
		exit(1);
	}

#if 0
	msync(baseptr, data_len
#ifdef	sun
		  ,0
#endif
		  );
#endif

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

find_rd_root_image(file)
	char *file;
{
	int data_va;
	int std_entry;

	if (nlist(file, wantsyms)) {
		printf("%s: no rd_root_image symbols?\n", file);
		exit(1);
	}
#if 0
	std_entry = N_TXTADDR(head) +
	    (head.a_entry & (N_PAGSIZ(head)-1));
	data_va = N_DATADDR(head);
	if (head.a_entry != std_entry) {
		printf("%s: warning: non-standard entry point: 0x%08x\n",
			   file, head.a_entry);
		printf("\texpecting entry=0x%X\n", std_entry);
		data_va += (head.a_entry - std_entry);
	}
#endif
	{

		Elf32_Ehdr *pehdr;
		Elf32_Phdr *phdr;
		int i;
		pehdr = (Elf32_Ehdr *)pexe;
		phdr = (Elf32_Phdr *)(pexe + pehdr->e_phoff);
		printf("sections:\n");
		for (i = 1; i <= pehdr->e_phnum; i++, phdr++) {
			fprintf(stderr, "phseg %d addr %x  size %x"
			" offset %x\n",
			i, phdr->p_vaddr, phdr->p_memsz, phdr->p_offset );
			if (wantsyms[0].n_value > phdr->p_vaddr &&
				(wantsyms[0].n_value <
				phdr->p_vaddr + phdr->p_filesz))
			{
				data_va = - (phdr->p_offset - phdr->p_vaddr);
			}
		}
		printf("esections:\n");

	}

	rd_root_size_off = wantsyms[0].n_value - data_va;
	rd_root_image_off     = wantsyms[1].n_value - data_va;
#ifdef	DEBUG
	printf(".data segment  va: 0x%08X\n", data_va);
	printf("rd_root_size   va: 0x%08X\n", wantsyms[0].n_value);
	printf("rd_root_image  va: 0x%08X\n", wantsyms[1].n_value);
	printf("rd_root_size  off: 0x%08X\n", rd_root_size_off);
	printf("rd_root_image off: 0x%08X\n", rd_root_image_off);

	{
		char *base;
		int *iptr;
		base= (char *)pexe + rd_root_size_off;
		iptr = (int *) base;
		base= (char *)pexe + rd_root_image_off;
		printf("size %x\n", *iptr);
		printf("string [%s]\n", base);

	}

#endif

#if 0
	/*
	 * Sanity check locations of db_* symbols
	 */
	if (rd_root_image_off < 0 || rd_root_image_off >= head.a_data) {
		printf("%s: rd_root_image not in data segment?\n", file);
		exit(1);
	}
	if (rd_root_size_off < 0 || rd_root_size_off >= head.a_data) {
		printf("%s: rd_root_size not in data segment?\n", file);
		exit(1);
	}
#endif
}

