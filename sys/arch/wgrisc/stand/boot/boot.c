/*	$OpenBSD: boot.c,v 1.2 1997/07/21 06:58:12 pefo Exp $ */

/*
 * Copyright (c) 1997 Per Fogelstrom
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
 *	This product includes software developed under OpenBSD by
 *	Per Fogelstrom.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/exec.h>
#include <sys/exec_elf.h>
#include <stand.h>
#include <errno.h>


void gets __P((char *));
ssize_t read __P((int, void *, size_t));
int close __P((int));
void prom_write __P((int, char *, int));

void main __P((int, char **));
int loadfile __P((char *));
/*
 */
void
main(argc, argv)
	int argc;
	char **argv;
{
static char boot[] = {"Boot:"};
	char *cp = boot;
	int   ask, entry;
	char  line[1024];

	ask = 1;

	if(strcmp(argv[0], "man") != 0) {
		cp = argv[0];
		ask = 0;
	}
	while(1) {
		do {
			printf("%s\n", cp);
			if (ask) {
				gets(line);
				cp = line;
				argv[0] = cp;
				argc = 1;
			}
		} while(ask && line[0] == '\0');

		entry = loadfile(cp);
		if (entry != -1) {
			((void (*)())entry)(argc, argv, 0, 0);
		}
		ask = 1;
		cp = boot;
	}
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
	char *errs = 0;

	if ((fd = oopen(fname, 0)) < 0) {
		errs="open(%s) err: %d\n";
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

	for(i = 0; i < eh.e_phnum; i++, ph++) {
		if(ph->p_type == PT_LOAD) {
			olseek(fd, ph->p_offset, 0);
			if(oread(fd, (char *)ph->p_paddr, ph->p_filesz) !=  ph->p_filesz) {
				goto serr;
			}
		}
	}
	return(eh.e_entry);
serr:
	errs = "rd(%s) sz err\n";
err:
	printf(errs, fname, errno);
	return (-1);
}

