/*	$OpenBSD: exec.h,v 1.4 2012/09/11 15:44:17 deraadt Exp $	*/
/*
 * Written by Artur Grabowski <art@openbsd.org> Public Domain
 */

#ifndef _MACHINE_EXEC_H_
#define _MACHINE_EXEC_H_

#define __LDPGSZ 4096

#define ARCH_ELFSIZE 64

#define ELF_TARG_CLASS		ELFCLASS64
#define ELF_TARG_DATA		ELFDATA2LSB
#define ELF_TARG_MACH		EM_AMD64

#define _NLIST_DO_ELF
#define _KERN_DO_ELF64

#endif
