/*	$OpenBSD: ecoffrdsetroot.c,v 1.4 2002/02/16 21:27:08 millert Exp $	*/

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
#include <sys/param.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/exec_ecoff.h>
#include <nlist.h>

void find_rd_root_image(char *);

struct ecoff_exechdr head;
char *file;

/* Virtual addresses of the symbols we frob. */
u_long rd_root_image_va, rd_root_size_va;

/* Offsets relative to start of data segment. */
off_t rd_root_image_off, rd_root_size_off;

/* value in the location at rd_root_size_off */
u_int32_t rd_root_size_val;

/* pointers to pieces of mapped file */
caddr_t dataseg;

/* and lengths */
size_t data_len;
off_t data_off;
u_int data_pgoff;

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
	u_int32_t *ip;

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
	printf("ecoff header\n");
	printf(" nscns:  %9ld\n", head.f.f_nscns);
	printf("timdat:  %9ld\n", head.f.f_timdat);
	printf("symptr:  %9ld\n", head.f.f_symptr);
	printf(" nsyms:  %9ld\n", head.f.f_nsyms);
	printf("opthdr:  %9ld\n", head.f.f_opthdr);
	printf(" flags:  %9ld\n", head.f.f_flags);
	printf("a.out header\n");
	printf("vstamp:  %9ld\n", head.a.vstamp);
	printf(" tsize:  %9ld\n", head.a.tsize);
	printf(" dsize:  %9ld\n", head.a.dsize);
	printf(" bsize:  %9ld\n", head.a.bsize);
	printf(" entry:  0x%016lx\n", head.a.entry);
	printf("  text:  0x%016lx\n", head.a.text_start);
	printf("  data:  0x%016lx\n", head.a.data_start);
	printf("   bss:  0x%016lx\n", head.a.bss_start);
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

	/* align... */
	data_pgoff = NBPG - 1;
	data_pgoff &= data_off;

#ifdef	DEBUG
	printf("data parameters\n");
	printf(" data_off:  0x%016lx\n", data_off);
	printf(" data_len:  0x%016lx\n", data_len);
	printf(" data_pgoff:  0x%08x\n", data_pgoff);
#endif

	data_off -= data_pgoff;
	data_len += data_pgoff;

#ifdef	DEBUG
	printf("adjusted parameters\n");
	printf(" data_off:  0x%016lx\n", data_off);
	printf(" data_len:  0x%016lx\n", data_len);
#endif
	/* Map it in... */
	dataseg = mmap(NULL, data_len, PROT_READ | PROT_WRITE,
	    MAP_FILE | MAP_SHARED, fd, data_off);

	if (dataseg == MAP_FAILED)
		err(1, "%s: can not map data seg", file);

	dataseg += data_pgoff;

	/*
	 * Find value in the location: rd_root_size
	 */
	ip = (u_int32_t *)(dataseg + rd_root_size_off);
	rd_root_size_val = *ip;
#ifdef	DEBUG
	printf("rd_root_size  val: 0x%08x (%d blocks)\n",
	    rd_root_size_val, (rd_root_size_val >> 9));
#endif

	/*
	 * Copy the root image
	 */
#ifdef	DEBUG
	printf("copying root image...\n");
#endif
	n = read(0, dataseg + rd_root_image_off, rd_root_size_val);
	if (n < 0)
		err(1, "read root image");

#ifdef	DEBUG
	printf("...copied %d bytes\n", n);
#endif
	close(fd);
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
	rd_root_image_off = wantsyms[1].n_value - data_va;
#ifdef	DEBUG
	printf(".data segment  va: 0x%016lx\n", data_va);
	printf("rd_root_size   va: 0x%016lx\n", wantsyms[0].n_value);
	printf("rd_root_image  va: 0x%016lx\n", wantsyms[1].n_value);
	printf("rd_root_size  off: 0x%016lx\n", rd_root_size_off);
	printf("rd_root_image off: 0x%016lx\n", rd_root_image_off);
#endif

	/*
	 * Sanity check locations of db_* symbols
	 */
	if (rd_root_image_off < 0 || rd_root_image_off >= head.a.data_start)
		errx(1, "%s: rd_root_image not in data segment?", file);
	if (rd_root_size_off < 0 || rd_root_size_off >= head.a.data_start)
		errx(1, "%s: rd_root_size not in data segment?", file);
}
