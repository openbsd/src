/*	$OpenBSD: rdsetroot.c,v 1.20 2009/04/17 07:23:27 deraadt Exp $	*/
/*	$NetBSD: rdsetroot.c,v 1.2 1995/10/13 16:38:39 gwr Exp $	*/

/*
 * Copyright (c) 1994 Gordon W. Ross
 * Copyright (c) 1997 Per Fogelstrom. (ELF modifications)
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

#include <sys/types.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <nlist.h>

#include <a.out.h>
struct	exec head;

char	*file;

/* Virtual addresses of the symbols we frob. */
long	rd_root_image_va, rd_root_size_va;

/* Offsets relative to start of data segment. */
long	rd_root_image_off, rd_root_size_off;

/* value in the location at rd_root_size_off */
off_t	rd_root_size_val;

/* pointers to pieces of mapped file */
char	*dataseg;

/* and lengths */
int	data_len;
off_t	mmap_off;
int	data_pgoff;

int	find_rd_root_image(char *);
__dead void usage(void);

int	debug;

int
main(int argc, char *argv[])
{
	int ch, fd, n, xflag = 0, fsd;
	char *fs = NULL;
	u_int32_t *ip;

	while ((ch = getopt(argc, argv, "dx")) != -1) {
		switch (ch) {
		case 'd':
			debug = 1;
			break;
		case 'x':
			xflag = 1;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc == 1)
		file = argv[0];
	else if (argc == 2) {
		file = argv[0];
		fs = argv[1];
	} else
		usage();

	fd = open(file, xflag ? O_RDONLY : O_RDWR, 0644);
	if (fd < 0) {
		perror(file);
		exit(1);
	}

	if (fs) {
		if (xflag)
			fsd = open(fs, O_RDWR | O_CREAT | O_TRUNC, 0644);
		else
			fsd = open(fs, O_RDONLY, 0644);
	} else {
		if (xflag)
			fsd = dup(STDOUT_FILENO);
		else
			fsd = dup(STDIN_FILENO);
	}
	if (fsd < 0) {
		perror(fs);
		exit(1);
	}

	n = read(fd, &head, sizeof(head));
	if (n < sizeof(head)) {
		fprintf(stderr, "%s: reading header\n", file);
		exit(1);
	}

	if (N_BADMAG(head)) {
		fprintf(stderr, "%s: bad magic number\n", file);
		exit(1);
	}

	if (debug) {
		fprintf(stderr, " text:  %9d\n", head.a_text);
		fprintf(stderr, " data:  %9d\n", head.a_data);
		fprintf(stderr, "  bss:  %9d\n", head.a_bss);
		fprintf(stderr, " syms:  %9d\n", head.a_syms);
		fprintf(stderr, "entry: 0x%08X\n", head.a_entry);
		fprintf(stderr, "trsiz:  %9d\n", head.a_trsize);
		fprintf(stderr, "drsiz:  %9d\n", head.a_drsize);
	}

	if (head.a_syms <= 0) {
		fprintf(stderr, "%s: no symbols\n", file);
		exit(1);
	}
	if (head.a_trsize || head.a_drsize) {
		fprintf(stderr, "%s: has relocations\n", file);
		exit(1);
	}

	find_rd_root_image(file);

	/*
	 * Map in the whole data segment.
	 * The file offset needs to be page aligned.
	 */
	mmap_off = N_DATOFF(head);

#ifdef BROKEN_NMAGIC
	/*
	 * XXX it seems that our ld has a bug when generating NMAGIC files.
	 * the data segment ends up one page too far into the file.
	 */
	if (N_GETMAGIC(head) == NMAGIC)
		mmap_off += __LDPGSZ;
#endif

	data_len = head.a_data;
	/* align... */
	data_pgoff = N_PAGSIZ(head) - 1;
	data_pgoff &= mmap_off;
	mmap_off -= data_pgoff;
	data_len += data_pgoff;
	/* map in in... */
	dataseg = mmap(NULL, data_len,
	    xflag ? PROT_READ : PROT_READ | PROT_WRITE,
	    MAP_SHARED, fd, mmap_off);
	if (dataseg == MAP_FAILED) {
		fprintf(stderr, "%s: can not map data seg\n", file);
		perror(file);
		exit(1);
	}
	dataseg += data_pgoff;

	/*
	 * Find value in the location: rd_root_size
	 */
	ip = (u_int32_t *) (dataseg + rd_root_size_off);
	rd_root_size_val = *ip;
	if (debug)
		fprintf(stderr, "rd_root_size  val: 0x%llx (%lld blocks)\n",
		    (unsigned long long)rd_root_size_val,
		    (unsigned long long)rd_root_size_val >> 9);

	/*
	 * Copy the symbol table and string table.
	 */
	if (debug)
		fprintf(stderr, "copying root image...\n");

	if (xflag) {
		n = write(fsd, dataseg + rd_root_image_off,
		    (size_t)rd_root_size_val);
		if (n != rd_root_size_val) {
			perror("write");
			exit(1);
		}
	} else {
		struct stat sstat;

		if (fstat(fsd, &sstat) == -1) {
			perror("fstat");
			exit(1);
		}
		if (S_ISREG(sstat.st_mode) &&
		    sstat.st_size > rd_root_size_val) {
			fprintf(stderr, "ramdisk too small 0x%llx 0x%llx\n",
			    (unsigned long long)sstat.st_size,
			    (unsigned long long)rd_root_size_val);
			exit(1);
		}
		n = read(fsd, dataseg + rd_root_image_off,
		    (size_t)rd_root_size_val);
		if (n < 0) {
			perror("read");
			exit(1);
		}

		msync(dataseg - data_pgoff, data_len, 0);
	}

	if (debug)
		fprintf(stderr, "...copied %d bytes\n", n);
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
find_rd_root_image(char *file)
{
	int data_va, std_entry;

	if (nlist(file, wantsyms)) {
		fprintf(stderr, "%s: no rd_root_image symbols?\n", file);
		exit(1);
	}
	std_entry = N_TXTADDR(head) + (head.a_entry & (N_PAGSIZ(head)-1));
	data_va = N_DATADDR(head);
	if (head.a_entry != std_entry) {
		fprintf(stderr,
		    "%s: warning: non-standard entry point: 0x%08x\n", file,
		    head.a_entry);
		fprintf(stderr, "\texpecting entry=0x%x\n", std_entry);
		data_va += (head.a_entry - std_entry);
	}

	rd_root_size_off = wantsyms[0].n_value - data_va;
	rd_root_image_off = wantsyms[1].n_value - data_va;

	if (debug) {
		fprintf(stderr, "data segment  va: 0x%x\n", data_va);
		fprintf(stderr, "rd_root_size   va: 0x%x\n", wantsyms[0].n_value);
		fprintf(stderr, "rd_root_image  va: 0x%x\n", wantsyms[1].n_value);
		fprintf(stderr, "rd_root_size  off: 0x%x\n", rd_root_size_off);
		fprintf(stderr, "rd_root_image off: 0x%x\n", rd_root_image_off);
	}

	/*
	 * Sanity check locations of db_* symbols
	 */
	if (rd_root_image_off < 0 || rd_root_image_off >= head.a_data) {
		fprintf(stderr, "%s: rd_root_image not in data segment?\n", file);
		exit(1);
	}
	if (rd_root_size_off < 0 || rd_root_size_off >= head.a_data) {
		fprintf(stderr, "%s: rd_root_size not in data segment?\n", file);
		exit(1);
	}
	return (1);
}

__dead void
usage(void)
{
	extern char *__progname;

	fprintf(stderr, "usage: %s [-dx] bsd [fs]\n", __progname);
	exit(1);
}
