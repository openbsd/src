/*	$NetBSD: elf_machdep.h,v 1.1 1996/09/26 21:50:59 cgd Exp $	*/

#define	ELF32_MACHDEP_ID_CASES						\
		case Elf_em_mips:					\
			break;

#define	ELF64_MACHDEP_ID_CASES						\
		/* no 64-bit ELF machine types supported */
