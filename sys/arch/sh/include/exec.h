/*	$OpenBSD: exec.h,v 1.1.1.1 2006/10/06 21:02:55 miod Exp $	*/
/*	$NetBSD: elf_machdep.h,v 1.8 2002/04/28 17:10:34 uch Exp $	*/

#define __LDPGSZ	4096

#define	NATIVE_EXEC_ELF

#define	ARCH_ELFSIZE		32	/* MD native binary size */
#define	ELF_TARG_CLASS		ELFCLASS32
#ifdef __LITTLE_ENDIAN__
#define	ELF_TARG_DATA		ELFDATA2LSB
#else
#define	ELF_TARG_DATA		ELFDATA2MSB
#endif
#define	ELF_TARG_MACH		EM_SH

#define	_KERN_DO_ELF
#define	_NLIST_DO_ELF

/*
 * SuperH ELF header flags.
 */
#define	EF_SH_MACH_MASK		0x1f

#define	EF_SH_UNKNOWN		0x00
#define	EF_SH_SH1		0x01
#define	EF_SH_SH2		0x02
#define	EF_SH_SH3		0x03
#define	EF_SH_DSP		0x04
#define	EF_SH_SH3_DSP		0x05
#define	EF_SH_SH3E		0x08
#define	EF_SH_SH4		0x09

#define	EF_SH_HAS_DSP(x)	((x) & EF_SH_DSP)
#define	EF_SH_HAS_FP(x)		((x) & EF_SH_SH3E)


#define	R_SH_NONE		0
#define	R_SH_DIR32		1
#define	R_SH_REL32		2
#define	R_SH_DIR8WPN		3
#define	R_SH_IND12W		4
#define	R_SH_DIR8WPL		5
#define	R_SH_DIR8WPZ		6
#define	R_SH_DIR8BP		7
#define	R_SH_DIR8W		8
#define	R_SH_DIR8L		9
#define	R_SH_SWITCH16		25
#define	R_SH_SWITCH32		26
#define	R_SH_USES		27
#define	R_SH_COUNT		28
#define	R_SH_ALIGN		29
#define	R_SH_CODE		30
#define	R_SH_DATA		31
#define	R_SH_LABEL		32
#define	R_SH_SWITCH8		33
#define	R_SH_GNU_VTINHERIT	34
#define	R_SH_GNU_VTENTRY	35
#define	R_SH_LOOP_START		36
#define	R_SH_LOOP_END		37
#define	R_SH_GOT32		160
#define	R_SH_PLT32		161
#define	R_SH_COPY		162
#define	R_SH_GLOB_DAT		163
#define	R_SH_JMP_SLOT		164
#define	R_SH_RELATIVE		165
#define	R_SH_GOTOFF		166
#define	R_SH_GOTPC		167

#define	R_TYPE(name)	__CONCAT(R_SH_,name)
