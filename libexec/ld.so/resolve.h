/*	$OpenBSD: resolve.h,v 1.16 2002/08/11 16:51:04 drahn Exp $ */

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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed under OpenBSD by
 *	Per Fogelstrom, Opsycon AB, Sweden.
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

#ifndef _RESOLVE_H_
#define _RESOLVE_H_

#include <link.h>

struct load_list {
	struct load_list *next;
	char	*start;
	size_t	size;
	int	prot;
};

/*
 *  Structure describing a loaded object.
 *  The head of this struct must be compatible
 *  with struct link_map in sys/link.h
 */
typedef struct elf_object {
	Elf_Addr load_addr;		/* Real load address */
	char	*load_name;		/* Pointer to object name */
	Elf_Dyn *load_dyn;		/* Pointer to object dynamic data */
	struct elf_object *next;
	struct elf_object *prev;
/* End struct link_map compatible */
	Elf_Addr load_offs;		/* Load offset from link address */

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
#define	STAT_RELOC_DONE	1
#define	STAT_GOT_DONE	2
#define	STAT_INIT_DONE	4

	Elf_Phdr	*phdrp;
	int		phdrc;

	int		refcount;
	int		obj_type;
#define	OBJTYPE_LDR	1
#define	OBJTYPE_EXE	2
#define	OBJTYPE_LIB	3
#define	OBJTYPE_DLO	4

	Elf_Word	*buckets;
	u_int32_t	nbuckets;
	Elf_Word	*chains;
	u_int32_t	nchains;
	Elf_Dyn	*dynamic;

} elf_object_t;

extern void _dl_rt_resolve(void);

extern elf_object_t *_dl_add_object(const char *objname, Elf_Dyn *dynp,
	    const u_long *, const int objtype,
	    const long laddr, const long loff);
extern void	_dl_remove_object(elf_object_t *object);

extern elf_object_t *_dl_lookup_object(const char *objname);
extern elf_object_t *_dl_load_shlib(const char *, elf_object_t *, int);
extern void	_dl_unload_shlib(elf_object_t *object);

extern int  _dl_md_reloc(elf_object_t *object, int rel, int relsz);
extern void _dl_md_reloc_got(elf_object_t *object, int lazy);

Elf_Addr _dl_find_symbol(const char *name, elf_object_t *startlook,
	    const Elf_Sym **ref, int myself, int warnnotfound, int inplt);

void _dl_rtld(elf_object_t *object);
void _dl_call_init(elf_object_t *object);

extern elf_object_t *_dl_objects;
extern elf_object_t *_dl_last_object;

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

#endif /* _RESOLVE_H_ */
