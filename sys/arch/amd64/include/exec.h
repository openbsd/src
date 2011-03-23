/*	$OpenBSD: exec.h,v 1.3 2011/03/23 16:54:34 pirofti Exp $	*/
/*
 * Written by Artur Grabowski <art@openbsd.org> Public Domain
 */

#ifndef _MACHINE_EXEC_H_
#define _MACHINE_EXEC_H_

#define __LDPGSZ 4096

#define NATIVE_EXEC_ELF

#define ARCH_ELFSIZE 64

#define ELF_TARG_CLASS		ELFCLASS64
#define ELF_TARG_DATA		ELFDATA2LSB
#define ELF_TARG_MACH		EM_AMD64

#define _NLIST_DO_ELF
#define _KERN_DO_ELF64

#endif
