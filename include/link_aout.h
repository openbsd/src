/*	$OpenBSD: link_aout.h,v 1.2 2004/01/22 21:48:02 espie Exp $	*/

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
 *      This product includes software developed by Paul Kranenburg.
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
 */

/*
 * RRS section definitions.
 *
 * The layout of some data structures defined in this header file is
 * such that we can provide compatibility with the SunOS 4.x shared
 * library scheme.
 */

/*
 * Symbol description with size. This is simply an `nlist' with
 * one field (nz_size) added.
 * Used to convey size information on items in the data segment
 * of shared objects. An array of these live in the shared object's
 * text segment and is addressed by the `sdt_nzlist' field.
 */
struct nzlist {
	struct nlist	nlist;
	unsigned long	nz_size;
#define nz_un		nlist.n_un
#define nz_strx		nlist.n_un.n_strx
#define nz_name		nlist.n_un.n_name
#define nz_type		nlist.n_type
#define nz_value	nlist.n_value
#define nz_desc		nlist.n_desc
#define nz_other	nlist.n_other
};

#define N_AUX(p)	((p)->n_other & 0xf)
#define N_BIND(p)	(((unsigned int)(p)->n_other >> 4) & 0xf)
#define N_OTHER(r, v)	(((unsigned int)(r) << 4) | ((v) & 0xf))

#define AUX_OBJECT	1
#define AUX_FUNC	2
/*#define BIND_LOCAL	0	not used */
/*#define BIND_GLOBAL	1	not used */
#define BIND_WEAK	2

/*
 * The `section_dispatch_table' structure contains offsets to various data
 * structures needed to do run-time relocation.
 */
struct section_dispatch_table {
	struct so_map *sdt_loaded;	/* List of loaded objects */
	long	sdt_sods;		/* List of shared objects descriptors */
	long	sdt_paths;		/* Library search paths */
	long	sdt_got;		/* Global offset table */
	long	sdt_plt;		/* Procedure linkage table */
	long	sdt_rel;		/* Relocation table */
	long	sdt_hash;		/* Symbol hash table */
	long	sdt_nzlist;		/* Symbol table itself */
	long	sdt_filler2;		/* Unused (was: stab_hash) */
	long	sdt_buckets;		/* Number of hash buckets */
	long	sdt_strings;		/* Symbol strings */
	long	sdt_str_sz;		/* Size of symbol strings */
	long	sdt_text_sz;		/* Size of text area */
	long	sdt_plt_sz;		/* Size of procedure linkage table */
};

/*
 * RRS symbol hash table, addressed by `sdt_hash' in section_dispatch_table.
 * Used to quickly lookup symbols of the shared object by hashing
 * on the symbol's name. `rh_symbolnum' is the index of the symbol
 * in the shared object's symbol list (`sdt_nzlist'), `rh_next' is
 * the next symbol in the hash bucket (in case of collisions).
 */
struct rrs_hash {
	int	rh_symbolnum;		/* Symbol number */
	int	rh_next;		/* Next hash entry */
};

/*
 * `rt_symbols' is used to keep track of run-time allocated commons
 * and data items copied from shared objects.
 */
struct rt_symbol {
	struct nzlist		*rt_sp;		/* The symbol */
	struct rt_symbol	*rt_next;	/* Next in linear list */
	struct rt_symbol	*rt_link;	/* Next in bucket */
	caddr_t			rt_srcaddr;	/* Address of "master" copy */
	struct so_map		*rt_smp;	/* Originating map */
};

/*
 * Debugger interface structure.
 */
struct so_debug {
	int	dd_version;		/* Version # of interface */
	int	dd_in_debugger;		/* Set when run by debugger */
	int	dd_sym_loaded;		/* Run-time linking brought more
					   symbols into scope */
	char   	 *dd_bpt_addr;		/* Address of rtld-generated bpt */
	int	dd_bpt_shadow;		/* Original contents of bpt */
	struct rt_symbol *dd_cc;	/* Allocated commons/copied data */
};

/*
 * Entry points into ld.so - user interface to the run-time linker.
 */
struct ld_entry {
	void	*(*dlopen)(const char *, int);
	int	(*dlclose)(void *);
	void	*(*dlsym)(void *, const char *);
	int	(*dlctl)(void *, int, void *);
	void	(*dlexit)(void);
	void	(*dlrsrvd[3])(void);
};

/*
 * This is the structure pointed at by the __DYNAMIC symbol if an
 * executable requires the attention of the run-time link editor.
 * __DYNAMIC is given the value zero if no run-time linking needs to
 * be done (it is always present in shared objects).
 * The union `d_un' provides for different versions of the dynamic
 * linking mechanism (switched on by `d_version'). The last version
 * used by Sun is 3. We leave some room here and go to version number
 * 8 for NetBSD, the main difference lying in the support for the
 * `nz_list' type of symbols.
 */

struct	_dynamic {
	int		d_version;	/* version # of this interface */
	struct so_debug	*d_debug;
	union {
		struct section_dispatch_table *d_sdt;
	} d_un;
	struct ld_entry *d_entry;	/* compat - now in crt_ldso */
};

#define LD_VERSION_SUN		(3)
#define LD_VERSION_BSD		(8)
#define LD_VERSION_NZLIST_P(v)	((v) >= 8)

#define LD_GOT(x)	((x)->d_un.d_sdt->sdt_got)
#define LD_PLT(x)	((x)->d_un.d_sdt->sdt_plt)
#define LD_REL(x)	((x)->d_un.d_sdt->sdt_rel)
#define LD_SYMBOL(x)	((x)->d_un.d_sdt->sdt_nzlist)
#define LD_HASH(x)	((x)->d_un.d_sdt->sdt_hash)
#define LD_STRINGS(x)	((x)->d_un.d_sdt->sdt_strings)
#define LD_NEED(x)	((x)->d_un.d_sdt->sdt_sods)
#define LD_BUCKETS(x)	((x)->d_un.d_sdt->sdt_buckets)
#define LD_PATHS(x)	((x)->d_un.d_sdt->sdt_paths)

#define LD_GOTSZ(x)	((x)->d_un.d_sdt->sdt_plt - (x)->d_un.d_sdt->sdt_got)
#define LD_RELSZ(x)	((x)->d_un.d_sdt->sdt_hash - (x)->d_un.d_sdt->sdt_rel)
#define LD_HASHSZ(x)	((x)->d_un.d_sdt->sdt_nzlist - (x)->d_un.d_sdt->sdt_hash)
#define LD_STABSZ(x)	((x)->d_un.d_sdt->sdt_strings - (x)->d_un.d_sdt->sdt_nzlist)
#define LD_PLTSZ(x)	((x)->d_un.d_sdt->sdt_plt_sz)
#define LD_STRSZ(x)	((x)->d_un.d_sdt->sdt_str_sz)
#define LD_TEXTSZ(x)	((x)->d_un.d_sdt->sdt_text_sz)

/*
 * Interface to ld.so
 */
struct crt_ldso {
	int		crt_ba;		/* Base address of ld.so */
	int		crt_dzfd;	/* "/dev/zero" file descriptor (SunOS) */
	int		crt_ldfd;	/* ld.so file descriptor */
	struct _dynamic	*crt_dp;	/* Main's __DYNAMIC */
	char		**crt_ep;	/* environment strings */
	caddr_t		crt_bp;		/* Breakpoint if run from debugger */
	char		*crt_prog;	/* Program name (v3) */
	char		*crt_ldso;	/* Link editor name (v4) */
	struct ld_entry	*crt_ldentry;	/* dl*() access (v4) */
};

/*
 * Version passed from crt0 to ld.so (1st argument to _rtld()).
 */
#define CRT_VERSION_SUN		1
#define CRT_VERSION_BSD_2	2
#define CRT_VERSION_BSD_3	3
#define CRT_VERSION_BSD_4	4
