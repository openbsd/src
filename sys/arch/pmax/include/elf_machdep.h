/*	$NetBSD: elf_machdep.h,v 1.2 1996/12/17 03:45:05 jonathan Exp $	*/

#define	ELF32_MACHDEP_ID_CASES						\

/*
 * pmaxes are mipsel machines
 */

#define ELF32_MACHDEP_ENDIANNESS	Elf_ed_2lsb

#define ELF64_MACHDEP_ENDIANNESS	XXX	/* break compilation */
		case Elf_em_mips:					\
			break;

#define	ELF64_MACHDEP_ID_CASES						\
		/* no 64-bit ELF machine types supported */

/* TTTTT - stuff from NetBSD mips dir */
/* mips relocs.  */

#define R_MIPS_NONE		0
#define R_MIPS_16		1
#define R_MIPS_32		2
#define R_MIPS_REL32		3
#define R_MIPS_REL		R_MIPS_REL32
#define R_MIPS_26		4
#define R_MIPS_HI16		5	/* high 16 bits of symbol value */
#define R_MIPS_LO16		6	/* low 16 bits of symbol value */
#define R_MIPS_GPREL16		7  	/* GP-relative reference  */
#define R_MIPS_LITERAL		8 	/* Reference to literal section  */
#define R_MIPS_GOT16		9	/* Reference to global offset table */
#define R_MIPS_GOT		R_MIPS_GOT16
#define R_MIPS_PC16		10  	/* 16 bit PC relative reference */
#define R_MIPS_CALL16 		11  	/* 16 bit call thru glbl offset tbl */
#define R_MIPS_CALL		R_MIPS_CALL16
#define R_MIPS_GPREL32		12

/* 13, 14, 15 are not defined at this point. */
#define R_MIPS_UNUSED1		13
#define R_MIPS_UNUSED2		14
#define R_MIPS_UNUSED3		15

/* 
 * The remaining relocs are apparently part of the 64-bit Irix ELF ABI.
 */
#define R_MIPS_SHIFT5		16
#define R_MIPS_SHIFT6		17

#define R_MIPS_64		18
#define R_MIPS_GOT_DISP		19
#define R_MIPS_GOT_PAGE		20
#define R_MIPS_GOT_OFST		21
#define R_MIPS_GOT_HI16		22
#define R_MIPS_GOT_LO16		23
#define R_MIPS_SUB 		24
#define R_MIPS_INSERT_A		25
#define R_MIPS_INSERT_B		26
#define R_MIPS_DELETE		27
#define R_MIPS_HIGHER		28
#define R_MIPS_HIGHEST		29
#define R_MIPS_CALL_HI16	30
#define R_MIPS_CALL_LO16	31
#define R_MIPS_SCN_DISP		32
#define R_MIPS_REL16		33
#define R_MIPS_ADD_IMMEDIATE	34
#define R_MIPS_PJUMP		35
#define R_MIPS_RELGOT		36

#define R_MIPS_max		37
#define R_TYPE(name)		__CONCAT(R_MIPS_,name)


/*
 * Tell the kernel ELF exec code not to try relocating the interpreter
 * (ld.so) for dynamically-linked ELF binaries.
 */
#ifdef _KERNEL
#define ELF_INTERP_NON_RELOCATABLE
#endif

/* TTTTT - end of stuff from NetBSD mips dir */
