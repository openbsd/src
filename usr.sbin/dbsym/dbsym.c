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
 *	$Id: dbsym.c,v 1.1.1.1 1995/10/18 08:47:31 deraadt Exp $
 */

/* Copy the symbol table into the space reserved for it. */

#include <sys/types.h>
#include <sys/file.h>
#include <sys/mman.h>

#include <stdio.h>
#include <a.out.h>

extern off_t lseek();

struct exec head;
char *file;

/* Virtual addresses of the symbols we frob. */
long db_symtab_va, db_symtabsize_va;

/* Offsets relative to start of data segment. */
long db_symtab_off, db_symtabsize_off;

/* value in the location at db_symtabsize_off */
int db_symtabsize_val;

/* pointers to pieces of mapped file */
char *dataseg;
char *symbols;
char *strings;
int *strtab;
/* and lengths */
int strtab_len;
int file_len;
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
	file_len = lseek(fd, (off_t)0, 2);

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

	find_db_symtab(file);

	/*
	 * Map in the file from start of data to EOF.
	 * The file offset needs to be page aligned.
	 */
	data_off = N_DATOFF(head);
	data_pgoff = N_PAGSIZ(head) - 1;
	data_pgoff &= data_off;
	data_off -= data_pgoff;
	dataseg = mmap(NULL,	/* any address is ok */
				   file_len - data_off, /* length */
				   PROT_READ | PROT_WRITE,
				   MAP_SHARED,
				   fd, data_off);
	if ((long)dataseg == -1) {
		printf("%s: can not map data seg\n", file);
		perror(file);
		exit(1);
	}
	dataseg += data_pgoff;
	symbols = dataseg + head.a_data;
	strings = symbols + head.a_syms;
	/* Find the string table length (first word after symtab). */
	strtab_len = *((int*)strings);
#ifdef	DEBUG
	printf("strtab_len: %d\n", strtab_len);
#endif

	/*
	 * Find value in the location: db_symtabsize
	 */
	ip = (int*) (dataseg + db_symtabsize_off);
	db_symtabsize_val = *ip;
#ifdef	DEBUG
	printf("db_symtabsize val: 0x%08X (%d)\n",
		   db_symtabsize_val, db_symtabsize_val);
#endif
	/* Is there room for the symbols + strings? */
	if (db_symtabsize_val < (head.a_syms + strtab_len)) {
		printf("%s: symbol space too small (%d < %d)\n", argv[0],
			   db_symtabsize_val, (head.a_syms + strtab_len));
		exit(1);
	}
	printf("Symbols use %d of %d bytes available (%d%%)\n",
		   head.a_syms + strtab_len,
		   db_symtabsize_val,
		   (head.a_syms + strtab_len) * 100 /
		   db_symtabsize_val);

	/*
	 * Copy the symbol table and string table.
	 */
#ifdef	DEBUG
	printf("copying symbol table...\n");
#endif
	ip = (int*) (dataseg + db_symtab_off);
	/* Write symtab length */
	*ip++ = head.a_syms;
	memcpy((char*)ip, symbols, head.a_syms + strtab_len);

	msync(dataseg - data_pgoff, file_len - data_off
#ifdef	sun
		  ,0
#endif
		  );

#ifdef	DEBUG
	printf("...done\n");
#endif
	close(fd);
	exit(0);
}


/*
 * Find locations of the symbols to patch.
 */
struct nlist wantsyms[] = {
	{ "_db_symtabsize", 0 },
	{ "_db_symtab", 0 },
	{ NULL, 0 },
};

find_db_symtab(file)
	char *file;
{
	int data_va;
	int std_entry;

	if (nlist(file, wantsyms)) {
		printf("%s: no db_symtab symbols?\n", file);
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

	db_symtabsize_off = wantsyms[0].n_value - data_va;
	db_symtab_off     = wantsyms[1].n_value - data_va;
#ifdef	DEBUG
	printf(".data segment  va: 0x%08X\n", data_va);
	printf("db_symtabsize  va: 0x%08X\n", wantsyms[0].n_value);
	printf("db_symtab      va: 0x%08X\n", wantsyms[1].n_value);
	printf("db_symtabsize off: 0x%08X\n", db_symtabsize_off);
	printf("db_symtab     off: 0x%08X\n", db_symtab_off);
#endif

	/*
	 * Sanity check locations of db_* symbols
	 */
	if (db_symtab_off < 0 || db_symtab_off >= head.a_data) {
		printf("%s: db_symtab not in data segment?\n", file);
		exit(1);
	}
	if (db_symtabsize_off < 0 || db_symtabsize_off >= head.a_data) {
		printf("%s: db_symtabsize not in data segment?\n", file);
		exit(1);
	}
}
