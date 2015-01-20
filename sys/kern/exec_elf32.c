/*	$OpenBSD: exec_elf32.c,v 1.3 2015/01/20 04:41:01 krw Exp $	*/
/*
 * Public domain. Author: Artur Grabowski <art@openbsd.org>
 */
#include <machine/exec.h>

#ifdef _KERN_DO_ELF
#define ELFSIZE 32
#include <kern/exec_elf.c>
#endif

