/*	$OpenBSD: elf_machdep.h,v 1.2 2001/01/16 15:33:04 art Exp $	*/
/*	$NetBSD: elf_machdep.h,v 1.1 1996/09/26 21:50:57 cgd Exp $	*/

#define	ELF32_MACHDEP_ID_CASES						\
		/* no 32-bit ELF machine types supported */

#define	ELF64_MACHDEP_ID_CASES						\
		case EM_ALPHA:						\
		case EM_ALPHA_EXP:					\
			break;
