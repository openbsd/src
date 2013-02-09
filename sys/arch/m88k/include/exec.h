/*	$OpenBSD: exec.h,v 1.3 2013/02/09 19:21:29 miod Exp $ */
#ifndef _M88K_EXEC_H_
#define _M88K_EXEC_H_

#define __LDPGSZ        4096

#define ARCH_ELFSIZE		32

#define ELF_TARG_CLASS		ELFCLASS32
#define ELF_TARG_DATA		ELFDATA2MSB
#define ELF_TARG_MACH		EM_88K

#define _NLIST_DO_AOUT
#define _NLIST_DO_ELF

#define _KERN_DO_AOUT
#define _KERN_DO_ELF

#endif /* _M88K_EXEC_H_ */
