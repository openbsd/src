/*	$OpenBSD: boot.c,v 1.3 2004/09/16 18:54:48 pefo Exp $ */

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

int main(int, char **);
void dobootopts(int, char **);
Elf32_Addr loadfile(char *);
Elf32_Addr loadfile32(int, Elf32_Ehdr *);
Elf32_Addr loadfile64(int, Elf64_Ehdr *);
int loadsymtab32(int, Elf32_Ehdr *, int);
int loadsymtab64(int, Elf64_Ehdr *, int);

enum { AUTO_NONE, AUTO_YES, AUTO_NO, AUTO_DEBUG } bootauto = AUTO_NONE;
char *OSLoadPartition = NULL;
char *OSLoadFilename = NULL;

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
gettable (int size, char *name, int flags, size_t align)
{
	long base;
	/* Put table after loaded code to support kernel DDB */
	tablebase = roundup(tablebase, align);
	base = tablebase;
	tablebase += size;
	return (void *) base;
}

/*
 */
int
main(argc, argv)
	int argc;
	char **argv;
{
	char  line[1024];
	int   i;
	Elf32_Addr entry;

	dobootopts(argc, argv);
	if (OSLoadPartition != NULL) {
		strlcpy(line, OSLoadPartition, sizeof(line));
		i = strlen(line);
		if (OSLoadFilename != NULL)
			strlcpy(&line[i], OSLoadFilename, sizeof(line) - i -1);
	} else
		strlcpy("invalid argument setup", line, sizeof(line));

	printf("\nOpenBSD/sgi Arcbios boot\n");

	for (entry = 0; entry < argc; entry++)
		printf("arg %d: %s\n", entry, argv[entry]);

	printf("Boot: %s\n", line);

	entry = loadfile(line);
	if (entry != NULL) {
		((void (*)())entry)(argc, argv);
	}
	printf("Boot FAILED!\n                ");
	Bios_Restart();
}

/*
 *  Decode boot options.
 */
void
dobootopts(int argc, char **argv)
{
	char *cp;
	int i;

	/* XXX Should this be done differently, eg env vs. args? */
	for (i = 1; i < argc; i++) {
		cp = argv[i];
		if (cp == NULL)
			continue;

		if (strncmp(cp, "OSLoadOptions=", 14) == 0) {
			if (strcmp(&cp[14], "auto") == 0)
					bootauto = AUTO_YES;
			else if (strcmp(&cp[14], "single") == 0)
					bootauto = AUTO_NO;
			else if (strcmp(&cp[14], "debug") == 0)
					bootauto = AUTO_DEBUG;
		}
		else if (strncmp(cp, "OSLoadPartition=", 16) == 0)
			OSLoadPartition = &cp[16];
		else if (strncmp(cp, "OSLoadFilename=", 15) == 0)
			OSLoadFilename = &cp[15];
	}
	/* If "OSLoadOptions=" is missing, see if any arg was given */
	if (bootauto == AUTO_NONE && *argv[1] == '/')
		OSLoadFilename = argv[1];
}

/*
 * Open 'filename', read in program and return the entry point or -1 if error.
 */
Elf32_Addr
loadfile(fname)
	register char *fname;
{
	union {
		Elf32_Ehdr eh32;
		Elf64_Ehdr eh64;
	} eh;
	int fd;
	Elf32_Addr entry;

	if ((fd = oopen(fname, 0)) < 0) {
		printf("can't open file %s\n", fname);
		return NULL;
	}

	/* read the ELF header and check that it IS an ELF header */
	if (oread(fd, (char *)&eh, sizeof(eh)) != sizeof(eh)) {
		printf("error: ELF header read error\n");
		return NULL;
	}
	if (!IS_ELF(eh.eh32)) {
		printf("not an elf file\n");
		return NULL;
	}

	/* Determine CLASS */
	if (eh.eh32.e_ident[EI_CLASS] == ELFCLASS32)
		entry = loadfile32(fd, (void *)&eh);
	else if (eh.eh32.e_ident[EI_CLASS] == ELFCLASS64)
		entry = loadfile64(fd, (void *)&eh);
	else {
		printf("unknown ELF class\n");
		return NULL;
	}
	return entry;
}

Elf32_Addr
loadfile32(int fd, Elf32_Ehdr *eh)
{
	char buf[4096];
	Elf32_Phdr *ph;
	int i;

	ph = (Elf32_Phdr *) buf;
	olseek(fd, eh->e_phoff, 0);
	if (oread(fd, (char *)ph, 4096) != 4096) {
		printf("unexpected EOF\n");
		return NULL;
	}

	tablebase = 0;
	printf("Loading ELF32 file\n");

	for (i = 0; i < eh->e_phnum; i++, ph++) {
		if (ph->p_type == PT_LOAD) {
			olseek(fd, ph->p_offset, 0);
			printf("0x%x:0x%x, ",(long)ph->p_paddr, (long)ph->p_filesz);
			if (oread(fd, (char *)ph->p_paddr, ph->p_filesz) !=  ph->p_filesz) {
				printf("unexpected EOF\n");
				return NULL;
			}
			if(ph->p_memsz > ph->p_filesz) {
				printf("Zero 0x%x:0x%x, ",
					(long)(ph->p_paddr + ph->p_filesz),
					(long)(ph->p_memsz - ph->p_filesz));
				bzero((void *)(ph->p_paddr + ph->p_filesz),
					ph->p_memsz - ph->p_filesz);
			}
			if((ph->p_paddr + ph->p_memsz) > tablebase) {
				tablebase = ph->p_paddr + ph->p_memsz;
			}
		}
	}
	memset((void *)tablebase, 0, 4096);
	loadsymtab32(fd, eh, 0);
	printf("Start at 0x%x\n", eh->e_entry);
	return(eh->e_entry);
}

Elf32_Addr
loadfile64(int fd, Elf64_Ehdr *eh)
{
	char buf[4096];
	Elf64_Phdr *ph;
	int i;

	ph = (Elf64_Phdr *) buf;
	olseek(fd, eh->e_phoff, 0);
	if (oread(fd, (char *)ph, 4096) != 4096) {
		printf("unexpected EOF\n");
		return NULL;
	}

	tablebase = 0;
	printf("Loading ELF64 file\n");

	for (i = 0; i < eh->e_phnum; i++, ph++) {
		if (ph->p_type == PT_LOAD) {
			olseek(fd, ph->p_offset, 0);
			printf("0x%llx:0x%llx, ",ph->p_paddr, ph->p_filesz);
			if (oread(fd, (char *)(long)ph->p_paddr, ph->p_filesz) !=  ph->p_filesz) {
				printf("unexpected EOF\n");
				return NULL;
			}
			if(ph->p_memsz > ph->p_filesz) {
				printf("Zero 0x%llx:0x%llx, ",
					ph->p_paddr + ph->p_filesz,
					ph->p_memsz - ph->p_filesz);
				bzero((void *)(long)(ph->p_paddr + ph->p_filesz),
					ph->p_memsz - ph->p_filesz);
			}
			if((ph->p_paddr + ph->p_memsz) > tablebase) {
				tablebase = ph->p_paddr + ph->p_memsz;
			}
		}
	}
	memset((void *)tablebase, 0, 4096);
	loadsymtab64(fd, eh, 0);
	printf("Start at 0x%llx\n", eh->e_entry);
	return(eh->e_entry);
}

int
loadsymtab32(int fd, Elf32_Ehdr *eh, int flags)
{
	Elf32_Ehdr *keh;
	Elf32_Shdr *shtab;
	Elf32_Shdr *sh, *ksh, *shstrh, *strh;
	Elf32_Sym *symtab;
	int *symptr;
	char *shstrtab, *strtab, *symend;
	int i, nsym, offs, size;

	printf("Loading symbol table\n");
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
	shstrtab = gettable(shstrh->sh_size, "shstrtab", flags, sizeof(long));
	strtab = gettable(strh->sh_size, "strtab", flags, sizeof(long));
	symtab = gettable(size, "symtab", flags, sizeof(long));
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

int
loadsymtab64(int fd, Elf64_Ehdr *eh, int flags)
{
	Elf64_Ehdr *keh;
	Elf64_Shdr *shtab;
	Elf64_Shdr *sh, *ksh, *shstrh, *strh;
	Elf64_Sym *symtab;
	u_int64_t *symptr;
	char *shstrtab, *strtab, *symend;
	int i, nsym;
	Elf64_Xword size;
	Elf64_Off offs;

	printf("Loading symbol table\n");
	size =  eh->e_shnum * sizeof(Elf64_Shdr);
	shtab = (Elf64_Shdr *) alloc(size);
	if (olseek (fd, (int)eh->e_shoff, SEEK_SET) != (int)eh->e_shoff ||
	    oread (fd, shtab, size) != size) {
		printf("Seek to section headers failed.\n");
		return -1;
        }

	tablebase = roundup(tablebase, sizeof(u_int64_t));
	symptr = (u_int64_t *)tablebase;
	tablebase = roundup(tablebase, 4096);
	keh = (Elf64_Ehdr *)tablebase;
	tablebase += sizeof(Elf64_Ehdr);
	tablebase = roundup(tablebase, sizeof(u_int64_t));
	ksh = (Elf64_Shdr *)tablebase;
	tablebase += roundup((sizeof(Elf64_Shdr) * eh->e_shnum), sizeof(u_int64_t));
	memcpy(ksh, shtab, roundup((sizeof(Elf64_Shdr) * eh->e_shnum), sizeof(u_int64_t)));
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
	shstrtab = gettable(shstrh->sh_size, "shstrtab", flags, sizeof(u_int64_t));
	strtab = gettable(strh->sh_size, "strtab", flags, sizeof(u_int64_t));
	symtab = gettable(size, "symtab", flags, sizeof(u_int64_t));
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
	shstrh->sh_offset = (Elf64_Off)(long)shstrtab - (Elf64_Off)(long)keh;
	strh->sh_offset = (Elf64_Off)(long)strtab - (Elf64_Off)(long)keh;
	sh->sh_offset = (Elf64_Off)(long)symtab - (Elf64_Off)(long)keh;
	memcpy(keh, eh, sizeof(Elf64_Ehdr));
	keh->e_phoff = 0;
	keh->e_shoff = sizeof(Elf64_Ehdr);
	keh->e_phentsize = 0;
	keh->e_phnum = 0;

	symptr[0] = (Elf64_Off)keh;
	symptr[1] = (Elf64_Off)roundup((Elf64_Off)symend, sizeof(u_int64_t));

	return(0);

}
