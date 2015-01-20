/*	$OpenBSD: exec_elf64.c,v 1.21 2015/01/20 04:41:01 krw Exp $	*/
/*
 * Public domain. Author: Artur Grabowski <art@openbsd.org>
 */
#include <machine/exec.h>

#ifdef _KERN_DO_ELF64
#define ELFSIZE 64
#include <kern/exec_elf.c>
#endif /* _KERN_DO_ELF64 */
