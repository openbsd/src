/*	$OpenBSD: boot.c,v 1.1 2004/08/23 14:22:40 pefo Exp $ */

/*
 * Copyright (c) 2004 Opsycon AB, www.opsycon.se.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

#include <mips64/arcbios.h>


void gets(char *);
ssize_t read(int, void *, size_t);
int close(int);
void pmon_write(int, char *, int);
void pmon_synccache(void);

int main(int, char **, char **);
int loadfile(char *);
int loadsymtab(int fd, Elf32_Ehdr *eh, int flags);

unsigned long tablebase;

static void *
readtable (int fd, int offs, void *base, int size, char *name, int flags)
{
	if (olseek(fd, offs, SEEK_SET) != offs ||
	    oread(fd, base, size) != size) {
		printf("\ncannot read %s table", name);
		return 0;
	}
	return (void *) base;
}

static void *
gettable (int size, char *name, int flags)
{
	long base;
	/* Put table after loaded code to support kernel DDB */
	tablebase = roundup(tablebase, sizeof(long));
	base = tablebase;
	tablebase += size;
	return (void *) base;
}

/*
 */
int
main(argc, argv, envp)
	int argc;
	char **argv;
	char **envp;
{
	char *cp;
	int   i, ask, entry;
	char  line[1024];

	ask = 0;

	cp = Bios_GetEnvironmentVariable("OSLoadPartition");
	if (cp != NULL) {
		strncpy(line, cp, sizeof(line));
		i = strlen(line);
		cp = Bios_GetEnvironmentVariable("OSLoadFilename");
		if (cp != NULL)
			strncpy(&line[i], cp, sizeof(line) - i -1);
		else
			ask = 1;
	} else
		ask = 1;

	printf("\nOpenBSD/sgi Arcbios boot\n");

	for (entry = 0; entry < argc; entry++)
		printf("arg %d: %s\n", entry, argv[entry]);

	while (1) {
		do {
			printf("Boot: ");
			if (ask) {
				gets(line);
			}
			else
				printf("%s\n", line);
		} while(ask && line[0] == '\0');

		entry = loadfile(line);
		if (entry != -1) {
			((void (*)())entry)(argc, argv);
		}
		ask = 1;
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
	char *errs = 0;
	char buf[4096];

	if ((fd = oopen(fname, 0)) < 0) {
		errs="open err: %s\n";
		goto err;
	}

	/* read the elf header */
	if(oread(fd, (char *)&eh, sizeof(eh)) != sizeof(eh)) {
		goto serr;
	}

	ph = (Elf32_Phdr *) buf;
	olseek(fd, eh.e_phoff, 0);
	if(oread(fd, (char *)ph, 4096) != 4096) {
		goto serr;
	}

	tablebase = 0;

	for(i = 0; i < eh.e_phnum; i++, ph++) {
		if(ph->p_type == PT_LOAD) {
			olseek(fd, ph->p_offset, 0);
			printf("0x%x:0x%x, ",ph->p_paddr, ph->p_filesz);
			if(oread(fd, (char *)ph->p_paddr, ph->p_filesz) !=  ph->p_filesz) {
				goto serr;
			}
			if(ph->p_memsz > ph->p_filesz) {
				printf("Zero 0x%x:0x%x, ",
					ph->p_paddr + ph->p_filesz,
					ph->p_memsz - ph->p_filesz);
				bzero((void *)(ph->p_paddr + ph->p_filesz),
					ph->p_memsz - ph->p_filesz);
			}
			if((ph->p_paddr + ph->p_memsz) > tablebase) {
				tablebase = ph->p_paddr + ph->p_memsz;
			}
		}
	}
	printf("start at 0x%x\n", eh.e_entry);
	memset(tablebase, 0, 4096);
	loadsymtab(fd, &eh, 0);
	return(eh.e_entry);
serr:
	errs = "%s sz err\n";
err:
	printf(errs, fname);
	return (-1);
}
int
loadsymtab(int fd, Elf32_Ehdr *eh, int flags)
{
	Elf32_Ehdr *keh;
	Elf32_Shdr *shtab;
	Elf32_Shdr *sh, *ksh, *shstrh, *strh;
	Elf32_Sym *symtab;
	int *symptr;
	char *shstrtab, *strtab, *symend;
	int i, nsym, offs, size;

	size =  eh->e_shnum * sizeof(Elf32_Shdr);
	shtab = (Elf32_Shdr *) alloc(size);
	if (olseek (fd, eh->e_shoff, SEEK_SET) != eh->e_shoff ||
	    oread (fd, shtab, size) != size) {
		printf("Seek to section headers failed.\n");
		return -1;
        }

	tablebase = roundup(tablebase, sizeof(long));
	symptr = (int *)tablebase;
	tablebase = roundup(tablebase, 4096);
	keh = (Elf32_Ehdr *)tablebase;
	tablebase += sizeof(Elf32_Ehdr);
	tablebase = roundup(tablebase, sizeof(long));
	ksh = (Elf32_Shdr *)tablebase;
	tablebase += roundup((sizeof(Elf32_Shdr) * eh->e_shnum), sizeof(long));
	memcpy(ksh, shtab, roundup((sizeof(Elf32_Shdr) * eh->e_shnum), sizeof(long)));
	sh = ksh;

	shstrh = &sh[eh->e_shstrndx];

	for (i = 0; i < eh->e_shnum; sh++, i++) {
		if (sh->sh_type == SHT_SYMTAB) {
			break;
		}
	}
	if (i >= eh->e_shnum) {
		printf("No symbol table found!\n");
		return -1;
	}

	strh = &ksh[sh->sh_link];
	nsym = sh->sh_size / sh->sh_entsize;
	offs = sh->sh_offset;
	size = sh->sh_size;

	/*
	 *  Allocate tables in correct order so the kernel grooks it.
	 *  Then we read them in the order they are in the ELF file.
	 */
	shstrtab = gettable(shstrh->sh_size, "shstrtab", flags);
	strtab = gettable(strh->sh_size, "strtab", flags);
	symtab = gettable(size, "symtab", flags);
	symend = (char *)symtab + size;

        do {
		if(shstrh->sh_offset < offs && shstrh->sh_offset < strh->sh_offset) {
#if 0
			/*
			 *  We would like to read the shstrtab from the file but since this
			 *  table is located in front of the shtab it is already gone. We can't
			 *  position backwards outside the current segment when using tftp.
			 *  Instead we create the names we need in the string table because
			 *  it can be reconstructed from the info we now have access to.
			 */
			if (!readtable (shstrh->sh_offset, (void *)shstrtab,
					shstrh->sh_size, "shstring", flags)) {
				return(0);
			}
#else
			memset(shstrtab, 0, shstrh->sh_size);
			strncpy(shstrtab + shstrh->sh_name, ".shstrtab", 10);
			strncpy(shstrtab + strh->sh_name, ".strtab", 10);
			strncpy(shstrtab + sh->sh_name, ".symtab", 10);
#endif
			shstrh->sh_offset = 0x7fffffff;
		}

			if (offs < strh->sh_offset && offs < shstrh->sh_offset) {
			if (!(readtable(fd, offs, (void *)symtab, size, "sym", flags))) {
				return (0);
			}
			offs = 0x7fffffff;
		}

		if (strh->sh_offset < offs && strh->sh_offset < shstrh->sh_offset) {
		if (!(readtable (fd, strh->sh_offset, (void *)strtab,
					 strh->sh_size, "string", flags))) {
				return (0);
			}
			strh->sh_offset = 0x7fffffff;
		}
		if (offs == 0x7fffffff && strh->sh_offset == 0x7fffffff &&
		    shstrh->sh_offset == 0x7fffffff) {
			break;
		}
	} while(1);

	/*
	 *  Update the kernel headers with the current info.
	 */
	shstrh->sh_offset = (Elf32_Off)shstrtab - (Elf32_Off)keh;
	strh->sh_offset = (Elf32_Off)strtab - (Elf32_Off)keh;
	sh->sh_offset = (Elf32_Off)symtab - (Elf32_Off)keh;
	memcpy(keh, eh, sizeof(Elf32_Ehdr));
	keh->e_phoff = 0;
	keh->e_shoff = sizeof(Elf32_Ehdr);
	keh->e_phentsize = 0;
	keh->e_phnum = 0;

	symptr[0] = (int)keh;
	symptr[1] = roundup((int)symend, sizeof(int));

	return(0);

}
