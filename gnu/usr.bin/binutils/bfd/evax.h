/* evax.h -- Header file for ALPHA EVAX (openVMS/Alpha) support.
   Copyright 1996, 1997 Free Software Foundation, Inc.

   Written by Klaus K"ampf (kkaempf@progis.de)
   of proGIS Softwareentwicklung, Aachen, Germany

This file is part of BFD, the Binary File Descriptor library.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifndef EVAX_H
#define EVAX_H

/* EVAX Text, information and relocation record (ETIR) definitions.  */

#define ETIR_S_C_MINSTACOD 0		/* Minimum store code		*/
#define ETIR_S_C_STA_GBL 0		/* Stack global symbol value	*/
#define ETIR_S_C_STA_LW 1		/* Stack longword		*/
#define ETIR_S_C_STA_QW 2		/* Stack quadword		*/
#define ETIR_S_C_STA_PQ 3		/* Stack psect base plus quadword offset  */
#define ETIR_S_C_STA_LI 4		/* Stack literal		*/
#define ETIR_S_C_STA_MOD 5		/* Stack module			*/
#define ETIR_S_C_STA_CKARG 6		/* Check Arguments		*/
#define ETIR_S_C_MAXSTACOD 6		/* Maximum stack code		*/
#define ETIR_S_C_MINSTOCOD 50		/* Minimum store code		*/
#define ETIR_S_C_STO_B 50		/* Store byte			*/
#define ETIR_S_C_STO_W 51		/* Store word			*/
#define ETIR_S_C_STO_LW 52		/* Store longword		*/
#define ETIR_S_C_STO_QW 53		/* Store quadword		*/
#define ETIR_S_C_STO_IMMR 54		/* Store immediate Repeated	*/
#define ETIR_S_C_STO_GBL 55		/* Store global			*/
#define ETIR_S_C_STO_CA 56		/* Store code address		*/
#define ETIR_S_C_STO_RB 57		/* Store relative branch	*/
#define ETIR_S_C_STO_AB 58		/* Store absolute branch	*/
#define ETIR_S_C_STO_OFF 59		/* Store offset within psect	*/
#define ETIR_S_C_STO_IMM 61		/* Store immediate		*/
#define ETIR_S_C_STO_GBL_LW 62		/* Store global Longword	*/
#define ETIR_S_C_STO_LP_PSB 63		/* STO_LP_PSB not valid in level 2 use STC_LP_PSB			*/
#define ETIR_S_C_STO_HINT_GBL 64	/* Store 14 bit HINT at global address */
#define ETIR_S_C_STO_HINT_PS 65		/* Store 14 bit HINT at psect + offset */
#define ETIR_S_C_MAXSTOCOD 65		/* Maximum store code		*/
#define ETIR_S_C_MINOPRCOD 100		/* Minimum operate code		*/
#define ETIR_S_C_OPR_NOP 100		/* No-op			*/
#define ETIR_S_C_OPR_ADD 101		/* Add				*/
#define ETIR_S_C_OPR_SUB 102		/* Subtract			*/
#define ETIR_S_C_OPR_MUL 103		/* Multiply			*/
#define ETIR_S_C_OPR_DIV 104		/* Divide			*/
#define ETIR_S_C_OPR_AND 105		/* Logical AND			*/
#define ETIR_S_C_OPR_IOR 106		/* Logical inclusive OR		*/
#define ETIR_S_C_OPR_EOR 107		/* Logical exclusive OR		*/
#define ETIR_S_C_OPR_NEG 108		/* Negate			*/
#define ETIR_S_C_OPR_COM 109		/* Complement			*/
#define ETIR_S_C_OPR_INSV 110		/* Insert bit field		*/
#define ETIR_S_C_OPR_ASH 111		/* Arithmetic shift		*/
#define ETIR_S_C_OPR_USH 112		/* Unsigned shift		*/
#define ETIR_S_C_OPR_ROT 113		/* Rotate			*/
#define ETIR_S_C_OPR_SEL 114		/* Select one of three longwords on top of stack  */
#define ETIR_S_C_OPR_REDEF 115		/* Redefine this symbol after pass 2  */
#define ETIR_S_C_OPR_DFLIT 116		/* Define a literal		*/
#define ETIR_S_C_MAXOPRCOD 116		/* Maximum operate code		*/
#define ETIR_S_C_MINCTLCOD 150		/* Minimum control code		*/
#define ETIR_S_C_CTL_SETRB 150		/* Set relocation base		*/
#define ETIR_S_C_CTL_AUGRB 151		/* Augment relocation base	*/
#define ETIR_S_C_CTL_DFLOC 152		/* Define debug location	*/
#define ETIR_S_C_CTL_STLOC 153		/* Set debug location		*/
#define ETIR_S_C_CTL_STKDL 154		/* Stack debug location		*/
#define ETIR_S_C_MAXCTLCOD 154		/* Maximum control code		*/
#define ETIR_S_C_MINSTCCOD 200		/* Minimum store-conditional code    */
#define ETIR_S_C_STC_LP 200		/* Store-conditional Linkage Pair    */
#define ETIR_S_C_STC_LP_PSB 201		/* Store-conditional Linkage Pair with Procedure Signature */
#define ETIR_S_C_STC_GBL 202		/* Store-conditional Address at global address */
#define ETIR_S_C_STC_GCA 203		/* Store-conditional Code Address at global address */
#define ETIR_S_C_STC_PS 204		/* Store-conditional Address at psect + offset */
#define ETIR_S_C_STC_NOP_GBL 205	/* Store-conditional NOP at address of global */
#define ETIR_S_C_STC_NOP_PS 206		/* Store-conditional NOP at pect + offset */
#define ETIR_S_C_STC_BSR_GBL 207	/* Store-conditional BSR at global address */
#define ETIR_S_C_STC_BSR_PS 208		/* Store-conditional BSR at pect + offset */
#define ETIR_S_C_STC_LDA_GBL 209	/* Store-conditional LDA at global address */
#define ETIR_S_C_STC_LDA_PS 210		/* Store-conditional LDA at psect + offset */
#define ETIR_S_C_STC_BOH_GBL 211	/* Store-conditional BSR or Hint at global address */
#define ETIR_S_C_STC_BOH_PS 212		/* Store-conditional BSR or Hint at pect + offset */
#define ETIR_S_C_STC_NBH_GBL 213	/* Store-conditional NOP,BSR or HINT at global address */
#define ETIR_S_C_STC_NBH_PS 214		/* Store-conditional NOP,BSR or HINT at psect + offset */
#define ETIR_S_C_MAXSTCCOD 214		/* Maximum store-conditional code    */

/* EVAX Global symbol definition record (EGSD).  */

#define EGSD_S_K_ENTRIES 2	/* Offset to first entry in record	*/
#define EGSD_S_C_ENTRIES 2	/* Offset to first entry in record	*/
#define EGSD_S_C_PSC 0		/* Psect definition			*/
#define EGSD_S_C_SYM 1		/* Symbol specification			*/
#define EGSD_S_C_IDC 2		/* Random entity check			*/
#define EGSD_S_C_SPSC 5		/* Shareable image psect definition	*/
#define EGSD_S_C_SYMV 6		/* Vectored (dual-valued) versions of SYM, */
#define EGSD_S_C_SYMM 7		/* Masked versions of SYM,		*/
#define EGSD_S_C_SYMG 8		/* EGST - gst version of SYM		*/
#define EGSD_S_C_MAXRECTYP 8	/* Maximum entry type defined		*/

#define EGPS_S_V_PIC	0x0001
#define EGPS_S_V_LIB	0x0002
#define EGPS_S_V_OVR	0x0004
#define EGPS_S_V_REL	0x0008
#define EGPS_S_V_GBL	0x0010
#define EGPS_S_V_SHR	0x0020
#define EGPS_S_V_EXE	0x0040
#define EGPS_S_V_RD	0x0080
#define EGPS_S_V_WRT	0x0100
#define EGPS_S_V_VEC	0x0200
#define EGPS_S_V_NOMOD	0x0400
#define EGPS_S_V_COM	0x0800

#define EGSY_S_V_WEAK	0x0001
#define EGSY_S_V_DEF	0x0002
#define EGSY_S_V_UNI	0x0004
#define EGSY_S_V_REL	0x0008
#define EGSY_S_V_COMM	0x0010
#define EGSY_S_V_VECEP	0x0020
#define EGSY_S_V_NORM	0x0040

/* EVAX Module header record (EMH) definitions.  */

#define EMH_S_C_MHD 0		/* Main header record		*/
#define EMH_S_C_LNM 1		/* Language name and version	*/
#define EMH_S_C_SRC 2		/* Source file specification	*/
#define EMH_S_C_TTL 3		/* Title text of module		*/
#define EMH_S_C_CPR 4		/* Copyright notice		*/
#define EMH_S_C_MTC 5		/* Maintenance status		*/
#define EMH_S_C_GTX 6		/* General text			*/
#define EMH_S_C_MAXHDRTYP 6	/* Maximum allowable type	*/

/* evax-alpha.c.  */

extern asymbol *_bfd_evax_make_empty_symbol PARAMS ((bfd *abfd));

/* evax-egsd.c.  */

extern int _bfd_evax_slurp_egsd PARAMS ((bfd *abfd));
extern int _bfd_evax_write_egsd PARAMS ((bfd *abfd));

/* evax-emh.c.  */

extern int _bfd_evax_slurp_emh PARAMS ((bfd *abfd));
extern int _bfd_evax_write_emh PARAMS ((bfd *abfd));
extern int _bfd_evax_slurp_eeom PARAMS ((bfd *abfd));
extern int _bfd_evax_write_eeom PARAMS ((bfd *abfd));

/* evax-etir.c.  */

extern int _bfd_evax_slurp_etir PARAMS ((bfd *abfd));
extern int _bfd_evax_slurp_edbg PARAMS ((bfd *abfd));
extern int _bfd_evax_slurp_etbt PARAMS ((bfd *abfd));

extern int _bfd_evax_write_etir PARAMS ((bfd *abfd));
extern int _bfd_evax_write_etbt PARAMS ((bfd *abfd));
extern int _bfd_evax_write_edbg PARAMS ((bfd *abfd));

/* The r_type field in a reloc is one of the following values.  */
#define ALPHA_R_IGNORE		0
#define ALPHA_R_REFQUAD		1
#define ALPHA_R_BRADDR		2
#define ALPHA_R_HINT		3
#define ALPHA_R_SREL16		4
#define ALPHA_R_SREL32		5
#define ALPHA_R_SREL64		6
#define ALPHA_R_OP_PUSH		7
#define ALPHA_R_OP_STORE	8
#define ALPHA_R_OP_PSUB		9
#define ALPHA_R_OP_PRSHIFT	10
#define ALPHA_R_LINKAGE		11
#define ALPHA_R_REFLONG		12
#define ALPHA_R_CODEADDR	13

/* Object language definitions.  */

#define EOBJ_S_C_EMH 8            /*EVAX module header record         */
#define EOBJ_S_C_EEOM 9           /*EVAX end of module record         */
#define EOBJ_S_C_EGSD 10          /*EVAX global symbol definition record  */
#define EOBJ_S_C_ETIR 11          /*EVAX text information record      */
#define EOBJ_S_C_EDBG 12          /*EVAX Debugger information record  */
#define EOBJ_S_C_ETBT 13          /*EVAX Traceback information record  */
#define EOBJ_S_C_MAXRECTYP 13     /*Last assigned record type         */
#define EOBJ_S_K_SUBTYP 4
#define EOBJ_S_C_SUBTYP 4
#define EOBJ_S_C_MAXRECSIZ 8192   /*Maximum legal record size         */
#define EOBJ_S_C_STRLVL 2         /*Structure level                   */
#define EOBJ_S_C_SYMSIZ 64        /*Maxymum symbol length             */
#define EOBJ_S_C_STOREPLIM -1     /*Maximum repeat count on store commands  */
#define EOBJ_S_C_PSCALILIM 16     /*Maximum p-sect alignment          */

/* Miscellaneous definitions.  */

#if __GNUC__
typedef unsigned long long uquad;
#else
typedef unsigned long uquad;
#endif

#define MAX_OUTREC_SIZE 4096
#define MIN_OUTREC_LUFT 64

typedef struct _evax_section {
  unsigned char *contents;
  bfd_vma offset;
  bfd_size_type size;
  struct _evax_section *next;
} evax_section;

extern boolean _bfd_save_evax_section
  PARAMS ((bfd *abfd, asection *section, PTR data, file_ptr offset,
	   bfd_size_type count));
extern evax_section *_bfd_get_evax_section PARAMS ((bfd *abfd, int index));

typedef struct _evax_reloc {
  struct _evax_reloc *next;
  arelent *reloc;
  asection *section;
} evax_reloc;

/* evax module header  */

struct emh_struc {
  int emh_b_strlvl;
  long emh_l_arch1;
  long emh_l_arch2;
  long emh_l_recsiz;
  char *emh_t_name;
  char *emh_t_version;
  char *emh_t_date;
  char *emh_c_lnm;
  char *emh_c_src;
  char *emh_c_ttl;
};


/* evax end of module  */

struct eeom_struc {
  long eeom_l_total_lps;
  unsigned char eeom_b_comcod;
  boolean eeom_has_transfer;
  unsigned char eeom_b_tfrflg;
  long eeom_l_psindx;
  long eeom_l_tfradr;
};

enum file_format_enum { FF_UNKNOWN, FF_FOREIGN, FF_NATIVE };

typedef struct evax_symbol_struct {
  struct bfd_hash_entry bfd_hash;
  asymbol *symbol;
} evax_symbol_entry;

/* stack value for push/pop commands  */

struct stack_struct {
  uquad value;
  int psect;
};
#define STACKSIZE 50

/* location stack definitions for CTL_DFLOC, CTL_STLOC, and CTL_STKDL  */

struct location_struct {
  unsigned long value;
  int psect;
};
#define LOCATION_SAVE_SIZE 32

#define EVAX_SECTION_COUNT 32

struct evax_private_data_struct {
  boolean fixup_done;			/* Flag to indicate if all
					   section pointers and PRIV(sections)
					   are set up correctly  */
  unsigned char *evax_buf;		/* buffer to record  */
  int buf_size;				/* max size of buffer  */
  unsigned char *evax_rec;		/* actual record ptr  */
  int rec_length;			/* remaining record length  */
  int rec_size;				/* actual record size  */
  int rec_type;				/* actual record type  */
  enum file_format_enum file_format;

  struct emh_struc emh_data;		/* data from EMH record  */
  struct eeom_struc eeom_data;		/* data from EEOM record  */
  int egsd_sec_count;			/* # of EGSD sections  */
  asection **sections;			/* vector of EGSD sections  */
  int egsd_sym_count;			/* # of EGSD symbols  */
  asymbol **symbols;			/* vector of EGSD symbols  */
  struct proc_value *procedure;

  struct stack_struct *stack;
  int stackptr;

  evax_section *evax_section_table[EVAX_SECTION_COUNT];

  struct bfd_hash_table *evax_symbol_table;
  struct symbol_cache_entry **symcache;
  int symnum;

  struct location_struct *location_stack;

  unsigned char *image_ptr;		/* a pointer to section->contents */

  unsigned char pdsc[8];		/* procedure descriptor */

  /* Output routine storage  */

  unsigned char *output_buf;		/* output data  */
  int push_level;
  int pushed_size;
  int length_pos;
  int output_size;
  int output_alignment;

  /* linkage index counter
   used by conditional store commands (ETIR_S_C_STC_)  */

  int evax_linkage_index;

  /* see tc-alpha.c of gas for a description.  */
  int flag_hash_long_names;	/* -+, hash instead of truncate */
  int flag_show_after_trunc;	/* -H, show hashing/truncation */
};

#define PRIV(name)	((struct evax_private_data_struct *)abfd->tdata.any)->name

#if EVAX_DEBUG
extern void _bfd_evax_debug PARAMS((int level, char *format, ...));
extern void _bfd_hexdump
  PARAMS ((int level, unsigned char *ptr, int size, int offset));

#define evax_debug _bfd_evax_debug

#endif

extern struct bfd_hash_entry *_bfd_evax_hash_newfunc
  PARAMS ((struct bfd_hash_entry *entry, struct bfd_hash_table *table,
	   const char *string));
extern void _bfd_evax_get_header_values
  PARAMS ((bfd *abfd, unsigned char *buf, int *type, int *length));
extern int _bfd_evax_get_record PARAMS ((bfd *abfd));
extern int _bfd_evax_next_record PARAMS ((bfd *abfd));

extern char *_bfd_evax_save_sized_string PARAMS ((char *str, int size));
extern char *_bfd_evax_save_counted_string PARAMS ((char *ptr));
extern void _bfd_evax_push PARAMS ((bfd *abfd, uquad val, int psect));
extern uquad _bfd_evax_pop PARAMS ((bfd *abfd, int *psect));

extern boolean _bfd_save_evax_section
  PARAMS ((bfd *abfd, asection *section, PTR data, file_ptr offset,
	   bfd_size_type count));
extern void _bfd_evax_output_begin
  PARAMS ((bfd *abfd, int rectype, int rechead));
extern void _bfd_evax_output_alignment PARAMS ((bfd *abfd, int alignto));
extern void _bfd_evax_output_push PARAMS ((bfd *abfd));
extern void _bfd_evax_output_pop PARAMS ((bfd *abfd));
extern void _bfd_evax_output_flush PARAMS ((bfd *abfd));
extern void _bfd_evax_output_end PARAMS ((bfd *abfd));
extern int _bfd_evax_output_check PARAMS ((bfd *abfd, int size));
extern void _bfd_evax_output_byte PARAMS ((bfd *abfd, unsigned int value));
extern void _bfd_evax_output_short PARAMS ((bfd *abfd, unsigned int value));
extern void _bfd_evax_output_long PARAMS ((bfd *abfd, unsigned long value));
extern void _bfd_evax_output_quad PARAMS ((bfd *abfd, uquad value));
extern void _bfd_evax_output_counted PARAMS ((bfd *abfd, char *value));
extern void _bfd_evax_output_dump PARAMS ((bfd *abfd, unsigned char *data,
					   int length));
extern void _bfd_evax_output_fill PARAMS ((bfd *abfd, int value, int length));
extern char *_bfd_evax_length_hash_symbol PARAMS ((bfd *abfd, const char *in));

#endif /* EVAX_H */
