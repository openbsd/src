/* $OpenBSD: prebind.c,v 1.9 2007/09/02 15:19:20 deraadt Exp $ */
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
#include <sys/stat.h>
#include <sys/syslimits.h>
#include <sys/param.h>
#include <sys/mman.h>
#include <errno.h>
#include <fcntl.h>
#include <nlist.h>
#include <elf_abi.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <err.h>
#include "resolve.h"
#include "link.h"
#include "sod.h"
#ifndef __mips64__
#include "machine/reloc.h"
#endif
#include "prebind.h"
#include "ld.h"

/* seems to make sense to limit how big of file can be dealt with */
#define MAX_FILE_SIZE (512 * 1024 * 1024)

char *shstrtab;

/* alpha uses RELOC_JMP_SLOT */
#ifdef __amd64__
#define RELOC_JMP_SLOT	R_X86_64_JUMP_SLOT
#endif
#ifdef __arm__
#define RELOC_JMP_SLOT	R_ARM_JUMP_SLOT
#endif
#ifdef __hppa__
#define RELOC_JMP_SLOT	RELOC_IPLT
#endif
#ifdef __hppa64__
#define RELOC_JMP_SLOT	RELOC_JMPSLOT
#endif
#ifdef __i386__
#define RELOC_JMP_SLOT	RELOC_JUMP_SLOT
#endif
#ifdef __sh__
#define RELOC_JMP_SLOT	R_SH_JMP_SLOT
#endif
#ifdef __mips64__
#define RELOC_JMP_SLOT	0		/* XXX mips64 doesnt have PLT reloc */
#endif
/* powerpc uses RELOC_JMP_SLOT */
/* sparc uses RELOC_JMP_SLOT */
/* sparc64 uses RELOC_JMP_SLOT */
#if defined(__sparc__) && !defined(__sparc64__)
/* ARGH, our sparc/include/reloc.h is wrong (for the moment) */
#undef RELOC_JMP_SLOT
#define RELOC_JMP_SLOT 21
#endif

#define BUFSZ (256 * 1024)

#include "prebind_struct.h"
struct proglist *curbin;

obj_list_ty library_list =
    TAILQ_HEAD_INITIALIZER(library_list);

prog_list_ty prog_list =
    TAILQ_HEAD_INITIALIZER(prog_list);

struct objarray_list {
	struct elf_object *obj;
	struct symcache_noflag *symcache;
	struct symcache_noflag *pltsymcache;
	struct proglist *proglist;
	u_int32_t id0;
	u_int32_t id1;
	u_int32_t *idxtolib;
	void *oprebind_data;
	int	numlibs;

	TAILQ_HEAD(, objlist) inst_list;
} *objarray;


struct prebind_info {
	struct elf_object *object;
	struct prebind_footer *footer;
	u_int32_t footer_offset;
	u_int32_t nfixup;
	struct nameidx *nameidx;
	struct symcachetab *symcache;
	struct symcachetab *pltsymcache;
	u_int32_t *fixuptab;
	u_int32_t *fixupcnt;
	struct fixup **fixup;
	u_int32_t *maptab;
	u_int32_t **libmap;
	u_int32_t *libmapcnt;
	char *nametab;
	u_int32_t nametablen;
};

int	objarray_cnt;
int	objarray_sz;

void	copy_oldsymcache(int objidx, void *prebind_data);
void	elf_load_existing_prebind(struct elf_object *object, int fd);

struct elf_object * elf_load_object(void *pexe, const char *name);
void elf_free_object(struct elf_object *object);
void map_to_virt(Elf_Phdr *, Elf_Ehdr *, Elf_Addr, u_long *);
int load_obj_needed(struct elf_object *object);
int load_lib(const char *name, struct elf_object *parent);
elf_object_t * elf_load_shlib_hint(struct sod *sod, struct sod *req_sod,
    int use_hints, const char *libpath);
char * elf_find_shlib(struct sod *sodp, const char *searchpath,
    int nohints);
elf_object_t * elf_tryload_shlib(const char *libname);
int elf_match_file(struct sod *sodp, char *name, int namelen);
void elf_init_objarray(void);
void elf_add_object(struct elf_object *object, int objtype);
void elf_print_objarray(void);
void elf_reloc(struct elf_object *object);

struct elf_object * elf_lookup_object(const char *name);
struct elf_object * elf_lookup_object_devino(dev_t dev, ino_t inode,
	    int objtype);
void	elf_free_curbin_list(struct elf_object *obj);
void	elf_resolve_curbin(void);
struct proglist *elf_newbin(void);
void	elf_sum_reloc();
int	elf_prep_lib_prebind(struct elf_object *object);
int	elf_prep_bin_prebind(struct proglist *pl);
void	add_fixup_prog(struct elf_object *prog, struct elf_object *obj, int idx,
	    const struct elf_object *ref_obj, const Elf_Sym *ref_sym, int flag);
void	add_fixup_oldprog(struct elf_object *prog, struct elf_object *obj,
	    int idx, const struct elf_object *ref_obj, const Elf_Sym *ref_sym,
	    int flag);

void	elf_dump_footer(struct prebind_footer *footer);

void	elf_fixup_prog_load(int fd, struct prebind_footer *footer,
	    struct elf_object *object);
void	elf_clear_prog_load(int fd, struct elf_object *object);

void	elf_find_symbol_rel(const char *s, struct elf_object *object,
	    Elf_Rel *rel, struct symcache_noflag *symcache,
	    struct symcache_noflag *pltsymcache);

void	elf_find_symbol_rela(const char *s, struct elf_object *object,
	    Elf_RelA *rela, struct symcache_noflag *symcache,
	    struct symcache_noflag *pltsymcache);

int	elf_find_symbol_obj(elf_object_t *object, const char *name,
	    unsigned long hash, int flags, const Elf_Sym **this,
	    const Elf_Sym **weak_sym, elf_object_t **weak_object);

int prebind_writefile(int fd, struct prebind_info *info);
int prebind_writenewfile(int infd, char *name, struct stat *st, off_t orig_size,
    struct prebind_info *info);

struct elf_object *load_object;

struct elf_object *load_file(const char *filename, int lib);
int elf_check_note(void *buf, Elf_Phdr *phdr);
void load_file_or_dir(char *name);
void load_dir(char *name);
void load_exe(char *name);

int
prebind(char **argv)
{
	int i;

	elf_init_objarray();

	for (i = 0; argv[i]; i++)
		load_file_or_dir(argv[i]);

	if (verbose > 4) {
		elf_print_objarray();
		elf_print_prog_list(&prog_list);
	}
	elf_sum_reloc();

	return (0);
}

/*
 * load ELF objects at the specified path it could be
 * either a either a directory or file, if the object is
 * a file, attempt to load it as an executable (will ignore shared objects
 * and any files that are not Elf execuables.
 * if the object is a directory pass it to a routine to deal with
 * directory parsing.
 */
void
load_file_or_dir(char *name)
{
	struct stat sb;
	int ret;

	ret = lstat(name, &sb);
	if (ret != 0)
		return;
	switch (sb.st_mode & S_IFMT) {
	case S_IFREG:
		load_exe(name);
		break;
	case S_IFDIR:
		if (verbose > 0)
			printf("loading dir %s\n", name);
		load_dir(name);
		break;
	default:
		; /* links and other files we skip */
	}

}

/*
 * for all of the objects in the directory, if it is a regular file
 * load it as a binary, if it is unknown (nfs mount) stat the file
 * and load the file for S_IFREG
 * any other type of directory object: symlink, directory, socket, ...
 * is ignored.
 */
void
load_dir(char *name)
{
	struct dirent *dp;
	struct stat sb;
	DIR *dirp;
	char *buf;

	dirp = opendir(name);

	/* if dir failes to open, skip */
	if (dirp == NULL)
		return;

	while ((dp = readdir(dirp)) != NULL) {
		switch (dp->d_type) {
		case DT_UNKNOWN:
			/*
			 * NFS will return unknown, since load_file
			 * does stat the file, this just
			 */
			asprintf(&buf, "%s/%s", name, dp->d_name);
			lstat(buf, &sb);
			if (sb.st_mode == S_IFREG)
				load_exe(buf);
			free(buf);
			break;
		case DT_REG:
			asprintf(&buf, "%s/%s", name, dp->d_name);
			load_exe(buf);
			free(buf);
			break;
		default:
			/* other files symlinks, dirs, ... we ignore */
			;
		}
	}
}

/*
 * the given pathname is a regular file, however it may or may not
 * be an ELF file. Attempt to load the given path and calculate prebind
 * data for it.
 * if the given file is not a ELF binary this will 'fail' and
 * should not change any of the prebind state.
 */
void
load_exe(char *name)
{
	struct elf_object *object;
	struct elf_object *interp;
	struct objlist *ol;
	int fail = 0;

	curbin = elf_newbin();
	if (verbose > 0)
		printf("processing %s\n", name);
	object = load_file(name, OBJTYPE_EXE);
	if (object != NULL && load_object != NULL &&
	    object->load_object == NULL) {
		TAILQ_FOREACH(ol, &(curbin->curbin_list), list) {
			fail = load_obj_needed(ol->object);
			if (fail != 0)
				break; /* XXX */

		}
		if (fail == 0) {
			interp = load_file(curbin->interp, OBJTYPE_DLO);
			object->load_object = interp;
			if (interp == NULL)
				fail = 1;
		}

		/* slight abuse of this field */

		if (fail == 0) {
			objarray[object->dyn.null].proglist = curbin;
			elf_resolve_curbin();
			TAILQ_INSERT_TAIL(&prog_list, curbin, list);
		} else {
			printf("failed to load %s\n", name);
			elf_free_curbin_list(object);
			free(curbin);
		}
		if (load_object != NULL) {
			load_object = NULL;
		}
	} else {
		free(curbin);
	}
}

/*
 * given a path to a file, attempt to open it and load any data necessary
 * for prebind. this function is used for executables, libraries and ld.so
 * file, it will do a lookup on the dev/inode to use a cached version
 * of the file if it was already loaded, in case a library is referenced
 * by more than one program or there are hardlinks between executable names.
 * if the file is not an elf file of the appropriate type, it will return
 * failure.
 */
struct elf_object *
load_file(const char *filename, int objtype)
{
	struct elf_object *obj = NULL;
	int fd = -1, i, note_found;
	struct stat ifstat;
	void *buf = NULL;
	Elf_Ehdr *ehdr;
	Elf_Shdr *shdr;
	Elf_Phdr *phdr;
	char *pexe;

	fd = open(filename, O_RDONLY);
	if (fd == -1) {
		perror(filename);
		goto done;
	}

	if (fstat(fd, &ifstat) == -1) {
		perror(filename);
		goto done;
	}

	if ((ifstat.st_mode & S_IFMT) != S_IFREG)
		goto done;

	if (ifstat.st_size < sizeof (Elf_Ehdr))
		goto done;

	obj = elf_lookup_object_devino(ifstat.st_dev, ifstat.st_ino, objtype);
	if (obj != NULL)
		goto done;

	buf = mmap(NULL, ifstat.st_size, PROT_READ, MAP_FILE | MAP_SHARED,
	    fd, 0);
	if (buf == MAP_FAILED) {
		printf("%s: cannot mmap\n", filename);
		goto done;
	}

	ehdr = (Elf_Ehdr *)buf;

	if (IS_ELF(*ehdr) == 0)
		goto done;

	if (ehdr->e_machine != ELF_TARG_MACH) {
		if (verbose > 0)
			printf("%s: wrong arch\n", filename);
		goto done;
	}

	if (objtype == OBJTYPE_EXE) {
		if (ehdr->e_type != ET_EXEC)
			goto done;

		note_found = 0;

		phdr = (Elf_Phdr *)((char *)buf + ehdr->e_phoff);
		for (i = 0; i < ehdr->e_phnum; i++) {
			if (phdr[i].p_type == PT_NOTE) {
				note_found = elf_check_note(buf,&phdr[i]);
				break;
			}
		}
		if (note_found == 0)
			goto done; /* no OpenBSD note found */
	}

	if ((objtype == OBJTYPE_LIB || objtype == OBJTYPE_DLO) &&
	    (ehdr->e_type != ET_DYN))
		goto done;

	pexe = buf;
	if (ehdr->e_shstrndx == 0)
		goto done;

	shdr = (Elf_Shdr *)(pexe + ehdr->e_shoff +
	    (ehdr->e_shstrndx * ehdr->e_shentsize));

	shstrtab = (char *)(pexe + shdr->sh_offset);

	obj = elf_load_object(pexe, filename);

	munmap(buf, ifstat.st_size);
	buf = NULL;

	if (obj != NULL) {
		obj->obj_type = objtype;

		obj->dev = ifstat.st_dev;
		obj->inode = ifstat.st_ino;
		if (load_object == NULL)
			load_object = obj;

		elf_add_object(obj, objtype);

#ifdef DEBUG1
		dump_info(obj);
#endif
	}
	if ((objtype == OBJTYPE_LIB || objtype == OBJTYPE_DLO) &&
	    merge == 1) {
		/*
		 * for libraries and dynamic linker, check if old prebind
		 * info exists and load it if we are in merge mode
		 */
		elf_load_existing_prebind(obj, fd);
	}
done:
	if (buf != NULL)
		munmap(buf, ifstat.st_size);
	if (fd != -1)
		close(fd);
	return obj;
}

/*
 * check if the given executable header on a ELF executable
 * has the proper OpenBSD note on the file if it is not present
 * binaries will be skipped.
 */
int
elf_check_note(void *buf, Elf_Phdr *phdr)
{
	Elf_Ehdr *ehdr;
	u_long address;
	u_int *pint;
	char *osname;

	ehdr = (Elf_Ehdr *)buf;
	address = phdr->p_offset;
	pint = (u_int *)((char *)buf + address);
	osname = (char *)buf + address + sizeof(*pint) * 3;

	if (pint[0] == 8 /* OpenBSD\0 */ &&
	    pint[1] == 4 /* ??? */ &&
	    pint[2] == 1 /* type_osversion */ &&
	    strcmp("OpenBSD", osname) == 0)
		return 1;

	return 0;
}

struct elf_object *
elf_load_object(void *pexe, const char *name)
{
	struct elf_object *object;
	Elf_Dyn *dynp = NULL, *odynp;
	Elf_Ehdr *ehdr;
	Elf_Phdr *phdr;
	const Elf_Sym	*symt;
	const char	*strt;
	Elf_Addr loff;
	Elf_Word *needed_list;
	int needed_cnt = 0, i;

	object = calloc(1, sizeof (struct elf_object));
	if (object == NULL) {
		printf("unable to allocate object for %s\n", name);
		exit(10);
	}
	ehdr = pexe;
	loff = (Elf_Addr)pexe;

	object->load_addr = 0;
	object->load_name = strdup(name);

	phdr = (Elf_Phdr *)((char *)pexe + ehdr->e_phoff);
	for (i = 0; i < ehdr->e_phnum; i++) {
		switch (phdr[i].p_type) {
		case PT_DYNAMIC:
			dynp = (Elf_Dyn *)(phdr[i].p_offset);
			break;
		case PT_INTERP:
			/* XXX can only occur in programs */
			curbin->interp = strdup((char *)((char *)pexe +
			    phdr[i].p_offset));
			break;
		default:
			break;
		}
	}

	if (dynp == 0) {
		free(object);
		return NULL; /* not a dynamic binary */
	}

	dynp = (Elf_Dyn *)((unsigned long)dynp + loff);
	odynp = dynp;
	while (dynp->d_tag != DT_NULL) {
		if (dynp->d_tag < DT_NUM)
			object->Dyn.info[dynp->d_tag] = dynp->d_un.d_val;
		else if (dynp->d_tag >= DT_LOPROC &&
		    dynp->d_tag < DT_LOPROC + DT_PROCNUM)
			object->Dyn.info[dynp->d_tag + DT_NUM - DT_LOPROC] =
			    dynp->d_un.d_val;
		if (dynp->d_tag == DT_TEXTREL)
			object->dyn.textrel = 1;
		if (dynp->d_tag == DT_SYMBOLIC)
			object->dyn.symbolic = 1;
		if (dynp->d_tag == DT_BIND_NOW)
			object->obj_flags = RTLD_NOW;
		if (dynp->d_tag == DT_NEEDED)
			needed_cnt++;

		dynp++;
	}

	needed_list = calloc((needed_cnt + 1), sizeof(Elf_Word));
	if (needed_list == NULL) {
		printf("unable to allocate needed_list for %s\n", name);
		exit(10);
	}
	needed_list[needed_cnt] = 0;
	for (dynp = odynp, i = 0; dynp->d_tag != DT_NULL; dynp++) {
		if (dynp->d_tag == DT_NEEDED) {
			needed_list[i] = dynp->d_un.d_val;
			i++;
		}
	}

	if (object->Dyn.info[DT_HASH])
		map_to_virt(phdr, ehdr, loff, &object->Dyn.info[DT_HASH]);
	if (object->Dyn.info[DT_STRTAB])
		map_to_virt(phdr, ehdr, loff, &object->Dyn.info[DT_STRTAB]);
	if (object->Dyn.info[DT_SYMTAB])
		map_to_virt(phdr, ehdr, loff, &object->Dyn.info[DT_SYMTAB]);

	if (object->Dyn.info[DT_RELA])
		map_to_virt(phdr, ehdr, loff, &object->Dyn.info[DT_RELA]);
	if (object->Dyn.info[DT_RPATH])
		object->Dyn.info[DT_RPATH] += object->Dyn.info[DT_STRTAB];
	if (object->Dyn.info[DT_REL])
		map_to_virt(phdr, ehdr, loff, &object->Dyn.info[DT_REL]);
	if (object->Dyn.info[DT_JMPREL])
		map_to_virt(phdr, ehdr, loff, &object->Dyn.info[DT_JMPREL]);

	symt = object->dyn.symtab;
	strt = object->dyn.strtab;

	{
		Elf_Sym *sym;
		char *str;
		Elf_Rel *rel;
		Elf_RelA *rela;
		Elf_Addr *hash;
		Elf_Word *hashtab;
		void *plt;
		size_t hashsz;

		if (object->Dyn.info[DT_HASH] != 0) {
			hash = object->dyn.hash;
			hashtab = (void *)hash;
			object->nbuckets = hashtab[0];
			object->nchains = hashtab[1];
			hashsz = (2 + object->nbuckets + object->nchains) *
			    sizeof (Elf_Word);
			hash = malloc(hashsz);
			if (hash == NULL) {
				printf("unable to allocate hash for %s\n",
				    name);
				exit(10);
			}
			bcopy(object->dyn.hash, hash, hashsz);
			object->dyn.hash = hash;
			object->buckets = ((Elf_Word *)hash + 2);
			object->chains = object->buckets + object->nbuckets;
		}

		str = malloc(object->dyn.strsz);
		if (str == NULL) {
			printf("unable to allocate strtab for %s\n",
			    name);
			exit(10);
		}
		bcopy(object->dyn.strtab, str, object->dyn.strsz);
		object->dyn.strtab = str;
		strt = str;

		sym = calloc(object->nchains, sizeof(Elf_Sym));
		if (sym == NULL) {
			printf("unable to allocate symtab for %s\n",
			    name);
			exit(10);
		}
		bcopy(object->dyn.symtab, sym,
		    object->nchains * sizeof(Elf_Sym));
		object->dyn.symtab = sym;
		symt = sym;

		if (object->dyn.relsz != 0) {
			rel = malloc(object->dyn.relsz);
			if (rel == NULL) {
				printf("unable to allocate rel reloc for %s\n",
				    name);
				exit(10);
			}
			bcopy(object->dyn.rel, rel, object->dyn.relsz);
			object->dyn.rel = rel;
		} else {
			object->dyn.rel = NULL;
		}
		if (object->dyn.relasz != 0) {
			rela = malloc(object->dyn.relasz);
			if (rela == NULL) {
				printf("unable to allocate rela reloc for %s\n",
				    name);
				exit(10);
			}
			bcopy(object->dyn.rela, rela, object->dyn.relasz);
			object->dyn.rela = rela;
		} else {
			object->dyn.rela = NULL;
		}
		if (object->dyn.pltrelsz != 0) {
			plt = malloc(object->dyn.pltrelsz);
			if (plt == NULL) {
				printf("unable to allocate plt reloc for %s\n",
				    name);
				exit(10);
			}
			bcopy((void*)object->dyn.jmprel, plt,
			    object->dyn.pltrelsz);
			object->dyn.jmprel = (long)plt;
		} else {
			object->dyn.jmprel = NULL;
		}
		if (object->dyn.rpath != NULL){
			object->dyn.rpath = strdup(object->dyn.rpath);
			if (object->dyn.rpath == NULL) {
				printf("unable to allocate rpath for %s\n",
				    name);
				exit(10);
			}
		}
		object->dyn.needed = (Elf_Addr)needed_list;
	}

#ifdef DEBUG1
	dump_info(object);
#endif
	return object;
}

/*
 * Free any extra pieces associated with 'object'
 */
void
elf_free_object(struct elf_object *object)
{
	free(object->load_name);
	if (object->dyn.hash != NULL)
		free(object->dyn.hash);
	free((void *)object->dyn.strtab);
	free((void *)object->dyn.symtab);
	if (object->dyn.rel != NULL)
		free(object->dyn.rel);
	if (object->dyn.rela != NULL)
		free(object->dyn.rela);
	if (object->dyn.rpath != NULL)
		free((void *)object->dyn.rpath);
	free(object);
}

/*
 * translate an object address into a file offset for the
 * file assuming that the file is mapped at base.
 */
void
map_to_virt(Elf_Phdr *phdr, Elf_Ehdr *ehdr, Elf_Addr base, u_long *vaddr)
{
	int i;

	for (i = 0; i < ehdr->e_phnum; i++) {
		switch (phdr[i].p_type) {
		case PT_LOAD:
			if (phdr[i].p_vaddr > *vaddr)
				continue;
			if (phdr[i].p_vaddr + phdr[i].p_memsz < *vaddr)
				continue;
#ifdef DEBUG1
			printf("input address %lx translated to ", *vaddr);
#endif
			*vaddr += phdr[i].p_offset - phdr[i].p_vaddr + base;
#ifdef DEBUG1
			printf("%lx, base %lx %lx %llx\n", *vaddr, base,
			    phdr[i].p_vaddr, phdr[i].p_offset );
#endif
			break;
		default:
			break;
		}
	}
}

/*
 * given a dynamic elf object (executable or binary)
 * load any DT_NEEDED entries which were found when
 * the object was initially loaded.
 */
int
load_obj_needed(struct elf_object *object)
{
	int i;
	Elf_Word *needed_list;
	int err;

	needed_list = (Elf_Word *)object->dyn.needed;
	for (i = 0; needed_list[i] != NULL; i++) {
		if (verbose > 1)
			printf("lib: %s\n", needed_list[i] +
			    object->dyn.strtab);
		err = load_lib(needed_list[i] + object->dyn.strtab, object);
		if (err) {
			printf("failed to load lib %s\n",
			    needed_list[i] + object->dyn.strtab);
			return 1;
		}
	}
	return 0;
}

/*
 * allocate a proglist entry for a new binary
 * so that it is available for libraries to reference
 */
struct proglist *
elf_newbin(void)
{
	struct proglist *proglist;
	proglist = malloc(sizeof (struct proglist));
	if (proglist == NULL) {
		printf("unable to allocate proglist\n");
		exit(10);
	}
	proglist->fixup = NULL;
	TAILQ_INIT(&(proglist->curbin_list));
	return proglist;
}

/*
 * Copy the contents of a libraries symbol cache instance into
 * the 'global' symbol cache for that library
 * this will currently resolve conflicts between mismatched
 * libraries by flagging any mismatches as invalid
 * which will cause all programs to generate a fixup
 * It probably would be interesting to modify this to keep the most
 * common entry as a library cache, and only have a fixup in programs
 * where the symbol is overridden.
 * This is run once each for the (got)symcache and pltsymcache
 */

struct elf_object badobj_store;
struct elf_object *badobj = &badobj_store;

/*
 * copy the symbols found in a library symcache to the 'master/common'
 * symbol table note that this will skip copying the following references
 * 1. non-existing entries
 * 2. symobj == prog &&& obj != prog
 * 3. symobj == prog's interpter (references to dl_open)
 */
void
elf_copy_syms(struct symcache_noflag *tcache, struct symcache_noflag *scache,
    struct elf_object *obj, struct elf_object *prog, int nsyms)
{
	int i;
	int lib_prog_ref;
	for (i = 0; i < nsyms; i++) {
		if (scache[i].obj == NULL)
			continue;

		if (tcache[i].obj != NULL) {
			lib_prog_ref = (obj != prog && scache[i].obj == prog);
			if (scache[i].obj != tcache[i].obj || lib_prog_ref) {
				if (verbose > 2) {
					printf("sym mismatch %d: "
					   "obj %d: sym %ld %s "
					   "nobj %s\n",
					    i, (int)scache[i].obj->dyn.null,
					    scache[i].sym -
					    scache[i].obj->dyn.symtab,
					    scache[i].sym->st_name +
					    scache[i].obj->dyn.strtab,
					    scache[i].obj->load_name);
				}

				/*
				 * if one of the symbol entries
				 * happens to be a self reference
				 * go ahead and keep that reference
				 * prevents some instances of fixups
				 * for every binary, eg one program
				 * overriding malloc() will not make
				 * ever binary have a fixup for libc
				 * references to malloc()
				 */
				if (scache[i].obj == obj) {
					tcache[i].obj = scache[i].obj;
					tcache[i].sym = scache[i].sym;
				} else if (tcache[i].obj == obj) {
					/* no change necessary */
				} else {
					tcache[i].obj = badobj;
					tcache[i].sym = NULL;
				}
			}
		} else {
			if (scache[i].obj != prog) {
				tcache[i].obj = scache[i].obj;
				tcache[i].sym = scache[i].sym;
			}
		}
	}
}

void
insert_sym_objcache(struct elf_object *obj, int idx,
    const struct elf_object *ref_obj, const Elf_Sym *ref_sym, int flags)
{
	struct symcache_noflag *tcache;
	struct elf_object *prog;

	prog = TAILQ_FIRST(&(curbin->curbin_list))->object;

	if (flags)
		tcache = objarray[obj->dyn.null].pltsymcache;
	else
		tcache = objarray[obj->dyn.null].symcache;

	if (tcache[idx].obj != NULL) {
		if (ref_obj != tcache[idx].obj ||
		    (obj != prog && ref_obj == prog)) {
			if (verbose > 2) {
				printf("sym mismatch %d: "
				   "obj %d: sym %ld %s "
				   "nobj %s\n",
				    idx, (int)ref_obj->dyn.null,
				    ref_sym -
				    ref_obj->dyn.symtab,
				    ref_sym->st_name +
				    ref_obj->dyn.strtab,
				    ref_obj->load_name);
			}

			/*
			 * if one of the symbol entries
			 * happens to be a self reference
			 * go ahead and keep that reference
			 * prevents some instances of fixups
			 * for every binary, eg one program
			 * overriding malloc() will not make
			 * ever binary have a fixup for libc
			 * references to malloc()
			 */
			if (ref_obj == obj) {
				tcache[idx].obj = ref_obj;
				tcache[idx].sym = ref_sym;
				add_fixup_oldprog(prog, obj, idx, ref_obj,
				ref_sym, flags);
			} else if (tcache[idx].obj == obj) {
				/* no change necessary */
				add_fixup_prog(prog, obj, idx, ref_obj,
				ref_sym, flags);
			} else {
				add_fixup_oldprog(prog, obj, idx,
				    tcache[idx].obj, tcache[idx].sym, flags);
				tcache[idx].obj = badobj;
				tcache[idx].sym = NULL;
				add_fixup_prog(prog, obj, idx, ref_obj,
				ref_sym, flags);
			}
		}
	} else {
		if (ref_obj != prog) {
			tcache[idx].obj = ref_obj;
			tcache[idx].sym = ref_sym;
		} else {
			add_fixup_prog(prog, obj, idx, ref_obj,
			ref_sym, flags);
		}
	}
}

void
add_fixup_prog(struct elf_object *prog, struct elf_object *obj, int idx,
    const struct elf_object *ref_obj, const Elf_Sym *ref_sym, int flag)
{
	struct proglist *pl;
	int i, libidx, cnt;

	pl = objarray[prog->dyn.null].proglist;

	libidx = -1;
	for (i = 0; i < pl->nobj; i++) {
		if (pl->libmap[0][i] == obj->dyn.null) {
			libidx = (i * 2) + ((flag & SYM_PLT) ? 1 : 0);
			break;
		}
	}
	if (libidx == -1) {
		printf("unable to find object\n");
		return;
	}

	/* have to check for duplicate patches */
	for (i = 0; i < pl->fixupcnt[libidx]; i++) {
		if (pl->fixup[libidx][i].sym == idx)
			return;
	}

	if (verbose > 1)
		printf("fixup for obj %s on prog %s sym %s: %d\n",
		    obj->load_name, prog->load_name,
		    ref_obj->dyn.strtab + ref_sym->st_name,
		    pl->fixupcnt[libidx]);

	if (pl->fixupcntalloc[libidx] < pl->fixupcnt[libidx] + 1) {
		pl->fixupcntalloc[libidx] += 16;
		pl->fixup[libidx] = realloc(pl->fixup[libidx],
		    sizeof (struct fixup) * pl->fixupcntalloc[libidx]);
		if (pl->fixup[libidx] == NULL)  {
			printf("realloc fixup, out of memory\n");
			exit(20);
		}
	}
	cnt = pl->fixupcnt[libidx];
	pl->fixup[libidx][cnt].sym = idx;
	pl->fixup[libidx][cnt].obj_idx = ref_obj->dyn.null;
	pl->fixup[libidx][cnt].sym_idx = ref_sym - ref_obj->dyn.symtab;
	pl->fixupcnt[libidx]++;
}

void
add_fixup_oldprog(struct elf_object *prog, struct elf_object *obj, int idx,
    const struct elf_object *ref_obj, const Elf_Sym *ref_sym, int flag)
{
	struct objlist *ol;

	TAILQ_FOREACH(ol, &(objarray[obj->dyn.null].inst_list), inst_list) {
		if (ol->load_prog == prog) {
			continue;
		}
		/* process here */

		add_fixup_prog(ol->load_prog, obj, idx, ref_obj, ref_sym, flag);
	}

}

struct elf_object *
elf_lookup_object(const char *name)
{
	struct objlist *ol;
	TAILQ_FOREACH(ol, &(curbin->curbin_list), list) {
		if (strcmp (name, ol->object->load_name) == 0) {
			return ol->object;
		}
	}
	TAILQ_FOREACH(ol, &library_list, list) {
		if (strcmp (name, ol->object->load_name) == 0) {
			elf_add_object_curbin_list(ol->object);
			return ol->object;
		}
	}
	return NULL;
}

struct elf_object *
elf_lookup_object_devino(dev_t dev, ino_t inode, int objtype)
{
	struct objlist *ol;
	TAILQ_FOREACH(ol, &(curbin->curbin_list), list) {
		if (ol->object->dev == dev &&
		    ol->object->inode == inode) {
			if (ol->object->obj_type != objtype)
				return NULL;
			return ol->object;
		}
	}
	TAILQ_FOREACH(ol, &library_list, list) {
		if (ol->object->dev == dev &&
		    ol->object->inode == inode) {
			if (ol->object->obj_type != objtype)
				return NULL;
			if (objtype != OBJTYPE_EXE)
				elf_add_object_curbin_list(ol->object);
			return ol->object;
		}
	}
	return NULL;
}

void
elf_find_symbol_rel(const char *name, struct elf_object *object,
    Elf_Rel *rel, struct symcache_noflag *symcache,
    struct symcache_noflag *pltsymcache)
{
	struct objlist *ol;
	unsigned long h = 0;
	const char *p = name;
	const Elf_Sym *sym, *ref_sym = NULL;
	const Elf_Sym *weak_sym = NULL;
	struct elf_object *weak_obj = NULL;
	int flags = 0;
	int found = 0;
	int type, idx;
	struct elf_object *ref_object = NULL;

	sym = object->dyn.symtab + ELF_R_SYM(rel->r_info);

	while (*p) {
		unsigned long g;
		h = (h << 4) + *p++;
		if ((g = h & 0xf0000000))
			h ^= g >> 24;
		h &= ~g;
	}

	type = ELF_R_TYPE(rel->r_info);
	flags = SYM_SEARCH_ALL|SYM_WARNNOTFOUND;
	if (type == RELOC_JMP_SLOT)
		flags |= SYM_PLT;

	TAILQ_FOREACH(ol, &(curbin->curbin_list), list) {
		found = elf_find_symbol_obj(ol->object, name, h, flags, &sym,
		    &weak_sym, &weak_obj);
		if (found) {
			ref_object = ol->object;
			break;
		}

	}
	if (found) {
		ref_object = ol->object;
		ref_sym = sym;
	} else if (weak_obj != NULL) {
		found = 1;
		ref_object = weak_obj;
		ref_sym = weak_sym;
	}
	if (found == 1) {
		idx = ELF_R_SYM(rel->r_info);
		if (flags & SYM_PLT) {
			pltsymcache[idx].obj = ref_object;
			pltsymcache[idx].sym = ref_sym;
		} else {
			symcache[idx].obj = ref_object;
			symcache[idx].sym = ref_sym;
		}
	} else {
		printf("symbol not found %s\n", name);
	}
}

void
elf_find_symbol_rela(const char *name, struct elf_object *object,
    Elf_RelA *rela, struct symcache_noflag *symcache,
    struct symcache_noflag *pltsymcache)
{
	struct objlist *ol;
	unsigned long h = 0;
	const char *p = name;
	const Elf_Sym *sym, *ref_sym = NULL;
	const Elf_Sym *weak_sym = NULL;
	struct elf_object *weak_obj = NULL;
	int flags = 0;
	int found = 0;
	int type, idx;
	struct elf_object *ref_object = NULL;

	sym = object->dyn.symtab + ELF_R_SYM(rela->r_info);

	while (*p) {
		unsigned long g;
		h = (h << 4) + *p++;
		if ((g = h & 0xf0000000))
			h ^= g >> 24;
		h &= ~g;
	}

	type = ELF_R_TYPE(rela->r_info);
	flags = SYM_SEARCH_ALL|SYM_WARNNOTFOUND;
	if (type == RELOC_JMP_SLOT)
		flags |= SYM_PLT;

	TAILQ_FOREACH(ol, &(curbin->curbin_list), list) {

//	printf("searching sym [%s] typ %d in obj %s\n", name, type, ol->object->load_name);
		found = elf_find_symbol_obj(ol->object, name, h, flags, &sym,
		    &weak_sym, &weak_obj);
		if (found) {
			ref_object = ol->object;
			break;
		}

	}
	if (found) {
		ref_object = ol->object;
		ref_sym = sym;
	} else if (weak_obj != NULL) {
		found = 1;
		ref_object = weak_obj;
		ref_sym = weak_sym;
	}
	if (found == 1) {
		idx = ELF_R_SYM(rela->r_info);
		if (flags & SYM_PLT) {
			pltsymcache[idx].obj = ref_object;
			pltsymcache[idx].sym = ref_sym;
		} else {
			symcache[idx].obj = ref_object;
			symcache[idx].sym = ref_sym;
		}
	} else {
		printf("symbol not found %s\n", name);
	}
}

int
elf_find_symbol_obj(elf_object_t *object, const char *name, unsigned long hash,
    int flags, const Elf_Sym **this, const Elf_Sym **weak_sym,
    elf_object_t **weak_object)
{
	const Elf_Sym	*symt = object->dyn.symtab;
	const char	*strt = object->dyn.strtab;
	long	si;
	const char *symn;

	for (si = object->buckets[hash % object->nbuckets];
	    si != STN_UNDEF; si = object->chains[si]) {
		const Elf_Sym *sym = symt + si;

		if (sym->st_value == 0)
			continue;

		if (ELF_ST_TYPE(sym->st_info) != STT_NOTYPE &&
		    ELF_ST_TYPE(sym->st_info) != STT_OBJECT &&
		    ELF_ST_TYPE(sym->st_info) != STT_FUNC)
			continue;

		symn = strt + sym->st_name;
		if (sym != *this && strcmp(symn, name))
			continue;

		/* allow this symbol if we are referring to a function
		 * which has a value, even if section is UNDEF.
		 * this allows &func to refer to PLT as per the
		 * ELF spec. st_value is checked above.
		 * if flags has SYM_PLT set, we must have actual
		 * symbol, so this symbol is skipped.
		 */
		if (sym->st_shndx == SHN_UNDEF) {
			if ((flags & SYM_PLT) || sym->st_value == 0 ||
			    ELF_ST_TYPE(sym->st_info) != STT_FUNC)
				continue;
		}

		if (ELF_ST_BIND(sym->st_info) == STB_GLOBAL) {
			*this = sym;
			return 1;
		} else if (ELF_ST_BIND(sym->st_info) == STB_WEAK) {
			if (!*weak_sym) {
				*weak_sym = sym;
				*weak_object = object;
			}
		}
	}
	return 0;
}

void
elf_reloc(struct elf_object *object)
{
	const Elf_Sym *sym;
	Elf_Rel *rel;
	Elf_RelA *rela;
	int numrel;
	int numrela;
	int i;
	struct symcache_noflag *symcache;
	struct symcache_noflag *pltsymcache;

	numrel = object->dyn.relsz / sizeof(Elf_Rel);
#ifdef DEBUG1
	printf("rel relocations: %d\n", numrel);
#endif
#if 1
	symcache = calloc(sizeof(struct symcache_noflag),
	    object->nchains);
	pltsymcache = calloc(sizeof(struct symcache_noflag),
	    object->nchains);
	if (symcache == NULL || pltsymcache == NULL) {
		printf("unable to allocate memory for cache %s\n",
		object->load_name);
		exit(20);
	}
#endif
	rel = object->dyn.rel;
	for (i = 0; i < numrel; i++) {
		const char *s;
		sym = object->dyn.symtab + ELF_R_SYM(rel[i].r_info);

		/* hppa has entries without names, skip them */
		if (sym->st_name == 0)
			continue;

		s = (ELF_R_SYM(rel[i].r_info) == 0) ? "<rel>" :
		    object->dyn.strtab + sym->st_name;
#ifdef DEBUG1
		printf("%d: %x sym %x %s type %d\n", i, rel[i].r_offset,
		    ELF_R_SYM(rel[i].r_info), s,
		    ELF_R_TYPE(rel[i].r_info));
#endif
		if (ELF_R_SYM(rel[i].r_info) != 0) {
			elf_find_symbol_rel(s, object, &rel[i],
			symcache, pltsymcache);
		}
	}
	if (numrel) {
		numrel = object->dyn.pltrelsz / sizeof(Elf_Rel);
		rel = (Elf_Rel *)(object->Dyn.info[DT_JMPREL]);
#ifdef DEBUG1
		printf("rel plt relocations: %d\n", numrel);
#endif
		for (i = 0; i < numrel; i++) {
			const char *s;
			sym = object->dyn.symtab + ELF_R_SYM(rel[i].r_info);

			/* hppa has entries without names, skip them */
			if (sym->st_name == 0)
				continue;

			s = (ELF_R_SYM(rel[i].r_info) == 0) ? "<rel>" :
			    object->dyn.strtab + sym->st_name;
#ifdef DEBUG1
			printf("%d: %x sym %d %s type %d\n", i, rel[i].r_offset,
			    ELF_R_SYM(rel[i].r_info), s,
			    ELF_R_TYPE(rel[i].r_info));
#endif
			if (ELF_R_SYM(rel[i].r_info) != 0) {
				elf_find_symbol_rel(s, object, &rel[i],
				symcache, pltsymcache);
			}
		}
	}

	numrela = object->dyn.relasz / sizeof(Elf_RelA);
#ifdef DEBUG1
	printf("rela relocations: %d\n", numrela);
#endif
	rela = object->dyn.rela;
	for (i = 0; i < numrela; i++) {
		const char *s;
		sym = object->dyn.symtab + ELF_R_SYM(rela[i].r_info);

			/* hppa has entries without names, skip them */
			if (sym->st_name == 0)
				continue;

		s = (ELF_R_SYM(rela[i].r_info) == 0) ? "<rel>" :
		    object->dyn.strtab + sym->st_name;
#ifdef DEBUG1
		printf("%d: %x sym %x %s type %d\n", i, rela[i].r_offset,
		    ELF_R_SYM(rela[i].r_info), s,
		    ELF_R_TYPE(rela[i].r_info));
#endif
		if (ELF_R_SYM(rela[i].r_info) != 0) {
			elf_find_symbol_rela(s, object, &rela[i],
			symcache, pltsymcache);
		}
	}
	if (numrela) {
		numrela = object->dyn.pltrelsz / sizeof(Elf_RelA);
#ifdef DEBUG1
		printf("rela plt relocations: %d\n", numrela);
#endif
		rela = (Elf_RelA *)(object->Dyn.info[DT_JMPREL]);

		for (i = 0; i < numrela; i++) {
			const char *s;
			sym = object->dyn.symtab + ELF_R_SYM(rela[i].r_info);

			/* hppa has entries without names, skip them */
			if (sym->st_name == 0)
				continue;

			s = (ELF_R_SYM(rela[i].r_info) == 0) ? "<rel>" :
			    object->dyn.strtab + sym->st_name;
#ifdef DEBUG1
			printf("%d: %x sym %x %s type %d\n", i,
			    rela[i].r_offset,
			    ELF_R_SYM(rela[i].r_info), s,
			    ELF_R_TYPE(rela[i].r_info));
#endif
			if (ELF_R_SYM(rela[i].r_info) != 0) {
				elf_find_symbol_rela(s, object, &rela[i],
				symcache, pltsymcache);
			}
		}
	}

	for (i = 0; i < object->nchains; i++)
		if (symcache[i].sym != NULL)
			insert_sym_objcache(object, i, symcache[i].obj,
			    symcache[i].sym, 0);

	for (i = 0; i < object->nchains; i++)
		if (pltsymcache[i].sym != NULL)
			insert_sym_objcache(object, i, pltsymcache[i].obj,
			    pltsymcache[i].sym, SYM_PLT);

	free(symcache);
	free(pltsymcache);
}

void
elf_resolve_curbin(void)
{
	struct objlist *ol;
	int numobj = 0;

#ifdef DEBUG1
	elf_print_curbin_list(curbin);
#endif
	TAILQ_FOREACH(ol, &(curbin->curbin_list), list) {
		numobj++;
	}
	curbin->nobj = numobj;
	curbin->libmap = xcalloc(numobj, sizeof (u_int32_t *));
	curbin->libmap[0] = xcalloc(numobj, sizeof (u_int32_t *));
	curbin->fixup = xcalloc(2 * numobj, sizeof (struct fixup *));
	curbin->fixupcnt = xcalloc(2 * numobj, sizeof (int));
	curbin->fixupcntalloc = xcalloc(2 * numobj, sizeof (int));

	numobj = 0;
	TAILQ_FOREACH(ol, &(curbin->curbin_list), list) {
		curbin->libmap[0][numobj] = ol->object->dyn.null;
		numobj++;
	}
	TAILQ_FOREACH(ol, &(curbin->curbin_list), list) {
		elf_reloc(ol->object);
	}
}

void
elf_add_object_curbin_list(struct elf_object *object)
{
	struct objlist *ol;

	ol = xmalloc(sizeof (struct objlist));
	ol->object = object;
	TAILQ_INSERT_TAIL(&(curbin->curbin_list), ol, list);
	if (load_object == NULL)
		load_object = object;
	ol->load_prog = load_object;

	TAILQ_INSERT_TAIL(&(objarray[object->dyn.null].inst_list), ol,
	    inst_list);
}
void
elf_init_objarray(void)
{
	objarray_sz = 512;
	objarray = xcalloc(sizeof (objarray[0]), objarray_sz);
}

void
elf_sum_reloc()
{
	int numobjs;
	int err = 0;
	struct objlist *ol;
	struct proglist *pl;

	TAILQ_FOREACH(ol, &library_list, list) {
		err += elf_prep_lib_prebind(ol->object);
	}
	TAILQ_FOREACH(pl, &prog_list, list) {
		numobjs = 0;
		TAILQ_FOREACH(ol, &(pl->curbin_list), list) {
			numobjs++;
		}
		pl->nobj = numobjs;
	}

	TAILQ_FOREACH(pl, &prog_list, list)
		err += elf_prep_bin_prebind(pl);

	if (err != 0)
		printf("failures %d\n", err);
}

int
elf_prep_lib_prebind(struct elf_object *object)
{
	int numlibs = 0;
	int ret = 0;
	int i;
	int ref_obj;
	int *libmap;
	int *idxtolib;
	struct nameidx *nameidx;
	char *nametab;
	int nametablen;
	struct symcache_noflag *symcache;
	struct symcache_noflag *pltsymcache;
	struct symcachetab *symcachetab;
	int symcache_cnt = 0;
	struct symcachetab *pltsymcachetab;
	int pltsymcache_cnt = 0;

	symcache = objarray[object->dyn.null].symcache;
	pltsymcache = objarray[object->dyn.null].pltsymcache;
	libmap = xcalloc(objarray_cnt, sizeof (int));
	idxtolib = xcalloc(objarray_cnt, sizeof (int));
	objarray[object->dyn.null].idxtolib = idxtolib;

	for (i = 0; i < objarray_cnt; i++)
		libmap[i] = -1;

	nametablen = 0;
	for (i = 0; i < object->nchains; i++) {
		if (symcache[i].sym == NULL)
			continue;
		ref_obj = symcache[i].obj->dyn.null;
		symcache_cnt++;
		if (libmap[ref_obj] != -1)
			continue;
		libmap[ref_obj] = numlibs;
		idxtolib[numlibs] = ref_obj;
		nametablen += strlen(symcache[i].obj->load_name) + 1;
		numlibs++;
	}
	symcachetab = xcalloc(symcache_cnt , sizeof(struct symcachetab));

	symcache_cnt = 0;
	for (i = 0; i < object->nchains; i++) {
		if (symcache[i].sym == NULL)
			continue;
		symcachetab[symcache_cnt].idx = i;
		symcachetab[symcache_cnt].obj_idx =
		    libmap[symcache[i].obj->dyn.null];
		symcachetab[symcache_cnt].sym_idx =
		    symcache[i].sym - symcache[i].obj->dyn.symtab;
		symcache_cnt++;
	}
	for (i = 0; i < object->nchains; i++) {
		if (pltsymcache[i].sym == NULL)
			continue;
		ref_obj = pltsymcache[i].obj->dyn.null;
		pltsymcache_cnt++;
		if (libmap[ref_obj] != -1)
			continue;
		libmap[ref_obj] = numlibs;
		idxtolib[numlibs] = ref_obj;
		nametablen += strlen(pltsymcache[i].obj->load_name) + 1;
		numlibs++;
	}
	pltsymcachetab = xcalloc(pltsymcache_cnt , sizeof(struct symcachetab));

	pltsymcache_cnt = 0;
	for (i = 0; i < object->nchains; i++) {
		if (pltsymcache[i].sym == NULL)
			continue;
		pltsymcachetab[pltsymcache_cnt].idx = i;
		pltsymcachetab[pltsymcache_cnt].obj_idx =
		    libmap[pltsymcache[i].obj->dyn.null];
		pltsymcachetab[pltsymcache_cnt].sym_idx =
		    pltsymcache[i].sym - pltsymcache[i].obj->dyn.symtab;
		pltsymcache_cnt++;
	}

	objarray[object->dyn.null].numlibs = numlibs;

	nameidx = xcalloc(numlibs, sizeof (struct nameidx));
	nametab = xmalloc(nametablen);

	nametablen = 0;
	for (i = 0; i < numlibs; i++) {
		nameidx[i].name = nametablen;
		nameidx[i].id0 = objarray[idxtolib[i]].id0;
		nameidx[i].id1 = objarray[idxtolib[i]].id1;
		nametablen += strlen(objarray[idxtolib[i]].obj->load_name) + 1;
		strlcpy(&nametab[nameidx[i].name],
		    objarray[idxtolib[i]].obj->load_name,
		    nametablen - nameidx[i].name);
	}

	/* skip writing lib if using old prebind data */
	if (objarray[object->dyn.null].oprebind_data == NULL)
		ret = elf_write_lib(object, nameidx, nametab, nametablen,
		    numlibs, 0, NULL, NULL, NULL, NULL, symcachetab,
		    symcache_cnt, pltsymcachetab, pltsymcache_cnt);

	free(nameidx);
	free(nametab);
	free(libmap);
	free(pltsymcachetab);
	free(symcachetab);

	return ret;
}

int
elf_prep_bin_prebind(struct proglist *pl)
{
	int ret;
	int numlibs = 0;
	int i, j;
	int ref_obj;
	int *libmap;
	int *idxtolib;
	struct nameidx *nameidx;
	char *nametab;
	int nametablen;
	struct symcache_noflag *symcache;
	struct symcache_noflag *pltsymcache;
	struct symcachetab *symcachetab;
	int symcache_cnt;
	struct symcachetab *pltsymcachetab;
	int pltsymcache_cnt;
	struct elf_object *object;
	struct objlist *ol;

	object = TAILQ_FIRST(&(pl->curbin_list))->object;
	symcache = objarray[object->dyn.null].symcache;
	pltsymcache = objarray[object->dyn.null].pltsymcache;
	libmap = xcalloc(objarray_cnt, sizeof (int));
	idxtolib = xcalloc(pl->nobj, sizeof (int));

	for (i = 0; i < objarray_cnt; i++)
		libmap[i] = -1;

	for (i = 0; i < pl->nobj; i++)
		idxtolib[i] = -1;

	nametablen = 0;
	TAILQ_FOREACH(ol, &(pl->curbin_list), list) {
		ref_obj = ol->object->dyn.null;
		nametablen += strlen(ol->object->load_name) + 1;
		libmap[ref_obj] = numlibs;
		idxtolib[numlibs] = ref_obj;
		numlibs++;
	}

	/* do got */
	symcache_cnt = 0;
	for (i = 0; i < object->nchains; i++) {
		if (symcache[i].sym != NULL)
			symcache_cnt++;
	}

	symcachetab = xcalloc(symcache_cnt , sizeof(struct symcachetab));

	symcache_cnt = 0;
	for (i = 0; i < object->nchains; i++) {
		if (symcache[i].sym == NULL)
			continue;
		symcachetab[symcache_cnt].idx = i;
		symcachetab[symcache_cnt].obj_idx =
		    libmap[symcache[i].obj->dyn.null];
		symcachetab[symcache_cnt].sym_idx =
		    symcache[i].sym - symcache[i].obj->dyn.symtab;
		symcache_cnt++;
	}

	/* now do plt */
	pltsymcache_cnt = 0;
	for (i = 0; i < object->nchains; i++) {
		if (pltsymcache[i].sym != NULL)
			pltsymcache_cnt++;
	}
	pltsymcachetab = xcalloc(pltsymcache_cnt , sizeof(struct symcachetab));

	pltsymcache_cnt = 0;
	for (i = 0; i < object->nchains; i++) {
		if (pltsymcache[i].sym == NULL)
			continue;
		pltsymcachetab[pltsymcache_cnt].idx = i;
		pltsymcachetab[pltsymcache_cnt].obj_idx =
		    libmap[pltsymcache[i].obj->dyn.null];
		pltsymcachetab[pltsymcache_cnt].sym_idx =
		    pltsymcache[i].sym - pltsymcache[i].obj->dyn.symtab;
		pltsymcache_cnt++;
	}

	objarray[object->dyn.null].numlibs = numlibs;

	nameidx = xcalloc(numlibs, sizeof (struct nameidx));
	nametab = xmalloc(nametablen);

	nametablen = 0;
	for (i = 0; i < numlibs; i++) {
		nameidx[i].name = nametablen;
		nameidx[i].id0 = objarray[idxtolib[i]].id0;
		nameidx[i].id1 = objarray[idxtolib[i]].id1;
		nametablen += strlen(objarray[idxtolib[i]].obj->load_name) + 1;

		strlcpy(&nametab[nameidx[i].name],
		    objarray[idxtolib[i]].obj->load_name,
		    nametablen - nameidx[i].name);
	}
	pl->libmapcnt = xcalloc(numlibs, sizeof(u_int32_t));

	/* have to do both got and plt fixups */
	for (i = 0; i < numlibs; i++) {
		for (j = 0; j < pl->fixupcnt[2*i]; j++) {
			pl->fixup[2*i][j].obj_idx =
			    libmap[pl->fixup[2*i][j].obj_idx];
		}
		for (j = 0; j < pl->fixupcnt[2*i+1]; j++) {
			pl->fixup[2*i+1][j].obj_idx =
			    libmap[pl->fixup[2*i+1][j].obj_idx];
		}

		pl->libmapcnt[i] = objarray[idxtolib[i]].numlibs;
		pl->libmap[i] = xcalloc(objarray[idxtolib[i]].numlibs,
		    sizeof(u_int32_t));
		if (i != 0) {
			for (j = 0; j < objarray[idxtolib[i]].numlibs; j++) {
				pl->libmap[i][j] =
				    libmap[objarray[idxtolib[i]].idxtolib[j]];
			}
		}
	}

	ret = elf_write_lib(object, nameidx, nametab, nametablen, numlibs,
	    numlibs, pl->fixup, pl->fixupcnt,
	    pl->libmap, pl->libmapcnt,
	    symcachetab, symcache_cnt,
	    pltsymcachetab, pltsymcache_cnt);

	free(symcachetab);
	free(pltsymcachetab);
	free(idxtolib);
	free(nameidx);
	free(nametab);
	free(libmap);

	return ret;
}


int
elf_write_lib(struct elf_object *object, struct nameidx *nameidx,
    char *nametab, int nametablen, int numlibs,
    int nfixup, struct fixup **fixup, int *fixupcnt,
    u_int32_t **libmap, int *libmapcnt,
    struct symcachetab *symcachetab, int symcache_cnt,
    struct symcachetab *pltsymcachetab, int pltsymcache_cnt)
{
	struct prebind_footer footer;
	struct prebind_info info;
	u_int32_t footer_offset, *maptab = NULL;
	u_int32_t next_start, *fixuptab = NULL;
	struct stat ifstat;
	off_t base_offset;
	size_t len;
	int fd = -1, i;
	int readonly = 0;

	/* open the file, if in safe mode, only open it readonly */
	if (safe == 0)
		fd = open(object->load_name, O_RDWR);
	if (fd == -1) {
		if (safe != 0 || errno == ETXTBSY)
			fd = open(object->load_name, O_RDONLY);
		if (fd == -1) {
			perror(object->load_name);
			return 1;
		}
		readonly = 1;
	}
	lseek(fd, -((off_t)sizeof(struct prebind_footer)), SEEK_END);
	len = read(fd, &footer, sizeof(struct prebind_footer));

	if (fstat(fd, &ifstat) == -1) {
		perror(object->load_name);
		exit(10);
	}

	if (footer.bind_id[0] == BIND_ID0 &&
	    footer.bind_id[1] == BIND_ID1 &&
	    footer.bind_id[2] == BIND_ID2 &&
	    footer.bind_id[3] == BIND_ID3 &&
	    readonly == 0) {

		ftruncate(fd, footer.orig_size);
		elf_clear_prog_load(fd, object);

		base_offset = footer.orig_size;
	} else {
		base_offset = ifstat.st_size;
	}

	bzero(&footer, sizeof(struct prebind_footer));


	/* verify dev/inode - do we care about last modified? */

	/* pieces to store on lib
	 *
	 * offset to footer
	 * nameidx		- numlibs * sizeof nameidx
	 * symcache		- symcache_cnt * sizeof (symcache_idx)
	 * pltsymcache		- pltsymcache_cnt * sizeof (symcache_idx)
	 * fixup(N/A for lib)	- nfixup * sizeof (symcache_idx)
	 * nametab		- nametablen
	 * footer	 (not aligned)
	 */

	footer.orig_size = base_offset;
	base_offset = ELF_ROUND(base_offset, sizeof(u_int64_t));
	footer.prebind_base = base_offset;
	footer.nameidx_idx = sizeof(u_int32_t);
	footer.symcache_idx = footer.nameidx_idx +
	    numlibs * sizeof (struct nameidx);
	footer.pltsymcache_idx = footer.symcache_idx +
	    symcache_cnt * sizeof (struct nameidx);
	footer.symcache_cnt = symcache_cnt;
	footer.pltsymcache_cnt = pltsymcache_cnt;
	footer.fixup_cnt = 0;
	footer.numlibs = numlibs;
	next_start = footer.pltsymcache_idx +
	    (pltsymcache_cnt * sizeof (struct symcachetab));
	if (nfixup != 0) {
		footer.fixup_cnt = nfixup;
		footer.fixup_idx = next_start;
		next_start += 2*nfixup * sizeof(u_int32_t);
		footer.fixupcnt_idx = next_start;
		next_start += 2*nfixup * sizeof(u_int32_t);
		fixuptab = xcalloc(2*nfixup, sizeof(u_int32_t));
		for (i = 0; i < 2*nfixup; i++) {
			fixuptab[i] = next_start;
			next_start += fixupcnt[i] * sizeof(struct fixup);
		}
		footer.libmap_idx = next_start;
		next_start += 2*nfixup * sizeof(u_int32_t);
		maptab = xcalloc(2*nfixup, sizeof(u_int32_t));
		maptab[0] = next_start;
		for (i = 1; i < nfixup; i++) {
			maptab[i] = next_start;
			next_start += libmapcnt[i] * sizeof(u_int32_t);
		}
	}

	footer.nametab_idx = next_start;
	next_start += nametablen;
	next_start = ELF_ROUND(next_start, sizeof(u_int64_t));
	footer_offset = next_start;
	if (verbose > 1) {
		printf("footer_offset %d\n", footer_offset);
	}
	footer.prebind_size = next_start + sizeof(struct prebind_footer);

	footer.prebind_version = PREBIND_VERSION;
	footer.id0 = objarray[object->dyn.null].id0;
	footer.id1 = objarray[object->dyn.null].id1;
	footer.bind_id[0] = BIND_ID0;
	footer.bind_id[1] = BIND_ID1;
	footer.bind_id[2] = BIND_ID2;
	footer.bind_id[3] = BIND_ID3;


	info.object = object;
	info.footer = &footer;
	info.footer_offset = footer_offset;
	info.nameidx = nameidx;
	info.symcache = symcachetab;


	info.pltsymcache = pltsymcachetab;
	info.nfixup = nfixup;
	if (nfixup != 0) {
		info.fixuptab = fixuptab;
		info.fixupcnt = fixupcnt;
		info.fixup = fixup;
		info.maptab = maptab;
		info.libmap = libmap;
		info.libmapcnt = libmapcnt;
	}

	info.nametab = nametab;
	info.nametablen = nametablen;


	if (readonly) {
		prebind_writenewfile(fd, object->load_name, &ifstat,
		    (off_t)footer.orig_size, &info);
	} else {
		prebind_writefile(fd, &info);
	}

	if (fstat(fd, &ifstat) == -1) {
		perror(object->load_name);
		exit(10);
	}
	if (nfixup != 0) {
		free(fixuptab);
		free(maptab);
	}

	if (verbose > 0)
		printf("%s: prebind info %d bytes old size %lld, growth %f\n",
		    object->load_name, footer.prebind_size, footer.orig_size,
		    (double)(footer.prebind_size) / footer.orig_size);

	if (verbose > 1)
		elf_dump_footer(&footer);

	close(fd);
	return 0;
}

int
prebind_writefile(int fd, struct prebind_info *info)
{
	int i;

	struct prebind_footer *footer = info->footer;

	lseek(fd, footer->prebind_base, SEEK_SET);
	write(fd, &info->footer_offset, sizeof(u_int32_t));

	lseek(fd, footer->prebind_base+footer->nameidx_idx, SEEK_SET);
	write(fd, info->nameidx, footer->numlibs * sizeof (struct nameidx));

	lseek(fd, footer->prebind_base+footer->symcache_idx, SEEK_SET);
	write(fd, info->symcache, footer->symcache_cnt *
	    sizeof (struct symcachetab));

	lseek(fd, footer->prebind_base+footer->pltsymcache_idx, SEEK_SET);
	write(fd, info->pltsymcache, footer->pltsymcache_cnt *
	    sizeof (struct symcachetab));

	if (info->nfixup != 0) {
		lseek(fd, footer->prebind_base+footer->fixup_idx, SEEK_SET);
		write(fd, info->fixuptab, 2*info->nfixup * sizeof(u_int32_t));
		lseek(fd, footer->prebind_base+footer->fixupcnt_idx, SEEK_SET);
		write(fd, info->fixupcnt, 2*info->nfixup * sizeof(u_int32_t));
		for (i = 0; i < 2*info->nfixup; i++) {
			lseek(fd, footer->prebind_base+info->fixuptab[i],
			    SEEK_SET);
			write(fd, info->fixup[i], info->fixupcnt[i] *
			    sizeof(struct fixup));
		}

		lseek(fd, footer->prebind_base+footer->libmap_idx, SEEK_SET);
		write(fd, info->maptab, info->nfixup * sizeof(u_int32_t));
		for (i = 0; i < info->nfixup; i++) {
			lseek(fd, footer->prebind_base+info->maptab[i],
			    SEEK_SET);
			write(fd, info->libmap[i], info->libmapcnt[i] *
			    sizeof(u_int32_t));
		}
	}
	lseek(fd, footer->prebind_base+footer->nametab_idx, SEEK_SET);
	write(fd, info->nametab, info->nametablen);

	lseek(fd, footer->prebind_base+info->footer_offset, SEEK_SET);
	write(fd, footer, sizeof (struct prebind_footer));

	if (info->object->obj_type == OBJTYPE_EXE)
		elf_fixup_prog_load(fd, info->footer, info->object);

	return 0;
}

int
prebind_writenewfile(int infd, char *name, struct stat *st, off_t orig_size,
    struct prebind_info *info)
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

	/* now back track, and delete the header */
	if (prebind_remove_load_section(outfd, newname) == -1)
		goto fail;
	if (orig_size != (off_t)-1 &&
	    ftruncate(outfd, orig_size) == -1)
		goto fail;

	prebind_writefile(outfd, info);

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
	if (fstat(outfd, st) == -1) {
		/* XXX */
		goto fail;
	}
	if (rename(newname, name) == -1)
		goto fail;

	close (outfd);
	return (0);

fail:
	free(newname);
	unlink(newname);
	close(outfd);
	return (-1);
}

void
elf_fixup_prog_load(int fd, struct prebind_footer *footer,
    struct elf_object *object)
{
	void *buf;
	Elf_Ehdr *ehdr;
	Elf_Phdr *phdr;
	Elf_Phdr phdr_empty;
	int loadsection;

	buf = mmap(NULL, 8192, PROT_READ|PROT_WRITE, MAP_FILE | MAP_SHARED,
	    fd, 0);
	if (buf == MAP_FAILED) {
		printf("%s: cannot mmap for write\n", object->load_name);
		return;
	}

	ehdr = (Elf_Ehdr *) buf;
	phdr = (Elf_Phdr *)((char *)buf + ehdr->e_phoff);

	for (loadsection = 0; loadsection < ehdr->e_phnum; loadsection++) {
		if (phdr[loadsection].p_type == PT_LOAD)
			break;
	}

	/* verify that extra slot is empty */
	bzero(&phdr_empty, sizeof(phdr_empty));
	if (bcmp(&phdr[ehdr->e_phnum], &phdr_empty, sizeof(phdr_empty)) != 0) {
		printf("extra slot not empty\n");
		goto done;
	}
	phdr[ehdr->e_phnum].p_type = PT_LOAD;
	phdr[ehdr->e_phnum].p_flags = PF_R | 0x08000000;
	phdr[ehdr->e_phnum].p_offset = footer->prebind_base;
	phdr[ehdr->e_phnum].p_vaddr = footer->prebind_base | 0x80000000;
	phdr[ehdr->e_phnum].p_paddr = footer->prebind_base | 0x40000000;
	phdr[ehdr->e_phnum].p_filesz = footer->prebind_size;
	phdr[ehdr->e_phnum].p_memsz = footer->prebind_size;
	phdr[ehdr->e_phnum].p_align = phdr[loadsection].p_align;
	ehdr->e_phnum++;

done:
	msync(buf, 8192, MS_SYNC);
	munmap(buf, 8192);
}

void
elf_clear_prog_load(int fd, struct elf_object *object)
{
	void *buf;
	Elf_Ehdr *ehdr;
	Elf_Phdr *phdr;
	Elf_Phdr phdr_empty;
	int loadsection;

	buf = mmap(NULL, 8192, PROT_READ|PROT_WRITE, MAP_FILE | MAP_SHARED,
	    fd, 0);
	if (buf == MAP_FAILED) {
		printf("%s: cannot mmap for write\n", object->load_name);
		return;
	}

	ehdr = (Elf_Ehdr *) buf;
	phdr = (Elf_Phdr *)((char *)buf + ehdr->e_phoff);

	if (ehdr->e_type != ET_EXEC) {
		goto done;
	}

	loadsection = ehdr->e_phnum - 1;
	if ((phdr[loadsection].p_type != PT_LOAD) ||
	    ((phdr[loadsection].p_flags & 0x08000000) == 0)) {
		/* doesn't look like ours */
		printf("mapped, %s id doesn't match %lx %d %d\n",
		    object->load_name,
		    (long)(phdr[loadsection].p_vaddr),
		    phdr[loadsection].p_flags, loadsection);
		goto done;
	}

	/* verify that extra slot is empty */
	bzero(&phdr[loadsection], sizeof(phdr_empty));

	ehdr->e_phnum--;

done:
	msync(buf, 8192, MS_SYNC);
	munmap(buf, 8192);
}

void
elf_add_object(struct elf_object *object, int objtype)
{
	struct objarray_list *newarray;
	struct objlist *ol;
	ol = xmalloc(sizeof (struct objlist));
	ol->object = object;
	if (objtype != OBJTYPE_EXE)
		TAILQ_INSERT_TAIL(&library_list, ol, list);
	if (objarray_cnt+1 >= objarray_sz) {
		objarray_sz += 512;
		newarray = realloc(objarray, sizeof (objarray[0]) *
		    objarray_sz);
		if (newarray != NULL)
			objarray = newarray;
		else {
			perror("objarray");
			exit(20);
		}
	}
	object->dyn.null = objarray_cnt; /* Major abuse, I know */
	TAILQ_INIT(&(objarray[objarray_cnt].inst_list));
	objarray[objarray_cnt].obj = object;
	objarray[objarray_cnt].id0 = arc4random();
	objarray[objarray_cnt].id1 = arc4random();

	objarray[objarray_cnt].symcache = xcalloc(
	    sizeof(struct symcache_noflag), object->nchains);
	objarray[objarray_cnt].pltsymcache = xcalloc(
	    sizeof(struct symcache_noflag), object->nchains);

	objarray[objarray_cnt].oprebind_data = NULL;
	objarray[objarray_cnt].proglist = NULL;
	objarray[objarray_cnt].numlibs = 0;
	objarray_cnt++;

	elf_add_object_curbin_list(object);
}

void
elf_free_curbin_list(elf_object_t *object)
{
	struct objlist *ol;
	int i;

	while (!TAILQ_EMPTY(&(curbin->curbin_list))) {
		ol = TAILQ_FIRST(&(curbin->curbin_list));
		TAILQ_REMOVE(&(objarray[ol->object->dyn.null].inst_list), ol, inst_list);
		TAILQ_REMOVE(&(curbin->curbin_list), ol, list);
		free(ol);
	}

	printf("trying to remove %s\n", object->load_name);
	for (i = objarray_cnt; i != 0;) {
		i--;
		printf("obj %s\n", objarray[i].obj->load_name);
		if (objarray[i].obj == object) {
			printf("found obj at %d max obj %d\n", i, objarray_cnt);
			TAILQ_FOREACH(ol, &(curbin->curbin_list), list) {
			}
			/* XXX - delete references */
			objarray_cnt = i;
			break;
		}
	}
}

void
elf_print_objarray(void)
{
	int i;
	struct objlist *ol;

	printf("loaded objs # %d\n", objarray_cnt);
	for (i = 0; i < objarray_cnt; i++) {
		printf("%3d: %d obj %s\n", i, (int)objarray[i].obj->dyn.null,
		    objarray[i].obj->load_name);
		TAILQ_FOREACH(ol, &(objarray[i].inst_list),
		    inst_list) {
			printf("\tprog %s\n", ol->load_prog->load_name);
		}
	}
}

void
elf_load_existing_prebind(struct elf_object *object, int fd)
{
	struct prebind_footer footer;
	void *prebind_data;

	lseek(fd, -((off_t)sizeof(struct prebind_footer)), SEEK_END);
	read(fd, &footer, sizeof(struct prebind_footer));

	if (footer.bind_id[0] != BIND_ID0 ||
	    footer.bind_id[1] != BIND_ID1 ||
	    footer.bind_id[2] != BIND_ID2 ||
	    footer.bind_id[3] != BIND_ID3)
		return;

	prebind_data = mmap(0, footer.prebind_size, PROT_READ,
	    MAP_FILE, fd, footer.prebind_base);
	objarray[object->dyn.null].oprebind_data = prebind_data;
	objarray[object->dyn.null].id0 = footer.id0;
	objarray[object->dyn.null].id1 = footer.id1;

	copy_oldsymcache(object->dyn.null, prebind_data);
}

void
copy_oldsymcache(int objidx, void *prebind_map)
{
	struct prebind_footer *footer;
	struct elf_object *object;
	struct elf_object *tobj;
	struct symcache_noflag *tcache;
	struct symcachetab *symcache;
	int i, j, found, *idxtolib;
	char *c, *nametab;
	u_int32_t offset;
	u_int32_t *poffset;
	struct nameidx *nameidx;

	object = objarray[objidx].obj;

	poffset = (u_int32_t *)prebind_map;
	c = prebind_map;
	offset = *poffset;
	c += offset;
	footer = (void *)c;

	nameidx = prebind_map + footer->nameidx_idx;
	nametab = prebind_map + footer->nametab_idx;

	idxtolib = xcalloc(footer->numlibs, sizeof(int));
	found = 0;
	for (i = 0; i < footer->numlibs; i++) {
		found = 0;
		for (j = 0; j < objarray_cnt; j++) {
			if (objarray[j].id0 == nameidx[i].id0 &&
			    objarray[j].id1 == nameidx[i].id1) {
				found = 1;
				idxtolib[i] = j;
				if (strcmp(objarray[j].obj->load_name,
				    &nametab[nameidx[i].name]) != 0) {
					printf("warning filename mismatch"
					    " [%s] [%s]\n",
					    objarray[j].obj->load_name,
					    &nametab[nameidx[i].name]);
				}
			}
		}
		if (found == 0)
			break;
	}
	if (found == 0)
		goto done;

	/* build idxtolibs */
	tcache = objarray[objidx].symcache;
	symcache = prebind_map + footer->symcache_idx;

	for (i = 0; i < footer->symcache_cnt; i++) {
		tobj = objarray[idxtolib[symcache[i].obj_idx]].obj;

		tcache[symcache[i].idx].obj = tobj;
		tcache[symcache[i].idx].sym = tobj->dyn.symtab +
		    symcache[i].sym_idx;
	}

	tcache = objarray[objidx].pltsymcache;
	symcache = prebind_map + footer->pltsymcache_idx;
	for (i = 0; i < footer->pltsymcache_cnt; i++) {
		tobj = objarray[idxtolib[symcache[i].obj_idx]].obj;

		tcache[symcache[i].idx].obj = tobj;
		tcache[symcache[i].idx].sym = tobj->dyn.symtab +
		    symcache[i].sym_idx;
	}
done:
	free(idxtolib);
	/* munmap(prebind_map, size);*/
}
