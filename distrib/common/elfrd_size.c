#include <sys/types.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <nlist.h>

#include <errno.h>
#include <limits.h>

#include <sys/exec_elf.h>

#include "elfrdsetroot.h"

void *
ELFNAME(locate_image)(int, struct elfhdr *,  char *, long *, long *, off_t *,
    size_t *);
int
ELFNAME(find_rd_root_image)(char *, int, Elf_Phdr *, int, long *, long *,
    off_t *, size_t *);

struct elf_fn ELFDEFNNAME(fn) =
{
	ELFNAME(locate_image),
	ELFNAME(find_rd_root_image)
};

void *
ELFNAME(locate_image)(int fd, struct elfhdr *ghead,  char *file,
    long *prd_root_size_off, long *prd_root_image_off, off_t *pmmap_off,
    size_t *pmmap_size)
{
	int n;
	int found = 0;
	size_t phsize;
	Elf_Ehdr head;

	Elf_Phdr *ph;

	/* elfhdr may not have the full header? */
	lseek(fd, 0, SEEK_SET);

	if (read(fd, &head, sizeof(head)) != sizeof(head)) {
		fprintf(stderr, "%s: can't read phdr area\n", file);
		exit(1);
	}

	phsize = head.e_phnum * sizeof(Elf_Phdr);
	ph = malloc(phsize);


	lseek(fd, head.e_phoff, SEEK_SET);

	if (read(fd, ph, phsize) != phsize) {
		fprintf(stderr, "%s: can't read phdr area\n", file);
		exit(1);
	}

        for (n = 0; n < head.e_phnum && !found; n++) {
                if (ph[n].p_type == PT_LOAD)
                        found = ELFNAME(find_rd_root_image)(file, fd, &ph[n],
                            n, prd_root_size_off, prd_root_image_off,
			    pmmap_off, pmmap_size);
        }
        if (!found) {
                fprintf(stderr, "%s: can't locate space for rd_root_image!\n",
                    file);
                exit(1);
        }
	free(ph);
}

struct nlist ELFNAME(wantsyms)[] = {
        { "_rd_root_size", 0 },
	{ "_rd_root_image", 0 },
	{ NULL, 0 }
};

int
ELFNAME(find_rd_root_image)(char *file, int fd, Elf_Phdr *ph, int segment,
    long *prd_root_size_off, long *prd_root_image_off, off_t *pmmap_off,
    size_t *pmmap_size)
{
	unsigned long kernel_start, kernel_size;
	uint64_t rd_root_size_off, rd_root_image_off;

	if (ELFNAME(nlist)(fd, ELFNAME(wantsyms))) {
		fprintf(stderr, "%s: no rd_root_image symbols?\n", file);
		exit(1);
	}
	kernel_start = ph->p_paddr;
	kernel_size = ph->p_filesz;

	rd_root_size_off = ELFNAME(wantsyms)[0].n_value - kernel_start;
	rd_root_size_off -= (ph->p_vaddr - ph->p_paddr);
	rd_root_image_off = ELFNAME(wantsyms)[1].n_value - kernel_start;
	rd_root_image_off -= (ph->p_vaddr - ph->p_paddr);

	if (debug) {
		fprintf(stderr, "segment %d rd_root_size_off = 0x%x\n", segment,
		    rd_root_size_off);
		if ((ph->p_vaddr - ph->p_paddr) != 0)
			fprintf(stderr, "root_off v %x p %x, diff %x altered %x\n",
			    ph->p_vaddr, ph->p_paddr,
			    (ph->p_vaddr - ph->p_paddr),
			    rd_root_size_off - (ph->p_vaddr - ph->p_paddr));
		fprintf(stderr, "rd_root_image_off = 0x%x\n", rd_root_image_off);
	}

	/*
	 * Sanity check locations of db_* symbols
	 */
	if (rd_root_image_off < 0 || rd_root_image_off >= kernel_size)
		return (0);
	if (rd_root_size_off < 0 || rd_root_size_off >= kernel_size) {
		fprintf(stderr, "%s: rd_root_size not in data segment?\n",
		    file);
		return (0);
	}
	*pmmap_off = ph->p_offset;
	*pmmap_size = kernel_size;
	*prd_root_size_off = rd_root_size_off;
	*prd_root_image_off = rd_root_image_off;
	return (1);
}

/*
 * __elf_is_okay__ - Determine if ehdr really
 * is ELF and valid for the target platform.
 *
 * WARNING:  This is NOT a ELF ABI function and
 * as such its use should be restricted.
 */
int
ELFNAME(__elf_is_okay__)(Elf_Ehdr *ehdr)
{
	int retval = 0;
	/*
	 * We need to check magic, class size, endianess,
	 * and version before we look at the rest of the
	 * Elf_Ehdr structure.  These few elements are
	 * represented in a machine independent fashion.
	 */
	if (IS_ELF(*ehdr) &&
	    ehdr->e_ident[EI_DATA] == ELF_TARG_DATA &&
	    ehdr->e_ident[EI_VERSION] == ELF_TARG_VER) {

#if 0		/* allow cross, no arch check */
		/* Now check the machine dependent header */
		if (ehdr->e_machine == ELF_TARG_MACH &&
		    ehdr->e_version == ELF_TARG_VER)
#endif
			retval = 1;
	}

	return retval;
}

#define ISLAST(p)       (p->n_name == 0 || p->n_name[0] == 0)
#define MIN(x, y)	((x)<(y)? (x) : (y))


int
ELFNAME(nlist)(int fd, struct nlist *list)
{
	struct nlist *p;
	caddr_t strtab;
	Elf_Off symoff = 0, symstroff = 0;
	Elf_Word symsize = 0;
	long symstrsize = 0;
	Elf_Sword nent, cc, i;
	Elf_Sym sbuf[1024];
	Elf_Sym *s;
	Elf_Ehdr ehdr;
	Elf_Shdr *shdr = NULL;
	size_t shdr_size;
	struct stat st;
	int usemalloc = 0;
	size_t left, len;

	/* Make sure obj is OK */
	if (pread(fd, &ehdr, sizeof(Elf_Ehdr), (off_t)0) != sizeof(Elf_Ehdr) ||
	    !ELFNAME(__elf_is_okay__)(&ehdr) || fstat(fd, &st) < 0)
		return (-1);

	/* calculate section header table size */
	shdr_size = ehdr.e_shentsize * ehdr.e_shnum;

	/* Make sure it's not too big to mmap */
	if (SIZE_MAX - ehdr.e_shoff < shdr_size ||
	    S_ISREG(st.st_mode) && ehdr.e_shoff + shdr_size > st.st_size) {
		errno = EFBIG;
		return (-1);
	}

	/* mmap section header table */
	shdr = (Elf_Shdr *)mmap(NULL, (size_t)shdr_size, PROT_READ,
	    MAP_SHARED|MAP_FILE, fd, (off_t) ehdr.e_shoff);
	if (shdr == MAP_FAILED) {
		usemalloc = 1;
		if ((shdr = malloc(shdr_size)) == NULL)
			return (-1);

		if (pread(fd, shdr, shdr_size, (off_t)ehdr.e_shoff) !=
		    shdr_size) {
			free(shdr);
			return (-1);
		}
	}

	/*
	 * Find the symbol table entry and its corresponding
	 * string table entry.	Version 1.1 of the ABI states
	 * that there is only one symbol table but that this
	 * could change in the future.
	 */
	for (i = 0; i < ehdr.e_shnum; i++) {
		if (shdr[i].sh_type == SHT_SYMTAB) {
			if (shdr[i].sh_link >= ehdr.e_shnum)
				continue;
			symoff = shdr[i].sh_offset;
			symsize = shdr[i].sh_size;
			symstroff = shdr[shdr[i].sh_link].sh_offset;
			symstrsize = shdr[shdr[i].sh_link].sh_size;
			break;
		}
	}

	/* Flush the section header table */
	if (usemalloc)
		free(shdr);
	else
		munmap((caddr_t)shdr, shdr_size);

	/*
	 * clean out any left-over information for all valid entries.
	 * Type and value defined to be 0 if not found; historical
	 * versions cleared other and desc as well.  Also figure out
	 * the largest string length so don't read any more of the
	 * string table than we have to.
	 *
	 * XXX clearing anything other than n_type and n_value violates
	 * the semantics given in the man page.
	 */
	nent = 0;
	for (p = list; !ISLAST(p); ++p) {
		p->n_type = 0;
		p->n_other = 0;
		p->n_desc = 0;
		p->n_value = 0;
		++nent;
	}

	/* Don't process any further if object is stripped. */
	/* ELFism - dunno if stripped by looking at header */
	if (symoff == 0)
		return nent;

	/* Check for files too large to mmap. */
	if (SIZE_MAX - symstrsize < symstroff ||
	    S_ISREG(st.st_mode) && symstrsize + symstroff > st.st_size) {
		errno = EFBIG;
		return (-1);
	}

	/*
	 * Map string table into our address space.  This gives us
	 * an easy way to randomly access all the strings, without
	 * making the memory allocation permanent as with malloc/free
	 * (i.e., munmap will return it to the system).
	 */
	if (usemalloc) {
		if ((strtab = malloc(symstrsize)) == NULL)
			return (-1);
		if (pread(fd, strtab, symstrsize, (off_t)symstroff) !=
		    symstrsize) {
			free(strtab);
			return (-1);
		}
	} else {
		strtab = mmap(NULL, (size_t)symstrsize, PROT_READ,
		    MAP_SHARED|MAP_FILE, fd, (off_t) symstroff);
		if (strtab == MAP_FAILED)
			return (-1);
	}

	while (symsize >= sizeof(Elf_Sym)) {
		cc = MIN(symsize, sizeof(sbuf));
		if (pread(fd, sbuf, cc, (off_t)symoff) != cc)
			break;
		symsize -= cc;
		symoff += cc;
		for (s = sbuf; cc > 0; ++s, cc -= sizeof(*s)) {
			Elf_Word soff = s->st_name;

			if (soff == 0 || soff >= symstrsize)
				continue;
			left = symstrsize - soff;

			for (p = list; !ISLAST(p); p++) {
				char *sym;

				/*
				 * First we check for the symbol as it was
				 * provided by the user. If that fails
				 * and the first char is an '_', skip over
				 * the '_' and try again.
				 * XXX - What do we do when the user really
				 *       wants '_foo' and there are symbols
				 *       for both 'foo' and '_foo' in the
				 *	 table and 'foo' is first?
				 */
				sym = p->n_name;
				len = strlen(sym);

				if ((len >= left ||
				    strcmp(&strtab[soff], sym) != 0) &&
				    (sym[0] != '_' || len - 1 >= left ||
				     strcmp(&strtab[soff], sym + 1) != 0))
					continue;

				p->n_value = s->st_value;

				/* XXX - type conversion */
				/*	 is pretty rude. */
				switch(ELF_ST_TYPE(s->st_info)) {
				case STT_NOTYPE:
					switch (s->st_shndx) {
					case SHN_UNDEF:
						p->n_type = N_UNDF;
						break;
					case SHN_ABS:
						p->n_type = N_ABS;
						break;
					case SHN_COMMON:
						p->n_type = N_COMM;
						break;
					default:
						p->n_type = N_COMM | N_EXT;
						break;
					}
					break;
				case STT_OBJECT:
					p->n_type = N_DATA;
					break;
				case STT_FUNC:
					p->n_type = N_TEXT;
					break;
				case STT_FILE:
					p->n_type = N_FN;
					break;
				}
				if (ELF_ST_BIND(s->st_info) == STB_LOCAL)
					p->n_type = N_EXT;
				p->n_desc = 0;
				p->n_other = 0;
				if (--nent <= 0)
					break;
			}
		}
	}
elf_done:
	if (usemalloc)
		free(strtab);
	else
		munmap(strtab, symstrsize);
	return (nent);
}
