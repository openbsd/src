/* @(#)coff.h	1.8 91/12/16 16:48:10, AMD */
/*
******************************************************************
**			29K COFF Declarations			**
**								**
**								**
** This file contains the declarations required to define	**
** the COFF format as proposed for use by AMD for the 29K	**
** family of RISC microprocessors.				**
** 								**
** No attempt is made here to describe in detail those portions	**
** of COFF which have not been modified or extended.  Pertinent	**
** #define's and struct's are included for completeness.  Those **
** declarations are distributed in several System V headers.	**
**								**
** For a better and more complete description of COFF with	**
** general and 29K Family specific clarifications, see the	**
** AMD's "Programmer's Guide to the Common Object File Format	**
** (COFF) for the Am29000" Application Note, order number 11963.**
**								**
** For non-29K-Family specific COFF information, consult AT&T	**
** UNIX System V Release 3, Programmer's Guide, Chapter 11	**
** (Manual 307-225, Issue 1).					**
**								**
**								**
** Revision history:						**
**								**
** 0.01	JG - first published					**
** 0.02 JG - added relocation type R_IFAR and renumbered	**
** 0.03 RC - COFF spec now compiles without error		**
** 0.04 RC - removed R_IPAIR and R_IFAR and renumbered		**
** 0.05 RC - added R_HWORD relocation type			**
** 0.06 RC - section types					**
**		changed value of STYP_BSSREG			**
**		replaced STYP_RDATA and STYP_IDATA		**
**		 with STYP_LIT, STYP_ABS, and STYP_ENVIR	**
**	   - relocation types					**
**		added R_IABS					**
**		replaced R_IBYTE with R_BYTE and renumbered	**
**	   - changed comments and removed comments  		**
** 0.07 RC - relocation types					**
**		Added R_IHCONST to support relocation offsets	**
**		 for CONSTH instruction.  Added commentary,	**
**		 and renumbered to make room for R_IHCONST,	**
**		 putting the (as yet unused) global reloc 	**
**		 types at the end.				**
**	   - bug fix (typo)					**
**		Added slash to terminate comment field on	**
**		 C_EXT so now C_STAT is defined.		**
** 0.08 RC - official magic numbers assigned by AT&T.		**
** 0.09 RC - support multiple address spaces by adding magic	**
**		a.out header numbers SASMAGIC and MASMAGIC.	**
** 0.10 RC - No changes.   Just added the comments below and	**
**		corrected comments on tsize, dsize, and bsize. 	**
** 	   - All portions of the COFF file described as C 	**
**		structs must use Host Endian byte ordering.	**
**	  	Files created on a machine with a byte		**
**		ordering different from	the host may be 	**
**		converted using the UNIX conv(1) command.	**
**	   - Assemblers and compilers must create section	**
**		headers for .text, .data, and .bss (in that	**
**		order) even if they are 0 length.		**
**	   - tsize, dsize, and bsize are the size of .text,	**
**		.data, and .bss respectively.   Other sections	**
**		of type STYP_TEXT, STYP_DATA, and STYP_BSS	**
**		are not included in the byte count.		**
**	   - Assemblers and compilers must create output	**
**		sections to the exact byte length (and not	**
**		round them up).   The linker will take care	**
**		of rounding.		                 	**
** 2.1.01  - Added C_STARTOF storage class for support of	**
**		assembler $startof(sect_name) and		**
**		$sizeof(sect_name) operators.			**
** 2.1.02  - Added a few more defines for completeness.		**
** 2.1.03  - Added more magic numbers for completeness.		**
******************************************************************
*/

/*
** Overall structure of a COFF file
*/

/*
		*--------------------------------*
		|	File Header		 |
		----------------------------------
		|	Optional Information	 |
		----------------------------------
		|	Section 1 Header	 |
		----------------------------------
		|		...		 |
		----------------------------------
		|	Section n Header	 |
		----------------------------------
		| 	Raw Data for Section 1	 |
		----------------------------------
		|		...		 |
		----------------------------------
		|	Raw Data for Section n	 |
		----------------------------------
		| Relocation Info for Section 1	 |
		----------------------------------
		|		...		 |
		----------------------------------
		| Relocation Info for Section n	 |
		----------------------------------
		|  Line Numbers for Section 1	 |
		----------------------------------
		|		...		 |
		----------------------------------
		|  Line Numbers for Section n	 |
		----------------------------------
		|	  Symbol Table		 |
		----------------------------------
		|	  String Table		 |
		*--------------------------------*
*/
 
/****************************************************************/


/*
** File Header and related definitions
*/

struct filehdr 
{
	unsigned short	f_magic;	/* magic number */
	unsigned short	f_nscns;	/* number of sections */
	long		f_timdat;	/* time & date stamp */
 	long		f_symptr;	/* file pointer to symtab */
	long		f_nsyms;	/* number of symtab entries */
	unsigned short	f_opthdr;	/* sizeof(optional hdr) */
	unsigned short	f_flags; 	/* flags */
};

#define FILHDR	struct filehdr
#define FILHSZ	sizeof (FILHDR)

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

/*
** Magic numbers currently known to us,
** Plus 29K magic numbers assigned by AT&T.
*/

#define M68MAGIC	0210
#define M68TVMAGIC	0211
#define B16MAGIC	0502
#define BTVMAGIC	0503
#define IAPX16		0504
#define IAPX16TV	0505
#define IAPX20		0506
#define IAPX20TV	0507
#define X86MAGIC	0510
#define XTVMAGIC	0511
#define I286SMAGIC	0512
#define I386MAGIC	0514
#define MC68MAGIC	0520
#define MC68KWRMAGIC	0520	/* 68K writeable text sections */
#define MC68TVMAGIC	0521
#define MC68KPGMAGIC	0522	/* 68K demand paged text (shared with i286) */
#define I286LMAGIC	0522	/* i286 (shared with 68K) */
/*			0524	 * reserved for NSC */
/*			0525	 * reserved for NSC */
/*			0544	 * reserved for Zilog */
/*			0545	 * reserved for Zilog */
#define N3BMAGIC	0550	/* 3B20S executable, no TV */    
#define NTVMAGIC	0551	/* 3B20 executable with TV */
#define FBOMAGIC	0560   	/* WE*-32 (Forward Byte Ordering) */
#define WE32MAGIC	0560	/* WE 32000, no TV */
#define MTVMAGIC	0561	/* WE 32000 with TV */
#define RBOMAGIC	0562	/* WE-32 (Reverse Byte Ordering) */
#define VAXWRMAGIC	0570	/* VAX-11/750 and VAX-11/780 */
				/* (writable text sections) */
#define VAXROMAGIC	0575	/* VAX-11/750 and VAX-11780 */
				/* (read-only text sections) */
#define U370WRMAGIC	0530	/* IBM 370 (writable text sections) */
#define AMDWRMAGIC	0531	/* Amdahl 470/580 writable text sections */
#define AMDROMAGIC	0534	/* Amdahl 470/580 read only sharable text */
#define U370ROMAGIC	0535	/* IBM 370 (read-only sharable text sections) */

#define	SIPFBOMAGIC	0572	/* 29K Family (Byte 0 is MSB - Big Endian) */
#define	SIPRBOMAGIC	0573	/* 29K Family (Byte 0 is LSB - Little Endian) */

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

/*
** File header flags currently known to us.
**
** Am29000 will use the F_AR32WR and F_AR32W flags to indicate
** the byte ordering in the file.
*/

#define F_RELFLG	00001	/* Relocation information stripped */
				/* from the file. */
#define F_EXEC		00002	/* File is executable (i.e. no */
				/* unresolved external references). */
#define F_LNNO		00004	/* Line numbers stripped from */
				/* the file. */
#define F_LSYMS		00010	/* Local symbols stripped from */
				/* the file. */
#define F_MINMAL	00020	/* Not used by UNIX. */
#define F_UPDATE	00040	/* Not used by UNIX. */
#define F_SWABD		00100	/* Not used by UNIX. */
#define F_AR16WR	00200	/* File has the byte ordering used */
				/* by the PDP*-11/70 processor. */
#define F_AR32WR	00400	/* File has 32 bits per word, */
				/* least significant byte first. */
#define F_AR32W		01000	/* File has 32 bits per word, */
				/* most significant byte first. */
#define F_PATCH		02000	/* Not used by UNIX. */
#define F_BM32BRST    0010000	/* 32100 required; has RESTORE work-around. */
#define F_BM32B       0020000	/* 32100 required. */
#define F_BM32MAU     0040000	/* MAU required. */
#define F_BM32ID      0160000	/* WE 32000 processor ID field. */

/*--------------------------------------------------------------*/

/*
** Optional (a.out) header 
*/

typedef	struct aouthdr 
{
	short	magic;		/* magic number */
	short	vstamp;		/* version stamp */
	long	tsize;		/* size of .text in bytes */
	long	dsize;		/* size of .data (initialized data) */
	long	bsize;		/* size of .bss (uninitialized data) */
	long	entry;		/* entry point */
	long	text_start;	/* base of text used for this file */
	long	data_start;	/* base of data used for this file */
} AOUTHDR;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

/*
** Magic a.out header numbers for cross development (non-UNIX),
** support of separate I and D address spaces.
*/

#define SASMAGIC	010000	/* Single Address Space */    
#define MASMAGIC	020000	/* Multiple (separate I & D) Address Spaces */

/*--------------------------------------------------------------*/

/*
** Section header and related definitions
*/

struct scnhdr 
{
	char		s_name[8];	/* section name */
	long		s_paddr;	/* physical address */
	long		s_vaddr;	/* virtual address */
	long		s_size;		/* section size */
	long		s_scnptr;	/* file ptr to raw data for section */
	long		s_relptr;	/* file ptr to relocation */
	long		s_lnnoptr;	/* file ptr to line numbers */
	unsigned short	s_nreloc;	/* number of relocation entries */
	unsigned short	s_nlnno;	/* number of line number entries */
	long		s_flags;	/* flags */
};

#define	SCNHDR	struct	scnhdr
#define	SCNHSZ	sizeof	(SCNHDR)

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

/*
** Section types - with additional section type for global 
** registers which will be relocatable for the Am29000.
**
** In instances where it is necessary for a linker to produce an
** output file which contains text or data not based at virtual
** address 0, e.g. for a ROM, then the linker should accept
** address base information as command input and use PAD sections
** to skip over unused addresses.
*/

#define STYP_REG	0x00	/* Regular section (allocated, */
				/* relocated, loaded) */
#define STYP_DSECT	0x01	/* Dummy section (not allocated, */
				/* relocated, not loaded) */
#define STYP_NOLOAD	0x02	/* Noload section (allocated, */
				/* relocated, not loaded) */
#define STYP_GROUP	0x04	/* Grouped section (formed from */
				/* input sections) */
#define STYP_PAD	0x08	/* Padded section (not allocated, */
				/* not relocated, loaded) */
#define	STYP_COPY	0x10	/* Copy section (for a decision */
				/* function used in updating fields; */
				/* not allocated, not relocated, */
				/* loaded, relocation and line */
				/* number entries processed */
				/* normally) */
#define	STYP_TEXT	0x20	/* Section contains executable text */
#define	STYP_DATA	0x40	/* Section contains initialized data */
#define	STYP_BSS	0x80	/* Section contains only uninitialized data */
#define STYP_INFO	0x200	/* Comment section (not allocated, */
				/* not relocated, not loaded) */
#define STYP_OVER	0x400	/* Overlay section (relocated, */
				/* not allocated, not loaded) */
#define STYP_LIB	0x800	/* For .lib section (like STYP_INFO) */

#define	STYP_BSSREG	0x1200	/* Global register area (like STYP_INFO) */
#define STYP_ENVIR	0x2200	/* Environment (like STYP_INFO) */
#define STYP_ABS	0x4000	/* Absolute (allocated, not reloc, loaded) */
#define STYP_LIT	0x8020	/* Literal data (like STYP_TEXT) */

/*
NOTE:  The use of STYP_BSSREG for relocation is not yet defined.
*/

/*--------------------------------------------------------------*/

/*
** Relocation information declaration and related definitions
*/

struct reloc 
{
	long		r_vaddr;	/* (virtual) address of reference */
	long		r_symndx;	/* index into symbol table */
	unsigned short	r_type;		/* relocation type */
};

#define	RELOC		struct reloc
#define	RELSZ		10		/* sizeof (RELOC) */ 

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

/*
** Relocation types for the Am29000 
*/

#define	R_ABS		0	/* reference is absolute */
 
#define	R_IREL		030	/* instruction relative (jmp/call) */
#define	R_IABS		031	/* instruction absolute (jmp/call) */
#define	R_ILOHALF	032	/* instruction low half  (const)  */
#define	R_IHIHALF	033	/* instruction high half (consth) part 1 */
#define	R_IHCONST	034	/* instruction high half (consth) part 2 */
				/* constant offset of R_IHIHALF relocation */
#define	R_BYTE		035	/* relocatable byte value */
#define R_HWORD		036	/* relocatable halfword value */
#define R_WORD		037	/* relocatable word value */

#define	R_IGLBLRC	040	/* instruction global register RC */
#define	R_IGLBLRA	041	/* instruction global register RA */
#define	R_IGLBLRB	042	/* instruction global register RB */
 
/*
NOTE:
All the "I" forms refer to Am29000 instruction formats.  The linker is 
expected to know how the numeric information is split and/or aligned
within the instruction word(s).  R_BYTE works for instructions, too.

If the parameter to a CONSTH instruction is a relocatable type, two 
relocation records are written.  The first has an r_type of R_IHIHALF 
(33 octal) and a normal r_vaddr and r_symndx.  The second relocation 
record has an r_type of R_IHCONST (34 octal), a normal r_vaddr (which 
is redundant), and an r_symndx containing the 32-bit constant offset 
to the relocation instead of the actual symbol table index.  This 
second record is always written, even if the constant offset is zero.
The constant fields of the instruction are set to zero.
*/

/*--------------------------------------------------------------*/

/*
** Line number entry declaration and related definitions
*/

struct lineno 
{
   union 
   {
      long	l_symndx;	/* sym table index of function name */
      long	l_paddr;	/* (physical) address of line number */
   } l_addr;
   unsigned short	l_lnno;		/* line number */
};

#define	LINENO		struct lineno
#define	LINESZ		6		/* sizeof (LINENO) */

/*--------------------------------------------------------------*/

/*
** Symbol entry declaration and related definitions
*/

#define	SYMNMLEN	8	/* Number of characters in a symbol name */

struct	syment 
{
   union  
   {
      char	_n_name [SYMNMLEN];	/* symbol name */
      struct 
      {
         long	_n_zeroes;		/* symbol name */
         long	_n_offset;		/* offset into string table */
      } _n_n;
      char	*_n_nptr[2];		/* allows for overlaying */
   } _n;
#ifndef pdp11
   unsigned
#endif
   long			n_value;		/* value of symbol */
   short		n_scnum;		/* section number */
   unsigned short	n_type;			/* type and derived type */
   char			n_sclass;		/* storage class */
   char			n_numaux;		/* number of aux entries */
};

#define	n_name		_n._n_name
#define	n_nptr		_n._n_nptr[1]
#define	n_zeroes	_n._n_n._n_zeroes 
#define	n_offset	_n._n_n._n_offset

#define	SYMENT	struct syment
#define	SYMESZ	18

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

/*
** Storage class definitions - new classes for global registers.
*/

#define	C_EFCN		-1		/* physical end of a function */
#define	C_NULL		0		/* - */
#define	C_AUTO		1		/* automatic variable */
#define	C_EXT		2		/* external symbol */
#define C_STAT		3		/* static */
#define C_REG		4		/* (local) register variable */
#define C_EXTDEF	5		/* external definition */
#define C_LABEL		6		/* label */
#define C_ULABEL	7		/* undefined label */
#define C_MOS		8		/* member of structure */
#define C_ARG		9		/* function argument */
#define C_STRTAG	10		/* structure tag */
#define C_MOU		11		/* member of union */
#define C_UNTAG		12		/* union tag */
#define C_TPDEF		13		/* type definition */
#define C_UNSTATIC	14		/* uninitialized static  */
#define C_USTATIC	14		/* uninitialized static  */
#define C_ENTAG		15		/* enumeration tag */
#define C_MOE		16		/* member of enumeration */
#define C_REGPARM	17		/* register parameter */
#define C_FIELD		18		/* bit field */

#define C_GLBLREG	19		/* global register */
#define C_EXTREG	20		/* external global register */
#define	C_DEFREG	21		/* ext. def. of global register */
#define C_STARTOF	22		/* as29 $SIZEOF and $STARTOF symbols */


#define C_BLOCK		100		/* beginning and end of block */
#define C_FCN		101		/* beginning and end of function */
#define C_EOS		102		/* end of structure */
#define C_FILE		103		/* file name */
#define C_LINE		104		/* used only by utility programs */
#define C_ALIAS		105		/* duplicated tag */
#define C_HIDDEN	106		/* like static, used to avoid name */
					/* conflicts */
#define C_SHADOW	107		/* shadow symbol */

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

/*
** Special section number definitions used in symbol entries.
** (Section numbers 1-65535 are used to indicate the section
** where the symbol was defined.)
*/

#define	N_DEBUG		-2		/* special symbolic debugging symbol */
#define	N_ABS		-1		/* absolute symbol */
#define	N_UNDEF		 0		/* undefined external symbol */
#define N_SCNUM	   1-65535		/* section num where symbol defined */
	 			 
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

/*
** Fundamental symbol types.
*/

#define	T_NULL		0		/* type not assigned */
#define T_VOID		1		/* void */
#define	T_CHAR		2		/* character */
#define	T_SHORT		3		/* short integer */
#define	T_INT		4		/* integer */
#define	T_LONG		5		/* long integer */
#define	T_FLOAT		6		/* floating point */
#define	T_DOUBLE	7		/* double word */
#define	T_STRUCT	8		/* structure */
#define	T_UNION		9		/* union */
#define	T_ENUM		10		/* enumeration */
#define	T_MOE		11		/* member of enumeration */
#define	T_UCHAR		12		/* unsigned character */
#define	T_USHORT	13		/* unsigned short */
#define T_UINT		14		/* unsigned integer */
#define	T_ULONG		15 		/* unsigned long */

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

/*
** Derived symbol types.
*/

#define	DT_NON		0		/* no derived type  */
#define	DT_PTR		1		/* pointer */
#define	DT_FCN		2		/* function */
#define	DT_ARY		3		/* array */

/*--------------------------------------------------------------*/

/*
** Auxiliary symbol table entry declaration and related 
** definitions.
*/

#define	FILNMLEN	14   /* Number of characters in a file name */
#define	DIMNUM		4    /* Number of array dimensions in auxiliary entry */

union auxent 
{
   struct 
   {
      long	x_tagndx;		/* str, un, or enum tag indx */
      union 
      {
         struct 
         {
            unsigned short	x_lnno;		/* declaration line number */
            unsigned short	x_size;		/* str, union, array size */
         } x_lnsz;
         long	x_size;				/* size of functions */
      } x_misc;
      union 
      {
         struct 				/* if ISFCN, tag, or .bb */
         {
            long	x_lnnoptr;		/* ptr to fcn line # */
            long	x_endndx;		/* entry ndx past block end */
         } x_fcn;
         struct 				/* if ISARY, up to 4 dimen */
         {
            unsigned short	x_dimen[DIMNUM];
         } x_ary;
      } x_fcnary;
      unsigned short	x_tvndx;		/* tv index */
   } x_sym;
   struct 
   {
      char		x_fname[FILNMLEN];
   } x_file;
   struct 
   {
      long		x_scnlen;	/* section length */
      unsigned short	x_nreloc;	/* number of relocation entries */
      unsigned short	x_nlinno;	/* number of line numbers */
   } x_scn;
   struct 
   {
      long		x_tvfill;			/* tv fill value */
      unsigned short	x_tvlen;			/* length of tv */
      unsigned short	x_tvrna[2];			/* tv range */
   } x_tv;	 /* info about  tv section (in auxent of symbol  tv)) */
};

#define	AUXENT		union auxent
#define	AUXESZ		18		/* sizeof(AUXENT) */

