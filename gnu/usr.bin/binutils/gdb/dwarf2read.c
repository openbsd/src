/* DWARF 2 debugging format support for GDB.
   Copyright 1994, 1995, 1996 Free Software Foundation, Inc.

   Adapted by Gary Funck (gary@intrepid.com), Intrepid Technology,
   Inc.  with support from Florida State University (under contract
   with the Ada Joint Program Office), and Silicon Graphics, Inc.
   Initial contribution by Brent Benson, Harris Computer Systems, Inc.,
   based on Fred Fish's (Cygnus Support) implementation of DWARF 1
   support in dwarfread.c

This file is part of GDB.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or (at
your option) any later version.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include "defs.h"
#include "bfd.h"
#include "symtab.h"
#include "gdbtypes.h"
#include "symfile.h"
#include "objfiles.h"
#include "elf/dwarf2.h"
#include "buildsym.h"
#include "demangle.h"
#include "expression.h"
#include "language.h"

#include <fcntl.h>
#include "gdb_string.h"
#include <sys/types.h>

/* .debug_info header for a compilation unit 
   Because of alignment constraints, this structure has padding and cannot
   be mapped directly onto the beginning of the .debug_info section.  */
typedef struct comp_unit_header
  {
    unsigned int length;	/* length of the .debug_info
				   contribution */
    unsigned short version;	/* version number -- 2 for DWARF
				   version 2 */
    unsigned int abbrev_offset;	/* offset into .debug_abbrev section */
    unsigned char addr_size;	/* byte size of an address -- 4 */
  }
_COMP_UNIT_HEADER;
#define _ACTUAL_COMP_UNIT_HEADER_SIZE 11

/* .debug_pubnames header
   Because of alignment constraints, this structure has padding and cannot
   be mapped directly onto the beginning of the .debug_info section.  */
typedef struct pubnames_header
  {
    unsigned int length;	/* length of the .debug_pubnames
				   contribution  */
    unsigned char version;	/* version number -- 2 for DWARF
				   version 2 */
    unsigned int info_offset;	/* offset into .debug_info section */
    unsigned int info_size;	/* byte size of .debug_info section
				   portion */
  }
_PUBNAMES_HEADER;
#define _ACTUAL_PUBNAMES_HEADER_SIZE 13

/* .debug_pubnames header
   Because of alignment constraints, this structure has padding and cannot
   be mapped directly onto the beginning of the .debug_info section.  */
typedef struct aranges_header
  {
    unsigned int length;	/* byte len of the .debug_aranges
				   contribution */
    unsigned short version;	/* version number -- 2 for DWARF
				   version 2 */
    unsigned int info_offset;	/* offset into .debug_info section */
    unsigned char addr_size;	/* byte size of an address */
    unsigned char seg_size;	/* byte size of segment descriptor */
  }
_ARANGES_HEADER;
#define _ACTUAL_ARANGES_HEADER_SIZE 12

/* .debug_line statement program prologue
   Because of alignment constraints, this structure has padding and cannot
   be mapped directly onto the beginning of the .debug_info section.  */
typedef struct statement_prologue
  {
    unsigned int total_length;	/* byte length of the statement
				   information */
    unsigned short version;	/* version number -- 2 for DWARF
				   version 2 */
    unsigned int prologue_length;	/* # bytes between prologue &
					   stmt program */
    unsigned char minimum_instruction_length;	/* byte size of
						   smallest instr */
    unsigned char default_is_stmt;	/* initial value of is_stmt
					   register */
    char line_base;
    unsigned char line_range;
    unsigned char opcode_base;	/* number assigned to first special
				   opcode */
    unsigned char *standard_opcode_lengths;
  }
_STATEMENT_PROLOGUE;

/* offsets and sizes of debugging sections */

static file_ptr dwarf_info_offset;
static file_ptr dwarf_abbrev_offset;
static file_ptr dwarf_line_offset;
static file_ptr dwarf_pubnames_offset;
static file_ptr dwarf_aranges_offset;
static file_ptr dwarf_loc_offset;
static file_ptr dwarf_macinfo_offset;
static file_ptr dwarf_str_offset;

static unsigned int dwarf_info_size;
static unsigned int dwarf_abbrev_size;
static unsigned int dwarf_line_size;
static unsigned int dwarf_pubnames_size;
static unsigned int dwarf_aranges_size;
static unsigned int dwarf_loc_size;
static unsigned int dwarf_macinfo_size;
static unsigned int dwarf_str_size;

/* names of the debugging sections */

#define INFO_SECTION     ".debug_info"
#define ABBREV_SECTION   ".debug_abbrev"
#define LINE_SECTION     ".debug_line"
#define PUBNAMES_SECTION ".debug_pubnames"
#define ARANGES_SECTION  ".debug_aranges"
#define LOC_SECTION      ".debug_loc"
#define MACINFO_SECTION  ".debug_macinfo"
#define STR_SECTION      ".debug_str"

/* Get at parts of an attribute structure */

#define DW_STRING(attr)    ((attr)->u.str)
#define DW_UNSND(attr)     ((attr)->u.unsnd)
#define DW_BLOCK(attr)     ((attr)->u.blk)
#define DW_SND(attr)       ((attr)->u.snd)
#define DW_ADDR(attr)	   ((attr)->u.addr)

/* local data types */

/* The data in a compilation unit header looks like this.  */
struct comp_unit_head
  {
    int length;
    short version;
    int abbrev_offset;
    unsigned char addr_size;
  };

/* The data in the .debug_line statement prologue looks like this.  */
struct line_head
  {
    unsigned int total_length;
    unsigned short version;
    unsigned int prologue_length;
    unsigned char minimum_instruction_length;
    unsigned char default_is_stmt;
    char line_base;
    unsigned char line_range;
    unsigned char opcode_base;
    unsigned char *standard_opcode_lengths;
  };

/* When we construct a partial symbol table entry we only
   need this much information. */
struct partial_die_info
  {
    unsigned short tag;
    unsigned char has_children;
    unsigned char is_external;
    unsigned int offset;
    unsigned int abbrev;
    char *name;
    CORE_ADDR lowpc;
    CORE_ADDR highpc;
    struct dwarf_block *locdesc;
    unsigned int language;
    int value;
  };

/* This data structure holds the information of an abbrev. */
struct abbrev_info
  {
    unsigned int number;	/* number identifying abbrev */
    unsigned int tag;		/* dwarf tag */
    int has_children;		/* boolean */
    unsigned int num_attrs;	/* number of attributes */
    struct attr_abbrev *attrs;	/* an array of attribute descriptions */
    struct abbrev_info *next;	/* next in chain */
  };

struct attr_abbrev
  {
    unsigned int name;
    unsigned int form;
  };

/* This data structure holds a complete die structure. */
struct die_info
  {
    unsigned short tag;		 /* Tag indicating type of die */
    unsigned short has_children; /* Does the die have children */
    unsigned int abbrev;	 /* Abbrev number */
    unsigned int offset;	 /* Offset in .debug_info section */
    unsigned int num_attrs;	 /* Number of attributes */
    struct attribute *attrs;	 /* An array of attributes */
    struct die_info *next_ref;	 /* Next die in ref hash table */
    struct die_info *next;	 /* Next die in linked list */
    struct type *type;		 /* Cached type information */
  };

/* Attributes have a name and a value */
struct attribute
  {
    unsigned short name;
    unsigned short form;
    union
      {
	char *str;
	struct dwarf_block *blk;
	unsigned int unsnd;
	int snd;
	CORE_ADDR addr;
      }
    u;
  };

/* Blocks are a bunch of untyped bytes. */
struct dwarf_block
  {
    unsigned int size;
    char *data;
  };

/* We only hold one compilation unit's abbrevs in
   memory at any one time.  */
#ifndef ABBREV_HASH_SIZE
#define ABBREV_HASH_SIZE 121
#endif
#ifndef ATTR_ALLOC_CHUNK
#define ATTR_ALLOC_CHUNK 4
#endif

/* FIXME: do away with this */

#ifndef DWARF2_MAX_STRING_SIZE
#define DWARF2_MAX_STRING_SIZE 1024
#endif

static struct abbrev_info *dwarf2_abbrevs[ABBREV_HASH_SIZE];

/* A hash table of die offsets for following references.  */
#ifndef REF_HASH_SIZE
#define REF_HASH_SIZE 1021
#endif

static struct die_info *die_ref_table[REF_HASH_SIZE];

/* Allocate fields for structs, unions and enums in this size.  */
#ifndef DW_FIELD_ALLOC_CHUNK
#define DW_FIELD_ALLOC_CHUNK 4
#endif

/* The language we are debugging.  */
static enum language cu_language;
static const struct language_defn *cu_language_defn;

/* Actually data from the sections.  */
static char *dwarf_info_buffer;
static char *dwarf_abbrev_buffer;
static char *dwarf_line_buffer;

/* A zeroed version of several structures for initialization purposes.  */
static struct partial_die_info zeroed_partial_die;
static struct die_info zeroed_die;

/* The generic symbol table building routines have separate lists for
   file scope symbols and all all other scopes (local scopes).  So
   we need to select the right one to pass to add_symbol_to_list().
   We do it by keeping a pointer to the correct list in list_in_scope.

   FIXME:  The original dwarf code just treated the file scope as the first
   local scope, and all other local scopes as nested local scopes, and worked
   fine.  Check to see if we really need to distinguish these
   in buildsym.c.  */
static struct pending **list_in_scope = &file_symbols;
static int isreg;		/* Kludge to identify register
				   variables */
static int offreg;		/* Kludge to identify basereg
				   references */

/* This value is added to each symbol value.  FIXME:  Generalize to 
   the section_offsets structure used by dbxread (once this is done,
   pass the appropriate section number to end_symtab).  */
static CORE_ADDR baseaddr;	/* Add to each symbol value */

/* Maintain an array of referenced fundamental types for the current
   compilation unit being read.  For DWARF version 1, we have to construct
   the fundamental types on the fly, since no information about the
   fundamental types is supplied.  Each such fundamental type is created by
   calling a language dependent routine to create the type, and then a
   pointer to that type is then placed in the array at the index specified
   by it's FT_<TYPENAME> value.  The array has a fixed size set by the
   FT_NUM_MEMBERS compile time constant, which is the number of predefined
   fundamental types gdb knows how to construct.  */
static struct type *ftypes[FT_NUM_MEMBERS];	/* Fundamental types */

/* FIXME - set from bfd function */
static int bits_per_byte = 8;

/* Keep track of whether we have given a warning about not
   handling DW_TAG_const_type dies.  */
static int tag_const_warning_given = 0;

/* Keep track of whether we have given a warning about not
   handling DW_TAG_volatile_type dies.  */
static int tag_volatile_warning_given = 0;

/* Keep track of constant array bound warning.  */
static int array_bound_warning_given = 0;

/* Remember the addr_size read from the dwarf.
   If a target expects to link compilation units with differing address
   sizes, gdb needs to be sure that the appropriate size is here for
   whatever scope is currently getting read. */
static int address_size;

/* Externals references.  */
extern int info_verbose;	/* From main.c; nonzero => verbose */

/* local function prototypes */

static void dwarf2_locate_sections PARAMS ((bfd *, asection *, PTR));

static void dwarf2_build_psymtabs_easy PARAMS ((struct objfile *,
						struct section_offsets *,
						int));
static void dwarf2_build_psymtabs_hard PARAMS ((struct objfile *,
						struct section_offsets *,
						int));

static char *scan_partial_symbols PARAMS ((char *, struct objfile *,
					   CORE_ADDR *, CORE_ADDR *));

static void add_partial_symbol PARAMS ((struct partial_die_info *,
					struct objfile *));

static void dwarf2_psymtab_to_symtab PARAMS ((struct partial_symtab *));

static void psymtab_to_symtab_1 PARAMS ((struct partial_symtab *));

static void add_die_to_symtab PARAMS ((struct die_info *, struct objfile *));

static char *dwarf2_read_section PARAMS ((bfd *, file_ptr, unsigned int));

static void dwarf2_read_abbrevs PARAMS ((bfd *, unsigned int));

static void dwarf2_empty_abbrev_table PARAMS ((void));

static struct abbrev_info *dwarf2_lookup_abbrev PARAMS ((unsigned int));

static char *read_partial_die PARAMS ((struct partial_die_info *,
				       bfd *, char *, int *));

static char *read_full_die PARAMS ((struct die_info **, bfd *, char *));

static unsigned int read_1_byte PARAMS ((bfd *, char *));

static unsigned int read_2_bytes PARAMS ((bfd *, char *));

static unsigned int read_4_bytes PARAMS ((bfd *, char *));

static unsigned int read_8_bytes PARAMS ((bfd *, char *));

static CORE_ADDR read_address PARAMS ((bfd *, char *));

static char *read_n_bytes PARAMS ((bfd *, char *, unsigned int));

static char *read_string PARAMS ((bfd *, char *, unsigned int *));

static unsigned int read_unsigned_leb128 PARAMS ((bfd *, char *,
						  unsigned int *));

static int read_signed_leb128 PARAMS ((bfd *, char *, unsigned int *));

static void set_cu_language PARAMS ((unsigned int));

static void record_minimal_symbol PARAMS ((char *, CORE_ADDR,
					   enum minimal_symbol_type,
					   struct objfile *));

static int convert_locdesc PARAMS ((struct dwarf_block *));

static struct attribute *dwarf_attr PARAMS ((struct die_info *,
					     unsigned int));

static void dwarf_decode_lines PARAMS ((unsigned int, bfd *));

static struct symbol *new_symbol PARAMS ((struct die_info * die, struct objfile * objfile));

static struct type *die_type PARAMS ((struct die_info * die, struct objfile * objfile));

static struct type *type_at_offset PARAMS ((unsigned int offset, struct objfile * objfile));

static struct type *tag_type_to_type PARAMS ((struct die_info * die, struct objfile * objfile));

static void read_type_die PARAMS ((struct die_info * die, struct objfile * objfile));

static void read_typedef PARAMS ((struct die_info * die, struct objfile * objfile));

static void read_base_type PARAMS ((struct die_info * die, struct objfile * objfile));

static void read_file_scope PARAMS ((struct die_info * die, struct objfile * objfile));

static void read_func_scope PARAMS ((struct die_info * die, struct objfile * objfile));

static void read_lexical_block_scope PARAMS ((struct die_info * die,
					      struct objfile * objfile));

static void read_structure_scope PARAMS ((struct die_info * die, struct objfile * objfile));

static void read_common_block PARAMS ((struct die_info * die, struct objfile * objfile));

static void read_enumeration PARAMS ((struct die_info * die, struct objfile * objfile));

static struct type * dwarf_base_type PARAMS ((int encoding, int size));

static CORE_ADDR decode_locdesc PARAMS ((struct dwarf_block *,
					 struct objfile *));

static char *create_name PARAMS ((char *, struct obstack *));

static void dwarf_read_array_type PARAMS ((struct die_info * die,
					   struct objfile * objfile));

static void read_tag_pointer_type PARAMS ((struct die_info * die,
					   struct objfile * objfile));

static void read_tag_const_type PARAMS ((struct die_info * die,
					 struct objfile * objfile));

static void read_tag_volatile_type PARAMS ((struct die_info * die,
					    struct objfile * objfile));

static void read_tag_string_type PARAMS ((struct die_info * die,
					  struct objfile * objfile));

static void read_subroutine_type PARAMS ((struct die_info * die,
					  struct objfile * objfile));

struct die_info *read_comp_unit PARAMS ((char *info_ptr, bfd * abfd));

static void free_die_list PARAMS ((struct die_info * dies));

static void process_die PARAMS ((struct die_info *, struct objfile *));

static char *dwarf_tag_name PARAMS ((unsigned tag));

static char *dwarf_attr_name PARAMS ((unsigned attr));

static char *dwarf_form_name PARAMS ((unsigned form));

static char *dwarf_stack_op_name PARAMS ((unsigned op));

static char *dwarf_bool_name PARAMS ((unsigned bool));

static char *dwarf_bool_name PARAMS ((unsigned tag));

static char *dwarf_type_encoding_name PARAMS ((unsigned enc));

static char *dwarf_cfi_name PARAMS ((unsigned cfi_opc));

struct die_info *copy_die PARAMS ((struct die_info *old_die));

struct die_info *sibling_die PARAMS ((struct die_info *die));

void dump_die PARAMS ((struct die_info *die));

void dump_die_list PARAMS ((struct die_info *dies));

void store_in_ref_table PARAMS ((unsigned int, struct die_info *));

struct die_info *follow_die_ref PARAMS ((unsigned int offset));

static struct type *dwarf2_fundamental_type PARAMS ((struct objfile *, int));

/* memory allocation interface */

static struct type *dwarf_alloc_type PARAMS ((struct objfile *));

static struct abbrev_info *dwarf_alloc_abbrev PARAMS ((void));

static struct dwarf_block *dwarf_alloc_block PARAMS ((void));

static struct die_info *dwarf_alloc_die PARAMS ((void));

/* Try to locate the sections we need for DWARF 2 debugging
   information and return true if we have enough to do something.  */

int
dwarf2_has_info (abfd)
     bfd *abfd;
{
  dwarf_info_offset = dwarf_abbrev_offset = dwarf_line_offset = 0;
  bfd_map_over_sections (abfd, dwarf2_locate_sections, NULL);
  if (dwarf_info_offset && dwarf_abbrev_offset && dwarf_line_offset)
    {
      return 1;
    }
  else
    {
      return 0;
    }
}

/* This function is mapped across the sections and remembers the
   offset and size of each of the debugging sections we are interested
   in.  */

static void
dwarf2_locate_sections (ignore_abfd, sectp, ignore_ptr)
     bfd *ignore_abfd;
     asection *sectp;
     PTR ignore_ptr;
{
  if (STREQ (sectp->name, INFO_SECTION))
    {
      dwarf_info_offset = sectp->filepos;
      dwarf_info_size = bfd_get_section_size_before_reloc (sectp);
    }
  else if (STREQ (sectp->name, ABBREV_SECTION))
    {
      dwarf_abbrev_offset = sectp->filepos;
      dwarf_abbrev_size = bfd_get_section_size_before_reloc (sectp);
    }
  else if (STREQ (sectp->name, LINE_SECTION))
    {
      dwarf_line_offset = sectp->filepos;
      dwarf_line_size = bfd_get_section_size_before_reloc (sectp);
    }
  else if (STREQ (sectp->name, PUBNAMES_SECTION))
    {
      dwarf_pubnames_offset = sectp->filepos;
      dwarf_pubnames_size = bfd_get_section_size_before_reloc (sectp);
    }
  else if (STREQ (sectp->name, ARANGES_SECTION))
    {
      dwarf_aranges_offset = sectp->filepos;
      dwarf_aranges_size = bfd_get_section_size_before_reloc (sectp);
    }
  else if (STREQ (sectp->name, LOC_SECTION))
    {
      dwarf_loc_offset = sectp->filepos;
      dwarf_loc_size = bfd_get_section_size_before_reloc (sectp);
    }
  else if (STREQ (sectp->name, MACINFO_SECTION))
    {
      dwarf_macinfo_offset = sectp->filepos;
      dwarf_macinfo_size = bfd_get_section_size_before_reloc (sectp);
    }
  else if (STREQ (sectp->name, STR_SECTION))
    {
      dwarf_str_offset = sectp->filepos;
      dwarf_str_size = bfd_get_section_size_before_reloc (sectp);
    }
}

/* Build a partial symbol table.  */

void
dwarf2_build_psymtabs (objfile, section_offsets, mainline)
    struct objfile *objfile;
    struct section_offsets *section_offsets;
    int mainline;
{
  bfd *abfd = objfile->obfd;

  /* We definitely need the .debug_info, .debug_abbrev, and .debug_line
     sections */

  dwarf_info_buffer = dwarf2_read_section (abfd,
					   dwarf_info_offset,
					   dwarf_info_size);
  dwarf_abbrev_buffer = dwarf2_read_section (abfd,
					     dwarf_abbrev_offset,
					     dwarf_abbrev_size);
  dwarf_line_buffer = dwarf2_read_section (abfd,
					   dwarf_line_offset,
					   dwarf_line_size);

  if (mainline || objfile->global_psymbols.size == 0 ||
      objfile->static_psymbols.size == 0)
    {
      init_psymbol_list (objfile, 1024);
    }

#if 0
  if (dwarf_aranges_offset && dwarf_pubnames_offset)
    {
      /* Things are significanlty easier if we have .debug_aranges and
         .debug_pubnames sections */

      dwarf2_build_psymtabs_easy (objfile, section_offsets, mainline);
    }
  else
#endif
    /* only test this case for now */
    {		
      /* In this case we have to work a bit harder */
      dwarf2_build_psymtabs_hard (objfile, section_offsets, mainline);
    }
}

/* Build the partial symbol table from the information in the
   .debug_pubnames and .debug_aranges sections.  */

static void
dwarf2_build_psymtabs_easy (objfile, section_offsets, mainline)
     struct objfile *objfile;
     struct section_offsets *section_offsets;
     int mainline;
{
  bfd *abfd = objfile->obfd;
  char *aranges_buffer, *pubnames_buffer;
  char *aranges_ptr, *pubnames_ptr;
  unsigned int entry_length, version, info_offset, info_size;

  pubnames_buffer = dwarf2_read_section (abfd,
					 dwarf_pubnames_offset,
					 dwarf_pubnames_size);
  pubnames_ptr = pubnames_buffer;
  while ((pubnames_ptr - pubnames_buffer) < dwarf_pubnames_size)
    {
      entry_length = read_4_bytes (abfd, pubnames_ptr);
      pubnames_ptr += 4;
      version = read_1_byte (abfd, pubnames_ptr);
      pubnames_ptr += 1;
      info_offset = read_4_bytes (abfd, pubnames_ptr);
      pubnames_ptr += 4;
      info_size = read_4_bytes (abfd, pubnames_ptr);
      pubnames_ptr += 4;
    }

  aranges_buffer = dwarf2_read_section (abfd,
					dwarf_aranges_offset,
					dwarf_aranges_size);

}

/* Build the partial symbol table by doing a quick pass through the
   .debug_info and .debug_abbrev sections.  */

static void
dwarf2_build_psymtabs_hard (objfile, section_offsets, mainline)
     struct objfile *objfile;
     struct section_offsets *section_offsets;
     int mainline;
{
  /* Instead of reading this into a big buffer, we should probably use
     mmap()  on architectures that support it. (FIXME) */
  bfd *abfd = objfile->obfd;
  char *info_ptr, *abbrev_ptr;
  char *beg_of_comp_unit, *comp_unit_die_offset;
  struct comp_unit_head cu_header;
  struct partial_die_info comp_unit_die;
  struct partial_symtab *pst;
  struct cleanup *back_to;
  int comp_unit_has_pc_info;
  int has_pc_info;
  CORE_ADDR lowpc, highpc;

  comp_unit_die = zeroed_partial_die;
  info_ptr = dwarf_info_buffer;
  abbrev_ptr = dwarf_abbrev_buffer;

  while ((info_ptr - dwarf_info_buffer)
	  + ((info_ptr - dwarf_info_buffer) % 4) < dwarf_info_size)
    {
      beg_of_comp_unit = info_ptr;
      cu_header.length = read_4_bytes (abfd, info_ptr);
      info_ptr += 4;
      cu_header.version = read_2_bytes (abfd, info_ptr);
      info_ptr += 2;
      cu_header.abbrev_offset = read_4_bytes (abfd, info_ptr);
      info_ptr += 4;
      cu_header.addr_size = read_1_byte (abfd, info_ptr);
      info_ptr += 1;
      address_size = cu_header.addr_size;

      if (cu_header.version != 2)
	{
	  error ("Dwarf Error: wrong version in compilation unit header.");
	  return;
	}

      /* Read the abbrevs for this compilation unit into a table */
      dwarf2_read_abbrevs (abfd, cu_header.abbrev_offset);
      back_to = make_cleanup (dwarf2_empty_abbrev_table, NULL);

      /* Read the compilation unit die */
      info_ptr = read_partial_die (&comp_unit_die, abfd,
				   info_ptr, &comp_unit_has_pc_info);

      /* Set the language we're debugging */
      set_cu_language (comp_unit_die.language);

      /* Allocate a new partial symbol table structure */
      pst = start_psymtab_common (objfile, section_offsets,
			          comp_unit_die.name,
				  comp_unit_die.lowpc,
				  objfile->global_psymbols.next,
				  objfile->static_psymbols.next);

      /* Store offset in the .debug_info section of the comp_unit_die.  */
      pst->read_symtab_private = (char *)
				 (beg_of_comp_unit - dwarf_info_buffer);

      /* Store the function that reads in the rest of the symbol table */
      pst->read_symtab = dwarf2_psymtab_to_symtab;

      /* Read the rest of the partial symbols from this comp unit */
      info_ptr = scan_partial_symbols (info_ptr, objfile, &lowpc, &highpc);

      /* If the compilation unit didn't have an explicit address range,
	 then use the information extracted from its child dies.  */
      if (!comp_unit_has_pc_info)
	{
	  comp_unit_die.lowpc  = lowpc;
	  comp_unit_die.highpc = highpc;
        }
      pst->textlow  = comp_unit_die.lowpc;
      pst->texthigh = comp_unit_die.highpc;

      pst->n_global_syms = objfile->global_psymbols.next -
	(objfile->global_psymbols.list + pst->globals_offset);
      pst->n_static_syms = objfile->static_psymbols.next -
	(objfile->static_psymbols.list + pst->statics_offset);
      sort_pst_symbols (pst);

      /* If there is already a psymtab or symtab for a file of this
         name, remove it. (If there is a symtab, more drastic things
         also happen.) This happens in VxWorks.  */
      free_named_symtabs (pst->filename);

      info_ptr = beg_of_comp_unit + cu_header.length + 4;
    }
  do_cleanups (back_to);
}

/* Read in all interesting dies to the end of the compilation unit.  */

static char *
scan_partial_symbols (info_ptr, objfile, lowpc, highpc)
     char *info_ptr;
     struct objfile *objfile;
     CORE_ADDR *lowpc;
     CORE_ADDR *highpc;
{
  /* FIXME: This should free the attributes of the partial die structure
     when it is done with them (is there a more efficient way
     to do this). */
  bfd *abfd = objfile->obfd;
  struct partial_die_info pdi;
  int nesting_level = 1;	/* we've already read in comp_unit_die */
  int has_pc_info;

  pdi = zeroed_partial_die;
  *lowpc  = ((CORE_ADDR) -1);
  *highpc = ((CORE_ADDR) 0);
  do
    {
      info_ptr = read_partial_die (&pdi, abfd, info_ptr, &has_pc_info);
      switch (pdi.tag)
	{
	case DW_TAG_subprogram:
	case DW_TAG_variable:
	case DW_TAG_typedef:
	case DW_TAG_class_type:
	case DW_TAG_structure_type:
	case DW_TAG_union_type:
	  if (pdi.is_external || nesting_level == 1)
	    {
	      if (pdi.name)
		{
		  add_partial_symbol (&pdi, objfile);
		}
	    }
	   if (has_pc_info)
	     {
	       if (pdi.lowpc < *lowpc)
		 {
		   *lowpc = pdi.lowpc;
		 }
	       if (pdi.highpc > *lowpc)
		 {
		   *highpc = pdi.highpc;
		 }
	     }
	}
      if (pdi.has_children)
	{
	  nesting_level++;
	}
      if (pdi.tag == 0)
	{
	  nesting_level--;
	}
    }
  while (nesting_level);
  return info_ptr;
}

static void
add_partial_symbol (pdi, objfile)
     struct partial_die_info *pdi;
     struct objfile *objfile;
{
  switch (pdi->tag)
    {
    case DW_TAG_subprogram:
      if (pdi->is_external)
	{
	  record_minimal_symbol (pdi->name, pdi->lowpc,
				 mst_text, objfile);
	  add_psymbol_to_list (pdi->name, strlen (pdi->name),
			       VAR_NAMESPACE, LOC_BLOCK,
			       &objfile->global_psymbols,
			       0, pdi->lowpc, cu_language, objfile);
	}
      else
	{
	  add_psymbol_to_list (pdi->name, strlen (pdi->name),
			       VAR_NAMESPACE, LOC_BLOCK,
			       &objfile->static_psymbols,
			       0, pdi->lowpc, cu_language, objfile);
	}
      break;
    case DW_TAG_variable:
      if (pdi->is_external)
	{
	  record_minimal_symbol (pdi->name, convert_locdesc (pdi->locdesc),
				 mst_data, objfile);
	  add_psymbol_to_list (pdi->name, strlen (pdi->name),
			       VAR_NAMESPACE, LOC_STATIC,
			       &objfile->global_psymbols,
			       0, 0, cu_language, objfile);
	}
      else
	{
	  add_psymbol_to_list (pdi->name, strlen (pdi->name),
			       VAR_NAMESPACE, LOC_STATIC,
			       &objfile->static_psymbols,
			       0, 0, cu_language, objfile);
	}
      break;
    case DW_TAG_typedef:
      add_psymbol_to_list (pdi->name, strlen (pdi->name),
			   VAR_NAMESPACE, LOC_TYPEDEF,
			   &objfile->static_psymbols,
			   0, 0, cu_language, objfile);
      break;
    case DW_TAG_class_type:
    case DW_TAG_structure_type:
    case DW_TAG_union_type:
    case DW_TAG_enumeration_type:
      add_psymbol_to_list (pdi->name, strlen (pdi->name),
			   STRUCT_NAMESPACE, LOC_TYPEDEF,
			   &objfile->static_psymbols,
			   0, 0, cu_language, objfile);
      if (cu_language == language_cplus)
	{
	  /* For C++, these implicitly act as typedefs as well. */
	  add_psymbol_to_list (pdi->name, strlen (pdi->name),
			       VAR_NAMESPACE, LOC_TYPEDEF,
			       &objfile->static_psymbols,
			       0, 0, cu_language, objfile);
	}
      break;
    }
}

/* Expand this partial symbol table into a full symbol table.  */

static void
dwarf2_psymtab_to_symtab (pst)
     struct partial_symtab *pst;
{
  /* FIXME: This is barely more than a stub.  */
  if (pst != NULL)
    {
      if (pst->readin)
	{
	  warning ("bug: psymtab for %s is already read in.", pst->filename);
	}
      else
	{
	  psymtab_to_symtab_1 (pst);
	}
    }
}

static void
psymtab_to_symtab_1 (pst)
     struct partial_symtab *pst;
{
  struct objfile *objfile = pst->objfile;
  bfd *abfd = objfile->obfd;
  struct comp_unit_head cu_header;
  struct die_info *dies;
  struct attribute *attr;
  unsigned long offset;
  unsigned long int nesting_level;
  CORE_ADDR highpc;
  struct attribute *high_pc_attr;
  struct die_info *child_die;
  char *info_ptr;
  struct context_stack *context;
  struct symtab *symtab;
  struct cleanup *abbrev_cleanup, *die_cleanup;

  /* Get the offset of this compilation units debug info  */
  offset = (unsigned long) pst->read_symtab_private;
  info_ptr = dwarf_info_buffer + offset;

  /* read in the comp_unit header  */
  cu_header.length = read_4_bytes (abfd, info_ptr);
  info_ptr += 4;
  cu_header.version = read_2_bytes (abfd, info_ptr);
  info_ptr += 2;
  cu_header.abbrev_offset = read_4_bytes (abfd, info_ptr);
  info_ptr += 4;
  cu_header.addr_size = read_1_byte (abfd, info_ptr);
  info_ptr += 1;

  /* Read the abbrevs for this compilation unit  */
  dwarf2_read_abbrevs (abfd, cu_header.abbrev_offset);
  abbrev_cleanup = make_cleanup (dwarf2_empty_abbrev_table, NULL);

  dies = read_comp_unit (info_ptr, abfd);

  die_cleanup = make_cleanup (free_die_list, dies);

  /* Do line number decoding in read_file_scope () */
  process_die (dies, objfile);

  attr = dwarf_attr (dies, DW_AT_high_pc);
  if (attr)
    {
      highpc = DW_ADDR (attr);
    }
  else
    {
      /* Some compilers don't define a DW_AT_high_pc attribute for
	 the compilation unit.   If the DW_AT_high_pc is missing,
	 synthesize it, by scanning the DIE's below the compilation unit.  */
      highpc = 0;
      if (dies->has_children)
	{
	  child_die = dies->next;
	  while (child_die && child_die->tag)
	    {
	      if (child_die->tag == DW_TAG_subprogram)
		{
		  high_pc_attr = dwarf_attr (child_die, DW_AT_high_pc);
		  if (high_pc_attr)
		    {
		      highpc = max (highpc, DW_ADDR (high_pc_attr));
		    }
		}
	      child_die = sibling_die (child_die);
	    }
	}
    }

  symtab = end_symtab (highpc, objfile, 0);
  if (symtab != NULL)
    {
      symtab->language = cu_language;
    }
  pst->symtab = symtab;
  pst->readin = 1;
  if (info_verbose)
    {
      printf_filtered ("Sorting symbol table...");
      wrap_here ("");
      fflush (stdout);
    }
  sort_symtab_syms (pst->symtab);
  do_cleanups (abbrev_cleanup);
}

/* Process a die and its children.  */

static void
process_die (die, objfile)
     struct die_info *die;
     struct objfile *objfile;
{
  switch (die->tag)
    {
    case DW_TAG_padding:
      break;
    case DW_TAG_compile_unit:
      read_file_scope (die, objfile);
      break;
    case DW_TAG_subprogram:
      if (dwarf_attr (die, DW_AT_low_pc))
	{
	  read_func_scope (die, objfile);
	}
      break;
    case DW_TAG_lexical_block:
      read_lexical_block_scope (die, objfile);
      break;
    case DW_TAG_class_type:
    case DW_TAG_structure_type:
    case DW_TAG_union_type:
      read_structure_scope (die, objfile);
      break;
    case DW_TAG_enumeration_type:
      read_enumeration (die, objfile);
      break;
    case DW_TAG_subroutine_type:
      read_subroutine_type (die, objfile);
      break;
    case DW_TAG_array_type:
      dwarf_read_array_type (die, objfile);
      break;
    case DW_TAG_pointer_type:
      read_tag_pointer_type (die, objfile);
      break;
    case DW_TAG_string_type:
      read_tag_string_type (die, objfile);
      break;
    case DW_TAG_base_type:
      read_base_type (die, objfile);
      break;
    case DW_TAG_common_block:
      read_common_block (die, objfile);
      break;
    case DW_TAG_common_inclusion:
      break;
    default:
      new_symbol (die, objfile);
      break;
    }
}

static void
read_file_scope (die, objfile)
     struct die_info *die;
     struct objfile *objfile;
{
  unsigned int line_offset = 0;
  CORE_ADDR lowpc  = ((CORE_ADDR) -1);
  CORE_ADDR highpc = ((CORE_ADDR) 0);
  struct attribute *attr, *low_pc_attr, *high_pc_attr;
  char *name = NULL;
  char *comp_dir = NULL;
  struct die_info *child_die;
  bfd *abfd = objfile->obfd;

  low_pc_attr = dwarf_attr (die, DW_AT_low_pc);
  if (low_pc_attr)
    {
      lowpc = DW_ADDR (low_pc_attr);
    }
  high_pc_attr = dwarf_attr (die, DW_AT_high_pc);
  if (high_pc_attr)
    {
      highpc = DW_ADDR (high_pc_attr);
    }
  if (!low_pc_attr || !high_pc_attr)
    {
      if (die->has_children)
	{
	  child_die = die->next;
	  while (child_die && child_die->tag)
	    {
	      if (child_die->tag == DW_TAG_subprogram)
		{
		  low_pc_attr = dwarf_attr (child_die, DW_AT_low_pc);
		  if (low_pc_attr)
		    {
		      lowpc = min (lowpc, DW_ADDR (low_pc_attr));
		    }
		  high_pc_attr = dwarf_attr (child_die, DW_AT_high_pc);
		  if (high_pc_attr)
		    {
		      highpc = max (highpc, DW_ADDR (high_pc_attr));
		    }
		}
	      child_die = sibling_die (child_die);
	    }
	}
    }

  attr = dwarf_attr (die, DW_AT_name);
  if (attr)
    {
      name = DW_STRING (attr);
    }
  attr = dwarf_attr (die, DW_AT_comp_dir);
  if (attr)
    {
      comp_dir = DW_STRING (attr);
    }

  if (objfile->ei.entry_point >= lowpc &&
      objfile->ei.entry_point < highpc)
    {
      objfile->ei.entry_file_lowpc = lowpc;
      objfile->ei.entry_file_highpc = highpc;
    }

  attr = dwarf_attr (die, DW_AT_language);
  if (attr)
    {
      set_cu_language (DW_UNSND (attr));
    }

#if 0
    /* FIXME:Do something here.  */
    if (dip->at_producer != NULL)
    {
      handle_producer (dip->at_producer);
    }
#endif

  start_symtab (name, comp_dir, lowpc);

  /* Decode line number information.  */
  attr = dwarf_attr (die, DW_AT_stmt_list);
  if (!attr)
    {
      error (
        "Dwarf Error: No line number information for compilation unit: %s.",
        name);
    }
  line_offset = DW_UNSND (attr);
  dwarf_decode_lines (line_offset, abfd);

  /* Process all dies in compilation unit.  */
  if (die->has_children)
    {
      child_die = die->next;
      while (child_die && child_die->tag)
	{
	  process_die (child_die, objfile);
	  child_die = sibling_die (child_die);
	}
    }
}

static void
read_func_scope (die, objfile)
     struct die_info *die;
     struct objfile *objfile;
{
  register struct context_stack *new;
  CORE_ADDR lowpc  = 0;
  CORE_ADDR highpc = 0;
  struct die_info *child_die;
  struct attribute *attr;
  struct minimal_symbol *min_sym;
  char *name = NULL;

  attr = dwarf_attr (die, DW_AT_name);
  if (attr)
    {
      name = DW_STRING (attr);
    }

  attr = dwarf_attr (die, DW_AT_low_pc);
  if (attr)
    {
      lowpc = DW_ADDR (attr);
    }

  attr = dwarf_attr (die, DW_AT_high_pc);
  if (attr)
    {
      highpc = DW_ADDR (attr);
    }

  if (objfile->ei.entry_point >= lowpc &&
      objfile->ei.entry_point < highpc)
    {
      objfile->ei.entry_func_lowpc = lowpc;
      objfile->ei.entry_func_highpc = highpc;
    }

  if (STREQ (name, "main"))	/* FIXME: hardwired name */
    {
      objfile->ei.main_func_lowpc = lowpc;
      objfile->ei.main_func_highpc = highpc;
    }
  new = push_context (0, lowpc);
  new->name = new_symbol (die, objfile);
  list_in_scope = &local_symbols;

  if (die->has_children)
    {
      child_die = die->next;
      while (child_die && child_die->tag)
	{
	  process_die (child_die, objfile);
	  child_die = sibling_die (child_die);
	}
    }

  new = pop_context ();
  /* Make a block for the local symbols within.  */
  finish_block (new->name, &local_symbols, new->old_blocks,
		lowpc, highpc, objfile);
  list_in_scope = &file_symbols;
}

/* Process all the DIES contained within a lexical block scope.  Start
   a new scope, process the dies, and then close the scope.  */

static void
read_lexical_block_scope (die, objfile)
     struct die_info *die;
     struct objfile *objfile;
{
  register struct context_stack *new;
  CORE_ADDR lowpc = 0, highpc = 0;
  struct attribute *attr;
  struct die_info *child_die;

  attr = dwarf_attr (die, DW_AT_low_pc);
  if (attr)
    {
      lowpc = DW_ADDR (attr);
    }
  attr = dwarf_attr (die, DW_AT_high_pc);
  if (attr)
    {
      highpc = DW_ADDR (attr);
    }

  push_context (0, lowpc);
  if (die->has_children)
    {
      child_die = die->next;
      while (child_die && child_die->tag)
	{
	  process_die (child_die, objfile);
	  child_die = sibling_die (child_die);
	}
    }
  new = pop_context ();

  if (local_symbols != NULL)
    {
      finish_block (0, &local_symbols, new->old_blocks, new->start_addr,
		    highpc, objfile);
    }
  local_symbols = new->locals;
}

/* Called when we find the DIE that starts a structure or union scope
   (definition) to process all dies that define the members of the
   structure or union.

   NOTE: we need to call struct_type regardless of whether or not the
   DIE has an at_name attribute, since it might be an anonymous
   structure or union.  This gets the type entered into our set of
   user defined types.

   However, if the structure is incomplete (an opaque struct/union)
   then suppress creating a symbol table entry for it since gdb only
   wants to find the one with the complete definition.  Note that if
   it is complete, we just call new_symbol, which does it's own
   checking about whether the struct/union is anonymous or not (and
   suppresses creating a symbol table entry itself).  */

static void
read_structure_scope (die, objfile)
     struct die_info *die;
     struct objfile *objfile;
{
  struct type *type, *member_type;
  struct field *fields;
  struct die_info *child_die;
  struct attribute *attr;
  struct symbol *sym;
  int num_fields;

  type = dwarf_alloc_type (objfile);

  INIT_CPLUS_SPECIFIC (type);
  attr = dwarf_attr (die, DW_AT_name);

  if (die->tag == DW_TAG_structure_type)
    {
      TYPE_CODE (type) = TYPE_CODE_STRUCT;
      if (attr)
	{
	  TYPE_NAME (type) = obconcat (&objfile->type_obstack,
				     "struct", " ", DW_STRING (attr));
	}
    }
  else
    {
      /* die->tag == DW_TAG_union_type */
      TYPE_CODE (type) = TYPE_CODE_UNION;
      if (attr)
	{
	  TYPE_NAME (type) = obconcat (&objfile->type_obstack,
				       "union", " ", DW_STRING (attr));
	}
    }

  attr = dwarf_attr (die, DW_AT_byte_size);
  if (attr)
    {
      TYPE_LENGTH (type) = DW_UNSND (attr);
    }
  else
    {
      TYPE_LENGTH (type) = 0;
    }

  /* We need to add the type field to the die immediately so we don't
     infinitely recurse when dealing with pointers to the structure
     type within the structure itself. */
  die->type = type;

  num_fields = 0;
  fields = NULL;
  if (die->has_children)
    {
      child_die = die->next;
      while (child_die && child_die->tag)
	{
	  if (child_die->tag != DW_TAG_member)
	    {
	      process_die (child_die, objfile);
	    }
	  else
	    {
	      if ((num_fields % DW_FIELD_ALLOC_CHUNK) == 0)
		{
		  fields = (struct field *)
		    xrealloc (fields,
			      (num_fields + DW_FIELD_ALLOC_CHUNK)
			       * sizeof (struct field));
		}

	      /* Get bit offset of field */
	      attr = dwarf_attr (child_die, DW_AT_bit_offset);
	      if (attr)
		{
		  fields[num_fields].bitpos = DW_UNSND (attr);
		}
	      else
		{
		  fields[num_fields].bitpos = 0;
		}
	      attr = dwarf_attr (child_die, DW_AT_data_member_location);
	      if (attr)
		{
		  fields[num_fields].bitpos +=
		    decode_locdesc (DW_BLOCK (attr), objfile) * bits_per_byte;
		}

	      /* Get bit size of field (zero if none). */
	      attr = dwarf_attr (child_die, DW_AT_bit_size);
	      if (attr)
		{
		  fields[num_fields].bitsize = DW_UNSND (attr);
		}
	      else
		{
		  fields[num_fields].bitsize = 0;
		}

	      /* Get type of member. */
	      member_type = die_type (child_die, objfile);
	      fields[num_fields].type = member_type;

	      /* Get name of member. */
	      attr = dwarf_attr (child_die, DW_AT_name);
	      if (attr)
		{
		  fields[num_fields].name = obsavestring (DW_STRING (attr),
					    strlen (DW_STRING (attr)),
					      &objfile->type_obstack);
#if 0
		  fields[num_fields].name = strdup (DW_STRING (attr));
#endif
		}
	      num_fields++;
	    }
	  child_die = sibling_die (child_die);
	}
      type->nfields = num_fields;
      type->fields = fields;
    }
  else
    {
      /* No children, must be stub. */
      TYPE_FLAGS (type) |= TYPE_FLAG_STUB;
    }

  die->type = type;
  sym = new_symbol (die, objfile);
  if (sym != NULL)
    {
      SYMBOL_TYPE (sym) = type;
    }
}

/* Given a pointer to a die which begins an enumeration, process all
   the dies that define the members of the enumeration.

   This will be much nicer in draft 6 of the DWARF spec when our
   members will be dies instead squished into the DW_AT_element_list
   attribute.

   NOTE: We reverse the order of the element list.  */

static void
read_enumeration (die, objfile)
     struct die_info *die;
     struct objfile *objfile;
{
  struct die_info *child_die;
  struct type *type;
  struct field *fields;
  struct attribute *attr;
  struct symbol *sym;
  struct dwarf_block *blk;
  int num_fields;
  unsigned int size, bytes_read, i;

  type = dwarf_alloc_type (objfile);

  TYPE_CODE (type) = TYPE_CODE_ENUM;
  attr = dwarf_attr (die, DW_AT_name);
  if (attr)
    {
      TYPE_NAME (type) = obconcat (&objfile->type_obstack,
				   "enum ", " ", DW_STRING (attr));
    }

  attr = dwarf_attr (die, DW_AT_byte_size);
  if (attr)
    {
      TYPE_LENGTH (type) = DW_UNSND (attr);
    }
  else
    {
      TYPE_LENGTH (type) = 0;
    }

  num_fields = 0;
  fields = NULL;
  if (die->has_children)
    {
      child_die = die->next;
      while (child_die && child_die->tag)
	{
	  if (child_die->tag != DW_TAG_enumerator)
	    {
	      process_die (child_die, objfile);
	    }
	  else
	    {
	      if ((num_fields % DW_FIELD_ALLOC_CHUNK) == 0)
		{
		  fields = (struct field *)
		    xrealloc (fields,
		      (num_fields + DW_FIELD_ALLOC_CHUNK)
			* sizeof (struct field));
		}

	      /* Handcraft a new symbol for this enum member. */
	      sym = (struct symbol *) obstack_alloc (&objfile->symbol_obstack,
					      sizeof (struct symbol));
	      memset (sym, 0, sizeof (struct symbol));

	      fields[num_fields].type = NULL;
	      fields[num_fields].bitsize = 0;
	      attr = dwarf_attr (child_die, DW_AT_name);
	      if (attr)
		{
		  fields[num_fields].name = strdup (DW_STRING (attr));
		  SYMBOL_NAME (sym) = strdup (fields[num_fields].name);
		}
	      attr = dwarf_attr (child_die, DW_AT_const_value);
	      if (attr)
		{
		  fields[num_fields].bitpos = DW_UNSND (attr);
		  SYMBOL_VALUE (sym) = DW_UNSND (attr);
		}

#if 0
	      SYMBOL_NAME (sym) = create_name (elist->str,
					    &objfile->symbol_obstack);
#endif
	      SYMBOL_INIT_LANGUAGE_SPECIFIC (sym, cu_language);
	      SYMBOL_NAMESPACE (sym) = VAR_NAMESPACE;
	      SYMBOL_CLASS (sym) = LOC_CONST;
	      SYMBOL_TYPE (sym) = type;
	      add_symbol_to_list (sym, list_in_scope);

	      num_fields++;
	    }

	  child_die = sibling_die (child_die);
	}
      type->fields = fields;
      type->nfields = num_fields;
    }
  die->type = type;
  sym = new_symbol (die, objfile);
  if (sym != NULL)
    {
      SYMBOL_TYPE (sym) = type;
    }
}

/* Extract all information from a DW_TAG_array_type DIE and put it in
   the DIE's type field.  For now, this only handles one dimensional
   arrays.  */

static void
dwarf_read_array_type (die, objfile)
     struct die_info *die;
     struct objfile *objfile;
{
  struct die_info *child_die;
  struct type *type, *element_type, *range_type, *index_type;
  struct attribute *attr;
  struct dwarf_block *blk;
  unsigned int size, i, type_form, bytes_read;
  unsigned int index_spec, lo_spec, hi_spec, type_ref;
  unsigned int low, high;

  /* Return if we've already decoded this type. */
  if (die->type)
    {
      return;
    }

  element_type = die_type (die, objfile);

  low = 0;
  high = 1;
  if (cu_language == DW_LANG_Fortran77 || cu_language == DW_LANG_Fortran90)
    {
      /* FORTRAN implies a lower bound of 1, if not given.  */
      low = 1;
    }

  child_die = die->next;
  while (child_die && child_die->tag)
    {
      if (child_die->tag == DW_TAG_subrange_type)
	{
	  index_type = die_type (child_die, objfile);
	  attr = dwarf_attr (child_die, DW_AT_lower_bound);
	  if (attr)
	    {
	      if (attr->form == DW_FORM_sdata)
		{
		  low = DW_SND (attr);
		}
	      else if (attr->form == DW_FORM_udata
	               || attr->form == DW_FORM_data1
	               || attr->form == DW_FORM_data2
	               || attr->form == DW_FORM_data4)
		{
		  low = DW_UNSND (attr);
		}
	      else
		{
		  if (!array_bound_warning_given)
		    {
		      warning ("Non-constant array bounds ignored.");
		      array_bound_warning_given = 1;
		    }
#ifdef FORTRAN_HACK
		  type = dwarf_alloc_type (objfile);
		  TYPE_TARGET_TYPE (type) = element_type;
		  TYPE_OBJFILE (type) = objfile;
		  TYPE_LENGTH (type) = 4;
		  TYPE_CODE (type) = TYPE_CODE_PTR;
		  TYPE_FLAGS (type) |= TYPE_FLAG_UNSIGNED;
		  TYPE_POINTER_TYPE (element_type) = type;
		  goto done;
#else
		  low = 0;
#endif
		}
	    }
	  attr = dwarf_attr (child_die, DW_AT_upper_bound);
	  if (attr)
	    {
	      if (attr->form == DW_FORM_sdata)
		{
		  high = DW_SND (attr);
		}
	      else if (attr->form == DW_FORM_udata
	               || attr->form == DW_FORM_data1
	               || attr->form == DW_FORM_data2
	               || attr->form == DW_FORM_data4)
		{
		  high = DW_UNSND (attr);
		}
	      else
		{
		  if (!array_bound_warning_given)
		    {
		      warning ("Non-constant array bounds ignored.");
		      array_bound_warning_given = 1;
		    }
#ifdef FORTRAN_HACK
		  type = dwarf_alloc_type (objfile);
		  TYPE_TARGET_TYPE (type) = element_type;
		  TYPE_OBJFILE (type) = objfile;
		  TYPE_LENGTH (type) = 4;
		  TYPE_CODE (type) = TYPE_CODE_PTR;
		  TYPE_FLAGS (type) |= TYPE_FLAG_UNSIGNED;
		  TYPE_POINTER_TYPE (element_type) = type;
		  goto done;
#else
		  high = 1;
#endif
		}
	    }
	}
      range_type = create_range_type (NULL, index_type, low, high);
      type = create_array_type (NULL, element_type, range_type);
      element_type = type;
      child_die = sibling_die (child_die);
    }
done:
  /* Install the type in the die. */
  die->type = type;
}

/* First cut: install each common block member as a global variable.  */

static void
read_common_block (die, objfile)
     struct die_info *die;
     struct objfile *objfile;
{
  struct die_info *child_die;
  struct attribute *attr;
  struct symbol *sym;
  CORE_ADDR base;

  attr = dwarf_attr (die, DW_AT_location);
  if (attr)
    {
      base = decode_locdesc (DW_BLOCK (attr), objfile);
    }
  if (die->has_children)
    {
      child_die = die->next;
      while (child_die && child_die->tag)
	{
	  sym = new_symbol (child_die, objfile);
	  attr = dwarf_attr (child_die, DW_AT_data_member_location);
	  if (attr)
	    {
	      SYMBOL_VALUE_ADDRESS (sym) =
		base + decode_locdesc (DW_BLOCK (attr), objfile);
	      add_symbol_to_list (sym, &global_symbols);
	    }
	  child_die = sibling_die (child_die);
	}
    }
}

/* Extract all information from a DW_TAG_pointer_type DIE and add to
   the user defined type vector.  */

static void
read_tag_pointer_type (die, objfile)
     struct die_info *die;
     struct objfile *objfile;
{
  struct type *type, *pointed_to_type;
  struct attribute *attr;

  if (die->type)
    {
      return;
    }

  pointed_to_type = die_type (die, objfile);

  type = dwarf_alloc_type (objfile);
  TYPE_TARGET_TYPE (type) = pointed_to_type;
  TYPE_OBJFILE (type) = objfile;
  attr = dwarf_attr (die, DW_AT_byte_size);
  if (attr)
    {
      TYPE_LENGTH (type) = DW_UNSND (attr);
    }
  else
    {
      TYPE_LENGTH (type) = address_size;
    }
  TYPE_CODE (type) = TYPE_CODE_PTR;
  TYPE_FLAGS (type) |= TYPE_FLAG_UNSIGNED;

  TYPE_POINTER_TYPE (pointed_to_type) = type;
  die->type = type;
}

/* Extract all information from a DW_TAG_reference_type DIE and add to
   the user defined type vector.  */

static void
read_tag_reference_type (die, objfile)
     struct die_info *die;
     struct objfile *objfile;
{
  struct type *type, *pointed_to_type;
  struct attribute *attr;

  if (die->type)
    {
      return;
    }

  pointed_to_type = die_type (die, objfile);

  type = dwarf_alloc_type (objfile);
  TYPE_TARGET_TYPE (type) = pointed_to_type;
  TYPE_OBJFILE (type) = objfile;
  attr = dwarf_attr (die, DW_AT_byte_size);
  if (attr)
    {
      TYPE_LENGTH (type) = DW_UNSND (attr);
    }
  else
    {
      TYPE_LENGTH (type) = address_size;
    }
  TYPE_CODE (type) = TYPE_CODE_REF;
  TYPE_FLAGS (type) |= TYPE_FLAG_UNSIGNED;

  TYPE_REFERENCE_TYPE (pointed_to_type) = type;
  die->type = type;
}

static void
read_tag_const_type (die, objfile)
     struct die_info *die;
     struct objfile *objfile;
{
  if (die->type)
    {
      return;
    }

  if (!tag_const_warning_given)
    {
      warning ("gdb ignores `const' qualifiers.");
      tag_const_warning_given = 1;
    }

  die->type = die_type (die, objfile);
}

static void
read_tag_volatile_type (die, objfile)
     struct die_info *die;
     struct objfile *objfile;
{
  if (die->type)
    {
      return;
    }

  if (!tag_volatile_warning_given)
    {
      warning ("gdb ignores `volatile' qualifiers.");
      tag_volatile_warning_given = 1;
    }

  die->type = die_type (die, objfile);
}

/* Extract all information from a DW_TAG_string_type DIE and add to
   the user defined type vector.  It isn't really a user defined type,
   but it behaves like one, with other DIE's using an AT_user_def_type
   attribute to reference it.  */

static void
read_tag_string_type (die, objfile)
     struct die_info *die;
     struct objfile *objfile;
{
  struct type *type, *range_type, *index_type, *char_type;
  struct attribute *attr;
  unsigned int length;

  if (die->type)
    {
      return;
    }

  attr = dwarf_attr (die, DW_AT_string_length);
  if (attr)
    {
      length = DW_UNSND (attr);
    }
  else
    {
      length = 1;
    }
  index_type = dwarf2_fundamental_type (objfile, FT_INTEGER);
  range_type = create_range_type (NULL, index_type, 1, length);
  char_type = dwarf2_fundamental_type (objfile, FT_CHAR);
  type = create_string_type (char_type, range_type);
  die->type = type;
}

/* Handle DIES due to C code like:

   struct foo
     {
       int (*funcp)(int a, long l);
       int b;
     };

   ('funcp' generates a DW_TAG_subroutine_type DIE)

   NOTE: parameter DIES are currently ignored.  See if gdb has a way to
   include this info in it's type system, and decode them if so.  Is
   this what the type structure's "arg_types" field is for?  (FIXME) */

static void
read_subroutine_type (die, objfile)
     struct die_info *die;
     struct objfile *objfile;
{
  struct type *type;		/* Type that this function returns */
  struct type *ftype;		/* Function that returns above type */

  /* Decode the type that this subroutine returns */
  if (die->type)
    {
      return;
    }
  type = die_type (die, objfile);
  ftype = lookup_function_type (type);

  TYPE_TARGET_TYPE (ftype) = type;
  TYPE_LENGTH (ftype) = 1;
  TYPE_CODE (ftype) = TYPE_CODE_FUNC;
  TYPE_OBJFILE (ftype) = objfile;

  die->type = type;
}

static void
read_typedef (die, objfile)
     struct die_info *die;
     struct objfile *objfile;
{
  struct type *type;

  if (!die->type)
    {
      type = die_type (die, objfile);
      die->type = type;
    }
}

/* Find a representation of a given base type and install
   it in the TYPE field of the die.  */

static void
read_base_type (die, objfile)
     struct die_info *die;
     struct objfile *objfile;
{
  struct type *type;
  struct attribute *attr;
  int encoding = 0, size = 0;

  /* If we've already decoded this die, this is a no-op. */
  if (die->type)
    {
      return;
    }

  attr = dwarf_attr (die, DW_AT_encoding);
  if (attr)
    {
      encoding = DW_UNSND (attr);
    }
  attr = dwarf_attr (die, DW_AT_byte_size);
  if (attr)
    {
      size = DW_UNSND (attr);
    }
  type = dwarf_base_type (encoding, size);
  die->type = type;
}

/* Read a whole compilation unit into a linked list of dies.  */

struct die_info *
read_comp_unit (info_ptr, abfd)
    char *info_ptr;
    bfd *abfd;
{
  struct die_info *first_die, *last_die, *die;
  char *cur_ptr;
  int nesting_level;

  cur_ptr = info_ptr;
  nesting_level = 0;
  first_die = last_die = NULL;
  do
    {
      cur_ptr = read_full_die (&die, abfd, cur_ptr);
      if (die->has_children)
	{
	  nesting_level++;
	}
      if (die->tag == 0)
	{
	  nesting_level--;
	}

      die->next = NULL;

      /* Enter die in reference hash table */
      store_in_ref_table (die->offset, die);

      if (!first_die)
	{
	  first_die = last_die = die;
	}
      else
	{
	  last_die->next = die;
	  last_die = die;
	}
    }
  while (nesting_level > 0);
  return first_die;
}

/* Free a linked list of dies.  */

static void
free_die_list (dies)
     struct die_info *dies;
{
  struct die_info *die, *next;

  die = dies;
  while (die)
    {
      next = die->next;
      free (die->attrs);
      free (die);
      die = next;
    }
}

/* Read the contents of the section at OFFSET and of size SIZE in the
   object file specified by ABFD into a buffer of bytes and return it.  */

static char *
dwarf2_read_section (abfd, offset, size)
     bfd * abfd;
     file_ptr offset;
     unsigned int size;
{
  char *buf;

  buf = xmalloc (size);
  if ((bfd_seek (abfd, offset, SEEK_SET) != 0) ||
      (bfd_read (buf, size, 1, abfd) != size))
    {
      free (buf);
      buf = NULL;
      error ("Dwarf Error: Can't read DWARF data from '%s'",
        bfd_get_filename (abfd));
    }
  return buf;
}

/* In DWARF version 2, the description of the debugging information is
   stored in a separate .debug_abbrev section.  Before we read any
   dies from a section we read in all abbreviations and install them
   in a hash table.  */

static void
dwarf2_read_abbrevs (abfd, offset)
     bfd * abfd;
     unsigned int offset;
{
  char *abbrev_ptr;
  struct abbrev_info *cur_abbrev;
  unsigned int abbrev_number, bytes_read, abbrev_name;
  unsigned int abbrev_form, hash_number;

  /* empty the table */
  dwarf2_empty_abbrev_table ();

  abbrev_ptr = dwarf_abbrev_buffer + offset;
  abbrev_number = read_unsigned_leb128 (abfd, abbrev_ptr, &bytes_read);
  abbrev_ptr += bytes_read;

  /* loop until we reach an abbrev number of 0 */
  while (abbrev_number)
    {
      cur_abbrev = dwarf_alloc_abbrev ();

      /* read in abbrev header */
      cur_abbrev->number = abbrev_number;
      cur_abbrev->tag = read_unsigned_leb128 (abfd, abbrev_ptr, &bytes_read);
      abbrev_ptr += bytes_read;
      cur_abbrev->has_children = read_1_byte (abfd, abbrev_ptr);
      abbrev_ptr += 1;

      /* now read in declarations */
      abbrev_name = read_unsigned_leb128 (abfd, abbrev_ptr, &bytes_read);
      abbrev_ptr += bytes_read;
      abbrev_form = read_unsigned_leb128 (abfd, abbrev_ptr, &bytes_read);
      abbrev_ptr += bytes_read;
      while (abbrev_name)
	{
	  if ((cur_abbrev->num_attrs % ATTR_ALLOC_CHUNK) == 0)
	    {
	      cur_abbrev->attrs = xrealloc (cur_abbrev->attrs,
			    (cur_abbrev->num_attrs + ATTR_ALLOC_CHUNK)
				       * sizeof (struct attr_abbrev));
	    }
	  cur_abbrev->attrs[cur_abbrev->num_attrs].name = abbrev_name;
	  cur_abbrev->attrs[cur_abbrev->num_attrs++].form = abbrev_form;
	  abbrev_name = read_unsigned_leb128 (abfd, abbrev_ptr, &bytes_read);
	  abbrev_ptr += bytes_read;
	  abbrev_form = read_unsigned_leb128 (abfd, abbrev_ptr, &bytes_read);
	  abbrev_ptr += bytes_read;
	}

      hash_number = abbrev_number % ABBREV_HASH_SIZE;
      cur_abbrev->next = dwarf2_abbrevs[hash_number];
      dwarf2_abbrevs[hash_number] = cur_abbrev;

      /* get next abbrev */
      abbrev_number = read_unsigned_leb128 (abfd, abbrev_ptr, &bytes_read);
      abbrev_ptr += bytes_read;
    }
}

/* Empty the abbrev table for a new compilation unit.  */

static void
dwarf2_empty_abbrev_table ()
{
  int i;
  struct abbrev_info *abbrev, *next;

  for (i = 0; i < ABBREV_HASH_SIZE; ++i)
    {
      next = NULL;
      abbrev = dwarf2_abbrevs[i];
      while (abbrev)
	{
	  next = abbrev->next;
	  free (abbrev->attrs);
	  free (abbrev);
	  abbrev = next;
	}
      dwarf2_abbrevs[i] = NULL;
    }
}

/* Lookup an abbrev_info structure in the abbrev hash table.  */

static struct abbrev_info *
dwarf2_lookup_abbrev (number)
     unsigned int number;
{
  unsigned int hash_number;
  struct abbrev_info *abbrev;

  hash_number = number % ABBREV_HASH_SIZE;
  abbrev = dwarf2_abbrevs[hash_number];

  while (abbrev)
    {
      if (abbrev->number == number)
	return abbrev;
      else
	abbrev = abbrev->next;
    }
  return NULL;
}

/* Read a minimal amount of information into the minimal die structure.  */

static char *
read_partial_die (part_die, abfd, info_ptr, has_pc_info)
     struct partial_die_info *part_die;
     bfd * abfd;
     char *info_ptr;
     int *has_pc_info;
{
  unsigned int abbrev_number, bytes_read, i;
  struct abbrev_info *abbrev;
  char ebuf[256];
  int has_low_pc_attr  = 0;
  int has_high_pc_attr = 0;

  *has_pc_info = 0;
  abbrev_number = read_unsigned_leb128 (abfd, info_ptr, &bytes_read);
  info_ptr += bytes_read;
  if (!abbrev_number)
    {
      part_die->tag = 0;
      part_die->has_children = 0;
      part_die->abbrev = abbrev_number;
      return info_ptr;
    }

  abbrev = dwarf2_lookup_abbrev (abbrev_number);
  if (!abbrev)
    {
      error ("Dwarf Error: Could not find abbrev number %d.", abbrev_number);
    }
  part_die->offset = info_ptr - dwarf_info_buffer;
  part_die->tag = abbrev->tag;
  part_die->has_children = abbrev->has_children;
  part_die->is_external = 0;
  part_die->abbrev = abbrev_number;

  {
    char *str = "";
    struct dwarf_block *blk = 0;
    CORE_ADDR addr = ((CORE_ADDR) -1);
    unsigned int unsnd = ((unsigned int) -1);
    int snd = -1;

    for (i = 0; i < abbrev->num_attrs; ++i)
      {
	/* read the correct type of data */
	switch (abbrev->attrs[i].form)
	  {
	  case DW_FORM_addr:
	    addr = read_address (abfd, info_ptr);
	    info_ptr += address_size;
	    break;
	  case DW_FORM_ref_addr:
	    addr = read_address (abfd, info_ptr);
	    info_ptr += address_size;
	    break;
	  case DW_FORM_block2:
	    blk = dwarf_alloc_block ();
	    blk->size = read_2_bytes (abfd, info_ptr);
	    info_ptr += 2;
	    blk->data = read_n_bytes (abfd, info_ptr, blk->size);
	    info_ptr += blk->size;
	    break;
	  case DW_FORM_block4:
	    blk = dwarf_alloc_block ();
	    blk->size = read_4_bytes (abfd, info_ptr);
	    info_ptr += 2;
	    blk->data = read_n_bytes (abfd, info_ptr, blk->size);
	    info_ptr += blk->size;
	    break;
	  case DW_FORM_data2:
	    unsnd = read_2_bytes (abfd, info_ptr);
	    info_ptr += 2;
	    break;
	  case DW_FORM_data4:
	    unsnd = read_4_bytes (abfd, info_ptr);
	    info_ptr += 4;
	    break;
	  case DW_FORM_data8:
	    unsnd = read_8_bytes (abfd, info_ptr);
	    info_ptr += 8;
	    break;
	  case DW_FORM_string:
	    str = read_string (abfd, info_ptr, &bytes_read);
	    info_ptr += bytes_read;
	    break;
	  case DW_FORM_block:
	    blk = dwarf_alloc_block ();
	    blk->size = read_unsigned_leb128 (abfd, info_ptr, &bytes_read);
	    info_ptr += bytes_read;
	    blk->data = read_n_bytes (abfd, info_ptr, blk->size);
	    info_ptr += blk->size;
	    break;
	  case DW_FORM_block1:
	    blk = dwarf_alloc_block ();
	    blk->size = read_1_byte (abfd, info_ptr);
	    info_ptr += 1;
	    blk->data = read_n_bytes (abfd, info_ptr, blk->size);
	    info_ptr += blk->size;
	    break;
	  case DW_FORM_data1:
	    unsnd = read_1_byte (abfd, info_ptr);
	    info_ptr += 1;
	    break;
	  case DW_FORM_ref1:
	    unsnd = read_1_byte (abfd, info_ptr);
	    info_ptr += 1;
	    break;
	  case DW_FORM_ref2:
	    unsnd = read_2_bytes (abfd, info_ptr);
	    info_ptr += 2;
	    break;
	  case DW_FORM_ref4:
	    unsnd = read_4_bytes (abfd, info_ptr);
	    info_ptr += 4;
	    break;
	  case DW_FORM_ref_udata:
	    unsnd = read_unsigned_leb128 (abfd, info_ptr, &bytes_read);
	    info_ptr += bytes_read;
	    break;
	  case DW_FORM_flag:
	    unsnd = read_1_byte (abfd, info_ptr);
	    info_ptr += 1;
	    break;
	  case DW_FORM_sdata:
	    snd = read_signed_leb128 (abfd, info_ptr, &bytes_read);
	    info_ptr += bytes_read;
	    break;
	  case DW_FORM_udata:
	    unsnd = read_unsigned_leb128 (abfd, info_ptr, &bytes_read);
	    info_ptr += bytes_read;
	    break;
	  case DW_FORM_indirect:
	  default:
	    sprintf (ebuf,
		     "Dwarf Error: Cannot handle %s in DWARF reader.",
		     dwarf_form_name (abbrev->attrs[i].form));
	    error (ebuf);
	  }

	/* store the data if it is of an attribute we want to keep in a
	   partial symbol table */
	switch (abbrev->attrs[i].name)
	  {
	  case DW_AT_name:
	    part_die->name = str;
	    break;
	  case DW_AT_low_pc:
	    has_low_pc_attr = 1;
	    part_die->lowpc = addr;
	    break;
	  case DW_AT_high_pc:
	    has_high_pc_attr = 1;
	    part_die->highpc = addr;
	    break;
	  case DW_AT_location:
	    part_die->locdesc = blk;
	    break;
	  case DW_AT_language:
	    part_die->language = unsnd;
	    break;
	  case DW_AT_external:
	    part_die->is_external = unsnd;
	  }
      }
  }
  *has_pc_info = has_low_pc_attr && has_high_pc_attr;
  return info_ptr;
}

/* Read the die from the .debug_info section buffer.  And set diep to
   point to a newly allocated die with its information.  */

static char *
read_full_die (diep, abfd, info_ptr)
     struct die_info **diep;
     bfd *abfd;
     char *info_ptr;
{
  unsigned int abbrev_number, bytes_read, i, offset;
  struct abbrev_info *abbrev;
  struct die_info *die;
  char ebuf[256];

  offset = info_ptr - dwarf_info_buffer;
  abbrev_number = read_unsigned_leb128 (abfd, info_ptr, &bytes_read);
  info_ptr += bytes_read;
  if (!abbrev_number)
    {
      die = dwarf_alloc_die ();
      die->tag = 0;
      die->abbrev = abbrev_number;
      die->type = NULL;
      *diep = die;
      return info_ptr;
    }

  abbrev = dwarf2_lookup_abbrev (abbrev_number);
  if (!abbrev)
    {
      error ("Dwarf Error: could not find abbrev number %d.", abbrev_number);
    }
  die = dwarf_alloc_die ();
  die->offset = offset;
  die->tag = abbrev->tag;
  die->has_children = abbrev->has_children;
  die->abbrev = abbrev_number;
  die->type = NULL;

  die->num_attrs = abbrev->num_attrs;
  die->attrs = xmalloc (die->num_attrs * sizeof (struct attribute));

  {
    char *str;
    struct dwarf_block *blk;
    unsigned long addr;
    unsigned int unsnd;
    int snd;

    for (i = 0; i < abbrev->num_attrs; ++i)
      {
	/* read the correct type of data */

	die->attrs[i].name = abbrev->attrs[i].name;
	die->attrs[i].form = abbrev->attrs[i].form;

	switch (abbrev->attrs[i].form)
	  {
	  case DW_FORM_addr:
	  case DW_FORM_ref_addr:
	    die->attrs[i].u.addr = read_address (abfd, info_ptr);
	    info_ptr += address_size;
	    break;
	  case DW_FORM_block2:
	    blk = dwarf_alloc_block ();
	    blk->size = read_2_bytes (abfd, info_ptr);
	    info_ptr += 2;
	    blk->data = read_n_bytes (abfd, info_ptr, blk->size);
	    info_ptr += blk->size;
	    die->attrs[i].u.blk = blk;
	    break;
	  case DW_FORM_block4:
	    blk = dwarf_alloc_block ();
	    blk->size = read_4_bytes (abfd, info_ptr);
	    info_ptr += 2;
	    blk->data = read_n_bytes (abfd, info_ptr, blk->size);
	    info_ptr += blk->size;
	    die->attrs[i].u.blk = blk;
	    break;
	  case DW_FORM_data2:
	    die->attrs[i].u.unsnd = read_2_bytes (abfd, info_ptr);
	    info_ptr += 2;
	    break;
	  case DW_FORM_data4:
	    die->attrs[i].u.unsnd = read_4_bytes (abfd, info_ptr);
	    info_ptr += 4;
	    break;
	  case DW_FORM_data8:
	    die->attrs[i].u.unsnd = read_8_bytes (abfd, info_ptr);
	    info_ptr += 8;
	    break;
	  case DW_FORM_string:
	    die->attrs[i].u.str = read_string (abfd, info_ptr, &bytes_read);
	    info_ptr += bytes_read;
	    break;
	  case DW_FORM_block:
	    blk = dwarf_alloc_block ();
	    blk->size = read_unsigned_leb128 (abfd, info_ptr, &bytes_read);
	    info_ptr += bytes_read;
	    blk->data = read_n_bytes (abfd, info_ptr, blk->size);
	    info_ptr += blk->size;
	    die->attrs[i].u.blk = blk;
	    break;
	  case DW_FORM_block1:
	    blk = dwarf_alloc_block ();
	    blk->size = read_1_byte (abfd, info_ptr);
	    info_ptr += 1;
	    blk->data = read_n_bytes (abfd, info_ptr, blk->size);
	    info_ptr += blk->size;
	    die->attrs[i].u.blk = blk;
	    break;
	  case DW_FORM_data1:
	    die->attrs[i].u.unsnd = read_1_byte (abfd, info_ptr);
	    info_ptr += 1;
	    break;
	  case DW_FORM_ref1:
	    die->attrs[i].u.unsnd = read_1_byte (abfd, info_ptr);
	    info_ptr += 1;
	    break;
	  case DW_FORM_ref2:
	    die->attrs[i].u.unsnd = read_2_bytes (abfd, info_ptr);
	    info_ptr += 2;
	    break;
	  case DW_FORM_ref4:
	    die->attrs[i].u.unsnd = read_4_bytes (abfd, info_ptr);
	    info_ptr += 4;
	    break;
	  case DW_FORM_ref_udata:
	    die->attrs[i].u.unsnd = read_unsigned_leb128 (abfd,
							  info_ptr,
							  &bytes_read);
	    info_ptr += bytes_read;
	    break;
	  case DW_FORM_flag:
	    die->attrs[i].u.unsnd = read_1_byte (abfd, info_ptr);
	    info_ptr += 1;
	    break;
	  case DW_FORM_sdata:
	    die->attrs[i].u.snd = read_signed_leb128 (abfd,
						      info_ptr,
						      &bytes_read);
	    info_ptr += bytes_read;
	    break;
	  case DW_FORM_udata:
	    die->attrs[i].u.unsnd = read_unsigned_leb128 (abfd,
							  info_ptr,
							  &bytes_read);
	    info_ptr += bytes_read;
	    break;
	  case DW_FORM_indirect:
	  default:
	    sprintf (ebuf,
		     "Dwarf Error: Cannot handle %s in DWARF reader.",
		     dwarf_form_name (abbrev->attrs[i].form));
	    error (ebuf);
	  }

      }
  }
  *diep = die;
  return info_ptr;
}

/* read dwarf information from a buffer */

static unsigned int
read_1_byte (abfd, buf)
     bfd *abfd;
     char *buf;
{
  return bfd_get_8 (abfd, (bfd_byte *) buf);
}

static unsigned int
read_2_bytes (abfd, buf)
     bfd *abfd;
     char *buf;
{
  return bfd_get_16 (abfd, (bfd_byte *) buf);
}

static unsigned int
read_4_bytes (abfd, buf)
     bfd *abfd;
     char *buf;
{
  return bfd_get_32 (abfd, (bfd_byte *) buf);
}

static unsigned int
read_8_bytes (abfd, buf)
     bfd *abfd;
     char *buf;
{
  return bfd_get_64 (abfd, (bfd_byte *) buf);
}

static CORE_ADDR
read_address (abfd, buf)
     bfd *abfd;
     char *buf;
{
  CORE_ADDR retval = 0;

  if (address_size == 4)
    {
      retval = bfd_get_32 (abfd, (bfd_byte *) buf);
    } else {			/* *THE* alternative is 8, right? */
      retval = bfd_get_64 (abfd, (bfd_byte *) buf);
    }
  return retval;
}

static char *
read_n_bytes (abfd, buf, size)
     bfd * abfd;
     char *buf;
     unsigned int size;
{
  char *ret;
  unsigned int i;

  ret = xmalloc (size);
  for (i = 0; i < size; ++i)
    {
      ret[i] = bfd_get_8 (abfd, (bfd_byte *) buf);
      buf++;
    }
  return ret;
}

/* FIXME : hardwired string size limit */

static char *
read_string (abfd, buf, bytes_read_ptr)
     bfd *abfd;
     char *buf;
     unsigned int *bytes_read_ptr;
{
  char ret_buf[DWARF2_MAX_STRING_SIZE], *ret, byte;
  unsigned int i;

  i = 0;
  do
    {
      byte = bfd_get_8 (abfd, (bfd_byte *) buf);
      buf++;
      ret_buf[i++] = byte;
    }
  while (byte);
  if (i == 1)
    {
      *bytes_read_ptr = 1;
      return NULL;
    }
  ret = xmalloc (i);
  strncpy (ret, ret_buf, i);
  *bytes_read_ptr = i;
  return ret;
}

static unsigned int
read_unsigned_leb128 (abfd, buf, bytes_read_ptr)
     bfd *abfd;
     char *buf;
     unsigned int *bytes_read_ptr;
{
  unsigned int result, num_read;
  int i, shift;
  unsigned char byte;

  result = 0;
  shift = 0;
  num_read = 0;
  i = 0;
  while (1)
    {
      byte = bfd_get_8 (abfd, (bfd_byte *) buf);
      buf++;
      num_read++;
      result |= ((byte & 127) << shift);
      if ((byte & 128) == 0)
	{
	  break;
	}
      shift += 7;
    }
  *bytes_read_ptr = num_read;
  return result;
}

static int
read_signed_leb128 (abfd, buf, bytes_read_ptr)
     bfd *abfd;
     char *buf;
     unsigned int *bytes_read_ptr;
{
  int result;
  int i, shift, size, num_read;
  unsigned char byte;

  result = 0;
  shift = 0;
  size = 32;
  num_read = 0;
  i = 0;
  while (1)
    {
      byte = bfd_get_8 (abfd, (bfd_byte *) buf);
      buf++;
      num_read++;
      result |= ((byte & 127) << shift);
      shift += 7;
      if ((byte & 128) == 0)
	{
	  break;
	}
    }
  if ((shift < size) && (byte & 0x40))
    {
      result |= -(1 << shift);
    }
  *bytes_read_ptr = num_read;
  return result;
}

static void
set_cu_language (lang)
     unsigned int lang;
{
  switch (lang)
    {
    case DW_LANG_C89:
    case DW_LANG_C:
    case DW_LANG_Fortran77:
      cu_language = language_c;
      break;
    case DW_LANG_C_plus_plus:
      cu_language = language_cplus;
      break;
    case DW_LANG_Ada83:
    case DW_LANG_Cobol74:
    case DW_LANG_Cobol85:
#if 0
    case DW_LANG_Fortran77:	/* moved up top for now */
#endif
    case DW_LANG_Fortran90:
    case DW_LANG_Pascal83:
    case DW_LANG_Modula2:
    default:
      cu_language = language_unknown;
      break;
    }
  cu_language_defn = language_def (cu_language);
}

static void
record_minimal_symbol (name, address, ms_type, objfile)
     char *name;
     CORE_ADDR address;
     enum minimal_symbol_type ms_type;
     struct objfile *objfile;
{
  name = obsavestring (name, strlen (name), &objfile->symbol_obstack);
  prim_record_minimal_symbol (name, address, ms_type, objfile);
}

/* Converts a location description into gdb form.  */

static int
convert_locdesc (blk)
     struct dwarf_block *blk;
{
  /* FIXME : this is only a stub! */
  return 0;
}

/* Return the named attribute or NULL if not there.  */

static struct attribute *
dwarf_attr (die, name)
     struct die_info *die;
     unsigned int name;
{
  unsigned int i;
  struct attribute *spec = NULL;

  for (i = 0; i < die->num_attrs; ++i)
    {
      if (die->attrs[i].name == name)
	{
	  return &die->attrs[i];
	}
      if (die->attrs[i].name == DW_AT_specification
	  || die->attrs[i].name == DW_AT_abstract_origin)
	spec = &die->attrs[i];
    }
  if (spec)
    return dwarf_attr (follow_die_ref (DW_UNSND (spec)), name);
    
  return NULL;
}

/* Decode the line number information for the compilation unit whose
   line number info is at OFFSET in the .debug_line section.  */

struct filenames
{
  int num_files;
  struct fileinfo
  {
    char *name;
    unsigned int dir;
    unsigned int time;
    unsigned int size;
  }
  *files;
};

struct directories
{
  int num_dirs;
  char **dirs;
};

static void
dwarf_decode_lines (offset, abfd)
     unsigned int offset;
     bfd *abfd;
{
  char *line_ptr;
  struct line_head lh;
  struct cleanup *back_to;
  unsigned int i, bytes_read;
  char *cur_file, *cur_dir;
  unsigned char op_code, extended_op, adj_opcode;

#define FILE_ALLOC_CHUNK 5
#define DIR_ALLOC_CHUNK 5

  struct filenames files;
  struct directories dirs;

  /* state machine registers  */
  unsigned int address = 0;
  unsigned int file = 1;
  unsigned int line = 1;
  unsigned int column = 0;
  int is_stmt;			/* initialized below */
  int basic_block = 0;
  int beg_of_comp_unit = 0;	/* is this right? */
  int end_sequence = 0;

  files.num_files = 0;
  files.files = NULL;

  dirs.num_dirs = 0;
  dirs.dirs = NULL;

  line_ptr = dwarf_line_buffer + offset;

  /* read in the prologue */
  lh.total_length = read_4_bytes (abfd, line_ptr);
  line_ptr += 4;
  lh.version = read_2_bytes (abfd, line_ptr);
  line_ptr += 2;
  lh.prologue_length = read_4_bytes (abfd, line_ptr);
  line_ptr += 4;
  lh.minimum_instruction_length = read_1_byte (abfd, line_ptr);
  line_ptr += 1;
  lh.default_is_stmt = read_1_byte (abfd, line_ptr);
  is_stmt = lh.default_is_stmt;
  line_ptr += 1;
  lh.line_base = read_1_byte (abfd, line_ptr);
  line_ptr += 1;
  lh.line_range = read_1_byte (abfd, line_ptr);
  line_ptr += 1;
  lh.opcode_base = read_1_byte (abfd, line_ptr);
  line_ptr += 1;
  lh.standard_opcode_lengths = (unsigned char *)
    xmalloc (lh.opcode_base * sizeof (unsigned char));
  back_to = make_cleanup (free, lh.standard_opcode_lengths);

  lh.standard_opcode_lengths[0] = 1;
  for (i = 1; i < lh.opcode_base; ++i)
    {
      lh.standard_opcode_lengths[i] = read_1_byte (abfd, line_ptr);
      line_ptr += 1;
    }

  /* Read directory table  */
  while (cur_dir = read_string (abfd, line_ptr, &bytes_read))
    {
      line_ptr += bytes_read;
      if ((dirs.num_dirs % DIR_ALLOC_CHUNK) == 0)
	{
	  dirs.dirs = xrealloc (dirs.dirs,
	    (dirs.num_dirs + DIR_ALLOC_CHUNK) * sizeof (char *));
	}
      dirs.dirs[dirs.num_dirs++] = cur_dir;
    }
  line_ptr += bytes_read;

  /* Read file name table */
  while (cur_file = read_string (abfd, line_ptr, &bytes_read))
    {
      line_ptr += bytes_read;
      if ((files.num_files % FILE_ALLOC_CHUNK) == 0)
	{
	  files.files = xrealloc (files.files,
	    (files.num_files + FILE_ALLOC_CHUNK) * sizeof (struct fileinfo));
	}
      files.files[files.num_files].name = cur_file;
      files.files[files.num_files].dir = read_unsigned_leb128 (abfd,
					   line_ptr, &bytes_read);
      line_ptr += bytes_read;
      files.files[files.num_files].time = read_unsigned_leb128 (abfd,
					   line_ptr, &bytes_read);
      line_ptr += bytes_read;
      files.files[files.num_files].size = read_unsigned_leb128 (abfd,
					   line_ptr, &bytes_read);
      line_ptr += bytes_read;
      files.num_files++;
    }
  line_ptr += bytes_read;

  /* Decode the table. */
  if (lh.total_length - (lh.prologue_length + 4 + 2) >= 4)
    do
      {
	op_code = read_1_byte (abfd, line_ptr);
	line_ptr += 1;
	switch (op_code)
	  {
	  case DW_LNS_extended_op:
	    line_ptr += 1;	/* ignore length */
	    extended_op = read_1_byte (abfd, line_ptr);
	    line_ptr += 1;
	    switch (extended_op)
	      {
	      case DW_LNE_end_sequence:
		end_sequence = 1;
		record_line (current_subfile, line, address);
		return;		/* return! */
		break;
	      case DW_LNE_set_address:
		address = read_address (abfd, line_ptr);
		line_ptr += address_size;
		break;
	      case DW_LNE_define_file:
		cur_file = read_string (abfd, line_ptr, &bytes_read);
		line_ptr += bytes_read;
		if ((files.num_files % FILE_ALLOC_CHUNK) == 0)
		  {
		    files.files = xrealloc (files.files,
		      (files.num_files + FILE_ALLOC_CHUNK)
			* sizeof (struct fileinfo));
		  }
		files.files[files.num_files].name = cur_file;
		files.files[files.num_files].dir = read_unsigned_leb128 (
		  abfd, line_ptr, &bytes_read);
		line_ptr += bytes_read;
		files.files[files.num_files].time = read_unsigned_leb128 (abfd,
		  line_ptr, &bytes_read);
		line_ptr += bytes_read;
		files.files[files.num_files].size = read_unsigned_leb128 (abfd,
		  line_ptr, &bytes_read);
		line_ptr += bytes_read;
		break;
	      default:
		error ("Dwarf Error: Mangled .debug_line section.");
		return;
	      }
	    break;
	  case DW_LNS_copy:
	    record_line (current_subfile, line, address);
	    basic_block = 0;
	    break;
	  case DW_LNS_advance_pc:
	    address += lh.minimum_instruction_length
	      * read_unsigned_leb128 (abfd, line_ptr, &bytes_read);
	    line_ptr += bytes_read;
	    break;
	  case DW_LNS_advance_line:
	    line += read_signed_leb128 (abfd, line_ptr, &bytes_read);
	    line_ptr += bytes_read;
	    break;
	  case DW_LNS_set_file:
	    /* The file table is 0 based and the references are 1
	       based, thus  the subtraction of `1' at the end of the
	       next line */
	    file = read_unsigned_leb128 (abfd, line_ptr, &bytes_read) - 1;
	    start_subfile (files.files[file].name,
			   (files.files[file].dir ?
			     dirs.dirs[files.files[file].dir] : 0));
	    line_ptr += bytes_read;
	    break;
	  case DW_LNS_set_column:
	    column = read_unsigned_leb128 (abfd, line_ptr, &bytes_read);
	    line_ptr += bytes_read;
	    break;
	  case DW_LNS_negate_stmt:
	    is_stmt = (!is_stmt);
	    break;
	  case DW_LNS_set_basic_block:
	    basic_block = 1;
	    break;
	  case DW_LNS_const_add_pc:
	    address += (255 - lh.opcode_base) / lh.line_range;
	    break;
	  case DW_LNS_fixed_advance_pc:
	    address += read_2_bytes (abfd, line_ptr);
	    line_ptr += 2;
	    break;
	  default:		/* special operand */
	    adj_opcode = op_code - lh.opcode_base;
	    address += (adj_opcode / lh.line_range)
	      * lh.minimum_instruction_length;
	    line += lh.line_base + (adj_opcode % lh.line_range);
	    /* append row to matrix using current values */
	    record_line (current_subfile, line, address);
	    basic_block = 1;
	  }
      }
    while (1);
  do_cleanups (back_to);
}

/* Given a pointer to a DWARF information entry, figure out if we need
   to make a symbol table entry for it, and if so, create a new entry
   and return a pointer to it.  */

static struct symbol *
new_symbol (die, objfile)
     struct die_info *die;
     struct objfile *objfile;
{
  struct symbol *sym = NULL;
  struct attribute *attr = NULL;
  struct attribute *attr2 = NULL;
  CORE_ADDR addr;

  attr = dwarf_attr (die, DW_AT_name);
  if (attr)
    {
#if 0
      sym = (struct symbol *) obstack_alloc (&objfile->symbol_obstack,
					     sizeof (struct symbol));
#endif
      sym = (struct symbol *) xmalloc (sizeof (struct symbol));
      memset (sym, 0, sizeof (struct symbol));
#if 0
      SYMBOL_NAME (sym) = create_name (DW_STRING (attr),
				       &objfile->symbol_obstack);
#endif
      SYMBOL_NAME (sym) = strdup (DW_STRING (attr));
      /* default assumptions */
      SYMBOL_NAMESPACE (sym) = VAR_NAMESPACE;
      SYMBOL_CLASS (sym) = LOC_STATIC;
      SYMBOL_TYPE (sym) = die_type (die, objfile);

      /* If this symbol is from a C++ compilation, then attempt to
         cache the demangled form for future reference.  This is a
         typical time versus space tradeoff, that was decided in favor
         of time because it sped up C++ symbol lookups by a factor of
         about 20. */

      SYMBOL_LANGUAGE (sym) = cu_language;
      SYMBOL_INIT_DEMANGLED_NAME (sym, &objfile->symbol_obstack);
      switch (die->tag)
	{
	case DW_TAG_label:
	  attr = dwarf_attr (die, DW_AT_low_pc);
	  if (attr)
	    {
	      SYMBOL_VALUE_ADDRESS (sym) = DW_ADDR (attr);
	    }
	  SYMBOL_CLASS (sym) = LOC_LABEL;
	  break;
	case DW_TAG_subprogram:
	  attr = dwarf_attr (die, DW_AT_low_pc);
	  if (attr)
	    {
	      SYMBOL_VALUE_ADDRESS (sym) = DW_ADDR (attr);
	    }
	  SYMBOL_TYPE (sym) = make_function_type (die_type (die, objfile),
						  NULL);
	  SYMBOL_CLASS (sym) = LOC_BLOCK;
	  attr2 = dwarf_attr (die, DW_AT_external);
	  if (attr2 && (DW_UNSND (attr2) != 0))
	    {
	      add_symbol_to_list (sym, &global_symbols);
	    }
	  else
	    {
	      add_symbol_to_list (sym, list_in_scope);
	    }
	  break;
	case DW_TAG_variable:
	  attr = dwarf_attr (die, DW_AT_location);
	  if (attr)
	    {
	      attr2 = dwarf_attr (die, DW_AT_external);
	      if (attr2 && (DW_UNSND (attr2) != 0))
		{
		  SYMBOL_VALUE_ADDRESS (sym) =
		    decode_locdesc (DW_BLOCK (attr), objfile);
		  add_symbol_to_list (sym, &global_symbols);
		  SYMBOL_CLASS (sym) = LOC_STATIC;
		  SYMBOL_VALUE_ADDRESS (sym) += baseaddr;
		}
	      else
		{
		  SYMBOL_VALUE (sym) = addr =
		    decode_locdesc (DW_BLOCK (attr), objfile);
		  add_symbol_to_list (sym, list_in_scope);
		  if (isreg)
		    {
		      SYMBOL_CLASS (sym) = LOC_REGISTER;
		    }
		  else if (offreg)
		    {
		      SYMBOL_CLASS (sym) = LOC_LOCAL;
		    }
		  else
		    {
		      SYMBOL_CLASS (sym) = LOC_STATIC;
		      SYMBOL_VALUE_ADDRESS (sym) = addr + baseaddr;
		    }
		}
	    }
	  break;
	case DW_TAG_formal_parameter:
	  attr = dwarf_attr (die, DW_AT_location);
	  if (attr != NULL)
	    {
	      SYMBOL_VALUE (sym) = decode_locdesc (DW_BLOCK (attr), objfile);
	    }
	  add_symbol_to_list (sym, list_in_scope);
	  if (isreg)
	    {
	      SYMBOL_CLASS (sym) = LOC_REGPARM;
	    }
	  else
	    {
	      SYMBOL_CLASS (sym) = LOC_ARG;
	    }
	  break;
	case DW_TAG_unspecified_parameters:
	  /* From varargs functions; gdb doesn't seem to have any
	     interest in this information, so just ignore it for now.
	     (FIXME?) */
	  break;
	case DW_TAG_class_type:
	case DW_TAG_structure_type:
	case DW_TAG_union_type:
	case DW_TAG_enumeration_type:
	  SYMBOL_CLASS (sym) = LOC_TYPEDEF;
	  SYMBOL_NAMESPACE (sym) = STRUCT_NAMESPACE;
	  add_symbol_to_list (sym, list_in_scope);
	  break;
	case DW_TAG_typedef:
	  SYMBOL_CLASS (sym) = LOC_TYPEDEF;
	  SYMBOL_NAMESPACE (sym) = VAR_NAMESPACE;
	  add_symbol_to_list (sym, list_in_scope);
	  break;
	default:
	  /* Not a tag we recognize.  Hopefully we aren't processing
	     trash data, but since we must specifically ignore things
	     we don't recognize, there is nothing else we should do at
	     this point. */
	  break;
	}
    }
  return (sym);
}

/* Return the type of the die in question using its DW_AT_type attribute.  */

static struct type *
die_type (die, objfile)
     struct die_info *die;
     struct objfile *objfile;
{
  struct type *type;
  struct attribute *attr, *type_attr;
  struct die_info *type_die;
  unsigned int size = 0, encoding = 0, ref;

  type_attr = dwarf_attr (die, DW_AT_type);
  if (!type_attr)
    {
      type = dwarf_base_type (0, 0);
      return type;
    }
  else
    {
      ref = DW_UNSND (type_attr);
      type_die = follow_die_ref (ref);
      if (!type_die)
	{
	  error ("Dwarf Error: Cannot find referent at offset %d.", ref);
	  return NULL;
	}
    }
  type = tag_type_to_type (type_die, objfile);
  if (!type)
    {
      error ("Dwarf Error: Problem turning type die at offset into gdb type:");
      dump_die (type_die);
    }
  return type;
}

static struct type *
type_at_offset (offset, objfile)
     unsigned int offset;
     struct objfile *objfile;
{
  struct die_info *die;
  struct type *type;

  die = follow_die_ref (offset);
  if (!die)
    {
      error ("Dwarf Error: Cannot find type referent at offset %d.", offset);
      return NULL;
    }
  type = tag_type_to_type (die, objfile);
  return type;
}

static struct type *
tag_type_to_type (die, objfile)
     struct die_info *die;
     struct objfile *objfile;
{
  if (die->type)
    {
      return die->type;
    }
  else
    {
      read_type_die (die, objfile);
      if (!die->type)
	{
	  dump_die (die);
	  error ("Dwarf Error: Cannot find type of die:");
	}
      return die->type;
    }
}

static void
read_type_die (die, objfile)
     struct die_info *die;
     struct objfile *objfile;
{
  switch (die->tag)
    {
    case DW_TAG_class_type:
    case DW_TAG_structure_type:
    case DW_TAG_union_type:
      read_structure_scope (die, objfile);
      break;
    case DW_TAG_enumeration_type:
      read_enumeration (die, objfile);
      break;
    case DW_TAG_subroutine_type:
      read_subroutine_type (die, objfile);
      break;
    case DW_TAG_array_type:
      dwarf_read_array_type (die, objfile);
      break;
    case DW_TAG_pointer_type:
      read_tag_pointer_type (die, objfile);
      break;
    case DW_TAG_reference_type:
      read_tag_reference_type (die, objfile);
      break;
    case DW_TAG_const_type:
      read_tag_const_type (die, objfile);
      break;
    case DW_TAG_volatile_type:
      read_tag_volatile_type (die, objfile);
      break;
    case DW_TAG_string_type:
      read_tag_string_type (die, objfile);
      break;
    case DW_TAG_typedef:
      read_typedef (die, objfile);
      break;
    case DW_TAG_base_type:
      read_base_type (die, objfile);
      break;
    case DW_TAG_padding:
    case DW_TAG_compile_unit:
    case DW_TAG_subprogram:
    case DW_TAG_lexical_block:
    default:
      break;
    }
}

static struct type *
dwarf_base_type (encoding, size)
     int encoding;
     int size;
{
  /* FIXME - this should not produce a new (struct type *)
     every time.  It should cache base types.  */
  struct type *type;
  switch (encoding)
    {
    case DW_ATE_address:
      type = dwarf2_fundamental_type (current_objfile, FT_VOID);
      return type;
    case DW_ATE_boolean:
      type = dwarf2_fundamental_type (current_objfile, FT_BOOLEAN);
      return type;
    case DW_ATE_complex_float:
      if (size == 16)
	{
	  type = dwarf2_fundamental_type (current_objfile, FT_DBL_PREC_COMPLEX);
	}
      else
	{
	  type = dwarf2_fundamental_type (current_objfile, FT_COMPLEX);
	}
      return type;
    case DW_ATE_float:
      if (size == 8)
	{
	  type = dwarf2_fundamental_type (current_objfile, FT_DBL_PREC_FLOAT);
	}
      else
	{
	  type = dwarf2_fundamental_type (current_objfile, FT_FLOAT);
	}
      return type;
    case DW_ATE_signed:
      switch (size)
	{
	case 1:
	  type = dwarf2_fundamental_type (current_objfile, FT_SIGNED_CHAR);
	  break;
	case 2:
	  type = dwarf2_fundamental_type (current_objfile, FT_SIGNED_SHORT);
	  break;
	default:
	case 4:
	  type = dwarf2_fundamental_type (current_objfile, FT_SIGNED_INTEGER);
	  break;
	}
      return type;
    case DW_ATE_signed_char:
      type = dwarf2_fundamental_type (current_objfile, FT_SIGNED_CHAR);
      return type;
    case DW_ATE_unsigned:
      switch (size)
	{
	case 1:
	  type = dwarf2_fundamental_type (current_objfile, FT_UNSIGNED_CHAR);
	  break;
	case 2:
	  type = dwarf2_fundamental_type (current_objfile, FT_SIGNED_SHORT);
	  break;
	default:
	case 4:
	  type = dwarf2_fundamental_type (current_objfile, FT_UNSIGNED_INTEGER);
	  break;
	}
      return type;
    case DW_ATE_unsigned_char:
      type = dwarf2_fundamental_type (current_objfile, FT_UNSIGNED_CHAR);
      return type;
    default:
      type = dwarf2_fundamental_type (current_objfile, FT_SIGNED_INTEGER);
      return type;
    }
}

/* Given a pointer to a string and a pointer to an obstack, allocates
   a fresh copy of the string on the specified obstack.  */

static char *
create_name (name, obstackp)
     char *name;
     struct obstack *obstackp;
{
  int length;
  char *newname;

  length = strlen (name) + 1;
  newname = (char *) obstack_alloc (obstackp, length);
  strcpy (newname, name);
  return (newname);
}

struct die_info *
copy_die (old_die)
     struct die_info *old_die;
{
  struct die_info *new_die;
  int i, num_attrs;

  new_die = (struct die_info *) xmalloc (sizeof (struct die_info));
  memset (new_die, 0, sizeof (struct die_info));

  new_die->tag = old_die->tag;
  new_die->has_children = old_die->has_children;
  new_die->abbrev = old_die->abbrev;
  new_die->offset = old_die->offset;
  new_die->type = NULL;

  num_attrs = old_die->num_attrs;
  new_die->num_attrs = num_attrs;
  new_die->attrs = (struct attribute *)
    xmalloc (num_attrs * sizeof (struct attribute));

  for (i = 0; i < old_die->num_attrs; ++i)
    {
      new_die->attrs[i].name = old_die->attrs[i].name;
      new_die->attrs[i].form = old_die->attrs[i].form;
      new_die->attrs[i].u.addr = old_die->attrs[i].u.addr;
    }

  new_die->next = NULL;
  return new_die;
}

/* Return sibling of die, NULL if no sibling.  */

struct die_info *
sibling_die (die)
     struct die_info *die;
{
  struct die_info *new;
  int nesting_level = 0;

  if (!die->has_children)
    {
      if (die->next && (die->next->tag == 0))
	{
	  return NULL;
	}
      else
	{
	  return die->next;
	}
    }
  else
    {
      do
	{
	  if (die->has_children)
	    {
	      nesting_level++;
	    }
	  if (die->tag == 0)
	    {
	      nesting_level--;
	    }
	  die = die->next;
	}
      while (nesting_level);
      if (die && (die->tag == 0))
	{
	  return NULL;
	}
      else
	{
	  return die;
	}
    }
}

/* Convert a DIE tag into its string name.  */

static char *
dwarf_tag_name (tag)
     register unsigned tag;
{
  switch (tag)
    {
    case DW_TAG_padding:
      return "DW_TAG_padding";
    case DW_TAG_array_type:
      return "DW_TAG_array_type";
    case DW_TAG_class_type:
      return "DW_TAG_class_type";
    case DW_TAG_entry_point:
      return "DW_TAG_entry_point";
    case DW_TAG_enumeration_type:
      return "DW_TAG_enumeration_type";
    case DW_TAG_formal_parameter:
      return "DW_TAG_formal_parameter";
    case DW_TAG_imported_declaration:
      return "DW_TAG_imported_declaration";
    case DW_TAG_label:
      return "DW_TAG_label";
    case DW_TAG_lexical_block:
      return "DW_TAG_lexical_block";
    case DW_TAG_member:
      return "DW_TAG_member";
    case DW_TAG_pointer_type:
      return "DW_TAG_pointer_type";
    case DW_TAG_reference_type:
      return "DW_TAG_reference_type";
    case DW_TAG_compile_unit:
      return "DW_TAG_compile_unit";
    case DW_TAG_string_type:
      return "DW_TAG_string_type";
    case DW_TAG_structure_type:
      return "DW_TAG_structure_type";
    case DW_TAG_subroutine_type:
      return "DW_TAG_subroutine_type";
    case DW_TAG_typedef:
      return "DW_TAG_typedef";
    case DW_TAG_union_type:
      return "DW_TAG_union_type";
    case DW_TAG_unspecified_parameters:
      return "DW_TAG_unspecified_parameters";
    case DW_TAG_variant:
      return "DW_TAG_variant";
    case DW_TAG_common_block:
      return "DW_TAG_common_block";
    case DW_TAG_common_inclusion:
      return "DW_TAG_common_inclusion";
    case DW_TAG_inheritance:
      return "DW_TAG_inheritance";
    case DW_TAG_inlined_subroutine:
      return "DW_TAG_inlined_subroutine";
    case DW_TAG_module:
      return "DW_TAG_module";
    case DW_TAG_ptr_to_member_type:
      return "DW_TAG_ptr_to_member_type";
    case DW_TAG_set_type:
      return "DW_TAG_set_type";
    case DW_TAG_subrange_type:
      return "DW_TAG_subrange_type";
    case DW_TAG_with_stmt:
      return "DW_TAG_with_stmt";
    case DW_TAG_access_declaration:
      return "DW_TAG_access_declaration";
    case DW_TAG_base_type:
      return "DW_TAG_base_type";
    case DW_TAG_catch_block:
      return "DW_TAG_catch_block";
    case DW_TAG_const_type:
      return "DW_TAG_const_type";
    case DW_TAG_constant:
      return "DW_TAG_constant";
    case DW_TAG_enumerator:
      return "DW_TAG_enumerator";
    case DW_TAG_file_type:
      return "DW_TAG_file_type";
    case DW_TAG_friend:
      return "DW_TAG_friend";
    case DW_TAG_namelist:
      return "DW_TAG_namelist";
    case DW_TAG_namelist_item:
      return "DW_TAG_namelist_item";
    case DW_TAG_packed_type:
      return "DW_TAG_packed_type";
    case DW_TAG_subprogram:
      return "DW_TAG_subprogram";
    case DW_TAG_template_type_param:
      return "DW_TAG_template_type_param";
    case DW_TAG_template_value_param:
      return "DW_TAG_template_value_param";
    case DW_TAG_thrown_type:
      return "DW_TAG_thrown_type";
    case DW_TAG_try_block:
      return "DW_TAG_try_block";
    case DW_TAG_variant_part:
      return "DW_TAG_variant_part";
    case DW_TAG_variable:
      return "DW_TAG_variable";
    case DW_TAG_volatile_type:
      return "DW_TAG_volatile_type";
    case DW_TAG_MIPS_loop:
      return "DW_TAG_MIPS_loop";
    case DW_TAG_format_label:
      return "DW_TAG_format_label";
    case DW_TAG_function_template:
      return "DW_TAG_function_template";
    case DW_TAG_class_template:
      return "DW_TAG_class_template";
    default:
      return "DW_TAG_<unknown>";
    }
}

/* Convert a DWARF attribute code into its string name.  */

static char *
dwarf_attr_name (attr)
     register unsigned attr;
{
  switch (attr)
    {
    case DW_AT_sibling:
      return "DW_AT_sibling";
    case DW_AT_location:
      return "DW_AT_location";
    case DW_AT_name:
      return "DW_AT_name";
    case DW_AT_ordering:
      return "DW_AT_ordering";
    case DW_AT_subscr_data:
      return "DW_AT_subscr_data";
    case DW_AT_byte_size:
      return "DW_AT_byte_size";
    case DW_AT_bit_offset:
      return "DW_AT_bit_offset";
    case DW_AT_bit_size:
      return "DW_AT_bit_size";
    case DW_AT_element_list:
      return "DW_AT_element_list";
    case DW_AT_stmt_list:
      return "DW_AT_stmt_list";
    case DW_AT_low_pc:
      return "DW_AT_low_pc";
    case DW_AT_high_pc:
      return "DW_AT_high_pc";
    case DW_AT_language:
      return "DW_AT_language";
    case DW_AT_member:
      return "DW_AT_member";
    case DW_AT_discr:
      return "DW_AT_discr";
    case DW_AT_discr_value:
      return "DW_AT_discr_value";
    case DW_AT_visibility:
      return "DW_AT_visibility";
    case DW_AT_import:
      return "DW_AT_import";
    case DW_AT_string_length:
      return "DW_AT_string_length";
    case DW_AT_common_reference:
      return "DW_AT_common_reference";
    case DW_AT_comp_dir:
      return "DW_AT_comp_dir";
    case DW_AT_const_value:
      return "DW_AT_const_value";
    case DW_AT_containing_type:
      return "DW_AT_containing_type";
    case DW_AT_default_value:
      return "DW_AT_default_value";
    case DW_AT_inline:
      return "DW_AT_inline";
    case DW_AT_is_optional:
      return "DW_AT_is_optional";
    case DW_AT_lower_bound:
      return "DW_AT_lower_bound";
    case DW_AT_producer:
      return "DW_AT_producer";
    case DW_AT_prototyped:
      return "DW_AT_prototyped";
    case DW_AT_return_addr:
      return "DW_AT_return_addr";
    case DW_AT_start_scope:
      return "DW_AT_start_scope";
    case DW_AT_stride_size:
      return "DW_AT_stride_size";
    case DW_AT_upper_bound:
      return "DW_AT_upper_bound";
    case DW_AT_abstract_origin:
      return "DW_AT_abstract_origin";
    case DW_AT_accessibility:
      return "DW_AT_accessibility";
    case DW_AT_address_class:
      return "DW_AT_address_class";
    case DW_AT_artificial:
      return "DW_AT_artificial";
    case DW_AT_base_types:
      return "DW_AT_base_types";
    case DW_AT_calling_convention:
      return "DW_AT_calling_convention";
    case DW_AT_count:
      return "DW_AT_count";
    case DW_AT_data_member_location:
      return "DW_AT_data_member_location";
    case DW_AT_decl_column:
      return "DW_AT_decl_column";
    case DW_AT_decl_file:
      return "DW_AT_decl_file";
    case DW_AT_decl_line:
      return "DW_AT_decl_line";
    case DW_AT_declaration:
      return "DW_AT_declaration";
    case DW_AT_discr_list:
      return "DW_AT_discr_list";
    case DW_AT_encoding:
      return "DW_AT_encoding";
    case DW_AT_external:
      return "DW_AT_external";
    case DW_AT_frame_base:
      return "DW_AT_frame_base";
    case DW_AT_friend:
      return "DW_AT_friend";
    case DW_AT_identifier_case:
      return "DW_AT_identifier_case";
    case DW_AT_macro_info:
      return "DW_AT_macro_info";
    case DW_AT_namelist_items:
      return "DW_AT_namelist_items";
    case DW_AT_priority:
      return "DW_AT_priority";
    case DW_AT_segment:
      return "DW_AT_segment";
    case DW_AT_specification:
      return "DW_AT_specification";
    case DW_AT_static_link:
      return "DW_AT_static_link";
    case DW_AT_type:
      return "DW_AT_type";
    case DW_AT_use_location:
      return "DW_AT_use_location";
    case DW_AT_variable_parameter:
      return "DW_AT_variable_parameter";
    case DW_AT_virtuality:
      return "DW_AT_virtuality";
    case DW_AT_vtable_elem_location:
      return "DW_AT_vtable_elem_location";

#ifdef MIPS
    case DW_AT_MIPS_fde:
      return "DW_AT_MIPS_fde";
    case DW_AT_MIPS_loop_begin:
      return "DW_AT_MIPS_loop_begin";
    case DW_AT_MIPS_tail_loop_begin:
      return "DW_AT_MIPS_tail_loop_begin";
    case DW_AT_MIPS_epilog_begin:
      return "DW_AT_MIPS_epilog_begin";
    case DW_AT_MIPS_loop_unroll_factor:
      return "DW_AT_MIPS_loop_unroll_factor";
    case DW_AT_MIPS_software_pipeline_depth:
      return "DW_AT_MIPS_software_pipeline_depth";
    case DW_AT_MIPS_linkage_name:
      return "DW_AT_MIPS_linkage_name";
#endif

    case DW_AT_sf_names:
      return "DW_AT_sf_names";
    case DW_AT_src_info:
      return "DW_AT_src_info";
    case DW_AT_mac_info:
      return "DW_AT_mac_info";
    case DW_AT_src_coords:
      return "DW_AT_src_coords";
    case DW_AT_body_begin:
      return "DW_AT_body_begin";
    case DW_AT_body_end:
      return "DW_AT_body_end";
    default:
      return "DW_AT_<unknown>";
    }
}

/* Convert a DWARF value form code into its string name.  */

static char *
dwarf_form_name (form)
     register unsigned form;
{
  switch (form)
    {
    case DW_FORM_addr:
      return "DW_FORM_addr";
    case DW_FORM_block2:
      return "DW_FORM_block2";
    case DW_FORM_block4:
      return "DW_FORM_block4";
    case DW_FORM_data2:
      return "DW_FORM_data2";
    case DW_FORM_data4:
      return "DW_FORM_data4";
    case DW_FORM_data8:
      return "DW_FORM_data8";
    case DW_FORM_string:
      return "DW_FORM_string";
    case DW_FORM_block:
      return "DW_FORM_block";
    case DW_FORM_block1:
      return "DW_FORM_block1";
    case DW_FORM_data1:
      return "DW_FORM_data1";
    case DW_FORM_flag:
      return "DW_FORM_flag";
    case DW_FORM_sdata:
      return "DW_FORM_sdata";
    case DW_FORM_strp:
      return "DW_FORM_strp";
    case DW_FORM_udata:
      return "DW_FORM_udata";
    case DW_FORM_ref_addr:
      return "DW_FORM_ref_addr";
    case DW_FORM_ref1:
      return "DW_FORM_ref1";
    case DW_FORM_ref2:
      return "DW_FORM_ref2";
    case DW_FORM_ref4:
      return "DW_FORM_ref4";
    case DW_FORM_ref8:
      return "DW_FORM_ref8";
    case DW_FORM_ref_udata:
      return "DW_FORM_ref_udata";
    case DW_FORM_indirect:
      return "DW_FORM_indirect";
    default:
      return "DW_FORM_<unknown>";
    }
}

/* Convert a DWARF stack opcode into its string name.  */

static char *
dwarf_stack_op_name (op)
     register unsigned op;
{
  switch (op)
    {
    case DW_OP_addr:
      return "DW_OP_addr";
    case DW_OP_deref:
      return "DW_OP_deref";
    case DW_OP_const1u:
      return "DW_OP_const1u";
    case DW_OP_const1s:
      return "DW_OP_const1s";
    case DW_OP_const2u:
      return "DW_OP_const2u";
    case DW_OP_const2s:
      return "DW_OP_const2s";
    case DW_OP_const4u:
      return "DW_OP_const4u";
    case DW_OP_const4s:
      return "DW_OP_const4s";
    case DW_OP_const8u:
      return "DW_OP_const8u";
    case DW_OP_const8s:
      return "DW_OP_const8s";
    case DW_OP_constu:
      return "DW_OP_constu";
    case DW_OP_consts:
      return "DW_OP_consts";
    case DW_OP_dup:
      return "DW_OP_dup";
    case DW_OP_drop:
      return "DW_OP_drop";
    case DW_OP_over:
      return "DW_OP_over";
    case DW_OP_pick:
      return "DW_OP_pick";
    case DW_OP_swap:
      return "DW_OP_swap";
    case DW_OP_rot:
      return "DW_OP_rot";
    case DW_OP_xderef:
      return "DW_OP_xderef";
    case DW_OP_abs:
      return "DW_OP_abs";
    case DW_OP_and:
      return "DW_OP_and";
    case DW_OP_div:
      return "DW_OP_div";
    case DW_OP_minus:
      return "DW_OP_minus";
    case DW_OP_mod:
      return "DW_OP_mod";
    case DW_OP_mul:
      return "DW_OP_mul";
    case DW_OP_neg:
      return "DW_OP_neg";
    case DW_OP_not:
      return "DW_OP_not";
    case DW_OP_or:
      return "DW_OP_or";
    case DW_OP_plus:
      return "DW_OP_plus";
    case DW_OP_plus_uconst:
      return "DW_OP_plus_uconst";
    case DW_OP_shl:
      return "DW_OP_shl";
    case DW_OP_shr:
      return "DW_OP_shr";
    case DW_OP_shra:
      return "DW_OP_shra";
    case DW_OP_xor:
      return "DW_OP_xor";
    case DW_OP_bra:
      return "DW_OP_bra";
    case DW_OP_eq:
      return "DW_OP_eq";
    case DW_OP_ge:
      return "DW_OP_ge";
    case DW_OP_gt:
      return "DW_OP_gt";
    case DW_OP_le:
      return "DW_OP_le";
    case DW_OP_lt:
      return "DW_OP_lt";
    case DW_OP_ne:
      return "DW_OP_ne";
    case DW_OP_skip:
      return "DW_OP_skip";
    case DW_OP_lit0:
      return "DW_OP_lit0";
    case DW_OP_lit1:
      return "DW_OP_lit1";
    case DW_OP_lit2:
      return "DW_OP_lit2";
    case DW_OP_lit3:
      return "DW_OP_lit3";
    case DW_OP_lit4:
      return "DW_OP_lit4";
    case DW_OP_lit5:
      return "DW_OP_lit5";
    case DW_OP_lit6:
      return "DW_OP_lit6";
    case DW_OP_lit7:
      return "DW_OP_lit7";
    case DW_OP_lit8:
      return "DW_OP_lit8";
    case DW_OP_lit9:
      return "DW_OP_lit9";
    case DW_OP_lit10:
      return "DW_OP_lit10";
    case DW_OP_lit11:
      return "DW_OP_lit11";
    case DW_OP_lit12:
      return "DW_OP_lit12";
    case DW_OP_lit13:
      return "DW_OP_lit13";
    case DW_OP_lit14:
      return "DW_OP_lit14";
    case DW_OP_lit15:
      return "DW_OP_lit15";
    case DW_OP_lit16:
      return "DW_OP_lit16";
    case DW_OP_lit17:
      return "DW_OP_lit17";
    case DW_OP_lit18:
      return "DW_OP_lit18";
    case DW_OP_lit19:
      return "DW_OP_lit19";
    case DW_OP_lit20:
      return "DW_OP_lit20";
    case DW_OP_lit21:
      return "DW_OP_lit21";
    case DW_OP_lit22:
      return "DW_OP_lit22";
    case DW_OP_lit23:
      return "DW_OP_lit23";
    case DW_OP_lit24:
      return "DW_OP_lit24";
    case DW_OP_lit25:
      return "DW_OP_lit25";
    case DW_OP_lit26:
      return "DW_OP_lit26";
    case DW_OP_lit27:
      return "DW_OP_lit27";
    case DW_OP_lit28:
      return "DW_OP_lit28";
    case DW_OP_lit29:
      return "DW_OP_lit29";
    case DW_OP_lit30:
      return "DW_OP_lit30";
    case DW_OP_lit31:
      return "DW_OP_lit31";
    case DW_OP_reg0:
      return "DW_OP_reg0";
    case DW_OP_reg1:
      return "DW_OP_reg1";
    case DW_OP_reg2:
      return "DW_OP_reg2";
    case DW_OP_reg3:
      return "DW_OP_reg3";
    case DW_OP_reg4:
      return "DW_OP_reg4";
    case DW_OP_reg5:
      return "DW_OP_reg5";
    case DW_OP_reg6:
      return "DW_OP_reg6";
    case DW_OP_reg7:
      return "DW_OP_reg7";
    case DW_OP_reg8:
      return "DW_OP_reg8";
    case DW_OP_reg9:
      return "DW_OP_reg9";
    case DW_OP_reg10:
      return "DW_OP_reg10";
    case DW_OP_reg11:
      return "DW_OP_reg11";
    case DW_OP_reg12:
      return "DW_OP_reg12";
    case DW_OP_reg13:
      return "DW_OP_reg13";
    case DW_OP_reg14:
      return "DW_OP_reg14";
    case DW_OP_reg15:
      return "DW_OP_reg15";
    case DW_OP_reg16:
      return "DW_OP_reg16";
    case DW_OP_reg17:
      return "DW_OP_reg17";
    case DW_OP_reg18:
      return "DW_OP_reg18";
    case DW_OP_reg19:
      return "DW_OP_reg19";
    case DW_OP_reg20:
      return "DW_OP_reg20";
    case DW_OP_reg21:
      return "DW_OP_reg21";
    case DW_OP_reg22:
      return "DW_OP_reg22";
    case DW_OP_reg23:
      return "DW_OP_reg23";
    case DW_OP_reg24:
      return "DW_OP_reg24";
    case DW_OP_reg25:
      return "DW_OP_reg25";
    case DW_OP_reg26:
      return "DW_OP_reg26";
    case DW_OP_reg27:
      return "DW_OP_reg27";
    case DW_OP_reg28:
      return "DW_OP_reg28";
    case DW_OP_reg29:
      return "DW_OP_reg29";
    case DW_OP_reg30:
      return "DW_OP_reg30";
    case DW_OP_reg31:
      return "DW_OP_reg31";
    case DW_OP_breg0:
      return "DW_OP_breg0";
    case DW_OP_breg1:
      return "DW_OP_breg1";
    case DW_OP_breg2:
      return "DW_OP_breg2";
    case DW_OP_breg3:
      return "DW_OP_breg3";
    case DW_OP_breg4:
      return "DW_OP_breg4";
    case DW_OP_breg5:
      return "DW_OP_breg5";
    case DW_OP_breg6:
      return "DW_OP_breg6";
    case DW_OP_breg7:
      return "DW_OP_breg7";
    case DW_OP_breg8:
      return "DW_OP_breg8";
    case DW_OP_breg9:
      return "DW_OP_breg9";
    case DW_OP_breg10:
      return "DW_OP_breg10";
    case DW_OP_breg11:
      return "DW_OP_breg11";
    case DW_OP_breg12:
      return "DW_OP_breg12";
    case DW_OP_breg13:
      return "DW_OP_breg13";
    case DW_OP_breg14:
      return "DW_OP_breg14";
    case DW_OP_breg15:
      return "DW_OP_breg15";
    case DW_OP_breg16:
      return "DW_OP_breg16";
    case DW_OP_breg17:
      return "DW_OP_breg17";
    case DW_OP_breg18:
      return "DW_OP_breg18";
    case DW_OP_breg19:
      return "DW_OP_breg19";
    case DW_OP_breg20:
      return "DW_OP_breg20";
    case DW_OP_breg21:
      return "DW_OP_breg21";
    case DW_OP_breg22:
      return "DW_OP_breg22";
    case DW_OP_breg23:
      return "DW_OP_breg23";
    case DW_OP_breg24:
      return "DW_OP_breg24";
    case DW_OP_breg25:
      return "DW_OP_breg25";
    case DW_OP_breg26:
      return "DW_OP_breg26";
    case DW_OP_breg27:
      return "DW_OP_breg27";
    case DW_OP_breg28:
      return "DW_OP_breg28";
    case DW_OP_breg29:
      return "DW_OP_breg29";
    case DW_OP_breg30:
      return "DW_OP_breg30";
    case DW_OP_breg31:
      return "DW_OP_breg31";
    case DW_OP_regx:
      return "DW_OP_regx";
    case DW_OP_fbreg:
      return "DW_OP_fbreg";
    case DW_OP_bregx:
      return "DW_OP_bregx";
    case DW_OP_piece:
      return "DW_OP_piece";
    case DW_OP_deref_size:
      return "DW_OP_deref_size";
    case DW_OP_xderef_size:
      return "DW_OP_xderef_size";
    case DW_OP_nop:
      return "DW_OP_nop";
    default:
      return "OP_<unknown>";
    }
}

static char *
dwarf_bool_name (bool)
     unsigned bool;
{
  if (bool)
    return "TRUE";
  else
    return "FALSE";
}

/* Convert a DWARF type code into its string name.  */

static char *
dwarf_type_encoding_name (enc)
     register unsigned enc;
{
  switch (enc)
    {
    case DW_ATE_address:
      return "DW_ATE_address";
    case DW_ATE_boolean:
      return "DW_ATE_boolean";
    case DW_ATE_complex_float:
      return "DW_ATE_complex_float";
    case DW_ATE_float:
      return "DW_ATE_float";
    case DW_ATE_signed:
      return "DW_ATE_signed";
    case DW_ATE_signed_char:
      return "DW_ATE_signed_char";
    case DW_ATE_unsigned:
      return "DW_ATE_unsigned";
    case DW_ATE_unsigned_char:
      return "DW_ATE_unsigned_char";
    default:
      return "DW_ATE_<unknown>";
    }
}

/* Convert a DWARF call frame info operation to its string name. */

static char *
dwarf_cfi_name (cfi_opc)
     register unsigned cfi_opc;
{
  switch (cfi_opc)
    {
    case DW_CFA_advance_loc:
      return "DW_CFA_advance_loc";
    case DW_CFA_offset:
      return "DW_CFA_offset";
    case DW_CFA_restore:
      return "DW_CFA_restore";
    case DW_CFA_nop:
      return "DW_CFA_nop";
    case DW_CFA_set_loc:
      return "DW_CFA_set_loc";
    case DW_CFA_advance_loc1:
      return "DW_CFA_advance_loc1";
    case DW_CFA_advance_loc2:
      return "DW_CFA_advance_loc2";
    case DW_CFA_advance_loc4:
      return "DW_CFA_advance_loc4";
    case DW_CFA_offset_extended:
      return "DW_CFA_offset_extended";
    case DW_CFA_restore_extended:
      return "DW_CFA_restore_extended";
    case DW_CFA_undefined:
      return "DW_CFA_undefined";
    case DW_CFA_same_value:
      return "DW_CFA_same_value";
    case DW_CFA_register:
      return "DW_CFA_register";
    case DW_CFA_remember_state:
      return "DW_CFA_remember_state";
    case DW_CFA_restore_state:
      return "DW_CFA_restore_state";
    case DW_CFA_def_cfa:
      return "DW_CFA_def_cfa";
    case DW_CFA_def_cfa_register:
      return "DW_CFA_def_cfa_register";
    case DW_CFA_def_cfa_offset:
      return "DW_CFA_def_cfa_offset";
      /* SGI/MIPS specific */
    case DW_CFA_MIPS_advance_loc8:
      return "DW_CFA_MIPS_advance_loc8";
    default:
      return "DW_CFA_<unknown>";
    }
}

void
dump_die (die)
     struct die_info *die;
{
  int i;

  fprintf (stderr, "Die: %s (abbrev = %d, offset = %d)\n",
	   dwarf_tag_name (die->tag), die->abbrev, die->offset);
  fprintf (stderr, "\thas children: %s\n",
	   dwarf_bool_name (die->has_children));

  fprintf (stderr, "\tattributes:\n");
  for (i = 0; i < die->num_attrs; ++i)
    {
      fprintf (stderr, "\t\t%s (%s) ",
	       dwarf_attr_name (die->attrs[i].name),
	       dwarf_form_name (die->attrs[i].form));
      switch (die->attrs[i].form)
	{
	case DW_FORM_ref_addr:
	case DW_FORM_addr:
	  fprintf (stderr, sizeof (CORE_ADDR) > sizeof (long) ?
		   "address: 0x%LLx" : "address: 0x%x",
		   die->attrs[i].u.addr);
	  break;
	case DW_FORM_block2:
	case DW_FORM_block4:
	case DW_FORM_block:
	case DW_FORM_block1:
	  fprintf (stderr, "block: size %d",
		   die->attrs[i].u.blk->size);
	  break;
	case DW_FORM_data1:
	case DW_FORM_data2:
	case DW_FORM_data4:
	case DW_FORM_ref1:
	case DW_FORM_ref2:
	case DW_FORM_ref4:
	case DW_FORM_udata:
	case DW_FORM_sdata:
	  fprintf (stderr, "constant: %d", die->attrs[i].u.unsnd);
	  break;
	case DW_FORM_string:
	  fprintf (stderr, "string: \"%s\"", die->attrs[i].u.str);
	  break;
	case DW_FORM_flag:
	  if (die->attrs[i].u.unsnd)
	    fprintf (stderr, "flag: TRUE");
	  else
	    fprintf (stderr, "flag: FALSE");
	  break;
	case DW_FORM_strp:	/* we do not support separate string
				   section yet */
	case DW_FORM_indirect:	/* we do not handle indirect yet */
	case DW_FORM_data8:	/* we do not have 64 bit quantities */
	  error ("Dwarf Error: Unsupported attribute form: %d.",
		 die->attrs[i].form);
	}
      fprintf (stderr, "\n");
    }
}

void
dump_die_list (die)
     struct die_info *die;
{
  while (die)
    {
      dump_die (die);
      die = die->next;
    }
}

void
store_in_ref_table (offset, die)
     unsigned int offset;
     struct die_info *die;
{
  int h;
  struct die_info *old;

  h = (offset % REF_HASH_SIZE);
  old = die_ref_table[h];
  die->next_ref = old;
  die_ref_table[h] = die;
}

struct die_info *
follow_die_ref (offset)
     unsigned int offset;
{
  struct die_info *die;
  int h;

  h = (offset % REF_HASH_SIZE);
  die = die_ref_table[h];
  while (die)
    {
      if (die->offset == offset)
	{
	  return die;
	}
      die = die->next_ref;
    }
  return NULL;
}

static struct type *
dwarf2_fundamental_type (objfile, typeid)
     struct objfile *objfile;
     int typeid;
{
  if (typeid < 0 || typeid >= FT_NUM_MEMBERS)
    {
      error ("Dwarf Error: internal error - invalid fundamental type id %d.",
	     typeid);
    }

  /* Look for this particular type in the fundamental type vector.  If
     one is not found, create and install one appropriate for the
     current language and the current target machine. */

  if (ftypes[typeid] == NULL)
    {
      ftypes[typeid] = cu_language_defn->la_fund_type (objfile, typeid);
    }

  return (ftypes[typeid]);
}

/* Decode simple location descriptions.
   There are three cases:
       An address: return the address.
       An address relative to frame pointer: return the offset.
       A register: return register number and set isreg to true.
       A constant followed by plus: return the constant.  */

static CORE_ADDR
decode_locdesc (blk, objfile)
     struct dwarf_block *blk;
     struct objfile *objfile;
{
  int i, snd;
  int size = blk->size;
  char *data = blk->data;
  unsigned int bytes_read, unsnd;
  unsigned char op;
  union
    {
      CORE_ADDR addr;
      char bytes[sizeof (CORE_ADDR)];
    }
  u;

  i = 0;
  isreg = 0;
  offreg = 0;

  /* FIXME: handle more general forms of location descriptors.  */
  while (i < size)
    {
      op = data[i++];
      switch (op)
	{
	case DW_OP_reg0:
	  isreg = 1;
	  return 0;
	case DW_OP_reg1:
	  isreg = 1;
	  return 1;
	case DW_OP_reg2:
	  isreg = 1;
	  return 2;
	case DW_OP_reg3:
	  isreg = 1;
	  return 3;
	case DW_OP_reg4:
	  isreg = 1;
	  return 4;
	case DW_OP_reg5:
	  isreg = 1;
	  return 5;
	case DW_OP_reg6:
	  isreg = 1;
	  return 6;
	case DW_OP_reg7:
	  isreg = 1;
	  return 7;
	case DW_OP_reg8:
	  isreg = 1;
	  return 8;
	case DW_OP_reg9:
	  isreg = 1;
	  return 9;
	case DW_OP_reg10:
	  isreg = 1;
	  return 10;
	case DW_OP_reg11:
	  isreg = 1;
	  return 11;
	case DW_OP_reg12:
	  isreg = 1;
	  return 12;
	case DW_OP_reg13:
	  isreg = 1;
	  return 13;
	case DW_OP_reg14:
	  isreg = 1;
	  return 14;
	case DW_OP_reg15:
	  isreg = 1;
	  return 15;
	case DW_OP_reg16:
	  isreg = 1;
	  return 16;
	case DW_OP_reg17:
	  isreg = 1;
	  return 17;
	case DW_OP_reg18:
	  isreg = 1;
	  return 18;
	case DW_OP_reg19:
	  isreg = 1;
	  return 19;
	case DW_OP_reg20:
	  isreg = 1;
	  return 20;
	case DW_OP_reg21:
	  isreg = 1;
	  return 21;
	case DW_OP_reg22:
	  isreg = 1;
	  return 22;
	case DW_OP_reg23:
	  isreg = 1;
	  return 23;
	case DW_OP_reg24:
	  isreg = 1;
	  return 24;
	case DW_OP_reg25:
	  isreg = 1;
	  return 25;
	case DW_OP_reg26:
	  isreg = 1;
	  return 26;
	case DW_OP_reg27:
	  isreg = 1;
	  return 27;
	case DW_OP_reg28:
	  isreg = 1;
	  return 28;
	case DW_OP_reg29:
	  isreg = 1;
	  return 29;
	case DW_OP_reg30:
	  isreg = 1;
	  return 30;
	case DW_OP_reg31:
	  isreg = 1;
	  return 31;

	case DW_OP_regx:
	  isreg = 1;
	  unsnd = read_unsigned_leb128 (NULL, (data + i), &bytes_read);
	  i += bytes_read;
#if defined(HARRIS_TARGET) && defined(_M88K)
	  /* The Harris 88110 gdb ports have long kept their special reg
	     numbers between their gp-regs and their x-regs.  This is
	     not how our dwarf is generated.  Punt. */
	  return unsnd + 6;
#else
	  return unsnd;
#endif

	case DW_OP_fbreg:
	case DW_OP_breg31:
	  offreg = 1;
	  snd = read_signed_leb128 (NULL, (data + i), &bytes_read);
	  i += bytes_read;
	  return snd;

	case DW_OP_addr:
	  isreg = 0;
	  return read_address (objfile->obfd, &data[i]);

	case DW_OP_constu:
	  unsnd = read_unsigned_leb128 (NULL, (data + i), &bytes_read);
	  i += bytes_read;
	  break;

	case DW_OP_plus:
	  return unsnd;

	}
    }
  return 0;
}

/* memory allocation interface */

static struct type *
dwarf_alloc_type (objfile)
     struct objfile *objfile;
{
  struct type *type;

  type = (struct type *) xmalloc (sizeof (struct type));
  memset (type, 0, sizeof (struct type));

#if 0
  type = alloc_type (objfile);
#endif

  return (type);
}

static struct abbrev_info *
dwarf_alloc_abbrev ()
{
  struct abbrev_info *abbrev;

  abbrev = xmalloc (sizeof (struct abbrev_info));
  memset (abbrev, 0, sizeof (struct abbrev_info));
  return (abbrev);
}

static struct dwarf_block *
dwarf_alloc_block ()
{
  struct dwarf_block *blk;

  blk = (struct dwarf_block *) xmalloc (sizeof (struct dwarf_block));
  return (blk);
}

static struct die_info *
dwarf_alloc_die ()
{
  struct die_info *die;

  die = (struct die_info *) xmalloc (sizeof (struct die_info));
  memset (die, 0, sizeof (struct die_info));
  return (die);
}
