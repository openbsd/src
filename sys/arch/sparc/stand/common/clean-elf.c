/*	$OpenBSD: clean-elf.c,v 1.1 2002/08/11 12:19:47 art Exp $	*/
/*
 * Public domain. I don't even want my name on this.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <err.h>
#include <elf_abi.h>
#include <fcntl.h>

int
elf_is_okay(Elf_Ehdr *ehdr)
{
	int retval = 0;

	/*
	 * We need to check magic, class size, endianess,
	 * and version before we look at the rest of the
	 * Elf_Ehdr structure.  These few elements are
	 * represented in a machine independant fashion.
	 */

	if (IS_ELF(*ehdr) &&
	    ehdr->e_ident[EI_CLASS] == ELF_TARG_CLASS &&
	    ehdr->e_ident[EI_DATA] == ELF_TARG_DATA &&
	    ehdr->e_ident[EI_VERSION] == ELF_TARG_VER) {
		/* Now check the machine dependant header */
		if (ehdr->e_machine == ELF_TARG_MACH &&
		    ehdr->e_version == ELF_TARG_VER)
			retval = 1;
	}

	return retval;
}

void
cleanit(caddr_t addr)
{
	Elf_Ehdr *ehdr;
	Elf_Shdr *shdr;
	char *strtab;
	int i;

	ehdr = (Elf_Ehdr *)addr;
	shdr = (Elf_Shdr *)(addr + ehdr->e_shoff);

	strtab = shdr[ehdr->e_shstrndx].sh_offset + addr;

	/*
	 * Simple. find a .text section, verify that the next section is
	 * .rodata and merge them.
	 */

	for (i = 0; i < ehdr->e_shnum; i++) {
		Elf_Shdr *t = &shdr[i];
		Elf_Shdr *r = &shdr[i + 1];

#if 0
		printf("foo: %s %d\n", strtab + t->sh_name, t->sh_type);
		continue;
#endif
		if (strcmp(strtab + t->sh_name, ".text"))
			continue;
		if (strcmp(strtab + r->sh_name, ".rodata"))
			errx(1, "sorry, rodata merge not possible.");

		t->sh_size += r->sh_size + (r->sh_offset - t->sh_offset - t->sh_size);
		r->sh_type = SHT_NULL;
		r->sh_name = 0;
	}
}

int
main(int argc, char **argv)
{
	int fd;
	void *addr;
	off_t len;
	int pgsz = getpagesize();

	if (argc != 2)
		errx(1, "usage");

	if ((fd = open(argv[1], O_RDWR)) < 0)
		err(1, "open");

	if ((len = lseek(fd, 0, SEEK_END)) < 0)
		err(1, "lseek");

	len = ((len + pgsz - 1) & ~(pgsz - 1));

	if ((addr = mmap(0, len, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0)) ==
	    MAP_FAILED)
		err(1, "mmap");

	if (!elf_is_okay((Elf_Ehdr *)addr))
		errx(1, "not an elf file");

	cleanit(addr);

	msync(addr, len, MS_SYNC|MS_INVALIDATE);

	close(fd);
	return (0);
}

