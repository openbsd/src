/*** coff information for TI TMS320C80 (MVP) */

/********************** FILE HEADER **********************/

struct external_filehdr {
	char f_magic[2];	/* magic number			*/
	char f_nscns[2];	/* number of sections		*/
	char f_timdat[4];	/* time & date stamp		*/
	char f_symptr[4];	/* file pointer to symtab	*/
	char f_nsyms[4];	/* number of symtab entries	*/
	char f_opthdr[2];	/* sizeof(optional hdr)		*/
	char f_flags[2];	/* flags			*/
	char f_target_id[2];	/* target id (TIc80 specific)	*/
};

#define	TIC80_ARCH_MAGIC	0x0C1	/* Goes in the file header magic number field */
#define TIC80_TARGET_ID		0x95	/* Goes in the target id field */

#define TIC80BADMAG(x) ((x).f_magic != TIC80_ARCH_MAGIC)

#define	FILHDR	struct external_filehdr
#define	FILHSZ	22


/********************** AOUT "OPTIONAL HEADER" **********************/


typedef struct 
{
  char 	magic[2];		/* type of file				*/
  char	vstamp[2];		/* version stamp			*/
  char	tsize[4];		/* text size in bytes, padded to FW bdry*/
  char	dsize[4];		/* initialized data "  "		*/
  char	bsize[4];		/* uninitialized data "   "		*/
  char	entry[4];		/* entry pt.				*/
  char 	text_start[4];		/* base of text used for this file */
  char 	data_start[4];		/* base of data used for this file */
}
AOUTHDR;

#define TIC80_AOUTHDR_MAGIC	0x108	/* Goes in the optional file header magic number field */

#define AOUTHDRSZ 28
#define AOUTSZ 28




/********************** SECTION HEADER **********************/


struct external_scnhdr {
	char		s_name[8];	/* section name			*/
	char		s_paddr[4];	/* physical address, aliased s_nlib */
	char		s_vaddr[4];	/* virtual address		*/
	char		s_size[4];	/* section size			*/
	char		s_scnptr[4];	/* file ptr to raw data for section */
	char		s_relptr[4];	/* file ptr to relocation	*/
	char		s_lnnoptr[4];	/* file ptr to line numbers	*/
	char		s_nreloc[2];	/* number of relocation entries	*/
	char		s_nlnno[2];	/* number of line number entries*/
	char		s_flags[2];	/* flags			*/
	char		s_reserved[1];	/* reserved (TIc80 specific)	*/
	char		s_mempage[1];	/* memory page number (TIc80)	*/
};

/*
 * names of "special" sections
 */
#define _TEXT	".text"
#define _DATA	".data"
#define _BSS	".bss"
#define _CINIT	".cinit"
#define _CONST	".const"
#define _SWITCH	".switch"
#define _STACK	".stack"
#define _SYSMEM	".sysmem"


#define	SCNHDR	struct external_scnhdr
#define	SCNHSZ	40


/********************** LINE NUMBERS **********************/

/* 1 line number entry for every "breakpointable" source line in a section.
 * Line numbers are grouped on a per function basis; first entry in a function
 * grouping will have l_lnno = 0 and in place of physical address will be the
 * symbol table index of the function name.
 */
struct external_lineno {
	union {
		char l_symndx[4];	/* function name symbol index, iff l_lnno == 0*/
		char l_paddr[4];	/* (physical) address of line number	*/
	} l_addr;
	char l_lnno[2];			/* line number	*/
};

#define GET_LINENO_LNNO(abfd, ext) bfd_h_get_16(abfd, (bfd_byte *) (ext->l_lnno));
#define PUT_LINENO_LNNO(abfd,val, ext) bfd_h_put_16(abfd,val,  (bfd_byte *) (ext->l_lnno));

#define	LINENO	struct external_lineno
#define	LINESZ	6


/********************** SYMBOLS **********************/

#define E_SYMNMLEN	8	/* # characters in a symbol name	*/
#define E_FILNMLEN	14	/* # characters in a file name		*/
#define E_DIMNUM	4	/* # array dimensions in auxiliary entry */

struct external_syment 
{
  union {
    char e_name[E_SYMNMLEN];
    struct {
      char e_zeroes[4];
      char e_offset[4];
    } e;
  } e;
  char e_value[4];
  char e_scnum[2];
  char e_type[2];
  char e_sclass[1];
  char e_numaux[1];
};



#define N_BTMASK	(017)
#define N_TMASK		(060)
#define N_BTSHFT	(4)
#define N_TSHIFT	(2)
  
/* FIXME - need to correlate with TIc80 Code Generation Tools User's Guide, CG:A-25 */
union external_auxent {
	struct {
		char x_tagndx[4];	/* str, un, or enum tag indx */
		union {
			struct {
			    char  x_lnno[2]; /* declaration line number */
			    char  x_size[2]; /* str/union/array size */
			} x_lnsz;
			char x_fsize[4];	/* size of function */
		} x_misc;
		union {
			struct {		/* if ISFCN, tag, or .bb */
			    char x_lnnoptr[4];	/* ptr to fcn line # */
			    char x_endndx[4];	/* entry ndx past block end */
			} x_fcn;
			struct {		/* if ISARY, up to 4 dimen. */
			    char x_dimen[E_DIMNUM][2];
			} x_ary;
		} x_fcnary;
		char x_tvndx[2];		/* tv index */
	} x_sym;

	union {
		char x_fname[E_FILNMLEN];
		struct {
			char x_zeroes[4];
			char x_offset[4];
		} x_n;
	} x_file;

	struct {
		char x_scnlen[4];			/* section length */
		char x_nreloc[2];	/* # relocation entries */
		char x_nlinno[2];	/* # line numbers */
	} x_scn;

        struct {
		char x_tvfill[4];	/* tv fill value */
		char x_tvlen[2];	/* length of .tv */
		char x_tvran[2][2];	/* tv range */
	} x_tv;		/* info about .tv section (in auxent of symbol .tv)) */


};

#define	SYMENT	struct external_syment
#define	SYMESZ	18	
#define	AUXENT	union external_auxent
#define	AUXESZ	18



/********************** RELOCATION DIRECTIVES **********************/

/* The external reloc has an offset field, because some of the reloc
   types on the h8 don't have room in the instruction for the entire
   offset - eg the strange jump and high page addressing modes */

struct external_reloc {
  char r_vaddr[4];
  char r_symndx[4];
  char r_reserved[2];
  char r_type[2];
};


#define RELOC struct external_reloc
#define RELSZ 12

/* TIc80 relocation types. */

#define R_ABS		0x00		/* Absolute address - no relocation */
#define R_RELLONGX	0x11		/* PP: 32 bits, direct */
#define R_PPBASE	0x34		/* PP: Global base address type */
#define R_PPLBASE	0x35		/* PP: Local base address type */
#define R_PP15		0x38		/* PP: Global 15 bit offset */
#define R_PP15W		0x39		/* PP: Global 15 bit offset divided by 4 */
#define R_PP15H		0x3A		/* PP: Global 15 bit offset divided by 2 */
#define R_PP16B		0x3B		/* PP: Global 16 bit offset for bytes */
#define R_PPL15		0x3C		/* PP: Local 15 bit offset */
#define R_PPL15W	0x3D		/* PP: Local 15 bit offset divided by 4 */
#define R_PPL15H	0x3E		/* PP: Local 15 bit offset divided by 2 */
#define R_PPL16B	0x3F		/* PP: Local 16 bit offset for bytes */
#define R_PPN15		0x40		/* PP: Global 15 bit negative offset */
#define R_PPN15W	0x41		/* PP: Global 15 bit negative offset divided by 4 */
#define R_PPN15H	0x42		/* PP: Global 15 bit negative offset divided by 2 */
#define R_PPN16B	0x43		/* PP: Global 16 bit negative byte offset */
#define R_PPLN15	0x44		/* PP: Local 15 bit negative offset */
#define R_PPLN15W	0x45		/* PP: Local 15 bit negative offset divided by 4 */
#define R_PPLN15H	0x46		/* PP: Local 15 bit negative offset divided by 2 */
#define R_PPLN16B	0x47		/* PP: Local 16 bit negative byte offset */
#define R_MPPCR15W	0x4E		/* MP: 15 bit PC-relative divided by 4 */
#define R_MPPCR		0x4F		/* MP: 32 bit PC-relative divided by 4 */
