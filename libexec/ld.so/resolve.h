/*	$OpenBSD: resolve.h,v 1.88 2018/12/05 04:28:32 guenther Exp $ */

/*
 * Copyright (c) 1998 Per Fogelstrom, Opsycon AB
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

#ifndef _RESOLVE_H_
#define _RESOLVE_H_

#include <sys/queue.h>
#include <link.h>
#include <dlfcn.h>

#define __relro		__attribute__((section(".data.rel.ro")))

/* Number of low tags that are used saved internally (0 .. DT_NUM-1) */
#define DT_NUM	(DT_PREINIT_ARRAYSZ + 1)

struct load_list {
	struct load_list *next;
	void		*start;
	size_t		size;
	int		prot;
	Elf_Addr	moff;
	long		foff;
};

/*
 *  Structure describing a loaded object.
 *  The head of this struct must be compatible
 *  with struct link_map in sys/link.h
 */
typedef struct elf_object elf_object_t;
struct elf_object {
	Elf_Addr obj_base;		/* object's address '0' base */
	char	*load_name;		/* Pointer to object name */
	Elf_Dyn *load_dyn;		/* Pointer to object dynamic data */
	struct elf_object *next;
	struct elf_object *prev;
/* End struct link_map compatible */
	Elf_Addr load_base;		/* Base address of loadable segments */

	struct load_list *load_list;

	u_int32_t  load_size;

	union {
		u_long		info[DT_NUM + DT_PROCNUM];
		struct {
			Elf_Addr	null;		/* Not used */
			Elf_Addr	needed;		/* Not used */
			Elf_Addr	pltrelsz;
			Elf_Addr	*pltgot;
			Elf_Addr	*hash;
			const char	*strtab;
			const Elf_Sym	*symtab;
			Elf_RelA	*rela;
			Elf_Addr	relasz;
			Elf_Addr	relaent;
			Elf_Addr	strsz;
			Elf_Addr	syment;
			void		(*init)(void);
			void		(*fini)(void);
			const char	*soname;
			const char	*rpath;
			Elf_Addr	symbolic;
			Elf_Rel		*rel;
			Elf_Addr	relsz;
			Elf_Addr	relent;
			Elf_Addr	pltrel;
			Elf_Addr	debug;
			Elf_Addr	textrel;
			Elf_Addr	jmprel;
			Elf_Addr	bind_now;
			void		(**init_array)(void);
			void		(**fini_array)(void);
			Elf_Addr	init_arraysz;
			Elf_Addr	fini_arraysz;
			const char	*runpath;
			Elf_Addr	flags;
			Elf_Addr	encoding;
			void		(**preinit_array)(void);
			Elf_Addr	preinit_arraysz;
		} u;
	} Dyn;
#define dyn Dyn.u

	Elf_Addr	relacount;	/* DT_RELACOUNT */
	Elf_Addr	relcount;	/* DT_RELCOUNT */

	int		status;
#define	STAT_RELOC_DONE	0x01
#define	STAT_GOT_DONE	0x02
#define	STAT_INIT_DONE	0x04
#define	STAT_FINI_DONE	0x08
#define	STAT_FINI_READY	0x10
#define	STAT_UNLOADED	0x20
#define	STAT_NODELETE	0x40
#define	STAT_VISITED	0x80
#define	STAT_GNU_HASH	0x100

	Elf_Phdr	*phdrp;
	int		phdrc;

	int		obj_type;
#define	OBJTYPE_LDR	1
#define	OBJTYPE_EXE	2
#define	OBJTYPE_LIB	3
#define	OBJTYPE_DLO	4
	int		obj_flags;	/* c.f. <sys/exec_elf.h> DF_1_* */

	/* shared by ELF and GNU hash */
	u_int32_t	nbuckets;
	u_int32_t	nchains;	/* really, number of symbols */
	union {
		struct {
			/* specific to ELF hash */
			const Elf_Word	*buckets;
			const Elf_Word	*chains;
		} u_elf;
		struct {
			/* specific to GNU hash */
			const Elf32_Word	*buckets;
			const Elf32_Word	*chains;
			const Elf_Addr		*bloom;
			Elf32_Word		mask_bm;
			Elf32_Word		shift2;
			Elf32_Word		symndx;
		} u_gnu;
	} hash_u;
#define buckets_elf	hash_u.u_elf.buckets
#define chains_elf	hash_u.u_elf.chains
#define buckets_gnu	hash_u.u_gnu.buckets
#define chains_gnu	hash_u.u_gnu.chains
#define bloom_gnu	hash_u.u_gnu.bloom
#define mask_bm_gnu	hash_u.u_gnu.mask_bm
#define shift2_gnu	hash_u.u_gnu.shift2
#define symndx_gnu	hash_u.u_gnu.symndx

	TAILQ_HEAD(,dep_node)	child_list;	/* direct dep libs of object */
	TAILQ_HEAD(,dep_node)	grpsym_list;	/* ordered complete dep list */
	TAILQ_HEAD(,dep_node)	grpref_list;	/* refs to other load groups */

	int		refcount;	/* dep libs only */
	int		opencount;	/* # dlopen() & exe */
	int		grprefcount;	/* load group refs */
#define OBJECT_REF_CNT(object) \
    ((object->refcount + object->opencount + object->grprefcount))
#define OBJECT_DLREF_CNT(object) \
    ((object->opencount + object->grprefcount))

	/* object that caused this module to be loaded, used in symbol lookup */
	elf_object_t	*load_object;
	struct sod	sod;

	/* for object confirmation */
	dev_t	dev;
	ino_t inode;

	/* thread local storage info */
	Elf_Addr	tls_fsize;
	Elf_Addr	tls_msize;
	Elf_Addr	tls_align;
	const void	*tls_static_data;
	int		tls_offset;

	/* relro bits */
	Elf_Addr	relro_addr;
	Elf_Addr	relro_size;

	/* generation number of last grpsym insert on this object */
	unsigned int grpsym_gen;

	char **rpath;
	char **runpath;

	/* nonzero if trace enabled for this object */
	int traced;
};

struct dep_node {
	TAILQ_ENTRY(dep_node) next_sib;
	elf_object_t *data;
};


/* Please don't rename or make hidden; gdb(1) knows about these. */
Elf_Addr _dl_bind(elf_object_t *object, int index);
void	_dl_debug_state(void);

/* exported to the application */
extern char *__progname;

__BEGIN_HIDDEN_DECLS
void _dl_add_object(elf_object_t *object);
elf_object_t *_dl_finalize_object(const char *objname, Elf_Dyn *dynp,
    Elf_Phdr *phdrp, int phdrc, const int objtype, const long lbase,
    const long obase);
void	_dl_remove_object(elf_object_t *object);
void	_dl_cleanup_objects(void);

elf_object_t *_dl_load_shlib(const char *, elf_object_t *, int, int);
elf_object_t *_dl_tryload_shlib(const char *libname, int type, int flags);

int _dl_md_reloc(elf_object_t *object, int rel, int relsz);
int _dl_md_reloc_got(elf_object_t *object, int lazy);

Elf_Addr _dl_find_symbol(const char *name, const Elf_Sym **this,
    int flags, const Elf_Sym *ref_sym, elf_object_t *object,
    const elf_object_t **pobj);
Elf_Addr _dl_find_symbol_bysym(elf_object_t *req_obj, unsigned int symidx,
    const Elf_Sym **ref, int flags, const Elf_Sym *ref_sym,
    const elf_object_t **pobj);
/*
 * defines for _dl_find_symbol() flag field, three bits of meaning
 * myself	- clear: search all objects,	set: search only this object
 * warnnotfound - clear: no warning,		set: warn if not found
 * inplt	- clear: possible plt ref	set: real matching function.
 *
 * inplt - due to how ELF handles function addresses in shared libraries
 * &func may actually refer to the plt entry in the main program
 * rather than the actual function address in the .so file.
 * This rather bizarre behavior is documented in the SVR4 ABI.
 * when getting the function address to relocate a PLT entry
 * the 'real' function address is necessary, not the possible PLT address.
 */
/* myself */
#define SYM_SEARCH_ALL		0x00
#define SYM_SEARCH_SELF		0x01
#define SYM_SEARCH_OTHER	0x02
#define SYM_SEARCH_NEXT		0x04
/* warnnotfound */
#define SYM_NOWARNNOTFOUND	0x00
#define SYM_WARNNOTFOUND	0x10
/* inplt */
#define SYM_NOTPLT		0x00
#define SYM_PLT			0x20

#define SYM_DLSYM		0x40

int _dl_load_dep_libs(elf_object_t *object, int flags, int booting);
int _dl_rtld(elf_object_t *object);
void _dl_call_init(elf_object_t *object);
void _dl_link_child(elf_object_t *dep, elf_object_t *p);
void _dl_link_grpsym(elf_object_t *object, int checklist);
void _dl_cache_grpsym_list(elf_object_t *object);
void _dl_cache_grpsym_list_setup(elf_object_t *object);
void _dl_link_grpref(elf_object_t *load_group, elf_object_t *load_object);
void _dl_link_dlopen(elf_object_t *dep);
void _dl_unlink_dlopen(elf_object_t *dep);
void _dl_notify_unload_shlib(elf_object_t *object);
void _dl_unload_shlib(elf_object_t *object);
void _dl_unload_dlopen(void);

void _dl_run_all_dtors(void);

int	_dl_match_file(struct sod *sodp, const char *name, int namelen);
char	*_dl_find_shlib(struct sod *sodp, char **searchpath, int nohints);
void	_dl_load_list_free(struct load_list *load_list);

typedef void lock_cb(int);
void	_dl_thread_kern_go(lock_cb *);
lock_cb	*_dl_thread_kern_stop(void);

char	*_dl_getenv(const char *, char **);
void	_dl_unsetenv(const char *, char **);

void	_dl_trace_setup(char **);
void	_dl_trace_object_setup(elf_object_t *);
int	_dl_trace_plt(const elf_object_t *, const char *);

/* tib.c */
void	_dl_allocate_tls_offsets(void);
void	_dl_allocate_first_tib(void);
void	_dl_set_tls(elf_object_t *_object, Elf_Phdr *_ptls, Elf_Addr _libaddr,
	    const char *_libname);
extern int _dl_tib_static_done;

extern elf_object_t *_dl_objects;
extern elf_object_t *_dl_last_object;

extern elf_object_t *_dl_loading_object;

extern struct r_debug *_dl_debug_map;

extern int  _dl_pagesz;
extern int  _dl_errno;

extern char **_dl_libpath;

extern int _dl_bindnow;
extern int _dl_traceld;
extern int _dl_debug;

extern char *_dl_preload;
extern char *_dl_tracefmt1;
extern char *_dl_tracefmt2;
extern char *_dl_traceprog;

extern int _dl_trust;

#define DL_DEB(P) do { if (_dl_debug) _dl_printf P ; } while (0)

#define	DL_NOT_FOUND		1
#define	DL_CANT_OPEN		2
#define	DL_NOT_ELF		3
#define	DL_CANT_OPEN_REF	4
#define	DL_CANT_MMAP		5
#define	DL_NO_SYMBOL		6
#define	DL_INVALID_HANDLE	7
#define	DL_INVALID_CTL		8
#define	DL_NO_OBJECT		9
#define	DL_CANT_FIND_OBJ	10
#define	DL_CANT_LOAD_OBJ	11
#define	DL_INVALID_MODE		12

#define ELF_ROUND(x,malign) (((x) + (malign)-1) & ~((malign)-1))
#define ELF_TRUNC(x,malign) ((x) & ~((malign)-1))

/* symbol lookup cache */
typedef struct sym_cache {
	const elf_object_t *obj;
	const Elf_Sym	*sym;
	int flags;
} sym_cache;

extern sym_cache *_dl_symcache;
extern int _dl_symcachestat_hits;
extern int _dl_symcachestat_lookups;
TAILQ_HEAD(dlochld, dep_node);
extern struct dlochld _dlopened_child_list;
__END_HIDDEN_DECLS

#endif /* _RESOLVE_H_ */
