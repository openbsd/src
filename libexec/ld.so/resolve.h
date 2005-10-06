/*	$OpenBSD: resolve.h,v 1.44 2005/10/06 21:53:10 kurt Exp $ */

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
	Elf_Addr load_addr;		/* Real load address */
	char	*load_name;		/* Pointer to object name */
	Elf_Dyn *load_dyn;		/* Pointer to object dynamic data */
	struct elf_object *next;
	struct elf_object *prev;
/* End struct link_map compatible */
	Elf_Addr load_offs;		/* Load offset from link address */

	struct load_list *load_list;

	u_int32_t  load_size;
	Elf_Addr	got_addr;
	Elf_Addr	got_start;
	size_t		got_size;
	Elf_Addr	plt_start;
	size_t		plt_size;

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
			Elf_Rel	*rel;
			Elf_Addr	relsz;
			Elf_Addr	relent;
			Elf_Addr	pltrel;
			Elf_Addr	debug;
			Elf_Addr	textrel;
			Elf_Addr	jmprel;
			Elf_Addr	bind_now;
		} u;
	} Dyn;
#define dyn Dyn.u

	struct elf_object *dep_next;	/* Shadow objects for resolve search */

	int		status;
#define	STAT_RELOC_DONE	0x01
#define	STAT_GOT_DONE	0x02
#define	STAT_INIT_DONE	0x04
#define	STAT_FINI_DONE	0x08
#define	STAT_FINI_READY	0x10
#define	STAT_UNLOADED	0x20

	Elf_Phdr	*phdrp;
	int		phdrc;

	int		obj_type;
#define	OBJTYPE_LDR	1
#define	OBJTYPE_EXE	2
#define	OBJTYPE_LIB	3
#define	OBJTYPE_DLO	4
	int		obj_flags;

	Elf_Word	*buckets;
	u_int32_t	nbuckets;
	Elf_Word	*chains;
	u_int32_t	nchains;
	Elf_Dyn		*dynamic;

	TAILQ_HEAD(,dep_node)	child_list;
	TAILQ_HEAD(,dep_node)	dload_list;
	TAILQ_HEAD(,dep_node)	grpref_list;	/* refs to other load groups */

	int		refcount;	/* dep libs only */
	int		opencount;	/* # dlopen() & exe */
	int		grprefcount;	/* load group refs */

	/* object that caused this module to be loaded, used in symbol lookup */
	elf_object_t	*load_object;

	/* for object confirmation */
	dev_t	dev;
	ino_t inode;
};

struct dep_node {
	TAILQ_ENTRY(dep_node) next_sib;
	elf_object_t *data;
};

void _dl_rt_resolve(void);

void _dl_add_object(elf_object_t *object);
elf_object_t *_dl_finalize_object(const char *objname, Elf_Dyn *dynp,
    const long *, const int objtype, const long laddr, const long loff);
void	_dl_remove_object(elf_object_t *object);
void	_dl_cleanup_objects(void);

elf_object_t *_dl_lookup_object(const char *objname);
elf_object_t *_dl_load_shlib(const char *, elf_object_t *, int, int);
elf_object_t *_dl_tryload_shlib(const char *libname, int type, int flags);

int  _dl_md_reloc(elf_object_t *object, int rel, int relsz);
void _dl_md_reloc_got(elf_object_t *object, int lazy);

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
#define SYM_SEARCH_OBJ		0x08
/* warnnotfound */
#define SYM_NOWARNNOTFOUND	0x00
#define SYM_WARNNOTFOUND	0x10
/* inplt */
#define SYM_NOTPLT		0x00
#define SYM_PLT			0x20

#define SYM_DLSYM		0x40

int _dl_rtld(elf_object_t *object);
void _dl_call_init(elf_object_t *object);
void _dl_link_sub(elf_object_t *dep, elf_object_t *p);
void _dl_link_grpref(elf_object_t *load_group, elf_object_t *load_object);
void _dl_link_dlopen(elf_object_t *dep);
void _dl_unlink_dlopen(elf_object_t *dep);
void _dl_notify_unload_shlib(elf_object_t *object);
void _dl_unload_shlib(elf_object_t *object);
void _dl_unload_dlopen(void);

void _dl_run_all_dtors(void);

Elf_Addr _dl_bind(elf_object_t *object, int index);

int	_dl_match_file(struct sod *sodp, char *name, int namelen);
char	*_dl_find_shlib(struct sod *sodp, const char *searchpath, int nohints);
void	_dl_load_list_free(struct load_list *load_list);

void	_dl_thread_kern_go(void);
void	_dl_thread_kern_stop(void);

extern elf_object_t *_dl_objects;
extern elf_object_t *_dl_last_object;

extern elf_object_t *_dl_loading_object;

extern const char *_dl_progname;
extern struct r_debug *_dl_debug_map;

extern int  _dl_pagesz;
extern int  _dl_errno;

extern char *_dl_libpath;
extern char *_dl_preload;
extern char *_dl_bindnow;
extern char *_dl_traceld;
extern char *_dl_debug;

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


#endif /* _RESOLVE_H_ */
