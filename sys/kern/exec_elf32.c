#include <machine/exec.h>

#ifdef _KERN_DO_ELF
#define ELFSIZE 32
#include <kern/exec_elf.c>
#endif

