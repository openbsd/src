/*	$OpenBSD: reloc.h,v 1.1 2013/07/05 21:10:50 miod Exp $	*/
/* VAX ELF relocation types */

#define	R_VAX_NONE		0
#define	R_VAX_32		1
#define	R_VAX_16		2
#define	R_VAX_8			3
#define	R_VAX_PC32		4
#define	R_VAX_PC16		5
#define	R_VAX_PC8		6
#define	R_VAX_GOT32		7
#define	R_VAX_PLT32		13
#define	R_VAX_COPY		19
#define	R_VAX_GLOB_DAT		20
#define	R_VAX_JMP_SLOT		21
#define	R_VAX_RELATIVE		22
#define	R_VAX_GNU_VTINHERIT	23
#define	R_VAX_GNU_VTENTRY	24
