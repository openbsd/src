/* ld.h -

   Copyright (C) 1991, 1993, 1994, 1995 Free Software Foundation, Inc.

   This file is part of GLD, the Gnu Linker.

   GLD is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   GLD is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GLD; see the file COPYING.  If not, write to
   the Free Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifndef LD_H
#define LD_H

/* Look in this environment name for the linker to pretend to be */
#define EMULATION_ENVIRON "LDEMULATION"
/* If in there look for the strings: */

/* Look in this variable for a target format */
#define TARGET_ENVIRON "GNUTARGET"

/* Extra information we hold on sections */
typedef struct  user_section_struct
{
  /* Pointer to the section where this data will go */
  struct lang_input_statement_struct *file;
} section_userdata_type;


#define get_userdata(x) ((x)->userdata)

#define BYTE_SIZE	(1)
#define SHORT_SIZE	(2)
#define LONG_SIZE	(4)
#define QUAD_SIZE	(8)

/* ALIGN macro changed to ALIGN_N to avoid	*/
/* conflict in /usr/include/machine/machparam.h */
/* WARNING: If THIS is a 64 bit address and BOUNDARY is a 32 bit int,
   you must coerce boundary to the same type as THIS.
   ??? Is there a portable way to avoid this.  */
#define ALIGN_N(this, boundary) \
  ((( (this) + ((boundary) -1)) & (~((boundary)-1))))

typedef struct
{
  /* 1 => assign space to common symbols even if `relocatable_output'.  */
  boolean force_common_definition;
  boolean relax;

  /* Name of runtime interpreter to invoke.  */
  char *interpreter;

  /* Name to give runtime libary from the -soname argument.  */
  char *soname;

  /* Runtime library search path from the -rpath argument.  */
  char *rpath;

  /* Link time runtime library search path from the -rpath-link
     argument.  */
  char *rpath_link;

  /* Big or little endian as set on command line.  */
  enum { ENDIAN_UNSET = 0, ENDIAN_BIG, ENDIAN_LITTLE } endian;

  /* If true, export all symbols in the dynamic symbol table of an ELF
     executable.  */
  boolean export_dynamic;

  /* If true, build MIPS embedded PIC relocation tables in the output
     file.  */
  boolean embedded_relocs;
} args_type;

extern args_type command_line;

typedef int token_code_type;

typedef struct 
{
  bfd_size_type specified_data_size;
  boolean magic_demand_paged;
  boolean make_executable;

  /* If true, request BFD to use the traditional format.  */
  boolean traditional_format;

  /* If true, doing a dynamic link.  */
  boolean dynamic_link;

  /* If true, build constructors.  */
  boolean build_constructors;

  /* If true, warn about any constructors.  */
  boolean warn_constructors;

  /* If true, warn about merging common symbols with others.  */
  boolean warn_common;

  /* If true, only warn once about a particular undefined symbol.  */
  boolean warn_once;

  boolean sort_common;

  boolean text_read_only;

  char *map_filename;
  FILE *map_file;

  boolean stats;

  int split_by_reloc;
  boolean split_by_file;
} ld_config_type;

extern ld_config_type config;

typedef enum
{
  lang_first_phase_enum,
  lang_allocating_phase_enum,
  lang_final_phase_enum
} lang_phase_type;

extern boolean had_script;
extern boolean force_make_executable;

/* Non-zero if we are processing a --defsym from the command line.  */
extern int parsing_defsym;

extern int yyparse PARAMS ((void));

#endif
