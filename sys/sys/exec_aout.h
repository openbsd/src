/*	$OpenBSD: exec_aout.h,v 1.9 1998/04/25 06:28:14 niklas Exp $	*/
/*	$NetBSD: exec_aout.h,v 1.15 1996/05/18 17:20:54 christos Exp $	*/

/*
 * Copyright (c) 1993, 1994 Christopher G. Demetriou
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
 *      This product includes software developed by Christopher G. Demetriou.
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

#ifndef N_PAGSIZ
#define	N_PAGSIZ(ex)	(__LDPGSZ)
#endif

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
#define MID_ROMPAOS	104	/* old IBM RT */
#define	MID_I386	134	/* i386 BSD binary */
#define	MID_M68K	135	/* m68k BSD binary with 8K page sizes */
#define	MID_M68K4K	136	/* DO NOT USE: m68k BSD binary with 4K page sizes */
#define	MID_NS32532	137	/* ns32532 */
#define	MID_SPARC	138	/* sparc */
#define	MID_PMAX	139	/* pmax */
#define	MID_VAX		140	/* vax */
#define	MID_ALPHA	141	/* Alpha BSD binary */
#define	MID_MIPS	142	/* big-endian MIPS */
#define	MID_ARM6	143	/* ARM6 */
#define MID_ROMP	149	/* IBM RT */
#define MID_M88K        151     /* m88k BSD binary */ 
#define	MID_HP200	200	/* hp200 (68010) BSD binary */
#define	MID_HP300	300	/* hp300 (68020+68881) BSD binary */
#define	MID_HPUX	0x20C	/* hp200/300 HP-UX binary */
#define	MID_HPUX800     0x20B   /* hp800 HP-UX binary */

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


#ifdef _KERNEL

/* the "a.out" format's entry in the exec switch */
int	exec_aout_makecmds __P((struct proc *, struct exec_package *));

/* functions which prepare various a.out executable types */
/*
 * MI portion
 */
int	exec_aout_prep_zmagic __P((struct proc *, struct exec_package *));
int	exec_aout_prep_nmagic __P((struct proc *, struct exec_package *));
int	exec_aout_prep_omagic __P((struct proc *, struct exec_package *));

/* For compatibility modules */
int	exec_aout_prep_oldzmagic __P((struct proc *, struct exec_package *));
int	exec_aout_prep_oldnmagic __P((struct proc *, struct exec_package *));
int	exec_aout_prep_oldomagic __P((struct proc *, struct exec_package *));

/*
 * MD portion
 */
int cpu_exec_aout_makecmds __P((struct proc *, struct exec_package *));

#endif /* _KERNEL */
