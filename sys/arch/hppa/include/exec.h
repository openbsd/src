/*	$OpenBSD: exec.h,v 1.1 1998/06/23 19:45:21 mickey Exp $	*/

/* 
 * Copyright (c) 1994, The University of Utah and
 * the Computer Systems Laboratory at the University of Utah (CSL).
 * All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software is hereby
 * granted provided that (1) source code retains these copyright, permission,
 * and disclaimer notices, and (2) redistributions including binaries
 * reproduce the notices in supporting documentation, and (3) all advertising
 * materials mentioning features or use of this software display the following
 * acknowledgement: ``This product includes software developed by the
 * Computer Systems Laboratory at the University of Utah.''
 *
 * THE UNIVERSITY OF UTAH AND CSL ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS
 * IS" CONDITION.  THE UNIVERSITY OF UTAH AND CSL DISCLAIM ANY LIABILITY OF
 * ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * CSL requests users of this software to return to csl-dist@cs.utah.edu any
 * improvements that they make and grant CSL redistribution rights.
 *
 * 	Utah $Hdr: exec.h 1.3 94/12/16$
 */

#define cpu_exec_aout_makecmds(p, epp)  ENOEXEC

/* Size of a page in an object file. */
#define	__LDPGSZ	4096

#define ELF_TARG_CLASS          ELFCLASS64
#define ELF_TARG_DATA           ELFDATA2LSB
#define ELF_TARG_MACH           EM_PARISC   

#define _NLIST_DO_AOUT
#define _NLIST_DO_ECOFF

