/*	$OpenBSD: exec_som.h,v 1.2 1998/07/14 15:26:34 mickey Exp $	*/

/*
 *  (c) Copyright 1991 HEWLETT-PACKARD COMPANY
 *
 *  To anyone who acknowledges that this file is provided "AS IS"
 *  without any express or implied warranty:
 *      permission to use, copy, modify, and distribute this file
 *  for any purpose is hereby granted without fee, provided that
 *  the above copyright notice and this notice appears in all
 *  copies, and that the name of Hewlett-Packard Company not be
 *  used in advertising or publicity pertaining to distribution
 *  of the software without specific, written prior permission.
 *  Hewlett-Packard Company makes no representations about the
 *  suitability of this software for any purpose.
 */

/*
 * Copyright (c) 1990,1994 The University of Utah and
 * the Computer Systems Laboratory (CSL).  All rights reserved.
 *
 * THE UNIVERSITY OF UTAH AND CSL PROVIDE THIS SOFTWARE IN ITS "AS IS"
 * CONDITION, AND DISCLAIM ANY LIABILITY OF ANY KIND FOR ANY DAMAGES
 * WHATSOEVER RESULTING FROM ITS USE.
 *
 * CSL requests users of this software to return to csl-dist@cs.utah.edu any
 * improvements that they make and grant CSL redistribution rights.
 *
 * 	Utah $Hdr: som.h 1.2 94/12/14$
 */

/*
 * HP object formats
 */

#ifndef _SYS_EXEC_SOM_H_
#define _SYS_EXEC_SOM_H_

struct aux_id { 
	u_int mandatory : 1;
	u_int copy : 1;
	u_int append : 1;
	u_int ignore : 1;
	u_int reserved : 12;
	u_int type : 16;
	u_int length;
};

struct som_exec_auxhdr {
	struct aux_id som_auxhdr;	/* som auxiliary header header  */
	long exec_tsize;		/* text size in bytes           */
	long exec_tmem;			/* offset of text in memory     */
	long exec_tfile;		/* location of text in file     */
	long exec_dsize;		/* initialized data             */
	long exec_dmem;			/* offset of data in memory     */
	long exec_dfile;		/* location of data in file     */
	long exec_bsize;		/* uninitialized data (bss)     */
	long exec_entry;		/* offset of entrypoint         */
	long exec_flags;		/* loader flags                 */
	long exec_bfill;		/* bss initialization value     */
};

struct sys_clock { 
	u_int secs; 
	u_int nanosecs; 
};

/*
 * system_id values
 */

#define	MID_HP800	800	/* hp800 BSD binary */
#define	MID_HPUX	0x20b	/* HP-UX or OSF SOM binary */

/*
 * a_magic values
 */

#define EXEC_MAGIC	0x0107		/* normal executable */
#define SHARE_MAGIC	0x0108		/* shared executable */
#define DEMAND_MAGIC	0x010B		/* demand-load executable */


struct header {
	short int system_id;		/* magic number - system        */
	short int a_magic;		/* magic number - file type     */
	u_int version_id;		/* version id; format= YYMMDDHH */
	struct sys_clock file_time;	/* system clock- zero if unused */
	u_int entry_space;		/* idx of space containing entry pt */
	u_int entry_subspace;		/* idx of subspace for entry point  */
	u_int entry_offset;		/* offset of entry point        */
	u_int aux_header_location;	/* auxiliary header location    */
	u_int aux_header_size;		/* auxiliary header size        */
	u_int som_length;		/* length in bytes of entire som*/
	u_int presumed_dp;		/* DP val assumed during compilation */
	u_int space_location;		/* loc in file of space dictionary */
	u_int space_total;		/* number of space entries      */
	u_int subspace_location;	/* location of subspace entries */
	u_int subspace_total;		/* number of subspace entries   */
	u_int loader_fixup_location;	/* space reference array        */
	u_int loader_fixup_total;	/* total number of space ref recs */
	u_int space_strings_location;	/* file location of string area
					   for space and subspace names */
	u_int space_strings_size;	/* size of string area for space 
					   and subspace names          */
	u_int init_array_location;	/* location in file of init pointers */
	u_int init_array_total;		/* number of init. pointers     */
	u_int compiler_location;	/* loc in file of module dictionary*/
	u_int compiler_total;		/* number of modules            */
	u_int symbol_location;		/* loc in file of symbol dictionary*/
	u_int symbol_total;		/* number of symbol records     */
	u_int fixup_request_location;	/* loc in file of fix_up requests */
	u_int fixup_request_total;	/* number of fixup requests     */
	u_int symbol_strings_location;	/* file location of string area 
					   for module and symbol names  */
	u_int symbol_strings_size;	/* size of string area for 
					   module and symbol names      */
	u_int unloadable_sp_location;	/* byte offset of first byte of 
					   data for unloadable spaces   */
	u_int unloadable_sp_size;	/* bytes of data for unloadable spaces*/
	u_int checksum;
};

typedef struct {
	struct header fhdr;
	struct som_exec_auxhdr ehdr;
} som_exec;

#endif	/* _SYS_EXEC_SOM_H_
