/*	$OpenBSD: rdsetroot.c,v 1.3 2000/03/01 22:10:04 todd Exp $	*/
/*	$NetBSD: rdsetroot.c,v 1.2 1995/10/13 16:38:39 gwr Exp $	*/

/*
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
 */

/*
 * Copy a ramdisk image into the space reserved for it.
 * Kernel variables: rd_root_size, rd_root_image
 */

#include <sys/types.h>
#include <sys/file.h>
#include <sys/mman.h>

#include <stdio.h>
#include <a.out.h>

extern off_t lseek();

struct exec head;
char *file;

/* Virtual addresses of the symbols we frob. */
long rd_root_image_va, rd_root_size_va;

/* Offsets relative to start of data segment. */
long rd_root_image_off, rd_root_size_off;

/* value in the location at rd_root_size_off */
int rd_root_size_val;

/* pointers to pieces of mapped file */
char *dataseg;

/* and lengths */
int data_len;
int data_off;
int data_pgoff;

main(argc,argv)
	char **argv;
{
	int fd, n;
	int *ip;
	char *cp;

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

	n = read(fd, &head, sizeof(head));
	if (n < sizeof(head)) {
		printf("%s: reading header\n", file);
		exit(1);
	}

	if (N_BADMAG(head)) {
		printf("%s: bad magic number\n");
		exit(1);
	}

#ifdef	DEBUG
	printf(" text:  %9d\n", head.a_text);
	printf(" data:  %9d\n", head.a_data);
	printf("  bss:  %9d\n", head.a_bss);
	printf(" syms:  %9d\n", head.a_syms);
	printf("entry: 0x%08X\n", head.a_entry);
	printf("trsiz:  %9d\n", head.a_trsize);
	printf("drsiz:  %9d\n", head.a_drsize);
#endif

	if (head.a_syms <= 0) {
		printf("%s: no symbols\n", file);
		exit(1);
	}
	if (head.a_trsize ||
		head.a_drsize)
	{
		printf("%s: has relocations\n");
		exit(1);
	}

	find_rd_root_image(file);

	/*
	 * Map in the whole data segment.
	 * The file offset needs to be page aligned.
	 */
	data_off = N_DATOFF(head);
	data_len = head.a_data;
	/* align... */
	data_pgoff = N_PAGSIZ(head) - 1;
	data_pgoff &= data_off;
	data_off -= data_pgoff;
	data_len += data_pgoff;
	/* map in in... */
	dataseg = mmap(NULL,	/* any address is ok */
				   data_len, /* length */
				   PROT_READ | PROT_WRITE,
				   MAP_SHARED,
				   fd, data_off);
	if ((long)dataseg == -1) {
		printf("%s: can not map data seg\n", file);
		perror(file);
		exit(1);
	}
	dataseg += data_pgoff;

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

	msync(dataseg - data_pgoff, data_len, 0);

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
	std_entry = N_TXTADDR(head) +
	    (head.a_entry & (N_PAGSIZ(head)-1));
	data_va = N_DATADDR(head);
	if (head.a_entry != std_entry) {
		printf("%s: warning: non-standard entry point: 0x%08x\n",
			   file, head.a_entry);
		printf("\texpecting entry=0x%X\n", std_entry);
		data_va += (head.a_entry - std_entry);
	}

	rd_root_size_off = wantsyms[0].n_value - data_va;
	rd_root_image_off     = wantsyms[1].n_value - data_va;
#ifdef	DEBUG
	printf(".data segment  va: 0x%08X\n", data_va);
	printf("rd_root_size   va: 0x%08X\n", wantsyms[0].n_value);
	printf("rd_root_image  va: 0x%08X\n", wantsyms[1].n_value);
	printf("rd_root_size  off: 0x%08X\n", rd_root_size_off);
	printf("rd_root_image off: 0x%08X\n", rd_root_image_off);
#endif

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
}
