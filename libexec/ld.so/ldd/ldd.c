/*	$OpenBSD: ldd.c,v 1.12 2007/05/29 04:47:17 jason Exp $	*/
/*
 * Copyright (c) 2001 Artur Grabowski <art@openbsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL  DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <elf_abi.h>
#include <err.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>

#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/param.h>

int usage(void);
int doit(char *);

int
main(int argc, char **argv)
{
	int c, xflag, ret;

	xflag = 0;
	while ((c = getopt(argc, argv, "x")) != -1) {
		switch (c) {
		case 'x':
			xflag = 1;
			break;
		default:
			usage();
			/*NOTREACHED*/
		}
	}

	if (xflag)
		errx(1, "-x not yet implemented");

	argc -= optind;
	argv += optind;

	if (argc == 0)
		usage();

	if (setenv("LD_TRACE_LOADED_OBJECTS", "true", 1) < 0)
		err(1, "setenv(LD_TRACE_LOADED_OBJECTS)");

	ret = 0;
	while (argc--) {
		ret |= doit(*argv);
		argv++;
	}

	return ret;
}

int
usage(void)
{
	extern char *__progname;

	fprintf(stderr, "Usage: %s [-x] <filename> ...\n", __progname);
	exit(1);
}


int
doit(char *name)
{
	Elf_Ehdr ehdr;
	Elf_Phdr *phdr;
	int fd, i, size, status;
	char buf[MAXPATHLEN];
	void * dlhandle; 


	if ((fd = open(name, O_RDONLY)) < 0) {
		warn("%s", name);
		return 1;
	}

	if (read(fd, &ehdr, sizeof(ehdr)) < 0) {
		warn("read(%s)", name);
		close(fd);
		return 1;
	}

	if (memcmp(ehdr.e_ident, ELFMAG, SELFMAG) ||
	    ehdr.e_machine != ELF_TARG_MACH) {
		warnx("%s: not an ELF executable", name);
		close(fd);
		return 1;
	}

	if (ehdr.e_type == ET_DYN) {
		printf("%s:\n", name);
		if (realpath(name, buf) == NULL) {
			warn("realpath(%s)", name);
			return 1;
		}
		dlhandle = dlopen(buf, RTLD_TRACE);
		if (dlhandle == NULL) {
			printf("%s\n", dlerror());
			return 1;
		}
		close(fd);
		return 0;
	}

	size = ehdr.e_phnum * sizeof(Elf_Phdr);
	if ((phdr = malloc(size)) == NULL)
		err(1, "malloc");

	if (pread(fd, phdr, size, ehdr.e_phoff) != size) {
		warn("read(%s)", name);
		close(fd);
		free(phdr);
		return 1;
	}
	for (i = 0; i < ehdr.e_phnum; i++)
		if (phdr[i].p_type == PT_DYNAMIC)
			break;
	close(fd);
	free(phdr);

	if (i == ehdr.e_phnum) {
		warnx("%s: not a dynamic executable", name);
		return 1;
	}

	printf("%s:\n", name);
	fflush(stdout);
	switch (fork()) {
	case -1:
		err(1, "fork");
	case 0:
		execl(name, name, (char *)NULL);
		perror(name);
		_exit(1);
	default:
		if (wait(&status) < 0) {
			warn("wait");
			return 1;
		}
		if (WIFSIGNALED(status)) {
			fprintf(stderr, "%s: signal %d\n", name,
			    WTERMSIG(status));
			return 1;
		}
		if (WEXITSTATUS(status)) {
			fprintf(stderr, "%s: exit status %d\n", name,
			    WEXITSTATUS(status));
			return 1;
		}
	}

	return 0;
}
