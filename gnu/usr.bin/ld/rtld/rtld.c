/*	$OpenBSD: rtld.c,v 1.31 2002/12/02 23:17:36 deraadt Exp $	*/
/*	$NetBSD: rtld.c,v 1.43 1996/01/14 00:35:17 pk Exp $	*/
/*
 * Copyright (c) 1993 Paul Kranenburg
 * All rights reserved.
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
 *	This product includes software developed by Paul Kranenburg.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/errno.h>
#include <sys/mman.h>
#ifndef MAP_COPY
#define MAP_COPY	MAP_PRIVATE
#endif
#include <err.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <a.out.h>
#include <stab.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#if __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

#include "ld.h"

/*
 * Stack protector dummies.
 * Ideally, a scheme to compile these stubs from libc should be used, but
 * this would end up dragging too much code from libc here.
 */
long __guard[8] = {0, 0, 0, 0, 0, 0, 0, 0};
void
__stack_smash_handler(char func[], int damaged)
{
	_exit(127);
}

#ifndef MAP_ANON
#define MAP_ANON	0
#define anon_open() do {					\
	if ((anon_fd = open("/dev/zero", O_RDWR, 0)) == -1)	\
		err("open: %s", "/dev/zero");			\
} while (0)
#define anon_close() do {	\
	(void)close(anon_fd);	\
	anon_fd = -1;		\
} while (0)
#else
#define anon_open()
#define anon_close()
#endif

/*
 * Loader private data, hung off <so_map>->som_spd
 */
struct somap_private {
	int		spd_version;
	struct so_map	*spd_parent;
	int		spd_refcount;
	int		spd_flags;
#define RTLD_MAIN	1
#define RTLD_RTLD	2
#define RTLD_DL		4
	size_t		spd_size;

#ifdef SUN_COMPAT
	long		spd_offset;	/* Correction for Sun main programs */
#endif
};

#define LM_PRIVATE(smp)	((struct somap_private *)(smp)->som_spd)

#ifdef SUN_COMPAT
#define LM_OFFSET(smp)	(LM_PRIVATE(smp)->spd_offset)
#else
#define LM_OFFSET(smp)	(0)
#endif

/* Base address for section_dispatch_table entries */
#define LM_LDBASE(smp)	(smp->som_addr + LM_OFFSET(smp))

/* Start of text segment */
#define LM_TXTADDR(smp)	(smp->som_addr == (caddr_t)0 ? PAGSIZ : 0)

/* Start of run-time relocation_info */
#define LM_REL(smp)	((struct relocation_info *) \
	(smp->som_addr + LM_OFFSET(smp) + LD_REL((smp)->som_dynamic)))

/* Start of symbols */
#define LM_SYMBOL(smp, i)	((struct nzlist *) \
	(smp->som_addr + LM_OFFSET(smp) + LD_SYMBOL((smp)->som_dynamic) + \
		i * (LD_VERSION_NZLIST_P(smp->som_dynamic->d_version) ? \
			sizeof(struct nzlist) : sizeof(struct nlist))))

/* Start of hash table */
#define LM_HASH(smp)	((struct rrs_hash *) \
	((smp)->som_addr + LM_OFFSET(smp) + LD_HASH((smp)->som_dynamic)))

/* Start of strings */
#define LM_STRINGS(smp)	((char *) \
	((smp)->som_addr + LM_OFFSET(smp) + LD_STRINGS((smp)->som_dynamic)))

/* Start of search paths */
#define LM_PATHS(smp)	((char *) \
	((smp)->som_addr + LM_OFFSET(smp) + LD_PATHS((smp)->som_dynamic)))

/* End of text */
#define LM_ETEXT(smp)	((char *) \
	((smp)->som_addr + LM_TXTADDR(smp) + LD_TEXTSZ((smp)->som_dynamic)))

/* PLT is in data segment, so don't use LM_OFFSET here */
#define LM_PLT(smp)	((jmpslot_t *) \
	((smp)->som_addr + LD_PLT((smp)->som_dynamic)))

/* Parent of link map */
#define LM_PARENT(smp)	(LM_PRIVATE(smp)->spd_parent)

static char		__main_progname[] = "main";
static char		*main_progname = __main_progname;
static char		us[] = "/usr/libexec/ld.so";

char			**environ;
char			*__progname = us;
int			errno;

static int		careful;
static int		anon_fd = -1;

struct so_map		*link_map_head, *main_map;
struct so_map		**link_map_tail = &link_map_head;
struct rt_symbol	*rt_symbol_head;

static char		*ld_library_path;
static char		*ld_preload_path;
static int		no_intern_search;
static int		ld_suppress_warnings;
static int		ld_warn_non_pure_code;

static int		ld_tracing;

static void		*__dlopen(const char *, int);
static int		__dlclose(void *);
static void		*__dlsym(void *, const char *);
static int		__dlctl(void *, int, void *);
static void		__dlexit(void);

static struct ld_entry	ld_entry = {
	__dlopen, __dlclose, __dlsym, __dlctl, __dlexit
};

       void		xprintf(char *, ...);
       int		rtld(int, struct crt_ldso *, struct _dynamic *);
       void		binder_entry(void);
       long		binder(jmpslot_t *);
static int		load_subs(struct so_map *);
static struct so_map	*map_object(struct sod *, struct so_map *);
static void		unmap_object(struct so_map *);
static struct so_map	*alloc_link_map(	char *, struct sod *,
						struct so_map *, caddr_t,
						size_t, struct _dynamic *);
static void		free_link_map(struct so_map *);
static inline void	check_text_reloc(	struct relocation_info *,
						struct so_map *,
						caddr_t);
static void		init_maps(struct so_map *);
static void		reloc_map(struct so_map *);
static void		reloc_copy(struct so_map *);
static void		call_map(struct so_map *, char *);
static char		*rtfindlib(char *, int, int, int *, char *);
static struct nzlist	*lookup(const char *, struct so_map **, int);
static inline struct rt_symbol	*lookup_rts(const char *);
static struct rt_symbol	*enter_rts(const char *, long, int, caddr_t,
				   long, struct so_map *);
static void		maphints(void);
static void		unmaphints(void);

static void		preload(char *);
static void		ld_trace(struct so_map *);

static inline int
strcmp (const char *s1, const char *s2)
{
	while (*s1 == *s2++)
		if (*s1++ == 0)
			return (0);
	return (*(unsigned char *)s1 - *(unsigned char *)--s2);
}

#include "md-static-funcs.c"

/*
 * Called from assembler stub that has set up crtp (passed from crt0)
 * and dp (our __DYNAMIC).
 */
int
rtld(int version, struct crt_ldso *crtp, struct _dynamic *dp)
{
	int			n;
	int			nreloc;		/* # of ld.so relocations */
	struct relocation_info	*reloc;
	struct so_debug		*ddp;
	struct so_map		*smp;

	/* Check version */
	if (		version != CRT_VERSION_BSD_2 &&
			version != CRT_VERSION_BSD_3 &&
			version != CRT_VERSION_BSD_4 &&
			version != CRT_VERSION_SUN)
		return -1;

	/* Fixup __DYNAMIC structure */
	(long)dp->d_un.d_sdt += crtp->crt_ba;

	/* Divide by hand to avoid possible use of library division routine */
	for (	nreloc = 0, n = LD_RELSZ(dp);
		n > 0;
		n -= sizeof(struct relocation_info) ) nreloc++;

	
	/* Relocate ourselves */
	for (reloc = (struct relocation_info *)(LD_REL(dp) + crtp->crt_ba);
	    nreloc; nreloc--, reloc++) {
		long	addr = reloc->r_address + crtp->crt_ba;
		md_relocate_simple(reloc, crtp->crt_ba, (char*)addr);
	}

	if (version >= CRT_VERSION_BSD_4)
		__progname = crtp->crt_ldso;

	if (version >= CRT_VERSION_BSD_3)
		main_progname = crtp->crt_prog;

	/* Setup out (private) environ variable */
	environ = crtp->crt_ep;

	if (issetugid())
		careful = 1;

	if (careful) {
		unsetenv("LD_LIBRARY_PATH");
		unsetenv("LD_PRELOAD");
	}

	/* Setup directory search */
	ld_library_path = getenv("LD_LIBRARY_PATH");
	add_search_path(ld_library_path);
	if (getenv("LD_NOSTD_PATH") == NULL)
		std_search_path();

	ld_suppress_warnings = getenv("LD_SUPPRESS_WARNINGS") != NULL;
	ld_warn_non_pure_code = getenv("LD_WARN_NON_PURE_CODE") != NULL;

	no_intern_search = careful || getenv("LD_NO_INTERN_SEARCH") != 0;

	anon_open();

	/*
	 * Init object administration. We start off with a map description
	 * for `main' and `rtld'.
	 */
	smp = alloc_link_map(main_progname, (struct sod *)0, (struct so_map *)0,
					(caddr_t)0, 0, crtp->crt_dp);
	LM_PRIVATE(smp)->spd_refcount++;
	LM_PRIVATE(smp)->spd_flags |= RTLD_MAIN;
	main_map = smp;

	smp = alloc_link_map(us, (struct sod *)0, (struct so_map *)0,
					(caddr_t)crtp->crt_ba, 0, dp);
	LM_PRIVATE(smp)->spd_refcount++;
	LM_PRIVATE(smp)->spd_flags |= RTLD_RTLD;

	/* Fill in some field in main's __DYNAMIC structure */
	if (version >= CRT_VERSION_BSD_4)
		crtp->crt_ldentry = &ld_entry;
	else
		crtp->crt_dp->d_entry = &ld_entry;

	/* Handle LD_PRELOAD's here */
	ld_preload_path = getenv("LD_PRELOAD");
	if (ld_preload_path != NULL)
		preload(ld_preload_path);

	/* Load subsidiary objects into the process address space */
	ld_tracing = (int)getenv("LD_TRACE_LOADED_OBJECTS");
	load_subs(link_map_head);
	if (ld_tracing) {
		ld_trace(link_map_head);
		exit(0);
	}

	init_maps(link_map_head);

	crtp->crt_dp->d_un.d_sdt->sdt_loaded = link_map_head->som_next;

	ddp = crtp->crt_dp->d_debug;
	ddp->dd_cc = rt_symbol_head;
	if (ddp->dd_in_debugger) {
		caddr_t	addr = (caddr_t)((long)crtp->crt_bp & (~(PAGSIZ - 1)));

		/* Set breakpoint for the benefit of debuggers */
		if (mprotect(addr, PAGSIZ,
		    PROT_READ|PROT_WRITE|PROT_EXEC) == -1) {
			err(1, "Cannot set breakpoint (%s)", main_progname);
		}
		md_set_breakpoint((long)crtp->crt_bp, (long *)&ddp->dd_bpt_shadow);
		if (mprotect(addr, PAGSIZ, PROT_READ|PROT_EXEC) == -1) {
			err(1, "Cannot re-protect breakpoint (%s)",
				main_progname);
		}

		ddp->dd_bpt_addr = crtp->crt_bp;
		if (link_map_head)
			ddp->dd_sym_loaded = 1;
	}

	/* Close the hints file */
	unmaphints();

	/* Close our file descriptor */
	(void)close(crtp->crt_ldfd);
	anon_close();
	return 0;
}


static int
load_subs(struct so_map	*smp)
{

	for (; smp; smp = smp->som_next) {
		struct sod	*sodp;
		long		next = 0;

		if (LM_PRIVATE(smp)->spd_flags & RTLD_RTLD)
			continue;

		if (smp->som_dynamic)
			next = LD_NEED(smp->som_dynamic);

		while (next) {
			struct so_map	*newmap;

			sodp = (struct sod *)(LM_LDBASE(smp) + next);

			if ((newmap = map_object(sodp, smp)) == NULL) {
				if (!ld_tracing) {
					char *fmt = sodp->sod_library ?
						"%s: lib%s.so.%d.%d" :
						"%s: %s";
					err(1, fmt, main_progname,
						sodp->sod_name+LM_LDBASE(smp),
						sodp->sod_major,
						sodp->sod_minor);
				}
				newmap = alloc_link_map(NULL, sodp, smp,
				    0, 0, 0);
			}
			LM_PRIVATE(newmap)->spd_refcount++;
			next = sodp->sod_next;
		}
	}
	return 0;
}

void
ld_trace(struct so_map *smp)
{
	char	*fmt1, *fmt2, *fmt, *main_local;
	int	c;

	if ((main_local = getenv("LD_TRACE_LOADED_OBJECTS_PROGNAME")) == NULL)
		main_local = "";

	if ((fmt1 = getenv("LD_TRACE_LOADED_OBJECTS_FMT1")) == NULL)
		fmt1 = "\t-l%o.%m => %p (%x)\n";

	if ((fmt2 = getenv("LD_TRACE_LOADED_OBJECTS_FMT2")) == NULL)
		fmt2 = "\t%o (%x)\n";

	for (; smp; smp = smp->som_next) {
		struct sod	*sodp;
		char		*name, *path;

		if ((sodp = smp->som_sod) == NULL)
			continue;

		name = (char *)sodp->sod_name;
		if (LM_PARENT(smp))
			name += (long)LM_LDBASE(LM_PARENT(smp));

		if ((path = smp->som_path) == NULL)
			path = "not found";

		fmt = sodp->sod_library ? fmt1 : fmt2;
		while ((c = *fmt++) != '\0') {
			switch (c) {
			default:
				putchar(c);
				continue;
			case '\\':
				switch (c = *fmt) {
				case '\0':
					continue;
				case 'n':
					putchar('\n');
					break;
				case 't':
					putchar('\t');
					break;
				}
				break;
			case '%':
				switch (c = *fmt) {
				case '\0':
					continue;
				case '%':
				default:
					putchar(c);
					break;
				case 'A':
					printf("%s", main_local);
					break;
				case 'a':
					printf("%s", main_progname);
					break;
				case 'o':
					printf("%s", name);
					break;
				case 'm':
					printf("%d", sodp->sod_major);
					break;
				case 'n':
					printf("%d", sodp->sod_minor);
					break;
				case 'p':
					printf("%s", path);
					break;
				case 'x':
					printf("%p", smp->som_addr);
					break;
				}
				break;
			}
			++fmt;
		}
	}
}

/*
 * Allocate a new link map for shared object NAME loaded at ADDR as a
 * result of the presence of link object LOP in the link map PARENT.
 */
static struct so_map *
alloc_link_map(char *path, struct sod *sodp, struct so_map *parent,
	       caddr_t addr, size_t size, struct _dynamic *dp)
{
	struct so_map		*smp;
	struct somap_private	*smpp;

	smpp = (struct somap_private *)xmalloc(sizeof(struct somap_private));
	smp = (struct so_map *)xmalloc(sizeof(struct so_map));
	smp->som_next = NULL;
	*link_map_tail = smp;
	link_map_tail = &smp->som_next;

	/*smp->som_sodbase = 0; NOT USED */
	smp->som_write = 0;
	smp->som_addr = addr;
	smp->som_path = path?strdup(path):NULL;
	smp->som_sod = sodp;
	smp->som_dynamic = dp;
	smp->som_spd = (caddr_t)smpp;

	smpp->spd_refcount = 0;
	smpp->spd_flags = 0;
	smpp->spd_parent = parent;
	smpp->spd_size = size;

#ifdef SUN_COMPAT
	smpp->spd_offset =
		(addr==0 && dp && dp->d_version==LD_VERSION_SUN) ? PAGSIZ : 0;
#endif
	return smp;
}

/*
 * Free the link map for an object being unmapped.  The link map
 * has already been removed from the link map list, so it can't be used
 * after it's been unmapped.
 */
static void
free_link_map(struct so_map *smp)
{

	if ((LM_PRIVATE(smp)->spd_flags & RTLD_DL) != 0) {
		/* free synthetic sod structure allocated in __dlopen() */
		free((char *)smp->som_sod->sod_name);
		free(smp->som_sod);
	}

	/* free the link map structure. */
	free(smp->som_spd);
	if (smp->som_path != NULL)
		free(smp->som_path);
	free(smp);
}

/*
 * Map object identified by link object SODP which was found
 * in link map SMP.
 */
static struct so_map *
map_object(struct sod *sodp, struct so_map *smp)
{
	char		*name;
	struct _dynamic	*dp;
	char		*path, *ipath;
	int		fd;
	caddr_t		addr;
	struct exec	hdr;
	int		usehints = 0;
	struct so_map	*p;

	name = (char *)sodp->sod_name;
	if (smp)
		name += (long)LM_LDBASE(smp);

	if (sodp->sod_library) {
		usehints = 1;
again:
		if (smp == NULL || no_intern_search ||
		    LD_PATHS(smp->som_dynamic) == 0) {
			ipath = NULL;
		} else {
			ipath = LM_PATHS(smp);
			add_search_path(ipath);
		}

		path = rtfindlib(name, sodp->sod_major,
				 sodp->sod_minor, &usehints, ipath);
		if (ipath)
			remove_search_path(ipath);

		if (path == NULL) {
			errno = ENOENT;
			return NULL;
		}
	} else {
		if (careful && *name != '/') {
			errno = EACCES;
			return NULL;
		}
		path = name;
	}

	/* Check if already loaded */
	for (p = link_map_head; p; p = p->som_next)
		if (p->som_path && strcmp(p->som_path, path) == 0)
			break;

	if (p != NULL)
		return p;

	if ((fd = open(path, O_RDONLY, 0)) == -1) {
		if (usehints) {
			usehints = 0;
			goto again;
		}
		return NULL;
	}

	if (read(fd, &hdr, sizeof(hdr)) != sizeof(hdr)) {
		(void)close(fd);
		/*errno = x;*/
		return NULL;
	}

	if (N_BADMAG(hdr)) {
		(void)close(fd);
		errno = EFTYPE;
		return NULL;
	}

	if ((addr = mmap(0, hdr.a_text + hdr.a_data + hdr.a_bss,
	    PROT_READ|PROT_EXEC,
	    MAP_COPY, fd, 0)) == (caddr_t)-1) {
		(void)close(fd);
		return NULL;
	}

#if DEBUG
	xprintf("map1: 0x%x for 0x%x\n", addr, hdr.a_text + hdr.a_data + hdr.a_bss);
#endif

	if (mprotect(addr + hdr.a_text, hdr.a_data,
	    PROT_READ|PROT_WRITE|PROT_EXEC) != 0) {
		(void)close(fd);
		return NULL;
	}

	if (mmap(addr + hdr.a_text + hdr.a_data, hdr.a_bss,
	    PROT_READ|PROT_WRITE,
	    MAP_ANON|MAP_COPY|MAP_FIXED,
	    anon_fd, 0) == (caddr_t)-1) {
		(void)close(fd);
		return NULL;
	}

	(void)close(fd);

	/* Assume _DYNAMIC is the first data item */
	dp = (struct _dynamic *)(addr+hdr.a_text);

	/* Fixup __DYNAMIC structure */
	(long)dp->d_un.d_sdt += (long)addr;

	return alloc_link_map(path, sodp, smp, addr,
	    hdr.a_text + hdr.a_data + hdr.a_bss, dp);
}

/*
 * Unmap a mapped object.
 */
static void
unmap_object(struct so_map *smp)
{
	struct so_map *p, **pp;

	/* remove from link map list */
	pp = &link_map_head;
	while ((p = *pp) != NULL) {
		if (p == smp)
			break;
		pp = &p->som_next;
	}
	if (p == NULL) {
		warnx("warning: link map entry for %s not on link map list!",
		    smp->som_path);
		return;
	}

	*pp = smp->som_next;			/* make list skip it */
	if (link_map_tail == &smp->som_next)	/* and readjust tail pointer */
		link_map_tail = pp;

	/* unmap from address space */
	(void)munmap(smp->som_addr, LM_PRIVATE(smp)->spd_size);
}

void
init_maps(struct so_map *head)
{
	struct so_map	*smp;

	/* Relocate all loaded objects according to their RRS segments */
	for (smp = head; smp; smp = smp->som_next) {
		if (LM_PRIVATE(smp)->spd_flags & RTLD_RTLD)
			continue;
		reloc_map(smp);
	}

	/* Copy any relocated initialized data. */
	for (smp = head; smp; smp = smp->som_next) {
		if (LM_PRIVATE(smp)->spd_flags & RTLD_RTLD)
			continue;
		reloc_copy(smp);
	}

	/* Call any object initialization routines. */
	for (smp = head; smp; smp = smp->som_next) {
		if (LM_PRIVATE(smp)->spd_flags & RTLD_RTLD)
			continue;
		call_map(smp, ".init");
		call_map(smp, "__init");
		call_map(smp, "__GLOBAL__DI");
	}
}

static inline void
check_text_reloc(struct relocation_info *r, struct so_map *smp, caddr_t addr)
{
	char	*sym;

	if (addr >= LM_ETEXT(smp))
		return;

	if (RELOC_EXTERN_P(r))
		sym = LM_STRINGS(smp) +
				LM_SYMBOL(smp, RELOC_SYMBOL(r))->nz_strx;
	else
		sym = "";

	if (ld_warn_non_pure_code && !ld_suppress_warnings)
		warnx("warning: non pure code in %s at %x (%s)",
				smp->som_path, r->r_address, sym);

	if (smp->som_write == 0 &&
		mprotect(smp->som_addr + LM_TXTADDR(smp),
		    LD_TEXTSZ(smp->som_dynamic),
		    PROT_READ|PROT_WRITE|PROT_EXEC) == -1) {

		err(1, "Cannot enable writes to %s:%s",
					main_progname, smp->som_path);
	}

	smp->som_write = 1;
}

static void
reloc_map(struct so_map *smp)
{
	struct _dynamic		*dp = smp->som_dynamic;
	struct relocation_info	*r = LM_REL(smp);
	struct relocation_info	*rend = r + LD_RELSZ(dp)/sizeof(*r);
	long			symbolbase = (long)LM_SYMBOL(smp, 0);
	char			*stringbase = LM_STRINGS(smp);
	int symsize		= LD_VERSION_NZLIST_P(dp->d_version) ?
					sizeof(struct nzlist) :
					sizeof(struct nlist);

	if (LD_PLTSZ(dp))
		md_fix_jmpslot(LM_PLT(smp),
				(long)LM_PLT(smp), (long)binder_entry);

	for (; r < rend; r++) {
		char	*sym;
		caddr_t	addr = smp->som_addr + r->r_address;

		check_text_reloc(r, smp, addr);

		if (RELOC_EXTERN_P(r)) {
			struct so_map	*src_map = NULL;
			struct nzlist	*p, *np;
			long	relocation = md_get_addend(r, addr);

			if (RELOC_LAZY_P(r))
				continue;

			p = (struct nzlist *)
				(symbolbase + symsize * RELOC_SYMBOL(r));

			if (p->nz_type == (N_SETV + N_EXT))
				src_map = smp;

			sym = stringbase + p->nz_strx;

			np = lookup(sym, &src_map, 0/*XXX-jumpslots!*/);
			if (np == NULL)
				errx(1, "Undefined symbol \"%s\" in %s:%s",
					sym, main_progname, smp->som_path);

			/*
			 * Found symbol definition.
			 * If it's in a link map, adjust value
			 * according to the load address of that map.
			 * Otherwise it's a run-time allocated common
			 * whose value is already up-to-date.
			 */
			relocation += np->nz_value;
			if (src_map)
				relocation += (long)src_map->som_addr;

			if (RELOC_PCREL_P(r))
				relocation -= (long)smp->som_addr;

			if (RELOC_COPY_P(r) && src_map) {
				(void)enter_rts(sym,
					(long)addr,
					N_DATA + N_EXT,
					src_map->som_addr + np->nz_value,
					np->nz_size, src_map);
				continue;
			}
			md_relocate(r, relocation, addr, 0);

		} else {
			md_relocate(r,
#ifdef SUN_COMPAT
				md_get_rt_segment_addend(r, addr)
#else
				md_get_addend(r, addr)
#endif
					+ (long)smp->som_addr, addr, 0);
		}

	}

	if (smp->som_write) {
		if (mprotect(smp->som_addr + LM_TXTADDR(smp),
		    LD_TEXTSZ(smp->som_dynamic),
		    PROT_READ|PROT_EXEC) == -1) {
			err(1, "Cannot disable writes to %s:%s",
			    main_progname, smp->som_path);
		}
		smp->som_write = 0;
	}
}

static void
reloc_copy(struct so_map *smp)
{
	struct rt_symbol	*rtsp;

	for (rtsp = rt_symbol_head; rtsp; rtsp = rtsp->rt_next)
		if ((rtsp->rt_smp == NULL || rtsp->rt_smp == smp) &&
				rtsp->rt_sp->nz_type == N_DATA + N_EXT) {
			bcopy(rtsp->rt_srcaddr, (caddr_t)rtsp->rt_sp->nz_value,
							rtsp->rt_sp->nz_size);
		}
}

static void
call_map(struct so_map *smp, char *sym)
{
	struct so_map		*src_map = smp;
	struct nzlist		*np;

	np = lookup(sym, &src_map, 1);
	if (np)
		(*(void (*) (void))(src_map->som_addr + np->nz_value))();
}

/*
 * Run-time common symbol table.
 */

#define RTC_TABSIZE		57
static struct rt_symbol 	*rt_symtab[RTC_TABSIZE];

/*
 * Compute hash value for run-time symbol table
 */
static inline int
hash_string(const char *key)
{
	const char *cp;
	int k;

	cp = key;
	k = 0;
	while (*cp)
		k = (((k << 1) + (k >> 14)) ^ (*cp++)) & 0x3fff;

	return k;
}

/*
 * Lookup KEY in the run-time common symbol table.
 */

static inline struct rt_symbol *
lookup_rts(const char *key)
{
	int			hashval;
	struct rt_symbol	*rtsp;

	/* Determine which bucket.  */

	hashval = hash_string(key) % RTC_TABSIZE;

	/* Search the bucket.  */

	for (rtsp = rt_symtab[hashval]; rtsp; rtsp = rtsp->rt_link)
		if (strcmp(key, rtsp->rt_sp->nz_name) == 0)
			return rtsp;

	return NULL;
}

static struct rt_symbol *
enter_rts(const char *name, long value, int type, caddr_t srcaddr, long size,
	  struct so_map *smp)
{
	int			hashval;
	struct rt_symbol	*rtsp, **rpp;

	/* Determine which bucket */
	hashval = hash_string(name) % RTC_TABSIZE;

	/* Find end of bucket */
	for (rpp = &rt_symtab[hashval]; *rpp; rpp = &(*rpp)->rt_link)
		;

	/* Allocate new common symbol */
	rtsp = (struct rt_symbol *)malloc(sizeof(struct rt_symbol));
	rtsp->rt_sp = (struct nzlist *)malloc(sizeof(struct nzlist));
	rtsp->rt_sp->nz_name = strdup(name);
	rtsp->rt_sp->nz_value = value;
	rtsp->rt_sp->nz_type = type;
	rtsp->rt_sp->nz_size = size;
	rtsp->rt_srcaddr = srcaddr;
	rtsp->rt_smp = smp;
	rtsp->rt_link = NULL;

	/* Link onto linear list as well */
	rtsp->rt_next = rt_symbol_head;
	rt_symbol_head = rtsp;

	*rpp = rtsp;

	return rtsp;
}


/*
 * Lookup NAME in the link maps. The link map producing a definition
 * is returned in SRC_MAP. If SRC_MAP is not NULL on entry the search is
 * confined to that map. If STRONG is set, the symbol returned must
 * have a proper type (used by binder()).
 */
static struct nzlist *
lookup(const char *name, struct so_map **src_map, int strong)
{
	long			common_size = 0;
	struct so_map		*smp, *weak_smp;
	struct rt_symbol	*rtsp;
	struct nzlist		*weak_np = 0;

	if ((rtsp = lookup_rts(name)) != NULL) {
		/* Common symbol is not a member of particular shlib. */
		*src_map = NULL;
		return rtsp->rt_sp;
	}

	weak_smp = NULL; /* XXX - gcc! */

	/*
	 * Search all maps for a definition of NAME
	 */
	for (smp = link_map_head; smp; smp = smp->som_next) {
		int		buckets;
		long		hashval;
		struct rrs_hash	*hp;
		const char	*cp;
		struct	nzlist	*np;

		/* Some local caching */
		long		symbolbase;
		struct rrs_hash	*hashbase;
		char		*stringbase;
		int		symsize;

		if (*src_map && smp != *src_map)
			continue;

		if ((buckets = LD_BUCKETS(smp->som_dynamic)) == 0)
			continue; 

		if (LM_PRIVATE(smp)->spd_flags & RTLD_RTLD)
			continue;

restart:
		/*
		 * Compute bucket in which the symbol might be found.
		 */
		for (hashval = 0, cp = name; *cp; cp++)
			hashval = (hashval << 1) + *cp;

		hashval = (hashval & 0x7fffffff) % buckets;

		hashbase = LM_HASH(smp);
		hp = hashbase + hashval;
		if (hp->rh_symbolnum == -1)
			/* Nothing in this bucket */
			continue;

		symbolbase = (long)LM_SYMBOL(smp, 0);
		stringbase = LM_STRINGS(smp);
		symsize	= LD_VERSION_NZLIST_P(smp->som_dynamic->d_version)?
				sizeof(struct nzlist) :
				sizeof(struct nlist);
		while (hp) {
			np = (struct nzlist *)
				(symbolbase + hp->rh_symbolnum * symsize);
			cp = stringbase + np->nz_strx;
			if (strcmp(cp, name) == 0)
				break;
			if (hp->rh_next == 0)
				hp = NULL;
			else
				hp = hashbase + hp->rh_next;
		}
		if (hp == NULL)
			/* Nothing in this bucket */
			continue;

		/*
		 * We have a symbol with the name we're looking for.
		 */
		if (np->nz_type == N_INDR+N_EXT) {
			/*
			 * Next symbol gives the aliased name. Restart
			 * search with new name and confine to this map.
			 */
			name = stringbase + (++np)->nz_strx;
			*src_map = smp;
			goto restart;
		}

		if (np->nz_value == 0)
			/* It's not a definition */
			continue;

		if (np->nz_type == N_UNDF+N_EXT && np->nz_value != 0) {
			if (np->nz_other == AUX_FUNC) {
				/* It's a weak function definition */
				if (strong)
					continue;
			} else {
				/* It's a common, note value and continue search */
				if (common_size < np->nz_value)
					common_size = np->nz_value;
				continue;
			}
		}
		if (N_BIND(&np->nlist) == BIND_WEAK && weak_np == 0) {
			weak_np = np;
			weak_smp = smp;
			continue;
		}

		*src_map = smp;
		return np;
	}

	if (weak_np) {
		*src_map = weak_smp;
		return weak_np;
	}

	if (common_size == 0)
		/* Not found */
		return NULL;

	/*
	 * It's a common, enter into run-time common symbol table.
	 */
	rtsp = enter_rts(name, (long)calloc(1, common_size),
					N_UNDF + N_EXT, 0, common_size, NULL);

	/* Common symbol is not a member of particular shlib. */
	*src_map = NULL;

#if DEBUG
xprintf("Allocating common: %s size %d at %#x\n", name, common_size, rtsp->rt_sp->nz_value);
#endif

	return rtsp->rt_sp;
}


/*
 * This routine is called from the jumptable to resolve
 * procedure calls to shared objects.
 */
long
binder(jmpslot_t *jsp)
{
	struct so_map	*smp, *src_map = NULL;
	long		addr;
	char		*sym;
	struct nzlist	*np;
	int		index;

	/*
	 * Find the PLT map that contains JSP.
	 */
	for (smp = link_map_head; smp; smp = smp->som_next) {
		if (LM_PLT(smp) < jsp &&
			jsp < LM_PLT(smp) + LD_PLTSZ(smp->som_dynamic)/sizeof(*jsp))
			break;
	}

	if (smp == NULL)
		errx(1, "Call to binder from unknown location: %p", jsp);

	index = jsp->reloc_index & JMPSLOT_RELOC_MASK;

	/* Get the local symbol this jmpslot refers to */
	sym = LM_STRINGS(smp) +
		LM_SYMBOL(smp,RELOC_SYMBOL(&LM_REL(smp)[index]))->nz_strx;

	np = lookup(sym, &src_map, 1);
	if (np == NULL)
		errx(1, "Undefined symbol \"%s\" called from %s:%s at %p",
				sym, main_progname, smp->som_path, jsp);

	/* Fixup jmpslot so future calls transfer directly to target */
	addr = np->nz_value;
	if (src_map)
		addr += (long)src_map->som_addr;

	md_fix_jmpslot(jsp, (long)jsp, addr);

#if DEBUG
xprintf(" BINDER: %s located at = %#x in %s\n", sym, addr, src_map->som_path);
#endif
	return addr;
}


static int			hfd;
static long			hsize;
static struct hints_header	*hheader;
static struct hints_bucket	*hbuckets;
static char			*hstrtab;
static char			*hint_search_path = "";

#define HINTS_VALID (hheader != NULL && hheader != (struct hints_header *)-1)

static void
maphints(void)
{
	caddr_t		addr;

	if ((hfd = open(_PATH_LD_HINTS, O_RDONLY, 0)) == -1) {
		hheader = (struct hints_header *)-1;
		return;
	}

	hsize = PAGSIZ;
	addr = mmap(0, hsize, PROT_READ, MAP_COPY, hfd, 0);

	if (addr == (caddr_t)-1) {
		close(hfd);
		hheader = (struct hints_header *)-1;
		return;
	}

	hheader = (struct hints_header *)addr;
	if (HH_BADMAG(*hheader)) {
		munmap(addr, hsize);
		close(hfd);
		hheader = (struct hints_header *)-1;
		return;
	}

	if (hheader->hh_version != LD_HINTS_VERSION_1 &&
	    hheader->hh_version != LD_HINTS_VERSION_2) {
		munmap(addr, hsize);
		close(hfd);
		hheader = (struct hints_header *)-1;
		return;
	}

	if (hheader->hh_ehints > hsize) {
		if (mmap(addr+hsize, hheader->hh_ehints - hsize,
				PROT_READ, MAP_COPY|MAP_FIXED,
				hfd, hsize) != (caddr_t)(addr+hsize)) {

			munmap((caddr_t)hheader, hsize);
			close(hfd);
			hheader = (struct hints_header *)-1;
			return;
		}
	}

	hbuckets = (struct hints_bucket *)(addr + hheader->hh_hashtab);
	hstrtab = (char *)(addr + hheader->hh_strtab);
	if (hheader->hh_version >= LD_HINTS_VERSION_2)
		hint_search_path = hstrtab + hheader->hh_dirlist;
}

static void
unmaphints(void)
{

	if (HINTS_VALID) {
		munmap((caddr_t)hheader, hsize);
		close(hfd);
		hheader = NULL;
	}
}

static int
hinthash(char *cp, int vmajor, int vminor)
{
	int	k = 0;

	while (*cp)
		k = (((k << 1) + (k >> 14)) ^ (*cp++)) & 0x3fff;

	k = (((k << 1) + (k >> 14)) ^ (vmajor*257)) & 0x3fff;
	if (hheader->hh_version == LD_HINTS_VERSION_1)
		k = (((k << 1) + (k >> 14)) ^ (vminor*167)) & 0x3fff;

	return k;
}

#undef major
#undef minor

static char *
findhint(char *name, int major, int minor, char *prefered_path)
{
	struct hints_bucket	*bp;

	bp = hbuckets + (hinthash(name, major, minor) % hheader->hh_nbucket);

	while (1) {
		/* Sanity check */
		if (bp->hi_namex >= hheader->hh_strtab_sz) {
			warnx("Bad name index: %#x\n", bp->hi_namex);
			break;
		}
		if (bp->hi_pathx >= hheader->hh_strtab_sz) {
			warnx("Bad path index: %#x\n", bp->hi_pathx);
			break;
		}

		if (strcmp(name, hstrtab + bp->hi_namex) == 0) {
			/* It's `name', check version numbers */
			if (bp->hi_major == major &&
				(bp->hi_ndewey < 2 || bp->hi_minor >= minor)) {
					if (prefered_path == NULL ||
					    strncmp(prefered_path,
						hstrtab + bp->hi_pathx,
						strlen(prefered_path)) == 0) {
						return hstrtab + bp->hi_pathx;
					}
			}
		}

		if (bp->hi_next == -1)
			break;

		/* Move on to next in bucket */
		bp = &hbuckets[bp->hi_next];
	}

	/* No hints available for name */
	return NULL;
}

static char *
rtfindlib(char *name, int major, int minor, int *usehints, char *ipath)
{
	char	*cp;
	int	realminor;

	if (hheader == NULL)
		maphints();

	if (!HINTS_VALID || !(*usehints))
		goto lose;

	/* NOTE: `ipath' may reside in a piece of read-only memory */

	if (ld_library_path || ipath) {
		/* Prefer paths from some explicit LD_LIBRARY_PATH */
		char	*lpath;
		char	*dp;

		dp = lpath = concat(ld_library_path ? ld_library_path : "",
				    (ld_library_path && ipath) ? ":" : "",
				    ipath ? ipath : "");

		while ((cp = strsep(&dp, ":")) != NULL) {
			if (*cp) {
				cp = findhint(name, major, minor, cp);
				if (cp) {
					free(lpath);
					return cp;
				}
			}
		}
		free(lpath);

		/*
		 * Not found in hints; try directory search now, before
		 * we get a spurious hint match below (i.e. a match not
		 * on one of the paths we're supposed to search first.
		 */
		realminor = -1;
		cp = (char *)findshlib(name, &major, &realminor, 0);
		if (cp && realminor >= minor)
			return cp;
	}

	/* No LD_LIBRARY_PATH or lib not found in there; check default */
	cp = findhint(name, major, minor, NULL);
	if (cp)
		return cp;

lose:
	/* No hints available for name */
	*usehints = 0;
	realminor = -1;
	add_search_path(hint_search_path);
	cp = (char *)findshlib(name, &major, &realminor, 0);
	remove_search_path(hint_search_path);
	if (cp) {
		if (realminor < minor && !ld_suppress_warnings)
			warnx("warning: lib%s.so.%d.%d: "
			      "minor version >= %d expected, using it anyway",
			      name, major, realminor, minor);
		return cp;
	}
	return NULL;
}

void
preload(char *paths)
{
	struct so_map	*nsmp;
	struct sod	*sodp;
	char		*cp, *dp;

	dp = paths = strdup(paths);
	if (dp == NULL) {
		errx(1, "preload: out of memory");
	}

	while ((cp = strsep(&dp, ":")) != NULL) {
		if ((sodp = (struct sod *)malloc(sizeof(struct sod))) == NULL) {
			errx(1, "preload: %s: out of memory", cp);
			return;
		}

		sodp->sod_name = (long)strdup(cp);
		sodp->sod_library = 0;
		sodp->sod_major = sodp->sod_minor = 0;

		if ((nsmp = map_object(sodp, 0)) == NULL) {
			errx(1, "preload: %s: cannot map object", cp);
		}
		LM_PRIVATE(nsmp)->spd_refcount++;
	}
	free(paths);
	return;
}

static int dlerrno;

/*
 * Populate sod struct for dlopen's call to map_object
 */
static void
build_sod(const char *name, struct sod *sodp)
{
	unsigned int	tuplet;
	int		major, minor;
	char		*realname, *tok, *etok, *cp;

	/* default is an absolute or relative path */
	sodp->sod_name = (long)strdup(name);	/* strtok is destructive */
	sodp->sod_library = 0;
	sodp->sod_major = sodp->sod_minor = 0;

	/* does it look like /^lib/ ? */
	if (strncmp((char *)sodp->sod_name, "lib", 3) != 0)
		return;

	/* is this a filename? */
	if (strchr((char *)sodp->sod_name, '/'))
		return;

	/* skip over 'lib' */
	cp = (char *)sodp->sod_name + 3;

	/* dot guardian */
	if ((strchr(cp, '.') == NULL) || (*(cp+strlen(cp)-1) == '.'))
		return;

	/* default */
	major = minor = -1;

	/* loop through name - parse skipping name */
	for (tuplet = 0; (tok = strsep(&cp, ".")) != NULL; tuplet++) {
		switch (tuplet) {
		case 0:
			/* removed 'lib' and extensions from name */
			realname = tok;
			break;
		case 1:
			/* 'so' extension */
			if (strcmp(tok, "so") != 0)
				goto backout;
			break;
		case 2:
			/* major version extension */
			major = strtol(tok, &etok, 10);
			if (*tok == '\0' || *etok != '\0')
				goto backout;
			break;
		case 3:
			/* minor version extension */
			minor = strtol(tok, &etok, 10);
			if (*tok == '\0' || *etok != '\0')
				goto backout;
			break;
		/* if we get here, it must be weird */
		default:
			goto backout;
		}
	}
	cp = (char *)sodp->sod_name;
	sodp->sod_name = (long)strdup(realname);
	free(cp);
	sodp->sod_library = 1;
	sodp->sod_major = major;
	sodp->sod_minor = minor;
	return;

backout:
	free((char *)sodp->sod_name);
	sodp->sod_name = (long)strdup(name);
}

static void *
__dlopen(const char *name, int mode)
{
	struct sod	*sodp;
	struct so_map	*smp;

	/*
	 * A NULL argument returns the current set of mapped objects.
	 */
	if (name == NULL) {
		LM_PRIVATE(link_map_head)->spd_refcount++;
		return link_map_head;
	}

	if ((sodp = (struct sod *)malloc(sizeof(struct sod))) == NULL) {
		dlerrno = ENOMEM;
		return NULL;
	}

	build_sod(name, sodp);

	if ((smp = map_object(sodp, main_map)) == NULL) {
#ifdef DEBUG
xprintf("%s: %s\n", name, strerror(errno));
#endif
		dlerrno = errno;
		free((char *)sodp->sod_name);
		free(sodp);
		return NULL;
	}

	LM_PRIVATE(smp)->spd_flags |= RTLD_DL;

	if (LM_PRIVATE(smp)->spd_refcount++ > 0) {
		free((char *)sodp->sod_name);
		free(sodp);
		return smp;
	}

	if (load_subs(smp) != 0) {
		if (--LM_PRIVATE(smp)->spd_refcount == 0) {
			unmap_object(smp);
			free_link_map(smp);
		}
		return NULL;
	}

	init_maps(smp);
	return smp;
}

static int
__dlclose(void *fd)
{
	struct so_map	*smp = (struct so_map *)fd;

#ifdef DEBUG
xprintf("dlclose(%s): refcount = %d\n", smp->som_path, LM_PRIVATE(smp)->spd_refcount);
#endif
	if (--LM_PRIVATE(smp)->spd_refcount != 0)
		return 0;

	if ((LM_PRIVATE(smp)->spd_flags & RTLD_DL) == 0)
		return 0;

	/* Dismantle shared object map and descriptor */
	call_map(smp, "__fini");
	call_map(smp, "__GLOBAL__DD");
#if 0
	unload_subs(smp);		/* XXX should unload implied objects */
#endif
	unmap_object(smp);
	free_link_map(smp);
	return 0;
}

static void *
__dlsym(void *fd, const char *sym)
{
	struct so_map	*smp = (struct so_map *)fd, *src_map = NULL;
	struct nzlist	*np;
	long		addr;

	/*
	 * Restrict search to passed map if dlopen()ed.
	 */
	if (LM_PRIVATE(smp)->spd_flags & RTLD_DL)
		src_map = smp;

	np = lookup(sym, &src_map, 1);
	if (np == NULL) {
		dlerrno = ENOENT;
		return NULL;
	}

	/* Fixup jmpslot so future calls transfer directly to target */
	addr = np->nz_value;
	if (src_map)
		addr += (long)src_map->som_addr;

	return (void *)addr;
}

static int
__dlctl(void *fd, int cmd, void *arg)
{
	switch (cmd) {
	case DL_GETERRNO:
		*(int *)arg = dlerrno;
		dlerrno = 0;
		return 0;
	default:
		dlerrno = EOPNOTSUPP;
		return -1;
	}
	return 0;
}

static void
__dlexit(void)
{
	struct so_map	*smp;

	/* Call any object initialization routines. */
	for (smp = link_map_head; smp; smp = smp->som_next) {
		if (LM_PRIVATE(smp)->spd_flags & RTLD_RTLD)
			continue;
		call_map(smp, ".fini");
		call_map(smp, "__GLOBAL__DD");
	}
}

void
#if __STDC__
xprintf(char *fmt, ...)
#else
xprintf(fmt, va_alist)
char	*fmt;
#endif
{
	char buf[256];
	va_list	ap;
#if __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif

	vsprintf(buf, fmt, ap);
	(void)write(1, buf, strlen(buf));
	va_end(ap);
}
