/* $OpenBSD: prebind_delete.c,v 1.10 2010/03/30 17:42:50 zinovik Exp $ */

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
#include <elf_abi.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <err.h>
#include <errno.h>
#include "prebind.h"

#define BUFSZ (256 * 1024)

int	strip_prebind(char *file);
int	prebind_remove_load_section(int fd, char *name);
int	prebind_newfile(int fd, char *name, struct stat *st, off_t orig_size);
int	strip_file_or_dir(char *name);
int	strip_dir(char *dir);

extern	int verbose;

int
prebind_delete(char **argv)
{
	while (*argv) {
		if (strip_file_or_dir(*argv) == -1)
			return (1);
		argv++;
	}
	return (0);
}

int
strip_file_or_dir(char *name)
{
	struct stat sb;
	int ret = -1;

	ret = lstat(name, &sb);
	if (ret != 0)
		return 0;
	switch (sb.st_mode & S_IFMT) {
	case S_IFREG:
		ret =  strip_prebind(name);
		break;
	case S_IFDIR:
		if (verbose > 0)
			printf("loading dir %s\n", name);
		ret = strip_dir(name);
		break;
	default:
		; /* links and other files we skip */
		ret = 0;
	}
	return ret;
}

int
strip_dir(char *dir)
{
	struct dirent *dp;
	struct stat sb;
	DIR *dirp;
	char *buf;
	int ret;

	dirp = opendir(dir);

	/* if dir failes to open, skip */
	if (dirp == NULL)
		return 0;

	ret = 0;
	while ((dp = readdir(dirp)) != NULL && ret != -1) {
		ret = -1;
		switch (dp->d_type) {
		case DT_UNKNOWN:
			/*
			 * NFS will return unknown, since load_file
			 * does stat the file, this just
			 */
			asprintf(&buf, "%s/%s", dir, dp->d_name);
			lstat(buf, &sb);
			if (sb.st_mode == S_IFREG)
				ret = strip_prebind(buf);
			free(buf);
			break;
		case DT_REG:
			asprintf(&buf, "%s/%s", dir, dp->d_name);
			ret = strip_prebind(buf);
			free(buf);
			break;
		default:
			/* other files symlinks, dirs, ... we ignore */
			ret = 0;
			;
		}
	}
	closedir(dirp);
	return ret;
}

int
strip_prebind(char *file)
{
	struct prebind_footer footer;
	extern char *__progname;
	int fd, rdonly = 0;
	struct stat st;
	ssize_t bytes;

	fd = open(file, O_RDWR);
	if (fd == -1 && errno == ETXTBSY) {
		fd = open(file, O_RDONLY);
		rdonly = 1;
	}
	if (fd == -1) {
		warn("%s", file);
		return (-1);
	}

	if (fstat(fd, &st) == -1)
		return (-1);

	lseek(fd, -((off_t)sizeof(struct prebind_footer)), SEEK_END);
	bytes = read(fd, &footer, sizeof(struct prebind_footer));
	if (bytes != sizeof(struct prebind_footer))
		goto done;

	if (footer.bind_id[0] != BIND_ID0 || footer.bind_id[1] != BIND_ID1 ||
	    footer.bind_id[2] != BIND_ID2 || footer.bind_id[3] != BIND_ID3) {
		if (verbose)
			fprintf(stderr, "%s: no prebind header\n", file);
		goto done;
	}

	if (rdonly) {
		fd = prebind_newfile(fd, file, &st, footer.orig_size);
	} else {
		prebind_remove_load_section(fd, file);
		ftruncate(fd, footer.orig_size);
	}

	if (verbose)
		fprintf(stderr, "%s: stripped %lld bytes from %s\n",
		    __progname, st.st_size - footer.orig_size, file);

done:
	if (fd != -1)
		close(fd);
	return (0);
}

int
prebind_remove_load_section(int fd, char *name)
{
	Elf_Ehdr *ehdr;
	Elf_Phdr *phdr;
	int loadsection;
	char *buf;

	buf = mmap(NULL, 8192, PROT_READ|PROT_WRITE, MAP_FILE | MAP_SHARED,
	    fd, 0);
	if (buf == MAP_FAILED) {
		if (verbose)
			warn("%s: cannot mmap for for write", name);
		return (-1);
	}

	ehdr = (Elf_Ehdr *)buf;
	phdr = (Elf_Phdr *)(buf + ehdr->e_phoff);
	loadsection = ehdr->e_phnum - 1;

	if (ehdr->e_type != ET_EXEC ||
	    (phdr[loadsection].p_flags & 0x08000000) == 0)
		goto done;

	if (phdr[loadsection].p_type != PT_LOAD ||
	    ((phdr[loadsection].p_flags & 0x08000000) == 0)) {
		/* doesn't look like ours */
		if (verbose)
			fprintf(stderr, "mapped, %s id doesn't match %lx\n", name,
			    (long)(phdr[loadsection].p_vaddr));
		goto done;
	}

	bzero(&phdr[loadsection], sizeof(Elf_Phdr));
	ehdr->e_phnum--;
done:
	munmap(buf, 8192);
	return (0);
}

int
prebind_newfile(int infd, char *name, struct stat *st, off_t orig_size)
{
	struct timeval tv[2];
	char *newname, *buf;
	ssize_t len, wlen;
	int outfd;

	if (asprintf(&newname, "%s.XXXXXXXXXX", name) == -1) {
		if (verbose)
			warn("asprintf");
		return (-1);
	}
	outfd = open(newname, O_CREAT|O_RDWR|O_TRUNC, 0600);
	if (outfd == -1) {
		warn("%s", newname);
		free(newname);
		return (-1);
	}

	buf = malloc(BUFSZ);
	if (buf == NULL) {
		if (verbose)
			warn("malloc");
		goto fail;
	}

	/* copy old file to new file */
	lseek(infd, (off_t)0, SEEK_SET);
	while (1) {
		len = read(infd, buf, BUFSIZ);
		if (len == -1) {
			if (verbose)
				warn("read");
			free(buf);
			goto fail;
		}
		if (len == 0)
			break;
		wlen = write(outfd, buf, len);
		if (wlen != len) {
			free(buf);
			goto fail;
		}
	}
	free(buf);
	close (infd);

	/* now back track, and delete the header */
	if (prebind_remove_load_section(outfd, newname) == -1)
		goto fail;
	if (orig_size != (off_t)-1 &&
	    ftruncate(outfd, orig_size) == -1)
		goto fail;

	/* move new file into place */
	TIMESPEC_TO_TIMEVAL(&tv[0], &st->st_atimespec);
	TIMESPEC_TO_TIMEVAL(&tv[1], &st->st_mtimespec);
	if (futimes(outfd, tv) == -1)
		goto fail;
	if (fchown(outfd, st->st_uid, st->st_gid) == -1)
		goto fail;
	if (fchmod(outfd, st->st_mode) == -1)
		goto fail;
	if (fchflags(outfd, st->st_flags) == -1)
		goto fail;
	if (rename(newname, name) == -1)
		goto fail;

	return (outfd);

fail:
	free(newname);
	unlink(newname);
	close(outfd);
	return (-1);
}
