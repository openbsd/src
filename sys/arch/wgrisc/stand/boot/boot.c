/*	$NetBSD: boot.c,v 1.6 1995/06/28 10:22:32 jonathan Exp $	*/

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ralph Campbell.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)boot.c	8.1 (Berkeley) 6/10/93
 */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/exec.h>
#include <sys/exec_elf.h>
#include <stand.h>
#include <errno.h>


char	line[1024];
void gets __P((char *));
ssize_t read __P((int, void *, size_t));
int close __P((int));
void prom_write __P((int, char *, int));

int main __P((int, char **));
int loadfile __P((char *));
/*
 * This gets arguments from the PROM, calls other routines to open
 * and load the program to boot, and then transfers execution to that
 * new program.
 * Argv[0] should be something like "rz(0,0,0)vmunix" on a DECstation 3100.
 * Argv[0,1] should be something like "boot 5/rz0/vmunix" on a DECstation 5000.
 * The argument "-a" means vmunix should do an automatic reboot.
 */
int
main(argc, argv)
	int argc;
	char **argv;
{
	char *cp = 0;
	int   ask, entry;

	ask = 1;

	if(strcmp(argv[0], "man") != 0) {
		cp = argv[0];
		ask = 0;
	}
	while(1) {
		do {
			printf("Boot: ");
			if (ask) {
				gets(line);
				cp = line;
				argv[0] = cp;
				argc = 1;
			} else
				printf("%s\n", cp);
		} while(ask && line[0] == '\0');

		entry = loadfile(cp);
		if (entry != -1) {
			printf("Starting at 0x%x\n\n", entry);
			((void (*)())entry)(argc, argv, 0, 0);
		}
	}
	return(0);
}

/*
 * Open 'filename', read in program and return the entry point or -1 if error.
 */
int
loadfile(fname)
	register char *fname;
{
	int fd, i;
	Elf32_Ehdr eh;
	Elf32_Phdr *ph;
	u_long phsize;

	if ((fd = oopen(fname, 0)) < 0) {
		printf("open(%s) failed: %d\n", fname, errno);
		goto err;
	}

	/* read the elf header */
	if(oread(fd, (char *)&eh, sizeof(eh)) != sizeof(eh)) {
		goto serr;
	}

	phsize = eh.e_phnum * sizeof(Elf32_Phdr);
	ph = (Elf32_Phdr *) alloc(phsize);
	olseek(fd, eh.e_phoff, 0);
	if(oread(fd, (char *)ph, phsize) != phsize) {
		goto serr;
	}

	for(i = 0; i < eh.e_phnum; i++) {
		switch (ph[i].p_type) {
		case PT_LOAD:
			olseek(fd, ph[i].p_offset, 0);
			if(oread(fd, (char *)ph[i].p_paddr, ph[i].p_filesz) !=  ph[i].p_filesz) {
				goto serr;
			}
			break;
		default:
			break;
		}
	}
	(void) oclose(fd);
	return(eh.e_entry);
serr:
	printf("Read size error\n");
err:
	printf("Can't boot '%s'\n", fname);
	(void) oclose(fd);
	return (-1);
}
