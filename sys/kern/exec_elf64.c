/*
 * Public domain. Author: Artur Grabowski <art@openbsd.org>
 */
#include <machine/exec.h>

#ifdef _KERN_DO_ELF64
#define ELFSIZE 64
#include <kern/exec_elf.c>
#endif /* _KERN_DO_ELF64 */
