/*	$OpenBSD: exec.h,v 1.31 2015/09/28 20:32:59 deraadt Exp $	*/
/*	$NetBSD: exec.h,v 1.59 1996/02/09 18:25:09 christos Exp $	*/

/*-
 * Copyright (c) 1994 Christopher G. Demetriou
 * Copyright (c) 1993 Theo de Raadt
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)exec.h	8.3 (Berkeley) 1/21/94
 */

#ifndef _SYS_EXEC_H_
#define _SYS_EXEC_H_

/*
 * The following structure is found at the top of the user stack of each
 * user process. The ps program uses it to locate argv and environment
 * strings. Programs that wish ps to display other information may modify
 * it; normally ps_argvstr points to argv[0], and ps_nargvstr is the same
 * as the program's argc. The fields ps_envstr and ps_nenvstr are the
 * equivalent for the environment.
 */
struct ps_strings {
	char	**ps_argvstr;	/* first of 0 or more argument strings */
	int	ps_nargvstr;	/* the number of argument strings */
	char	**ps_envstr;	/* first of 0 or more environment strings */
	int	ps_nenvstr;	/* the number of environment strings */
};

/*
 * Below the PS_STRINGS and sigtramp, we may require a gap on the stack
 * (used to copyin/copyout various emulation data structures).
 */
#define	STACKGAPLEN	(2*1024)	/* plenty enough for now */

/*
 * the following structures allow execve() to put together processes
 * in a more extensible and cleaner way.
 *
 * the exec_package struct defines an executable being execve()'d.
 * it contains the header, the vmspace-building commands, the vnode
 * information, and the arguments associated with the newly-execve'd
 * process.
 *
 * the exec_vmcmd struct defines a command description to be used
 * in creating the new process's vmspace.
 */

struct proc;
struct exec_package;

typedef int (*exec_makecmds_fcn)(struct proc *, struct exec_package *);

struct execsw {
	u_int	es_hdrsz;		/* size of header for this format */
	exec_makecmds_fcn es_check;	/* function to check exec format */
	struct emul *es_emul;		/* emulation */
};

struct exec_vmcmd {
	int	(*ev_proc)(struct proc *p, struct exec_vmcmd *cmd);
				/* procedure to run for region of vmspace */
	u_long	ev_len;		/* length of the segment to map */
	u_long	ev_addr;	/* address in the vmspace to place it at */
	struct	vnode *ev_vp;	/* vnode pointer for the file w/the data */
	u_long	ev_offset;	/* offset in the file for the data */
	u_int	ev_prot;	/* protections for segment */
	int	ev_flags;
#define VMCMD_RELATIVE  0x0001  /* ev_addr is relative to base entry */
#define VMCMD_BASE      0x0002  /* marks a base entry */
};

#define	EXEC_DEFAULT_VMCMD_SETSIZE	8	/* # of cmds in set to start */

/* exec vmspace-creation command set; see below */
struct exec_vmcmd_set {
	u_int	evs_cnt;
	u_int	evs_used;
	struct	exec_vmcmd *evs_cmds;
	struct	exec_vmcmd evs_start[EXEC_DEFAULT_VMCMD_SETSIZE];
};

struct exec_package {
	char	*ep_name;		/* file's name */
	void	*ep_hdr;		/* file's exec header */
	u_int	ep_hdrlen;		/* length of ep_hdr */
	u_int	ep_hdrvalid;		/* bytes of ep_hdr that are valid */
	struct nameidata *ep_ndp;	/* namei data pointer for lookups */
	struct	exec_vmcmd_set ep_vmcmds;  /* vmcmds used to build vmspace */
	struct	vnode *ep_vp;		/* executable's vnode */
	struct	vattr *ep_vap;		/* executable's attributes */
	u_long	ep_taddr;		/* process's text address */
	u_long	ep_tsize;		/* size of process's text */
	u_long	ep_daddr;		/* process's data(+bss) address */
	u_long	ep_dsize;		/* size of process's data(+bss) */
	u_long	ep_maxsaddr;		/* proc's max stack addr ("top") */
	u_long	ep_minsaddr;		/* proc's min stack addr ("bottom") */
	u_long	ep_ssize;		/* size of process's stack */
	u_long	ep_entry;		/* process's entry point */
	u_int	ep_flags;		/* flags; see below. */
	char	**ep_fa;		/* a fake args vector for scripts */
	int	ep_fd;			/* a file descriptor we're holding */
	struct  emul *ep_emul;		/* os emulation */
	void	*ep_emul_arg;		/* emulation argument */
	size_t	ep_emul_argsize;	/* emulation argument size */
	void	*ep_emul_argp;		/* emulation argument pointer */
	char	*ep_interp;		/* name of interpreter if any */
	u_long	ep_interp_pos;		/* interpreter load position */
};
#define	EXEC_INDIR	0x0001		/* script handling already done */
#define	EXEC_HASFD	0x0002		/* holding a shell script */
#define	EXEC_HASARGL	0x0004		/* has fake args vector */
#define	EXEC_SKIPARG	0x0008		/* don't copy user-supplied argv[0] */
#define	EXEC_DESTR	0x0010		/* destructive ops performed */

#ifdef _KERNEL
/*
 * functions used either by execve() or the various cpu-dependent execve()
 * hooks.
 */
int	exec_makecmds(struct proc *, struct exec_package *);
int	exec_runcmds(struct proc *, struct exec_package *);
void	vmcmdset_extend(struct exec_vmcmd_set *);
void	kill_vmcmds(struct exec_vmcmd_set *evsp);
int	vmcmd_map_pagedvn(struct proc *, struct exec_vmcmd *);
int	vmcmd_map_readvn(struct proc *, struct exec_vmcmd *);
int	vmcmd_map_zero(struct proc *, struct exec_vmcmd *);
int	vmcmd_randomize(struct proc *, struct exec_vmcmd *);
void	*copyargs(struct exec_package *,
				    struct ps_strings *,
				    void *, void *);
void	setregs(struct proc *, struct exec_package *,
				    u_long, register_t *);
int	check_exec(struct proc *, struct exec_package *);
int	exec_setup_stack(struct proc *, struct exec_package *);
int	exec_process_vmcmds(struct proc *, struct exec_package *);

#ifdef DEBUG
void	new_vmcmd(struct exec_vmcmd_set *evsp,
		    int (*proc)(struct proc *p, struct exec_vmcmd *),
		    u_long len, u_long addr, struct vnode *vp, u_long offset,
		    u_int prot, int flags);
#define	NEW_VMCMD(evsp,proc,len,addr,vp,offset,prot) \
	new_vmcmd(evsp,proc,len,addr,vp,offset,prot, 0);
#define NEW_VMCMD2(evsp,proc,len,addr,vp,offset,prot,flags) \
	new_vmcmd(evsp,proc,len,addr,vp,offset,prot,flags)
#else	/* DEBUG */
#define NEW_VMCMD(evsp,proc,len,addr,vp,offset,prot) \
	NEW_VMCMD2(evsp,proc,len,addr,vp,offset,prot,0)
#define	NEW_VMCMD2(evsp,proc,len,addr,vp,offset,prot,flags) do { \
	struct exec_vmcmd *vcp; \
	if ((evsp)->evs_used >= (evsp)->evs_cnt) \
		vmcmdset_extend(evsp); \
	vcp = &(evsp)->evs_cmds[(evsp)->evs_used++]; \
	vcp->ev_proc = (proc); \
	vcp->ev_len = (len); \
	vcp->ev_addr = (addr); \
	if ((vcp->ev_vp = (vp)) != NULLVP) \
		vref(vp); \
	vcp->ev_offset = (offset); \
	vcp->ev_prot = (prot); \
	vcp->ev_flags = (flags); \
} while (0)

#endif /* DEBUG */

/* Initialize an empty vmcmd set */
#define VMCMDSET_INIT(vmc) do { \
	(vmc)->evs_cnt = EXEC_DEFAULT_VMCMD_SETSIZE; \
	(vmc)->evs_cmds = (vmc)->evs_start; \
	(vmc)->evs_used = 0; \
} while (0)	

/*
 * Exec function switch:
 *
 * Note that each makecmds function is responsible for loading the
 * exec package with the necessary functions for any exec-type-specific
 * handling.
 *
 * Functions for specific exec types should be defined in their own
 * header file.
 */
extern struct	execsw execsw[];
extern int	nexecs;
extern int	exec_maxhdrsz;

/*
 * If non-zero, stackgap_random specifies the upper limit of the random gap size
 * added to the fixed stack position. Must be n^2.
 */
extern int	stackgap_random;

/* Limit on total PT_OPENBSD_RANDOMIZE bytes. */
#define ELF_RANDOMIZE_LIMIT 64*1024

#endif /* _KERNEL */

#ifndef N_PAGSIZ
#define	N_PAGSIZ(ex)	(__LDPGSZ)
#endif

/*
 * Legacy a.out structures and defines; start deleting these when
 * external use no longer exist.
 */


/*
 * Header prepended to each a.out file.
 * only manipulate the a_midmag field via the
 * N_SETMAGIC/N_GET{MAGIC,MID,FLAG} macros below.
 */
struct exec {
	u_int32_t	a_midmag;	/* htonl(flags<<26|mid<<16|magic) */
	u_int32_t	a_text;		/* text segment size */
	u_int32_t	a_data;		/* initialized data size */
	u_int32_t	a_bss;		/* uninitialized data size */
	u_int32_t	a_syms;		/* symbol table size */
	u_int32_t	a_entry;	/* entry point */
	u_int32_t	a_trsize;	/* text relocation size */
	u_int32_t	a_drsize;	/* data relocation size */
};

/* a_magic */
#define	OMAGIC		0407	/* old impure format */
#define	NMAGIC		0410	/* read-only text */
#define	ZMAGIC		0413	/* demand load format */
#define	QMAGIC		0314	/* "compact" demand load format; deprecated */

/*
 * a_mid - keep sorted in numerical order for sanity's sake
 * ensure that: 0 < mid < 0x3ff
 */
#define	MID_ZERO	0	/* unknown - implementation dependent */
#define	MID_SUN010	1	/* sun 68010/68020 binary */
#define	MID_SUN020	2	/* sun 68020-only binary */
#define	MID_PC386	100	/* 386 PC binary. (so quoth BFD) */
#define	MID_ROMPAOS	104	/* old IBM RT */
#define	MID_I386	134	/* i386 BSD binary */
#define	MID_M68K	135	/* m68k BSD binary with 8K page sizes */
#define	MID_M68K4K	136	/* DO NOT USE: m68k BSD binary with 4K page sizes */
#define	MID_NS32532	137	/* ns32532 */
#define	MID_SPARC	138	/* sparc */
#define	MID_PMAX	139	/* pmax */
#define	MID_VAX1K	140	/* vax 1k page size */
#define	MID_ALPHA	141	/* Alpha BSD binary */
#define	MID_MIPS	142	/* big-endian MIPS */
#define	MID_ARM6	143	/* ARM6 */
#define	MID_SH3		145	/* SH3 */
#define	MID_POWERPC	149	/* big-endian PowerPC */
#define	MID_VAX		150	/* vax */
#define	MID_SPARC64	151	/* LP64 sparc */
#define MID_MIPS2	152	/* MIPS2 */
#define	MID_M88K	153	/* m88k BSD binary */ 
#define	MID_HPPA	154	/* hppa */
#define	MID_AMD64	157	/* AMD64 */
#define	MID_MIPS64	158	/* big-endian MIPS64 */
#define	MID_HP200	200	/* hp200 (68010) BSD binary */
#define	MID_HP300	300	/* hp300 (68020+68881) BSD binary */
#define	MID_HPUX	0x20C	/* hp200/300 HP-UX binary */
#define	MID_HPUX800	0x20B	/* hp800 HP-UX binary pa1.0 */
#define	MID_HPPA11	0x210	/* hp700 HP-UX binary pa1.1 */
#define	MID_HPPA20	0x214	/* hp700 HP-UX binary pa2.0 */

/*
 * a_flags
 */
#define EX_DYNAMIC	0x20
#define EX_PIC		0x10
#define EX_DPMASK	0x30
/*
 * Interpretation of the (a_flags & EX_DPMASK) bits:
 *
 *	00		traditional executable or object file
 *	01		object file contains PIC code (set by `as -k')
 *	10		dynamic executable
 *	11		position independent executable image
 * 			(eg. a shared library)
 *
 */

/*
 * The a.out structure's a_midmag field is a network-byteorder encoding
 * of this int
 *	FFFFFFmmmmmmmmmmMMMMMMMMMMMMMMMM
 * Where `F' is 6 bits of flag like EX_DYNAMIC,
 *       `m' is 10 bits of machine-id like MID_I386, and
 *       `M' is 16 bits worth of magic number, ie. ZMAGIC.
 * The macros below will set/get the needed fields.
 */
#define	N_GETMAGIC(ex) \
    ( (((ex).a_midmag)&0xffff0000) ? (ntohl(((ex).a_midmag))&0xffff) : ((ex).a_midmag))
#define	N_GETMAGIC2(ex) \
    ( (((ex).a_midmag)&0xffff0000) ? (ntohl(((ex).a_midmag))&0xffff) : \
    (((ex).a_midmag) | 0x10000) )
#define	N_GETMID(ex) \
    ( (((ex).a_midmag)&0xffff0000) ? ((ntohl(((ex).a_midmag))>>16)&0x03ff) : MID_ZERO )
#define	N_GETFLAG(ex) \
    ( (((ex).a_midmag)&0xffff0000) ? ((ntohl(((ex).a_midmag))>>26)&0x3f) : 0 )
#define	N_SETMAGIC(ex,mag,mid,flag) \
    ( (ex).a_midmag = htonl( (((flag)&0x3f)<<26) | (((mid)&0x03ff)<<16) | \
    (((mag)&0xffff)) ) )

#define	N_ALIGN(ex,x) \
	(N_GETMAGIC(ex) == ZMAGIC || N_GETMAGIC(ex) == QMAGIC ? \
	((x) + __LDPGSZ - 1) & ~(__LDPGSZ - 1) : (x))

/* Valid magic number check. */
#define	N_BADMAG(ex) \
	(N_GETMAGIC(ex) != NMAGIC && N_GETMAGIC(ex) != OMAGIC && \
	N_GETMAGIC(ex) != ZMAGIC && N_GETMAGIC(ex) != QMAGIC)

/* Address of the bottom of the text segment. */
#define	N_TXTADDR(ex)	(N_GETMAGIC2(ex) == (ZMAGIC|0x10000) ? 0 : __LDPGSZ)

/* Address of the bottom of the data segment. */
#define	N_DATADDR(ex) \
	(N_GETMAGIC(ex) == OMAGIC ? N_TXTADDR(ex) + (ex).a_text : \
	(N_TXTADDR(ex) + (ex).a_text + __LDPGSZ - 1) & ~(__LDPGSZ - 1))

/* Address of the bottom of the bss segment. */
#define	N_BSSADDR(ex) \
	(N_DATADDR(ex) + (ex).a_data)

/* Text segment offset. */
#define	N_TXTOFF(ex) \
	( N_GETMAGIC2(ex)==ZMAGIC || N_GETMAGIC2(ex)==(QMAGIC|0x10000) ? \
	0 : (N_GETMAGIC2(ex)==(ZMAGIC|0x10000) ? __LDPGSZ : \
	sizeof(struct exec)) )

/* Data segment offset. */
#define	N_DATOFF(ex) \
	N_ALIGN(ex, N_TXTOFF(ex) + (ex).a_text)

/* Text relocation table offset. */
#define	N_TRELOFF(ex) \
	(N_DATOFF(ex) + (ex).a_data)

/* Data relocation table offset. */
#define	N_DRELOFF(ex) \
	(N_TRELOFF(ex) + (ex).a_trsize)

/* Symbol table offset. */
#define	N_SYMOFF(ex) \
	(N_DRELOFF(ex) + (ex).a_drsize)

/* String table offset. */
#define	N_STROFF(ex) \
	(N_SYMOFF(ex) + (ex).a_syms)

#include <machine/exec.h>

#endif /* !_SYS_EXEC_H_ */
