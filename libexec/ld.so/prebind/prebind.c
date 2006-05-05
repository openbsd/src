/* $OpenBSD: prebind.c,v 1.10 2006/05/05 13:52:41 jmc Exp $ */
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
#include <fcntl.h>
#include <nlist.h>
#include <elf_abi.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include "resolve.h"
#include "link.h"
#include "sod.h"
#ifndef __mips64__
#include "machine/reloc.h"
#endif
#include "prebind.h"

/* seems to make sense to limit how big of file can be dealt with */
#define MAX_FILE_SIZE (512 * 1024 * 1024)

char *shstrtab;

#define DEBUG

/* TODO - library path from ldconfig */
#define DEFAULT_PATH "/usr/lib:/usr/X11R6/lib:/usr/local/qte/lib"

/* alpha uses  RELOC_JMP_SLOT */
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
#ifdef __mips64__
#define RELOC_JMP_SLOT	0		/* XXX mips64 doesnt have PLT reloc */
#endif
/* powerpc uses  RELOC_JMP_SLOT */
/* sparc uses  RELOC_JMP_SLOT */
/* sparc64 uses  RELOC_JMP_SLOT */
#if defined(__sparc__) && !defined(__sparc64__)
/* ARGH, our sparc/include/reloc.h is wrong (for the moment) */
#undef RELOC_JMP_SLOT
#define RELOC_JMP_SLOT 21
#endif

#include "prebind_struct.h"
struct proglist *curbin;

obj_list_ty library_list =
    TAILQ_HEAD_INITIALIZER(library_list);

prog_list_ty prog_list =
    TAILQ_HEAD_INITIALIZER(prog_list);


struct elf_object * elf_load_object (void *pexe, const char *name);
void elf_free_object(struct elf_object *object);
void map_to_virt(Elf_Phdr *, Elf_Ehdr *, Elf_Addr, u_long *);
#ifdef DEBUG
#endif
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

struct elf_object * elf_lookup_object(const char *name);
struct elf_object * elf_lookup_object_devino(dev_t dev, ino_t inode,
    int objtype);
void elf_free_curbin_list(struct elf_object *obj);
void elf_resolve_curbin(void);
struct proglist *elf_newbin(void);
void elf_add_prog(struct proglist *object);
void elf_sum_reloc();
int elf_prep_lib_prebind(struct elf_object *object);
int elf_prep_bin_prebind(struct proglist *pl);

void elf_dump_footer(struct prebind_footer *footer);


void elf_fixup_prog_load(int fd, struct prebind_footer *footer,
    struct elf_object *object);
void elf_clear_prog_load(int fd, struct elf_object *object);

void
elf_find_symbol_rel(const char *s, struct elf_object *object,
    Elf_Rel *rel, struct symcache_noflag *symcache,
    struct symcache_noflag *pltsymcache);

void
elf_find_symbol_rela(const char *s, struct elf_object *object,
    Elf_RelA *rela, struct symcache_noflag *symcache,
    struct symcache_noflag *pltsymcache);

int elf_find_symbol_obj(elf_object_t *object, const char *name,
    unsigned long hash, int flags, const Elf_Sym **this,
    const Elf_Sym **weak_sym, elf_object_t **weak_object);

int verbose;	 /* how verbose to be when operating */
int merge_mode;	 /* merge (do not overwrite) existing prebind library info */

struct elf_object *load_object;

struct elf_object * load_file(const char *filename, int lib);
int elf_check_note(void *buf, Elf_Phdr *phdr);
void load_dir(char *name);
void load_exe(char *name);

void
load_file_or_dir(char *name)
{
	struct stat sb;
	int ret;

	ret = lstat(name, &sb);
	if (ret != 0)
		return;
	switch(sb.st_mode & S_IFMT) {
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
void
load_dir(char *name)
{
	DIR *dirp;
	struct dirent *dp;
	struct stat sb;
	char *buf;

	dirp = opendir(name);
	if (dirp == NULL) {
		/* dir failed to open, skip */
		return;
	}
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
			free (buf);
		case DT_REG:
			asprintf(&buf, "%s/%s", name, dp->d_name);
			load_exe(buf);
			free (buf);
		default:
			/* other files symlinks, dirs, ... we ignore */
			;
		}
	}
}
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
				break;  /* XXX */

		}
		if (fail == 0) {
			interp = load_file(curbin->interp, OBJTYPE_DLO);
			object->load_object = interp;
			if (interp == NULL)
				fail = 1;
		}

		/* slight abuse of this field */

		if (fail == 0) {
			elf_resolve_curbin();
			elf_add_prog(curbin);
		} else {
			printf("failed to load %s\n", name);
			elf_free_curbin_list(object);
			free (curbin);
		}
		if (load_object != NULL) {
			load_object = NULL;
		}
	} else {
		free (curbin);
	}
}

struct elf_object *
load_file(const char *filename, int objtype)
{
	int fd = -1;
	void *buf = NULL;
	struct stat ifstat;
	Elf_Ehdr *ehdr;
	Elf_Shdr *shdr;
	Elf_Phdr *phdr;
	char *pexe;
	struct elf_object *obj = NULL;
	int note_found;
	int i;

	fd = open(filename, O_RDONLY);
	if (fd == -1) {
		perror (filename);
		goto done;
	}

	if (fstat(fd, &ifstat) == -1) {
		perror (filename);
		goto done;
	}

        if  ((ifstat.st_mode & S_IFMT) != S_IFREG)
		goto done;

	if (ifstat.st_size < sizeof (Elf_Ehdr)) {
		if (verbose > 0)
			printf("%s: short file\n", filename);
		goto done;
	}

	obj = elf_lookup_object_devino( ifstat.st_dev, ifstat.st_ino, objtype);
	if (obj != NULL)
		goto done;

	buf = mmap(NULL, ifstat.st_size, PROT_READ, MAP_FILE | MAP_SHARED,
	    fd, 0);
	if (buf == MAP_FAILED) {
		printf("%s: cannot mmap\n", filename);
		goto done;
	}

	ehdr = (Elf_Ehdr *) buf;

	if (IS_ELF(*ehdr) == 0) {
		goto done;
	}

	if( ehdr->e_machine !=  ELF_TARG_MACH) {
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
	if (ehdr->e_shstrndx == 0) {
		goto done;
	}
	shdr = (Elf_Shdr *) (pexe + ehdr->e_shoff +
	    (ehdr->e_shstrndx * ehdr->e_shentsize));

	
#if 0
printf("e_ehsize %x\n", ehdr->e_ehsize);
printf("e_phoff %x\n", ehdr->e_phoff);
printf("e_shoff %x\n", ehdr->e_shoff);
printf("e_phentsize %x\n", ehdr->e_phentsize);
printf("e_phnum %x\n", ehdr->e_phnum);
printf("e_shentsize %x\n", ehdr->e_shentsize);
printf("e_shstrndx %x\n\n", ehdr->e_shstrndx);
#endif

	shstrtab = (char *) (pexe + shdr->sh_offset);


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
	if ((objtype == OBJTYPE_LIB || objtype == OBJTYPE_DLO)
	    && merge_mode == 1) {
		/*
		 * for libraries, check if old prebind info exists
		 * and load it if we are in merge mode
		 */

		elf_load_existing_prebind(obj, fd);
	}
done:
	if (buf != NULL)
		munmap(buf, ifstat.st_size);
	if (fd != -1)
		close (fd);
	return obj;
}

int
elf_check_note(void *buf, Elf_Phdr *phdr)
{
	Elf_Ehdr *ehdr;

	ehdr = (Elf_Ehdr *) buf;
	u_long address = phdr->p_offset;
	u_int *plong = (u_int *)((char *)buf + address);
	char *osname = (char *)buf + address + sizeof(*plong) * 3;

	if (plong[0] == 8 /* OpenBSD\0 */ &&
	    plong[1] == 4 /* ??? */ &&
	    plong[2] == 1 /* type_osversion */ &&
	    strcmp("OpenBSD", osname) == 0)
		return 1;

	return 0;
}

void __dead 
usage()
{
	extern char *__progname;
	printf("%s [-mv] {programlist}\n", __progname);
	exit(1);
}

extern int64_t prebind_blocks;

int
main(int argc, char **argv)
{
	int i;
	int ch;
	extern int optind;

	/* GETOPT */
	while ((ch = getopt(argc, argv, "mv")) != -1) {
		switch (ch) {
		case 'm':
			merge_mode = 1;
			break;
		case 'v':
			verbose++;
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;

	if (argc == 0) {
		usage();
		/* NOTREACHED */
	}

	elf_init_objarray();

	for (i = 0; i < argc; i++) {
		load_file_or_dir(argv[i]);
	}
	if (verbose > 4) {
		elf_print_objarray();
		elf_print_prog_list(&prog_list);
	}
	elf_sum_reloc();

	printf("total new blocks %lld\n", prebind_blocks);

	return 0;
}

struct elf_object *
elf_load_object (void *pexe, const char *name)
{
	int i;
	struct elf_object *object;
	Elf_Dyn *dynp = NULL, *odynp;
	Elf_Ehdr *ehdr;
	Elf_Phdr *phdr;
	const Elf_Sym   *symt;
        const char      *strt;
	Elf_Addr loff;
	Elf_Word *needed_list;
	int needed_cnt = 0;

	object =  calloc (1, sizeof (struct elf_object));
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
			curbin->interp = strdup ((char *)((char *)pexe +
			     phdr[i].p_offset));
		default:
			break;
		}
	}

	if (dynp == 0) {
		return NULL; /* XXX ??? */
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

	needed_list = calloc((needed_cnt + 1), (sizeof (Elf_Word)));
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
#if 0
	if (object->Dyn.info[DT_SONAME])
		map_to_virt(phdr, ehdr, loff, &object->Dyn.info[DT_SONAME]);
#endif
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
			bcopy (object->dyn.hash, hash, hashsz);
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

		sym = malloc(object->nchains * sizeof(Elf_Sym));
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
 *  Free any extra pieces associated with 'object'
 */
void
elf_free_object(struct elf_object *object)
{
	free (object->load_name);
	if (object->dyn.hash != NULL)
		free (object->dyn.hash);
	free ((void *)object->dyn.strtab);
	free ((void *)object->dyn.symtab);
	if (object->dyn.rel != NULL)
		free (object->dyn.rel);
	if (object->dyn.rela != NULL)
		free (object->dyn.rela);
	if (object->dyn.rpath != NULL)
		free ((void *)object->dyn.rpath);
	free (object);
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
			if(phdr[i].p_vaddr > *vaddr)
				continue;
			if(phdr[i].p_vaddr + phdr[i].p_memsz < *vaddr)
				continue;
#ifdef DEBUG1
			printf("input address %lx translated to ", *vaddr);
#endif
			*vaddr += phdr[i].p_offset - phdr[i].p_vaddr + base;
#ifdef DEBUG1
			printf("%lx, base %lx %lx %llx\n", *vaddr, base,
			    phdr[i].p_vaddr, phdr[i].p_offset );
#endif

		default:
			break;
		}
	}
}

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

int
load_lib(const char *name, struct elf_object *parent)
{
	struct sod sod, req_sod;
	int ignore_hints;
	int try_any_minor = 0;
	struct elf_object *object = NULL;

#if 0
	printf("load_lib %s\n", name);
#endif

	ignore_hints = 0;

	if(strchr(name, '/')) {
		char *lpath, *lname;
		lpath = strdup(name);
		lname = strrchr(lpath, '/');
		if (lname == NULL || lname[1] == '\0') {
			free(lpath);
			return (1); /* failed */
		}
		*lname = '\0';
		lname++;

		_dl_build_sod(lname, &sod);
		req_sod = sod;

		/* this code does not allow lower minors */
fullpathagain:
		object = elf_load_shlib_hint(&sod, &req_sod,
			ignore_hints, lpath);
		if (object != NULL) 
			goto fullpathdone;

		if (try_any_minor == 0) {
			try_any_minor = 1;
			ignore_hints = 1;
			req_sod.sod_minor = -1;
			goto fullpathagain;
		}
		/* ERR */
fullpathdone:
		free(lpath);
		free((char *)sod.sod_name);
		return (object == NULL); /* failed */
	}
	_dl_build_sod(name, &sod);
	req_sod = sod;

	/* ignore LD_LIBRARY_PATH */

again:
	if (parent->dyn.rpath != NULL) {
		object = elf_load_shlib_hint(&sod, &req_sod,
			ignore_hints, parent->dyn.rpath);
		if (object != NULL)
			goto done;
	}
	if (parent != load_object && load_object->dyn.rpath != NULL) {
		object = elf_load_shlib_hint(&sod, &req_sod,
			ignore_hints, load_object->dyn.rpath);
		if (object != NULL)
			goto done;
	}
	object = elf_load_shlib_hint(&sod, &req_sod,
		ignore_hints, NULL);

	if (try_any_minor == 0) {
		try_any_minor = 1;
		ignore_hints = 1;
		req_sod.sod_minor = -1;
		goto again;
	}
	if (object == NULL)
		printf ("unable to load %s\n", name);

done:
	free((char *)sod.sod_name);

	return (object == NULL);
}

/*
 * attempt to locate and load a library based on libpath, sod info and
 * if it needs to respect hints, passing type and flags to perform open
 */
elf_object_t *
elf_load_shlib_hint(struct sod *sod, struct sod *req_sod,
    int ignore_hints, const char *libpath)
{
	elf_object_t *object = NULL;
	char *hint;

	hint = elf_find_shlib(req_sod, libpath, ignore_hints);
	if (hint != NULL) {
		if (req_sod->sod_minor < sod->sod_minor)
			printf("warning: lib%s.so.%d.%d: "
			    "minor version >= %d expected, "
			    "using it anyway\n",
			    (char *)sod->sod_name, sod->sod_major,
			    req_sod->sod_minor, sod->sod_minor);
		object = elf_tryload_shlib(hint);
	}
	return object;
}

char elf_hint_store[MAXPATHLEN];

char *
elf_find_shlib(struct sod *sodp, const char *searchpath, int nohints)
{
	char *hint, lp[PATH_MAX + 10], *path;
	struct dirent *dp;
	const char *pp;
	int match, len;
	DIR *dd;
	struct sod tsod, bsod;		/* transient and best sod */

	/* if we are to search default directories, and hints
	 * are not to be used, search the standard path from ldconfig
	 * (_dl_hint_search_path) or use the default path
	 */
	if (nohints)
		goto nohints;

	if (searchpath == NULL) {
		/* search 'standard' locations, find any match in the hints */
		hint = _dl_findhint((char *)sodp->sod_name, sodp->sod_major,
		    sodp->sod_minor, NULL);
		if (hint)
			return hint;
	} else {
		/* search hints requesting matches for only
		 * the searchpath directories,
		 */
		pp = searchpath;
		while (pp) {
			path = lp;
			while (path < lp + PATH_MAX &&
			    *pp && *pp != ':' && *pp != ';')
				*path++ = *pp++;
			*path = 0;

			/* interpret "" as curdir "." */
			if (lp[0] == '\0') {
				lp[0] = '.';
				lp[1] = '\0';
			}

			hint = _dl_findhint((char *)sodp->sod_name,
			    sodp->sod_major, sodp->sod_minor, lp);
			if (hint != NULL)
				return hint;

			if (*pp)	/* Try curdir if ':' at end */
				pp++;
			else
				pp = 0;
		}
	}

	/*
	 * For each directory in the searchpath, read the directory
	 * entries looking for a match to sod. filename compare is
	 * done by _dl_match_file()
	 */
nohints:
	if (searchpath == NULL) {
		if (_dl_hint_search_path != NULL)
			searchpath = _dl_hint_search_path;
		else
			searchpath = DEFAULT_PATH;
	}
	pp = searchpath;
	while (pp) {
		path = lp;
		while (path < lp + PATH_MAX && *pp && *pp != ':' && *pp != ';')
			*path++ = *pp++;
		*path = 0;

		/* interpret "" as curdir "." */
		if (lp[0] == '\0') {
			lp[0] = '.';
			lp[1] = '\0';
		}

		if ((dd = opendir(lp)) != NULL) {
			match = 0;
			while ((dp = readdir(dd)) != NULL) {
				tsod = *sodp;
				if (elf_match_file(&tsod, dp->d_name,
				    dp->d_namlen)) {
					/*
					 * When a match is found, tsod is
					 * updated with the major+minor found.
					 * This version is compared with the
					 * largest so far (kept in bsod),
					 * and saved if larger.
					 */
					if (!match ||
					    tsod.sod_major == -1 ||
					    tsod.sod_major > bsod.sod_major ||
					    ((tsod.sod_major ==
					    bsod.sod_major) &&
					    tsod.sod_minor > bsod.sod_minor)) {
						bsod = tsod;
						match = 1;
						len = strlcpy(
						    elf_hint_store, lp,
						    MAXPATHLEN);
						if (lp[len-1] != '/') {
							elf_hint_store[len] =
							    '/';
							len++;
						}
						strlcpy(
						    &elf_hint_store[len],
						    dp->d_name,
						    MAXPATHLEN-len);
						if (tsod.sod_major == -1)
							break;
					}
				}
			}
			closedir(dd);
			if (match) {
				*sodp = bsod;
				return (elf_hint_store);
			}
		}

		if (*pp)	/* Try curdir if ':' at end */
			pp++;
		else
			pp = 0;
	}
	return NULL;
}

elf_object_t *
elf_tryload_shlib(const char *libname)
{
	struct elf_object *object;
	object = elf_lookup_object(libname);
	if (object == NULL) {
		object = load_file(libname, OBJTYPE_LIB);
	}
	if (object == NULL)
		printf("tryload_shlib %s\n", libname);
	return object;
}

/*
 * elf_match_file()
 *
 * This fucntion determines if a given name matches what is specified
 * in a struct sod. The major must match exactly, and the minor must
 * be same or larger.
 *
 * sodp is updated with the minor if this matches.
 */

int
elf_match_file(struct sod *sodp, char *name, int namelen)
{
	int match;
	struct sod lsod;
	char *lname;

	lname = name;
	if (sodp->sod_library) {
		if (strncmp(name, "lib", 3))
			return 0;
		lname += 3;
	}
	if (strncmp(lname, (char *)sodp->sod_name,
	    strlen((char *)sodp->sod_name)))
		return 0;

	_dl_build_sod(name, &lsod);

	match = 0;
	if ((strcmp((char *)lsod.sod_name, (char *)sodp->sod_name) == 0) &&
	    (lsod.sod_library == sodp->sod_library) &&
	    ((sodp->sod_major == -1) || (sodp->sod_major == lsod.sod_major)) &&
	    ((sodp->sod_minor == -1) ||
	    (lsod.sod_minor >= sodp->sod_minor))) {
		match = 1;

		/* return version matched */
		sodp->sod_major = lsod.sod_major;
		sodp->sod_minor = lsod.sod_minor;
	}
	free((char *)lsod.sod_name);
	return match;
}
void
elf_add_prog(struct proglist *curbin)
{
	TAILQ_INSERT_TAIL(&prog_list, curbin, list);
}

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
 *
 * Note 1: If 'merge' mode is ever written, this will need to keep
 * the 'cached' copies symbols and do fixups for the rest, regardless
 * of conflicts
 *
 * Note 2: This is run once each for the (got)symcache and pltsymcache
 */

struct elf_object badobj_store;
struct elf_object *badobj = &badobj_store;

/* 
 * copy the symbols found in a library symcache to the 'master/common'
 * symbol table note that this will skip copying the following references
 * 1. non-existing entries
 * 2. symobj == prog &&& obj != prog
 * 3  symobj == prog's interpter (references to dl_open)
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

		lib_prog_ref = (obj != prog && scache[i].obj == prog);

		if (tcache[i].obj != NULL || lib_prog_ref) {
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

#if 0
				if (tcache[i].obj != badobj)
					printf("%s: %s conflict\n", 
					    obj->load_name,
					    scache[i].sym->st_name +
					    scache[i].obj->dyn.strtab);
#endif
				tcache[i].obj = badobj;
				tcache[i].sym = NULL;
			}
		} else {
			if (scache[i].obj != prog) {
#if 0
				printf("%s: %s copying\n",
				    obj->load_name,
				    scache[i].sym->st_name +
				    scache[i].obj->dyn.strtab);
#endif
				tcache[i].sym = scache[i].sym;
				tcache[i].obj = scache[i].obj;
			}
		}

#if 0
		printf("symidx %d: obj %d sym %ld %s\n",
		    i, scache[i].obj->dyn.null,
		    scache[i].sym -
		    scache[i].obj->dyn.symtab,
		    scache[i].sym->st_name +
		    scache[i].obj->dyn.strtab
		    );
#endif
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
#if 0
			printf("found sym %s in obj %s\n", name, ol->object->load_name);
#endif
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
#if 0
		printf("found sym %s in obj %s %d\n", name,
		     weak_obj->load_name, flags);
#endif
	}
	if (found == 1) {
#if 0
		printf("object %s sym %s, ref_object %s flags %d\n", 
		    object->load_name, name,
		    ref_object->load_name, flags);
#endif
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
#if 0
			printf("found sym %s in obj %s\n", name, ol->object->load_name);
#endif
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
#if 0
		printf("found weak sym %s in obj %s\n", name, weak_obj->load_name);
#endif
	}
	if (found == 1) {
#if 0
		printf("object %s sym %s, ref_object %s %s %d %d\n", 
		    object->load_name, name,
		    ref_object->load_name,
		    (flags & SYM_PLT) ? "plt" : "got", type, RELOC_JMP_SLOT);

#endif
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
elf_reloc(struct elf_object *object, struct symcache_noflag *symcache,
    struct symcache_noflag *pltsymcache)
{
	int numrel;
	int numrela;
	int i;
	const Elf_Sym *sym;
	Elf_Rel *rel;
	Elf_RelA *rela;
        numrel = object->dyn.relsz / sizeof(Elf_Rel);
#ifdef DEBUG1
	printf("rel relocations: %d\n", numrel);
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
			if (ELF_R_SYM(rel[i].r_info) != 0)  {
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
		if (ELF_R_SYM(rela[i].r_info) != 0)  {
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
			if (ELF_R_SYM(rela[i].r_info) != 0)  {
				elf_find_symbol_rela(s, object, &rela[i],
				symcache, pltsymcache);
			}
		}
	}
}

void
elf_resolve_curbin(void)
{
	struct objlist *ol;

#ifdef DEBUG1
	elf_print_curbin_list(curbin);
#endif
	TAILQ_FOREACH(ol, &(curbin->curbin_list), list) {
		ol->cache = calloc(sizeof(struct symcache_noflag),
		    ol->object->nchains);
		ol->pltcache = calloc(sizeof(struct symcache_noflag),
		    ol->object->nchains);
		if (ol->cache == NULL || ol->pltcache == NULL) {
			printf("unable to allocate memory for cache %s\n",
			ol->object->load_name);
			exit(20);
		}
		elf_reloc(ol->object, ol->cache, ol->pltcache);
	}
}
