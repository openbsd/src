/*	$OpenBSD: macinfo.h,v 1.3 1997/11/30 06:12:30 gene Exp $	*/
/*	$NetBSD: scsi96reg.h,v 1.5 1996/05/05 06:18:02 briggs Exp $	*/

/*
 * Copyright (C) 1996	Allen K. Briggs
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _MAC68K_MACINFO_H_
#define _MAC68K_MACINFO_H_

/* values for machineid --
 * 	These are equivalent to the MacOS Gestalt values. */
#define MACH_MACII		6
#define MACH_MACIIX		7
#define MACH_MACIICX		8
#define MACH_MACSE30		9
#define MACH_MACIICI		11
#define MACH_MACIIFX		13
#define MACH_MACIISI		18
#define MACH_MACQ900		20
#define MACH_MACPB170		21
#define MACH_MACQ700		22
#define MACH_MACCLASSICII	23
#define MACH_MACPB100		24
#define MACH_MACPB140		25
#define MACH_MACQ950		26
#define MACH_MACLCIII		27
#define MACH_MACPB210		29
#define MACH_MACC650		30
#define MACH_MACPB230		32
#define MACH_MACPB180		33
#define MACH_MACPB160		34
#define MACH_MACQ800		35
#define MACH_MACQ650		36
#define MACH_MACLCII		37
#define MACH_MACPB250		38
#define MACH_MACIIVI		44
#define MACH_MACP600		45
#define MACH_MACIIVX		48
#define MACH_MACCCLASSIC	49
#define MACH_MACPB165C		50
#define MACH_MACC610		52
#define MACH_MACQ610		53
#define MACH_MACPB145		54
#define MACH_MACLC520		56
#define MACH_MACC660AV		60
#define MACH_MACP460		62
#define MACH_MACPB180C		71
#define	MACH_MACPB500		72
#define MACH_MACPB270		77
#define MACH_MACQ840AV		78
#define MACH_MACP550		80
#define MACH_MACCCLASSICII	83
#define MACH_MACPB165		84
#define MACH_MACTV		88
#define MACH_MACLC475		89
#define MACH_MACLC575		92
#define MACH_MACQ605		94
#define MACH_MACQ630		98
#define MACH_MACPB280		102
#define MACH_MACPB280C		103
#define MACH_MACPB150		115

/*
 * Machine classes.  These define subsets of the above machines.
 */
#define MACH_CLASSH	0x0000	/* Hopeless cases... */
#define MACH_CLASSII	0x0001	/* MacII class */
#define MACH_CLASSIIci	0x0004	/* Have RBV, but no Egret */
#define MACH_CLASSIIsi	0x0005	/* Similar to IIci -- Have Egret. */
#define MACH_CLASSIIvx	0x0006	/* Similar to IIsi -- different via2 emul? */
#define MACH_CLASSLC	0x0007	/* Low-Cost/Performa/Wal-Mart Macs. */
#define MACH_CLASSPB	0x0008	/* Powerbooks.  Power management. */
#define MACH_CLASSDUO	0x0009	/* Powerbooks Duos.  More integration/Docks. */
#define MACH_CLASSIIfx	0x0080	/* The IIfx is in a class by itself. */
#define MACH_CLASSQ	0x0100	/* non-A/V Centris/Quadras. */
#define MACH_CLASSAV	0x0101	/* A/V Centris/Quadras. */
#define MACH_CLASSQ2	0x0102	/* More Centris/Quadras, different sccA. */

#define MACH_68020	0
#define MACH_68030	1
#define MACH_68040	2
#define MACH_PENTIUM	3	/* 66 and 99 MHz versions *only* */

#ifdef _KERNEL
struct mac68k_machine_S {
	int			cpu_model_index;
	/*
	 * Misc. info from booter.
	 */
	int			machineid;
	int			mach_processor;
	int			mach_memsize;
	int			booter_version;
	/*
	 * Debugging flags.
	 */
	int			do_graybars;
	int			serial_boot_echo;
	int			serial_console;
	int			modem_flags;
	int			modem_cts_clk;
	int			modem_dcd_clk;
	int			print_flags;
	int			print_cts_clk;
	int			print_dcd_clk;
	/*
	 * Misc. hardware info.
	 */
	int			scsi80;		/* Has NCR 5380 */
	int			scsi96;		/* Has NCR 53C96 */
	int			scsi96_2;	/* Has 2nd 53C96 */
	int			sonic;		/* Has SONIC e-net */

	int			sccClkConst;	/* "Constant" for SCC bps */
};

/* What kind of model is this */
struct cpu_model_info {
	int	machineid;	/* MacOS Gestalt value. */
	char	*model_major;	/* Make this distinction to save a few */
	char	*model_minor;	/*      bytes--might be useful, too. */
	int	class;		/* Rough class of machine. */
	  /* forwarded romvec_s is defined in mac68k/macrom.h */
	struct romvec_s *rom_vectors; /* Pointer to our known rom vectors */
};
extern struct cpu_model_info *current_mac_model;

extern unsigned long		IOBase;		/* Base address of I/O */
extern unsigned long		NuBusBase;	/* Base address of NuBus */

extern  struct mac68k_machine_S	mac68k_machine;
extern	unsigned long		load_addr;
#endif /* _KERNEL */

#endif /* _MAC68K_MACINFO_H_ */
