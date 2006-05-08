/* $OpenBSD: prebind_strip.c,v 1.3 2006/05/08 20:34:36 deraadt Exp $ */
/*
 * Copyright (c) 2006 Dale Rahn <drahn@dalerahn.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/exec_elf.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "prebind.h"

void strip_prebind(char *file, char *output);
void prebind_remove_load_section(int fd, char *name);
int prebind_cat(int fd, struct prebind_footer *footer, char *name);

extern char *__progname;

void __dead
usage(void)
{
	fprintf(stderr, "Usage:%s [-o <outfile>] <filelist>\n", __progname);
	exit(1);
}


int
main(int argc, char **argv)
{
	char *outputfile = NULL;
	int i;
	int ch;
	while ((ch = getopt(argc, argv, "o:")) != -1) {
		switch (ch) {
		case 'o':
			outputfile = optarg;
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;

	if (outputfile != NULL && argc > 1) {
		fprintf(stderr, "%s:-o will not work with multiple files\n",
		    __progname);
		usage();
	}
	for (i = 0; i < argc; i++) {
		strip_prebind(argv[i], outputfile);
	}

	return 0;
}

void
strip_prebind(char *file, char *outfile)
{
	struct prebind_footer footer;
	int fd;
	ssize_t bytes;
	int mode;

	if (outfile == NULL)
		mode = O_RDWR;
	else
		mode = O_RDONLY;

	fd = open(file, mode);
	if (fd == -1) {
		perror(file);
		return;
	}
	lseek(fd, -((off_t)sizeof(struct prebind_footer)), SEEK_END);
	bytes = read(fd, &footer, sizeof(struct prebind_footer));
	if (bytes != sizeof(struct prebind_footer)) {
		perror("short read\n");
		goto done;
	}

	if (footer.bind_id[0] == BIND_ID0 &&
	    footer.bind_id[1] == BIND_ID1 &&
	    footer.bind_id[2] == BIND_ID2 &&
	    footer.bind_id[3] == BIND_ID3) {

	} else {
		fprintf(stderr, "%s: no prebind header\n", file);
		goto done;
	}

	if (outfile == NULL) {
		prebind_remove_load_section(fd, file);

		ftruncate(fd, footer.orig_size);
	} else {
		prebind_cat(fd, &footer, outfile);
	}
done:
	close(fd);
}

void
prebind_remove_load_section(int fd, char *name)
{
	void *buf;
	Elf_Ehdr *ehdr;
	Elf_Phdr *phdr;
	int loadsection;

	buf = mmap(NULL, 8192, PROT_READ|PROT_WRITE, MAP_FILE | MAP_SHARED,
	    fd, 0);
	if (buf == MAP_FAILED) {
		perror(name);
		fprintf(stderr, "%s: cannot mmap for write\n", name);
		return;
	}

	ehdr = (Elf_Ehdr *) buf;
	phdr = (Elf_Phdr *)((char *)buf + ehdr->e_phoff);

	loadsection = ehdr->e_phnum - 1;

	if(ehdr->e_type != ET_EXEC ||
	    (phdr[loadsection].p_flags & 0x08000000) == 0) {
		goto done;
	}

	if ((phdr[loadsection].p_type != PT_LOAD) ||
	    ((phdr[loadsection].p_flags & 0x08000000) == 0)) {
		/* doesn't look like ours */
		fprintf(stderr, "mapped, %s id doesn't match %lx\n", name,
		    (long)(phdr[loadsection].p_vaddr));
		goto done;
	}

	bzero(&phdr[loadsection], sizeof(Elf_Phdr));

	ehdr->e_phnum--;
done:
	munmap(buf, 8192);
}

int
prebind_cat(int fd, struct prebind_footer *footer, char *name)
{
	int outfd;
	void *buf;
	Elf_Ehdr *ehdr;
	Elf_Phdr *phdr;
	size_t len, wlen, rlen, remlen;
	int header_done = 0;
	int err = 0;
	int loadsection;

	if (strcmp(name, "-") == 0)
		outfd = 1;
	else
		outfd = open(name, O_RDWR|O_CREAT|O_TRUNC, 0644);

	if (outfd == -1) {
		fprintf(stderr, "unable to open file %s\n", name);
		return 1;
	}
#define BUFSZ (256 * 1024)
	buf = malloc(BUFSZ);

	if (buf == NULL) {
		fprintf(stderr, "failed to allocate copy buffer\n");
		return 1;
	}

	lseek(fd, 0, SEEK_SET);
	remlen = footer->orig_size;
	while (remlen > 0) {
		if (remlen > BUFSZ)
			rlen = BUFSZ;
		else
			rlen = remlen;
		len = read(fd, buf, rlen);
		if (len <= 0) {
			break; /* read failure */
			err=1;
		}
		remlen -= len;
		if (header_done == 0) {
			header_done = 1;
			ehdr = (Elf_Ehdr *) buf;
			phdr = (Elf_Phdr *)((char *)buf + ehdr->e_phoff);

			loadsection = ehdr->e_phnum - 1;

			if ((len >= ehdr->e_phoff +
			    sizeof(Elf_Phdr) * ehdr->e_phnum) &&
			    ehdr->e_type == ET_EXEC &&
			    (phdr[loadsection].p_flags & 0x08000000) != 0 &&
			    (phdr[loadsection].p_type == PT_LOAD)) {
				bzero(&phdr[loadsection], sizeof(Elf_Phdr));
				ehdr->e_phnum--;
			}
		}
		wlen = write(outfd, buf, len);
		if (wlen != len) {
			/* write failed */
			err=1;
			break;
		}
	}

	return err;
}
