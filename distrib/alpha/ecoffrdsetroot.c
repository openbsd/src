/*	$OpenBSD: ecoffrdsetroot.c,v 1.1 1997/05/07 12:46:49 niklas Exp $	*/

/*
 * Copyright (c) 1997 Todd C. Milller
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
 */

/*
 * Copy a ramdisk image into the space reserved for it.
 * Kernel variables: rd_root_size, rd_root_image
 */

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/exec_ecoff.h>
#include <nlist.h>

void find_rd_root_image __P((char *));

struct ecoff_exechdr head;
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

/*
 * To find locations of the symbols to patch.
 */
struct nlist wantsyms[] = {
	{ "_rd_root_size", 0 },
	{ "_rd_root_image", 0 },
	{ NULL, 0 },
};

int
main(argc,argv)
	int argc;
	char **argv;
{
	int fd, n;
	int *ip;
	char *cp;

	if (argc < 2)
		errx(1, "missing file name");
	file = argv[1];

	fd = open(file, O_RDWR);
	if (fd < 0)
		err(1, "open %s", file);

	n = read(fd, &head, sizeof(head));
	if (n < sizeof(head))
		errx(1, "%s: short read reading header", file);

	if (head.a.magic != ECOFF_OMAGIC)
		errx(1, "%s: bad magic number (0%o)", file, head.a.magic);

#ifdef	DEBUG
	(void)printf("ecoff header\n");
	(void)printf(" nscns:  %9ld\n", head.f.f_nscns); */
	(void)printf("timdat:  %9ld\n", head.f.f_timdat); */
	(void)printf("symptr:  %9ld\n", head.f.f_symptr); */
	(void)printf(" nsyms:  %9ld\n", head.f.f_nsyms); */
	(void)printf("opthdr:  %9ld\n", head.f.f_opthdr); */
	(void)printf(" flags:  %9ld\n", head.f.f_flags); */
	(void)printf("a.out header\n");
	(void)printf("vstamp:  %9ld\n", head.a.vstamp);
	(void)printf(" tsize:  %9ld\n", head.a.tsize);
	(void)printf(" dsize:  %9ld\n", head.a.dsize);
	(void)printf(" bsize:  %9ld\n", head.a.bsize);
	(void)printf(" entry:  0x%08X\n", head.a.entry);
	(void)printf("  text:  %9ld\n", head.a.text_start);
	(void)printf("  data:  %9ld\n", head.a.data_start);
	(void)printf("   bss:  %9ld\n", head.a.bss_start);
#endif

	if (head.f.f_nsyms <= 0)
		errx(1, "%s: no symbols", file);

	find_rd_root_image(file);

	/*
	 * Map in the whole data segment.
	 * The file offset needs to be page aligned.
	 */
	data_off = ECOFF_DATOFF(&head);
	data_len = head.a.dsize;
#if 0
	/* align... */
	data_pgoff = N_PAGSIZ(head) - 1;
	data_pgoff &= data_off;
	data_off -= data_pgoff;
	data_len += data_pgoff;
#endif
	/* map in in... */
	dataseg = mmap(NULL,	/* any address is ok */
				   data_len, /* length */
				   PROT_READ | PROT_WRITE,
				   MAP_SHARED,
				   fd, data_off);

	if ((long)dataseg == -1)
		err(1, "%s: can not map data seg", file);
#if 0
	dataseg += data_pgoff;
#endif

	/*
	 * Find value in the location: rd_root_size
	 */
	ip = (int *) (dataseg + rd_root_size_off);
	rd_root_size_val = *ip;
#ifdef	DEBUG
	(void)printf("rd_root_size  val: 0x%08X (%d blocks)\n",
		rd_root_size_val, (rd_root_size_val >> 9));
#endif

	/*
	 * Copy the symbol table and string table.
	 */
#ifdef	DEBUG
	(void)printf("copying root image...\n");
#endif
	n = read(0, dataseg + rd_root_image_off, rd_root_size_val);
	if (n < 0)
		err(1, "read root image");

	msync(dataseg - data_pgoff, data_len
#ifdef	sun
		  ,0
#endif
		  );

#ifdef	DEBUG
	(void)printf("...copied %d bytes\n", n);
#endif
	(void)close(fd);
	exit(0);
}


void
find_rd_root_image(file)
	char *file;
{
	u_long data_va = head.a.data_start;

	if (nlist(file, wantsyms))
		errx(1, "%s: no rd_root_image symbols?", file);

	rd_root_size_off = wantsyms[0].n_value - data_va;
	rd_root_image_off     = wantsyms[1].n_value - data_va;
#ifdef	DEBUG
	(void)printf(".data segment  va: 0x%08X\n", data_va);
	(void)printf("rd_root_size   va: 0x%08X\n", wantsyms[0].n_value);
	(void)printf("rd_root_image  va: 0x%08X\n", wantsyms[1].n_value);
	(void)printf("rd_root_size  off: 0x%08X\n", rd_root_size_off);
	(void)printf("rd_root_image off: 0x%08X\n", rd_root_image_off);
#endif

	/*
	 * Sanity check locations of db_* symbols
	 */
	if (rd_root_image_off < 0 || rd_root_image_off >= head.a.data_start)
		errx(1, "%s: rd_root_image not in data segment?", file);
	if (rd_root_size_off < 0 || rd_root_size_off >= head.a.data_start)
		errx(1, "%s: rd_root_size not in data segment?", file);
}
