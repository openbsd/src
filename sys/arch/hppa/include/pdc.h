/*	$OpenBSD: pdc.h,v 1.7 1998/12/05 17:34:04 mickey Exp $	*/

/*
 * Copyright (c) 1990 mt Xinu, Inc.  All rights reserved.
 * Copyright (c) 1990,1991,1992,1994 University of Utah.  All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software is hereby
 * granted provided that (1) source code retains these copyright, permission,
 * and disclaimer notices, and (2) redistributions including binaries
 * reproduce the notices in supporting documentation, and (3) all advertising
 * materials mentioning features or use of this software display the following
 * acknowledgement: ``This product includes software developed by the
 * Computer Systems Laboratory at the University of Utah.''
 *
 * Copyright (c) 1990 mt Xinu, Inc.
 * This file may be freely distributed in any form as long as
 * this copyright notice is included.
 * MTXINU, THE UNIVERSITY OF UTAH, AND CSL PROVIDE THIS SOFTWARE ``AS
 * IS'' AND WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,
 * WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE.
 *
 * CSL requests users of this software to return to csl-dist@cs.utah.edu any
 * improvements that they make and grant CSL redistribution rights.
 *
 *	Utah $Hdr: pdc.h 1.12 94/12/14$
 */

#ifndef	_MACHINE_PDC_H_
#define _MACHINE_PDC_H_

/*
 * Definitions for interaction with "Processor Dependent Code",
 * which is a set of ROM routines used to provide information to the OS.
 * Also includes definitions for the layout of "Page Zero" memory when
 * boot code is invoked.
 *
 * Glossary:
 *	PDC:	Processor Dependent Code (ROM or copy of ROM).
 *	IODC:	I/O Dependent Code (module-type dependent code).
 *	IPL:	Boot program (loaded into memory from boot device).
 *	HPA:	Hard Physical Address (hardwired address).
 *	SPA:	Soft Physical Address (reconfigurable address).
 *
 *
 *
 *
 * Definitions for talking to IODC (I/O Dependent Code).
 *
 * The PDC is used to load I/O Dependent Code from a particular module.
 * I/O Dependent Code is module-type dependent software which provides
 * a uniform way to identify, initialize, and access a module (and in
 * some cases, their devices).
 */

/*
 * Our Initial Memory Module is laid out as follows.
 *
 *	0x000		+--------------------+
 *			| Page Zero (iomod.h)|
 *	0x800		+--------------------+
 *			|                    |
 *			|                    |
 *			|        PDC         |
 *			|                    |
 *			|                    |
 *	MEM_FREE	+--------------------+
 *			|                    |
 *              	|    Console IODC    |
 *			|                    |
 *	MEM_FREE+16k	+--------------------+
 *			|                    |
 *              	|  Boot Device IODC  |
 *			|                    |
 *	IPL_START	+--------------------+
 *			|                    |
 *			| IPL Code or Kernel |
 *			|                    |
 *			+--------------------+
 *
 * Restrictions:
 *	MEM_FREE (pagezero.mem_free) can be no greater than 32K.
 *	The PDC may use up to MEM_FREE + 32K (for Console & Boot IODC).
 *	IPL_START must be less than or equal to 64K.
 *
 * The IPL (boot) Code is immediately relocated to RELOC (check
 * "../stand/Makefile") to make way for the Kernel.
 */

#define	IODC_MAXSIZE	(16 * 1024)	/* maximum size of IODC */
#define	MINIOSIZ	64		/* minimum buffer size for IODC call */
#define	MAXIOSIZ	(64 * 1024)	/* maximum buffer size for IODC call */


/*
 * The PDC Entry Points and their arguments...
 */

#define	PDC_POW_FAIL	1	/* prepare for power failure */
#define PDC_POW_FAIL_DFLT	0

#define	PDC_CHASSIS	2	/* update chassis display (see below) */
#define	PDC_CHASSIS_DISP	0	/* update display */
#define	PDC_CHASSIS_WARN	1	/* return warnings */
#define	PDC_CHASSIS_ALL		2	/* update display & return warnings */

#define	PDC_PIM		3	/* access Processor Internal Memory */
#define	PDC_PIM_HPMC		0	/* read High Pri Mach Chk data */
#define	PDC_PIM_SIZE		1	/* return size */
#define	PDC_PIM_LPMC		2	/* read Low Pri Mach Chk data */
#define	PDC_PIM_SBD		3	/* read soft boot data */
#define	PDC_PIM_TOC		4	/* read TOC data (used to use HPMC) */

#define	PDC_MODEL	4	/* processor model number info */
#define	PDC_MODEL_INFO		0	/* processor model number info */
#define	PDC_MODEL_BOOTID	1	/* set BOOT_ID of processor */
#define	PDC_MODEL_COMP		2	/* return component version numbers */
#define	PDC_MODEL_MODEL		3	/* return system model information */
#define	PDC_MODEL_ENSPEC	4	/* enable product-specific instrs */
#define	PDC_MODEL_DISPEC	5	/* disable product-specific instrs */

#define	PDC_CACHE	5	/* return cache and TLB params */
#define	PDC_CACHE_DFLT		0

#define	PDC_HPA		6	/* return HPA of processor */
#define	PDC_HPA_DFLT		0

#define	PDC_COPROC	7	/* return co-processor configuration */
#define	PDC_COPROC_DFLT		0

#define	PDC_IODC	8	/* talk to IODC */
#define	PDC_IODC_READ		0	/* read IODC entry point */
#define		IODC_DATA	0	/* get first 16 bytes from mod IODC */
#define		IODC_INIT	3	/* initialize (see options below) */
#define		IODC_INIT_FIRST	2	/* find first device on module */
#define		IODC_INIT_NEXT	3	/* find subsequent devices on module */
#define		IODC_INIT_ALL	4	/* initialize module and device */
#define		IODC_INIT_DEV	5	/* initialize device */
#define		IODC_INIT_MOD	6	/* initialize module */
#define		IODC_INIT_MSG	9	/* return error message(s) */
#define		IODC_INIT_STR	20	/* find device w/ spec in string */
#define		IODC_IO		4	/* perform I/O (see options below) */
#define		IODC_IO_READ	0	/* read from boot device */
#define		IODC_IO_WRITE	1	/* write to boot device */
#define		IODC_IO_CONSIN	2	/* read from console */
#define		IODC_IO_CONSOUT	3	/* write to conosle */
#define		IODC_IO_CLOSE	4	/* close device */
#define		IODC_IO_MSG	9	/* return error message(s) */
#define		IODC_SPA	5	/* get extended SPA information */
#define		IODC_SPA_DFLT	0	/* return SPA information */
#define		IODC_TEST	8	/* perform self tests */
#define		IODC_TEST_INFO	0	/* return test information */
#define		IODC_TEST_STEP	1	/* execute a particular test */
#define		IODC_TEST_TEST	2	/* describe a test section */
#define		IODC_TEST_MSG	9	/* return error message(s) */
#define	PDC_IODC_NINIT		2	/* non-destructive init */
#define	PDC_IODC_DINIT		3	/* destructive init */
#define	PDC_IODC_MEMERR		4	/* check for memory errors */
#define	PDC_IODC_IMEMMASTER	5	/* interlieved memory master ID */

#define	PDC_TOD		9	/* access time-of-day clock */
#define	PDC_TOD_READ		0	/* read TOD clock */
#define	PDC_TOD_WRITE		1	/* write TOD clock */
#define	PDC_TOD_ITIMER		2	/* calibrate Interval Timer (CR16) */

#define	PDC_STABLE	10	/* access Stable Storage (SS) */
#define	PDC_STABLE_READ		0	/* read SS */
#define	PDC_STABLE_WRITE	1	/* write SS */
#define	PDC_STABLE_SIZE		2	/* return size of SS */
#define	PDC_STABLE_VRFY		3	/* verify contents of SS */
#define	PDC_STABLE_INIT		4	/* initialize SS */

#define	PDC_NVM		11	/* access Non-Volatile Memory (NVM) */
#define	PDC_NVM_READ		0	/* read NVM */
#define	PDC_NVM_WRITE		1	/* write NVM */
#define	PDC_NVM_SIZE		2	/* return size of NVM */
#define	PDC_NVM_VRFY		3	/* verify contents of NVM */
#define	PDC_NVM_INIT		4	/* initialize NVM */

#define	PDC_ADD_VALID	12	/* check address for validity */
#define	PDC_ADD_VALID_DFLT	0

#define	PDC_BUS_BAD	13	/* verify Error Detection Circuitry (EDC) */
#define	PDC_BUS_BAD_DLFT	0

#define	PDC_DEBUG	14	/* return address of PDC debugger */
#define	PDC_DEBUG_DFLT		0

#define	PDC_INSTR	15	/* return instr that invokes PDCE_CHECK */
#define	PDC_INSTR_DFLT		0

#define	PDC_PROC	16	/* stop currently executing processor */
#define	PDC_PROC_DFLT		0

#define	PDC_CONF	17	/* (de)configure a module */
#define	PDC_CONF_DECONF		0	/* deconfigure module */
#define	PDC_CONF_RECONF		1	/* reconfigure module */
#define	PDC_CONF_INFO		2	/* get config informaion */

#define PDC_BLOCK_TLB	18	/* Manage Block TLB entries (BTLB) */
#define PDC_BTLB_DEFAULT	0	/* Return BTLB configuration info  */
#define PDC_BTLB_INSERT		1	/* Insert a BTLB entry             */
#define PDC_BTLB_PURGE		2	/* Purge a BTLB entry              */
#define PDC_BTLB_PURGE_ALL	3	/* Purge all BTLB entries          */

#define PDC_TLB		19	/* Manage Hardware TLB handling */
#define PDC_TLB_INFO		0	/* Return HW-TLB configuration info  */
#define PDC_TLB_CONFIG		1	/* Set HW-TLB pdir base and size */

#define PDC_TLB_CURRPDE		1	/* cr28 points to current pde on miss */
#define PDC_TLB_RESERVD		3	/* reserved */
#define PDC_TLB_NEXTPDE		5	/* cr28 points to next pde on miss */
#define PDC_TLB_WORD3		7	/* cr28 is word 3 of 16 byte pde */

#define	PDC_SOFT_POWER		23	/* support for soft power switch */
#define	PDC_SOFT_POWER_INFO	0	/* get info about soft power switch */
#define	PDC_SOFT_POWER_ENABLE	1	/* enable/disable soft power switch */

#define	PDC_MEMMAP		128	/* hp700: return page information */
#define	PDC_MEMMAP_HPA		0	/* map module # to HPA */

#define	PDC_EEPROM		129	/* Hversion dependent */
#define	PDC_EEPROM_READ_WORD	0
#define	PDC_EEPROM_WRITE_WORD	1
#define	PDC_EEPROM_READ_BYTE	2
#define	PDC_EEPROM_WRITE_BYTE	3

#define	PDC_LAN_STATION_ID	138     /* Hversion dependent mechanism for */
#define	PDC_LAN_STATION_ID_READ	0       /* getting the lan station address  */

#define	PDC_ERR_OK		0	/* operation complete */
#define	PDC_ERR_WARNING		3	/* OK, but warning */
#define	PDC_ERR_NOPROC		-1	/* no such procedure */
#define	PDC_ERR_NOPT		-2	/* no such option */
#define	PDC_ERR_COMPL		-3	/* unable to complete w/o error */
#define	PDC_ERR_EOD		-9	/* end of device list */
#define	PDC_ERR_INVAL		-10	/* invalid argument */
#define	PDC_ERR_PFAIL		-12	/* aborted by powerfail */

#if !defined(_LOCORE)

struct iomod;

typedef int (*pdcio_t) __P((int, int, ...));
typedef int (*iodcio_t) __P((struct iomod *, int, ...));

/*
 * Commonly used PDC calls and the structures they return.
 */

struct pdc_pim {	/* PDC_PIM */
	u_int	count;		/* actual (HPMC, LPMC) or total (SIZE) count */
	u_int	archsize;	/* size of architected regions (see "pim.h") */
	u_int	filler[30];
};

struct pdc_model {	/* PDC_MODEL */
	u_int	hvers;		/* hardware version */
	u_int	svers;		/* software version */
	u_int	hw_id;		/* unique processor hardware identifier */
	u_int	boot_id;	/* same as hw_id */
	u_int	sw_id;		/* software security and licensing */
	u_int	sw_cap;		/* OS capabilities of processor */
	u_int	arch_rev;	/* architecture revision */
	u_int	pot_key;	/* potential key */
	u_int	curr_key;	/* current key */
	int	filler1;
	u_int	filler2[22];
};

struct cache_cf {	/* PDC_CACHE (for "struct pdc_cache") */
	u_int	cc_resv0: 4,
		cc_block: 4,	/* used to determine most efficient stride */
		cc_line	: 3,	/* max data written by store (16-byte mults) */
		cc_resv1: 2,	/* (reserved) */
		cc_wt	: 1,	/* D-cache: write-to = 0, write-through = 1 */
		cc_sh	: 2,	/* separate I and D = 0, shared I and D = 1 */
		cc_cst  : 3,	/* D-cache: incoherent = 0, coherent = 1 */
		cc_resv2: 5,	/* (reserved) */
		cc_assoc: 8;	/* D-cache: associativity of cache */
};

struct tlb_cf {		/* PDC_CACHE (for "struct pdc_cache") */
	u_int	tc_resv1:12,	/* (reserved) */
		tc_sh	: 2,	/* separate I and D = 0, shared I and D = 1 */
		tc_hvers: 1,	/* H-VERSION dependent */
		tc_page : 1,	/* 2K page size = 0, 4k page size = 1 */
		tc_cst  : 3,	/* incoherent = 0, coherent = 1 */
		tc_resv2: 5,	/* (reserved) */
		tc_assoc: 8;	/* associativity of TLB */
};

struct pdc_cache {	/* PDC_CACHE */
/* Instruction cache */
	u_int	ic_size;	/* size of I-cache (in bytes) */
	struct cache_cf ic_conf;/* cache configuration (see above) */
	u_int	ic_base;	/* start addr of I-cache (for FICE flush) */
	u_int	ic_stride;	/* addr incr per i_count iteration (flush) */
	u_int	ic_count;	/* number of i_loop iterations (flush) */
	u_int	ic_loop;	/* number of FICE's per addr stride (flush) */
/* Data cache */
	u_int	dc_size;	/* size of D-cache (in bytes) */
	struct cache_cf dc_conf;/* cache configuration (see above) */
	u_int	dc_base;	/* start addr of D-cache (for FDCE flush) */
	u_int	dc_stride;	/* addr incr per d_count iteration (flush) */
	u_int	dc_count;	/* number of d_loop iterations (flush) */
	u_int	dc_loop;	/* number of FDCE's per addr stride (flush) */
/* Instruction TLB */
	u_int	it_size;	/* number of entries in I-TLB */
	struct tlb_cf it_conf;	/* I-TLB configuration (see above) */
	u_int	it_sp_base;	/* start space of I-TLB (for PITLBE flush) */
	u_int	it_sp_stride;	/* space incr per sp_count iteration (flush) */
	u_int	it_sp_count;	/* number of off_count iterations (flush) */
	u_int	it_off_base;	/* start offset of I-TLB (for PITLBE flush) */
	u_int	it_off_stride;	/* offset incr per off_count iteration (flush)*/
	u_int	it_off_count;	/* number of it_loop iterations/space (flush) */
	u_int	it_loop;	/* number of PITLBE's per off_stride (flush) */
/* Data TLB */
	u_int	dt_size;	/* number of entries in D-TLB */
	struct tlb_cf dt_conf;	/* D-TLB configuration (see above) */
	u_int	dt_sp_base;	/* start space of D-TLB (for PDTLBE flush) */
	u_int	dt_sp_stride;	/* space incr per sp_count iteration (flush) */
	u_int	dt_sp_count;	/* number of off_count iterations (flush) */
	u_int	dt_off_base;	/* start offset of D-TLB (for PDTLBE flush) */
	u_int	dt_off_stride;	/* offset incr per off_count iteration (flush)*/
	u_int	dt_off_count;	/* number of dt_loop iterations/space (flush) */
	u_int	dt_loop;	/* number of PDTLBE's per off_stride (flush) */
	u_int	filler[2];
};

struct pdc_hpa {	/* PDC_HPA */
	hppa_hpa_t hpa;	/* HPA of processor */
	int	filler1;
	u_int	filler2[30];
};

struct pdc_coproc {	/* PDC_COPROC */
	u_int	ccr_enable;	/* same format as CCR (CR 10) */
	u_int	ccr_present;	/* which co-proc's are present (bitset) */
	u_int	filler2[30];
};

struct pdc_tod {	/* PDC_TOD, PDC_TOD_READ */
	u_int	sec;		/* elapsed time since 00:00:00 GMT, 1/1/70 */
	u_int	usec;		/* accurate to microseconds */
	u_int	filler2[30];
};

struct pdc_instr {	/* PDC_INSTR */
	u_int	instr;		/* instruction that invokes PDC mchk entry pt */
	int	filler1;
	u_int	filler2[30];
};

struct pdc_iodc_read {	/* PDC_IODC, PDC_IODC_READ */
	int	size;		/* number of bytes in selected entry point */
	int	filler1;
	u_int	filler2[30];
};

struct pdc_iodc_minit {	/* PDC_IODC, PDC_IODC_NINIT or PDC_IODC_DINIT */
	u_int	stat;		/* HPA.io_status style error returns */
	u_int	max_spa;	/* size of SPA (in bytes) > max_mem+map_mem */
	u_int	max_mem;	/* size of "implemented" memory (in bytes) */
	u_int	map_mem;	/* size of "mapable-only" memory (in bytes) */
	u_int	filler[28];
};

struct btlb_info {		/* for "struct pdc_btlb" (PDC_BTLB) */
	u_int	resv0: 8,	/* (reserved) */
		num_i: 8,	/* Number of instruction slots */
		num_d: 8,	/* Number of data slots */
		num_c: 8;	/* Number of combined slots */
};

struct pdc_btlb {	/* PDC_BLOCK_TLB */
	u_int	min_size;	/* Min size in pages */
	u_int	max_size;	/* Max size in pages */
	struct btlb_info finfo;	/* Fixed range info */
	struct btlb_info vinfo; /* Variable range info */
	u_int 	filler[28];
};

struct pdc_hwtlb {	/* PDC_TLB */
	u_int	min_size;	/* What do these mean? */
	u_int	max_size;
	u_int	filler[30];
};

struct pdc_memmap {	/* PDC_MEMMAP */
	u_int	hpa;		/* HPA for module */
	u_int	morepages;	/* additional IO pages */
	u_int	filler[30];
};

/*
 * The PDC_CHASSIS is a strange bird.  The format for updating the display
 * is as follows:
 *
 *	0     11 12      14    15   16    19 20    23 24    27 28    31
 *	+-------+----------+-------+--------+--------+--------+--------+
 *	|   R   | OS State | Blank |  Hex1  |  Hex2  |  Hex3  |  Hex4  |
 *	+-------+----------+-------+--------+--------+--------+--------+
 *
 * Unfortunately, someone forgot to tell the hardware designers that
 * there was supposed to be a hex display somewhere.  The result is,
 * you can only toggle 5 LED's and the fault light.
 *
 * Interesting values for Hex1-Hex4 and the resulting LED displays:
 *
 *	FnFF			CnFF:
 *	 0	- - - - -		Counts in binary from 0x0 - 0xF 
 *	 2	o - - - -		for corresponding values of `n'.
 *	 4	o o - - -
 *	 6	o o o - -
 *	 8	o o o o -
 *	 A	o o o o o
 *
 * If the "Blank" bit is set, the display should be made blank.
 * The values for "OS State" are defined below.
 */

#define	PDC_CHASSIS_BAR	0xF0FF	/* create a bar graph with LEDs */
#define	PDC_CHASSIS_CNT	0xC0FF	/* count with LEDs */

#define	PDC_OSTAT(os)	(((os) & 0x7) << 17)
#define	PDC_OSTAT_OFF	0x0	/* all off */
#define	PDC_OSTAT_FAULT	0x1	/* the red LED of death */
#define	PDC_OSTAT_TEST	0x2	/* self test */
#define	PDC_OSTAT_BOOT	0x3	/* boot program running */
#define	PDC_OSTAT_SHUT	0x4	/* shutdown in progress */
#define	PDC_OSTAT_WARN	0x5	/* battery dying, etc */
#define	PDC_OSTAT_RUN	0x6	/* OS running */
#define	PDC_OSTAT_ON	0x7	/* all on */

/*
 * Device path specifications used by PDC.
 */
struct device_path {
	u_char	dp_flags;	/* see bit definitions below */
	char	dp_bc[6];	/* Bus Converter routing info to a specific */
				/* I/O adaptor (< 0 means none, > 63 resvd) */
	u_char	dp_mod;		/* fixed field of specified module */
	int	dp_layers[6];	/* device-specific info (ctlr #, unit # ...) */
};

/* dp_flags */
#define	PF_AUTOBOOT	0x80	/* These two are PDC flags for how to locate */
#define	PF_AUTOSEARCH	0x40	/*	the "boot device" */
#define	PF_TIMER	0x0f	/* power of 2 # secs "boot timer" (0 == dflt) */

/*
 * A processors Stable Storage is accessed through the PDC.  There are
 * at least 96 bytes of stable storage (the device path information may
 * or may not exist).  However, as far as I know, processors provide at
 * least 192 bytes of stable storage.
 */
struct stable_storage {
	struct device_path ss_pri_boot;	/* (see above) */
	char	ss_filenames[32];
	u_short	ss_os_version;	/* 0 == none, 1 == HP-UX, 2 == MPE-XL */
	char	ss_os[22];	/* OS-dependant information */
	char	ss_pdc[7];	/* reserved */
	char	ss_fast_size;	/* how much memory to test.  0xf == all, or */
				/*	else it's (256KB << ss_fast_size) */
	struct device_path ss_console;
	struct device_path ss_alt_boot;
	struct device_path ss_keyboard;
};

/*
 * Recoverable error indications provided to boot code by the PDC.
 * Any non-zero value indicates error.
 */
struct boot_err {
	u_int	be_resv : 10,	/* (reserved) */
		be_fixed : 6,	/* module that produced error */
		be_chas : 16;	/* error code (interpret as 4 hex digits) */
};


/*
 * The PDC uses the following structure to completely define an I/O
 * module and the interface to its IODC.
 */
typedef
struct pz_device {
	struct device_path pz_dp;
#define	pz_flags	pz_dp.dp_flags
#define	pz_bc		pz_dp.dp_bc
#define	pz_mod		pz_dp.dp_mod
#define	pz_layers	pz_dp.dp_layers
	struct iomod *pz_hpa;	/* HPA base address of device */
	caddr_t	pz_spa;		/* SPA base address (zero if no SPA exists) */
	iodcio_t pz_iodc_io;	/* entry point of device's driver routines */
	short	pz_resv;	/* (reserved) */
	u_short	pz_class;	/* (see below) */
} pz_device_t;

/* pz_class */
#define	PCL_NULL	0	/* illegal */
#define	PCL_RANDOM	1	/* random access (disk) */
#define	PCL_SEQU	2	/* sequential access (tape) */
#define	PCL_DUPLEX	7	/* full-duplex point-to-point (RS-232, Net) */
#define	PCL_KEYBD	8	/* half-duplex input (HIL Keyboard) */
#define	PCL_DISPL	9	/* half-duplex ouptput (display) */
#define	PCL_CLASS_MASK	0xf	/* XXX class mask */
#define	PCL_NET_MASK	0x1000	/* mask for bootp/tftp device */

/*
 * The following structure defines what a particular IODC returns when
 * given the IODC_DATA argument.
 */
struct iodc_data {
	u_int	iodc_model: 8,		/* hardware model number */
		iodc_revision:8,	/* software revision */
		iodc_spa_io: 1,		/* 0:memory, 1:device */
		iodc_spa_pack:1,	/* 1:packed multiplexor */
		iodc_spa_enb:1,		/* 1:has an spa */
		iodc_spa_shift:5,	/* power of two # bytes in SPA space */
		iodc_more: 1,		/* iodc_data is: 0:8-byte, 1:16-byte */
		iodc_word: 1,		/* iodc_data is: 0:byte, 1:word */
		iodc_pf: 1,		/* 1:supports powerfail */
		iodc_type: 5;		/* see below */
	u_int	iodc_sv_rev: 4,		/* software version revision number */
		iodc_sv_model:20,	/* software interface model # */
		iodc_sv_opt: 8;		/* type-specific options */
	u_char	iodc_rev;		/* revision number of IODC code */
	u_char	iodc_dep;		/* module-dependent information */
	u_char	iodc_rsv[2];		/* reserved */
	u_short	iodc_cksum;		/* 16-bit checksum of whole IODC */
	u_short	iodc_length;		/* number of entry points in IODC */
		/* IODC entry points follow... */
};

extern pdcio_t pdc;

#ifdef _KERNEL
struct consdev;

extern int kernelmapped;

void pdc_init __P((void));
int pdc_call __P((iodcio_t, int, ...));

void pdccnprobe __P((struct consdev *));
void pdccninit __P((struct consdev *));
int pdccngetc __P((dev_t));
void pdccnputc __P((dev_t, int));
void pdccnpollc __P((dev_t, int));
#endif

#endif	/* !(_LOCORE) */

#endif	/* _MACHINE_PDC_H_ */
