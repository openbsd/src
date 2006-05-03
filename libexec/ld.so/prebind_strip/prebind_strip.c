/* $OpenBSD: prebind_strip.c,v 1.1 2006/05/03 16:10:52 drahn Exp $ */
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

void dump_prebind(char *file);
void prebind_dump_footer(struct prebind_footer *footer, char *file);
void prebind_dump_symcache(struct symcachetab *symcachetab, u_int32_t cnt);
void prebind_dump_nameidx(struct nameidx *nameidx, u_int32_t numblibs,
    char *nametab);
void prebind_dump_fixup(struct fixup *fixup, u_int32_t numfixups);
void prebind_dump_libmap(u_int32_t *libmap, u_int32_t numlibs, void *a);

void prebind_remove_load_section(int fd, char *name);

int
main(int argc, char **argv)
{
	int i;
	for (i = 1; i < argc; i++) {
		printf("stripping %s\n", argv[i]);
		dump_prebind(argv[i]);
	}

	return 0;
}

void
dump_prebind(char *file)
{
	struct prebind_footer footer;
	int fd;
	ssize_t bytes;

	fd = open(file, O_RDWR);
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
		printf("%s: no prebind header\n", file);
		goto done;
	}

	prebind_remove_load_section(fd, file);

	ftruncate(fd, footer.orig_size);
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
		printf("%s: cannot mmap for write\n", name);
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
		printf("mapped, %s id doesn't match %lx\n", name,
		    (long)(phdr[loadsection].p_vaddr));
		goto done;
	}

	bzero(&phdr[loadsection], sizeof(Elf_Phdr));

	ehdr->e_phnum--;
done:
	munmap(buf, 8192);
}
