/* objdump.c -- dump information about an object file.
   Copyright (C) 1990, 1991 Free Software Foundation, Inc.

This file is part of BFD, the Binary File Diddler.

BFD is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

BFD is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with BFD; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

/*
 * Until there is other documentation, refer to the manual page dump(1) in
 * the system 5 program's reference manual
 */

#include "bfd.h"
#include "sysdep.h"
#include "getopt.h"
#include <stdio.h>
#include <ctype.h>

#define	ELF_STAB_DISPLAY	/* This code works, but uses internal
				   bfd and elf stuff.  Flip this define
				   off if you need to just use generic
				   BFD interfaces.  */

#ifdef	ELF_STAB_DISPLAY
/* Internal headers for the ELF .stab-dump code - sorry.  */
#define	BYTES_IN_WORD	32
#include "aout/aout64.h"
#include "elf/internal.h"
extern Elf_Internal_Shdr *bfd_elf_find_section();
#endif	/* ELF_STAB_DISPLAY */

char *xmalloc ();

char *default_target = NULL;	/* default at runtime */

extern *program_version;
char *program_name = NULL;

int show_version = 0;		/* show the version number */
int dump_section_contents;	/* -s */
int dump_section_headers;	/* -h */
boolean dump_file_header;	/* -f */
int dump_symtab;		/* -t */
int dump_reloc_info;		/* -r */
int dump_ar_hdrs;		/* -a */
int with_line_numbers;		/* -l */
int dump_stab_section_info;	/* -stabs */
boolean disassemble;		/* -d */
boolean info;			/* -i */
char *only;

PROTO (void, display_file, (char *filename, char *target));
PROTO (void, dump_data, (bfd * abfd));
PROTO (void, dump_relocs, (bfd * abfd));
PROTO (void, dump_symbols, (bfd * abfd));
PROTO (void, print_arelt_descr, (FILE *, bfd * abfd, boolean verbose));







char *machine = (char *) NULL;
asymbol **syms;
asymbol **syms2;


unsigned int storage;

unsigned int symcount = 0;

void
usage ()
{
  fprintf (stderr,
	 "usage: %s [-ahifdrtxsl] [-m machine] [-j section_name] obj ...\n",
	   program_name);
  exit (1);
}

static struct option long_options[]=
{
  {"syms", no_argument, &dump_symtab, 1},
  {"reloc", no_argument, &dump_reloc_info, 1},
  {"header", no_argument, &dump_section_headers, 1},
  {"version", no_argument, &show_version,    1},
#ifdef	ELF_STAB_DISPLAY
  {"stabs", no_argument, &dump_stab_section_info, 1},
#endif
  {0, no_argument, 0, 0}};


static void
dump_headers (abfd)
     bfd *abfd;
{
  asection *section;

  for (section = abfd->sections;
       section != (asection *) NULL;
       section = section->next)
    {
      char *comma = "";

#define PF(x,y) \
	if (section->flags & x) {  printf("%s%s",comma,y); comma = ", "; }


      printf ("SECTION %d [%s]\t: size %08x",
	      section->index,
	      section->name,
	      (unsigned) bfd_get_section_size_before_reloc (section));
      printf (" vma ");
      printf_vma (section->vma);
      printf (" align 2**%u\n ",
	      section->alignment_power);
      PF (SEC_ALLOC, "ALLOC");
      PF (SEC_CONSTRUCTOR, "CONSTRUCTOR");
      PF (SEC_CONSTRUCTOR_TEXT, "CONSTRUCTOR TEXT");
      PF (SEC_CONSTRUCTOR_DATA, "CONSTRUCTOR DATA");
      PF (SEC_CONSTRUCTOR_BSS, "CONSTRUCTOR BSS");
      PF (SEC_LOAD, "LOAD");
      PF (SEC_RELOC, "RELOC");
#ifdef SEC_BALIGN
      PF (SEC_BALIGN, "BALIGN");
#endif
      PF (SEC_READONLY, "READONLY");
      PF (SEC_CODE, "CODE");
      PF (SEC_DATA, "DATA");
      PF (SEC_ROM, "ROM");
      printf ("\n");
#undef PF
    }
}

static asymbol **
DEFUN (slurp_symtab, (abfd),
       bfd * abfd)
{
  asymbol **sy = (asymbol **) NULL;

  if (!(bfd_get_file_flags (abfd) & HAS_SYMS))
    {
      (void) printf ("No symbols in \"%s\".\n", bfd_get_filename (abfd));
      return (NULL);
    }

  storage = get_symtab_upper_bound (abfd);
  if (storage)
    {
      sy = (asymbol **) malloc (storage);
      if (sy == NULL)
	{
	  fprintf (stderr, "%s: out of memory.\n", program_name);
	  exit (1);
	}
    }
  symcount = bfd_canonicalize_symtab (abfd, sy);
  return sy;
}

/* Sort symbols into value order */
static int 
comp (ap, bp)
     PTR ap;
     PTR bp;
{
  asymbol *a = *(asymbol **)ap;
  asymbol *b = *(asymbol **)bp;
  int diff;

  if (a->name == (char *) NULL || (a->flags & (BSF_DEBUGGING)))
    a->the_bfd = 0;
  if (b->name == (char *) NULL || (b->flags & (BSF_DEBUGGING)))
    b->the_bfd = 0;

  diff = a->the_bfd - b->the_bfd;
  if (diff)
    {
      return -diff;
    }
  diff = a->value - b->value;
  if (diff)
    {
      return diff;
    }
  return a->section - b->section;
}

/* Print the supplied address symbolically if possible */
void
print_address (vma, stream)
     bfd_vma vma;
     FILE *stream;
{
  /* Perform a binary search looking for the closest symbol to
     the required value */

  unsigned int min = 0;
  unsigned int max = symcount;

  unsigned int thisplace = 1;
  unsigned int oldthisplace;

  int vardiff;

  if (symcount == 0)
    {
      fprintf_vma (stream, vma);
    }
  else
    {
      while (true)
	{
	  oldthisplace = thisplace;
	  thisplace = (max + min) / 2;
	  if (thisplace == oldthisplace)
	    break;
	  vardiff = syms[thisplace]->value - vma;

	  if (vardiff)
	    {
	      if (vardiff > 0)
		{
		  max = thisplace;
		}
	      else
		{
		  min = thisplace;
		}
	    }
	  else
	    {
	      /* Totally awesome! the exact right symbol */
	      CONST char *match_name = syms[thisplace]->name;
	      int sym_len = strlen (match_name);

	      /* Avoid "filename.o" as a match */
	      if (sym_len > 2
		  && match_name[sym_len - 2] == '.'
		  && match_name[sym_len - 1] == 'o'
		  && thisplace + 1 < symcount
		  && syms[thisplace + 1]->value == vma)
		match_name = syms[thisplace + 1]->name;
	      /* Totally awesome! the exact right symbol */
	      fprintf_vma (stream, vma);
	      fprintf (stream, " (%s+)0000", syms[thisplace]->name);
	      return;
	    }
	}
      /* We've run out of places to look, print the symbol before this one
         see if this or the symbol before describes this location the best */

      if (thisplace != 0)
	{
	  if (syms[thisplace - 1]->value - vma >
	      syms[thisplace]->value - vma)
	    {
	      /* Previous symbol is in correct section and is closer */
	      thisplace--;
	    }
	}

      fprintf_vma (stream, vma);
      if (syms[thisplace]->value > vma)
	{
	  fprintf (stream, " (%s-)", syms[thisplace]->name);
	  fprintf (stream, "%04x", syms[thisplace]->value - vma);
	}
      else
	{
	  fprintf (stream, " (%s+)", syms[thisplace]->name);
	  fprintf (stream, "%04x", vma - syms[thisplace]->value);
	}
    }
}

void
disassemble_data (abfd)
     bfd *abfd;
{
  bfd_byte *data = NULL;
  bfd_arch_info_type *info;
  bfd_size_type datasize = 0;
  bfd_size_type i;
  unsigned int (*print) ()= 0;
  unsigned int print_insn_m68k ();
  unsigned int print_insn_a29k ();
  unsigned int print_insn_z8k ();
  unsigned int print_insn_i960 ();
  unsigned int print_insn_sparc ();
  unsigned int print_insn_i386 ();
  unsigned int print_insn_h8300 ();
  enum bfd_architecture a;

  asection *section;

  /* Replace symbol section relative values with abs values */
  boolean done_dot = false;

  for (i = 0; i < symcount; i++)
    {
      syms[i]->value += syms[i]->section->vma;
    }

  /* We keep a copy of the symbols in the original order */
  syms2 = slurp_symtab (abfd);

  /* Sort the symbols into section and symbol order */
  (void) qsort (syms, symcount, sizeof (asymbol *), comp);

  /* Find the first useless symbol */
  {
    unsigned int i;

    for (i = 0; i < symcount; i++)
      {
	if (syms[i]->the_bfd == 0)
	  {
	    symcount = i;
	    break;
	  }
      }
  }


  if (machine != (char *) NULL)
    {
      info = bfd_scan_arch (machine);
      if (info == 0)
	{
	  fprintf (stderr, "%s: Can't use supplied machine %s\n",
		   program_name,
		   machine);
	  exit (1);
	}
      abfd->arch_info = info;
    }

  /* See if we can disassemble using bfd */

  if (abfd->arch_info->disassemble)
    {
      print = abfd->arch_info->disassemble;
    }
  else
    {
      a = bfd_get_arch (abfd);
      switch (a)
	{
#ifdef SELECT_ARCHITECTURES
	case SELECT_ARCHITECTURES:
	  print = PRINT_INSN;
	  break;
#else
	case bfd_arch_sparc:
	  print = print_insn_sparc;
	  break;
	case bfd_arch_i386:
	  print = print_insn_i386;
	  break;
	case bfd_arch_m68k:
	  print = print_insn_m68k;
	  break;
	case bfd_arch_a29k:
	  print = print_insn_a29k;
	  break;
	case bfd_arch_i960:
	  print = print_insn_i960;
	  break;
	 case bfd_arch_z8k:
	  print = print_insn_z8k;
	  break;
#endif
	default:
	  fprintf (stderr, "%s: Can't disassemble for architecture %s\n",
		   program_name,
		   bfd_printable_arch_mach (bfd_get_arch (abfd), 0));
	  exit (1);
	}

    }

  for (section = abfd->sections;
       section != (asection *) NULL;
       section = section->next)
    {

      if ((section->flags & SEC_LOAD)
	  && (only == (char *) NULL || strcmp (only, section->name) == 0))
	{
	  printf ("Disassembly of section %s:\n", section->name);

	  if (bfd_get_section_size_before_reloc (section) == 0)
	    continue;

	  data = (bfd_byte *) malloc (bfd_get_section_size_before_reloc (section));

	  if (data == (bfd_byte *) NULL)
	    {
	      fprintf (stderr, "%s: memory exhausted.\n", program_name);
	      exit (1);
	    }
	  datasize = bfd_get_section_size_before_reloc (section);

	  bfd_get_section_contents (abfd, section, data, 0, bfd_get_section_size_before_reloc (section));

	  i = 0;
	  while (i < bfd_get_section_size_before_reloc (section))
	    {
	      if (data[i] == 0 && data[i + 1] == 0 && data[i + 2] == 0 &&
		  data[i + 3] == 0)
		{
		  if (done_dot == false)
		    {
		      printf ("...\n");
		      done_dot = true;
		    }
		  i += 4;
		}
	      else
		{
		  done_dot = false;
		  if (with_line_numbers)
		    {
		      static prevline;
		      CONST char *filename;
		      CONST char *functionname;
		      unsigned int line;

		      bfd_find_nearest_line (abfd,
					     section,
					     syms,
					     section->vma + i,
					     &filename,
					     &functionname,
					     &line);

		      if (filename && functionname && line && line != prevline)
			{
			  printf ("%s:%u\n", filename, line);
			  prevline = line;
			}
		    }
		  print_address (section->vma + i, stdout);
		  printf (" ");

		  i += print (section->vma + i,
			      data + i,
			      stdout);
		  putchar ('\n');
		}
	    }
	  free (data);
	}
    }
}

#ifdef	ELF_STAB_DISPLAY

/* Define a table of stab values and print-strings.  We wish the initializer
   could be a direct-mapped table, but instead we build one the first
   time we need it.  */

#define	STAB_STRING_LENGTH	6

char stab_name[256][STAB_STRING_LENGTH];

struct stab_print {
  int value;
  char string[STAB_STRING_LENGTH];
};

struct stab_print stab_print[] = {
#define __define_stab(NAME, CODE, STRING) {CODE, STRING},
#include "aout/stab.def"
#undef __define_stab
  {0, 0}
};

void dump_elf_stabs_1 ();

/* This is a kludge for dumping the stabs section from an ELF file that
   uses Sun stabs encoding.  It has to use some hooks into BFD because
   string table sections are not normally visible to BFD callers.  */

void
dump_elf_stabs (abfd)
     bfd *abfd;
{
  int i;

  /* Initialize stab name array if first time.  */
  if (stab_name[0][0] == 0) 
    {
      /* Fill in numeric values for all possible strings.  */
      for (i = 0; i < 256; i++)
	{
	  sprintf (stab_name[i], "%d", i);
	}
      for (i = 0; stab_print[i].string[0]; i++)
	strcpy (stab_name[stab_print[i].value], stab_print[i].string);
    }

  if (0 != strncmp ("elf", abfd->xvec->name, 3))
    {
      fprintf (stderr, "%s: %s is not in ELF format.\n", program_name,
	       abfd->filename);
      return;
    }

  dump_elf_stabs_1 (abfd, ".stab", ".stabstr");
  dump_elf_stabs_1 (abfd, ".stab.excl", ".stab.exclstr");
  dump_elf_stabs_1 (abfd, ".stab.index", ".stab.indexstr");
}

void
dump_elf_stabs_1 (abfd, name1, name2)
     bfd *abfd;
     char *name1;		/* Section name of .stab */
     char *name2;		/* Section name of its string section */
{
  Elf_Internal_Shdr *stab_hdr, *stabstr_hdr;
  char *strtab;
  struct internal_nlist *stabs, *stabs_end;
  int i;
  unsigned file_string_table_offset, next_file_string_table_offset;

  stab_hdr = bfd_elf_find_section (abfd, name1);
  if (0 == stab_hdr)
    {
      printf ("Contents of %s section:  none.\n\n", name1);
      return;
    }

  stabstr_hdr = bfd_elf_find_section (abfd, name2);
  if (0 == stabstr_hdr)
    {
      fprintf (stderr, "%s: %s has no %s section.\n", program_name,
	       abfd->filename, name2);
      return;
    }

  stabs  = (struct internal_nlist *) xmalloc (stab_hdr   ->sh_size);
  strtab = (char *)		     xmalloc (stabstr_hdr->sh_size);
  stabs_end = (struct internal_nlist *) (stab_hdr->sh_size + (char *)stabs);
  
  if (bfd_seek (abfd, stab_hdr->sh_offset, L_SET) < 0 ||
      stab_hdr->sh_size != bfd_read ((PTR)stabs, stab_hdr->sh_size, 1, abfd))
    {
      fprintf (stderr, "%s: reading %s section of %s failed.\n",
	       program_name, name1, 
	       abfd->filename);
      return;
    }

  if (bfd_seek (abfd, stabstr_hdr->sh_offset, L_SET) < 0 ||
      stabstr_hdr->sh_size != bfd_read ((PTR)strtab, stabstr_hdr->sh_size,
					1, abfd))
    {
      fprintf (stderr, "%s: reading %s section of %s failed.\n",
	       program_name, name2,
	       abfd->filename);
      return;
    }

#define SWAP_SYMBOL(symp, abfd) \
  { \
    (symp)->n_strx = bfd_h_get_32(abfd,			\
				(unsigned char *)&(symp)->n_strx);	\
    (symp)->n_desc = bfd_h_get_16 (abfd,			\
				(unsigned char *)&(symp)->n_desc);  	\
    (symp)->n_value = bfd_h_get_32 (abfd,			\
				(unsigned char *)&(symp)->n_value); 	\
  }

  printf ("Contents of %s section:\n\n", name1);
  printf ("Symnum n_type n_othr n_desc n_value  n_strx String\n");

  file_string_table_offset = 0;
  next_file_string_table_offset = 0;

  /* Loop through all symbols and print them.

     We start the index at -1 because there is a dummy symbol on
     the front of Sun's stabs-in-elf sections.  */

  for (i = -1; stabs < stabs_end; stabs++, i++)
    {
      SWAP_SYMBOL (stabs, abfd);
      printf ("\n%-6d %-6s %-6d %-6d %08x %-6d", i,
	      stab_name [stabs->n_type],
	      stabs->n_other, stabs->n_desc, stabs->n_value,
	      stabs->n_strx);

      /* Symbols with type == 0 (N_UNDF) specify the length of the
	 string table associated with this file.  We use that info
	 to know how to relocate the *next* file's string table indices.  */

      if (stabs->n_type == N_UNDF)
	{
	  file_string_table_offset = next_file_string_table_offset;
	  next_file_string_table_offset += stabs->n_value;
	}

      /* Now, using the possibly updated string table offset, print the
	 string (if any) associated with this symbol.  */

      if ((stabs->n_strx + file_string_table_offset) < stabstr_hdr->sh_size)
	printf (" %s", &strtab[stabs->n_strx + file_string_table_offset]);
      else
        printf (" *");
    }
  printf ("\n\n");
}
#endif	/* ELF_STAB_DISPLAY */

display_bfd (abfd)
     bfd *abfd;
{

  if (!bfd_check_format (abfd, bfd_object))
    {
      fprintf (stderr, "%s: %s not an object file\n", program_name,
	       abfd->filename);
      return;
    }
  printf ("\n%s:     file format %s\n", abfd->filename, abfd->xvec->name);
  if (dump_ar_hdrs)
    print_arelt_descr (stdout, abfd, true);

  if (dump_file_header)
    {
      char *comma = "";

      printf ("architecture: %s, ",
	      bfd_printable_arch_mach (bfd_get_arch (abfd),
				       bfd_get_mach (abfd)));
      printf ("flags 0x%08x:\n", abfd->flags);

#define PF(x, y)    if (abfd->flags & x) {printf("%s%s", comma, y); comma=", ";}
      PF (HAS_RELOC, "HAS_RELOC");
      PF (EXEC_P, "EXEC_P");
      PF (HAS_LINENO, "HAS_LINENO");
      PF (HAS_DEBUG, "HAS_DEBUG");
      PF (HAS_SYMS, "HAS_SYMS");
      PF (HAS_LOCALS, "HAS_LOCALS");
      PF (DYNAMIC, "DYNAMIC");
      PF (WP_TEXT, "WP_TEXT");
      PF (D_PAGED, "D_PAGED");
      PF (BFD_IS_RELAXABLE, "BFD_IS_RELAXABLE");
      printf ("\nstart address 0x");
      printf_vma (abfd->start_address);
    }
  printf ("\n");

  if (dump_section_headers)
    dump_headers (abfd);
  if (dump_symtab || dump_reloc_info || disassemble)
    {
      syms = slurp_symtab (abfd);
    }
  if (dump_symtab)
    dump_symbols (abfd);
#ifdef	ELF_STAB_DISPLAY
  if (dump_stab_section_info)
    dump_elf_stabs (abfd);
#endif
  if (dump_reloc_info)
    dump_relocs (abfd);
  if (dump_section_contents)
    dump_data (abfd);
  if (disassemble)
    disassemble_data (abfd);
}

void
display_file (filename, target)
     char *filename;
     char *target;
{
  bfd *file, *arfile = (bfd *) NULL;

  file = bfd_openr (filename, target);
  if (file == NULL)
    {
      bfd_perror (filename);
      return;
    }

  if (bfd_check_format (file, bfd_archive) == true)
    {
      printf ("In archive %s:\n", bfd_get_filename (file));
      for (;;)
	{
	  bfd_error = no_error;

	  arfile = bfd_openr_next_archived_file (file, arfile);
	  if (arfile == NULL)
	    {
	      if (bfd_error != no_more_archived_files)
		bfd_perror (bfd_get_filename (file));
	      return;
	    }

	  display_bfd (arfile);
	  /* Don't close the archive elements; we need them for next_archive */
	}
    }
  else
    display_bfd (file);

  bfd_close (file);
}

/* Actually display the various requested regions */

void
dump_data (abfd)
     bfd *abfd;
{
  asection *section;
  bfd_byte *data = 0;
  bfd_size_type datasize = 0;
  bfd_size_type i;

  for (section = abfd->sections; section != NULL; section =
       section->next)
    {
      int onaline = 16;

      if (only == (char *) NULL ||
	  strcmp (only, section->name) == 0)
	{
	  if (section->flags & SEC_HAS_CONTENTS)
	    {
	      printf ("Contents of section %s:\n", section->name);

	      if (bfd_get_section_size_before_reloc (section) == 0)
		continue;
	      data = (bfd_byte *) malloc (bfd_get_section_size_before_reloc (section));
	      if (data == (bfd_byte *) NULL)
		{
		  fprintf (stderr, "%s: memory exhausted.\n", program_name);
		  exit (1);
		}
	      datasize = bfd_get_section_size_before_reloc (section);


	      bfd_get_section_contents (abfd, section, (PTR) data, 0, bfd_get_section_size_before_reloc (section));

	      for (i = 0; i < bfd_get_section_size_before_reloc (section); i += onaline)
		{
		  bfd_size_type j;

		  printf (" %04lx ", (unsigned long int) (i + section->vma));
		  for (j = i; j < i + onaline; j++)
		    {
		      if (j < bfd_get_section_size_before_reloc (section))
			printf ("%02x", (unsigned) (data[j]));
		      else
			printf ("  ");
		      if ((j & 3) == 3)
			printf (" ");
		    }

		  printf (" ");
		  for (j = i; j < i + onaline; j++)
		    {
		      if (j >= bfd_get_section_size_before_reloc (section))
			printf (" ");
		      else
			printf ("%c", isprint (data[j]) ? data[j] : '.');
		    }
		  putchar ('\n');
		}
	    }
	}
      free (data);
    }
}

/* Should perhaps share code and display with nm? */
void
dump_symbols (abfd)
     bfd *abfd;
{

  unsigned int count;
  asymbol **current = syms;

  printf ("SYMBOL TABLE:\n");

  for (count = 0; count < symcount; count++)
    {

      if (*current && (*current)->the_bfd)
	{
	  bfd_print_symbol ((*current)->the_bfd,
			    stdout,
			    *current, bfd_print_symbol_all);

	  printf ("\n");

	}
      current++;
    }
  printf ("\n");
  printf ("\n");
}

void
dump_relocs (abfd)
     bfd *abfd;
{
  arelent **relpp;
  unsigned int relcount;
  asection *a;

  for (a = abfd->sections; a != (asection *) NULL; a = a->next)
    {
      if (a == &bfd_abs_section)
	continue;
      if (a == &bfd_und_section)
	continue;
      if (a == &bfd_com_section)
	continue;

      printf ("RELOCATION RECORDS FOR [%s]:", a->name);

      if (bfd_get_reloc_upper_bound (abfd, a) == 0)
	{
	  printf (" (none)\n\n");
	}
      else
	{
	  arelent **p;

	  relpp = (arelent **) xmalloc (bfd_get_reloc_upper_bound (abfd, a));
	  relcount = bfd_canonicalize_reloc (abfd, a, relpp, syms);
	  if (relcount == 0)
	    {
	      printf (" (none)\n\n");
	    }
	  else
	    {
	      printf ("\n");
	      printf ("OFFSET   TYPE      VALUE \n");

	      for (p = relpp; relcount && *p != (arelent *) NULL; p++,
		   relcount--)
		{
		  arelent *q = *p;
		  CONST char *sym_name;

		  /*	  CONST char *section_name =	    q->section == (asection *)NULL ? "*abs" :*/
		  /*	  q->section->name;*/
		  CONST char *section_name = (*(q->sym_ptr_ptr))->section->name;

		  if (q->sym_ptr_ptr && *q->sym_ptr_ptr)
		    {
		      sym_name = (*(q->sym_ptr_ptr))->name;
		    }
		  else
		    {
		      sym_name = 0;
		    }
		  if (sym_name)
		    {
		      printf_vma (q->address);
		      printf (" %-8s  %s",
			      q->howto->name,
			      sym_name);
		    }
		  else
		    {
		      printf_vma (q->address);
		      printf (" %-8s  [%s]",
			      q->howto->name,
			      section_name);
		    }
		  if (q->addend)
		    {
		      printf ("+0x");
		      printf_vma (q->addend);
		    }
		  printf ("\n");
		}
	      printf ("\n\n");
	      free (relpp);
	    }
	}

    }
}

#ifdef unix
#define _DUMMY_NAME_ "/dev/null"
#else
#define _DUMMY_NAME_ "##dummy"
#endif
static void
DEFUN (display_info_table, (first, last),
       int first AND int last)
{
  unsigned int i, j;
  extern bfd_target *target_vector[];

  printf ("\n%12s", " ");
  for (i = first; i++ < last && target_vector[i];)
    printf ("%s ", target_vector[i]->name);
  printf ("\n");

  for (j = (int) bfd_arch_obscure + 1; (int) j < (int) bfd_arch_last; j++)
    if (strcmp (bfd_printable_arch_mach (j, 0), "UNKNOWN!") != 0)
      {
	printf ("%11s ", bfd_printable_arch_mach (j, 0));
	for (i = first; i++ < last && target_vector[i];)
	  {
	    bfd_target *p = target_vector[i];
	    bfd *abfd = bfd_openw (_DUMMY_NAME_, p->name);
	    int l = strlen (p->name);
	    int ok;
	    bfd_set_format (abfd, bfd_object);
	    ok = bfd_set_arch_mach (abfd, j, 0);

	    if (ok)
	      printf ("%s ", p->name);
	    else
	      {
		while (l--)
		  printf ("%c", ok ? '*' : '-');
		printf (" ");
	      }
	  }
	printf ("\n");
      }
}

static void
DEFUN_VOID (display_info)
{
  char *colum;
  unsigned int i, j, columns;
  extern bfd_target *target_vector[];
  extern char *getenv ();

  printf ("BFD header file version %s\n", BFD_VERSION);
  for (i = 0; target_vector[i]; i++)
    {
      bfd_target *p = target_vector[i];
      bfd *abfd = bfd_openw (_DUMMY_NAME_, p->name);
      bfd_set_format (abfd, bfd_object);
      printf ("%s\n (header %s, data %s)\n", p->name,
	      p->header_byteorder_big_p ? "big endian" : "little endian",
	      p->byteorder_big_p ? "big endian" : "little endian");
      for (j = (int) bfd_arch_obscure + 1; j < (int) bfd_arch_last; j++)
	if (bfd_set_arch_mach (abfd, (enum bfd_architecture) j, 0))
	  printf ("  %s\n",
		  bfd_printable_arch_mach ((enum bfd_architecture) j, 0));
    }
  columns = 0;
  if (colum = getenv ("COLUMNS"))
    columns = atoi (colum);
  if (!columns)
    columns = 80;
  for (i = 0; target_vector[i];)
    {
      int old;
      old = i;
      for (j = 12; target_vector[i] && j < columns; i++)
	j += strlen (target_vector[i]->name) + 1;
      i--;
      if (old == i)
	break;
      display_info_table (old, i);
    }
}

/** main and like trivia */
int
main (argc, argv)
     int argc;
     char **argv;
{
  int c;
  extern int optind;
  extern char *optarg;
  char *target = default_target;
  boolean seenflag = false;
  int ind = 0;

  bfd_init ();
  program_name = *argv;

  while ((c = getopt_long (argc, argv, "ib:m:Vdlfahrtxsj:", long_options, &ind))
	 != EOF)
    {
      seenflag = true;
      switch (c)
	{
	case 'm':
	  machine = optarg;
	  break;
	case 'j':
	  only = optarg;
	  break;
	case 'l':
	  with_line_numbers = 1;
	  break;
	case 'b':
	  target = optarg;
	  break;
	case 'f':
	  dump_file_header = true;
	  break;
	case 'i':
	  info = true;
	  break;
	case 'x':
	  dump_symtab = 1;
	  dump_reloc_info = 1;
	  dump_file_header = true;
	  dump_ar_hdrs = 1;
	  dump_section_headers = 1;
	  break;
	case 0:
	  break;		/* we've been given a long option */
	case 't':
	  dump_symtab = 1;
	  break;
	case 'd':
	  disassemble = true;
	  break;
	case 's':
	  dump_section_contents = 1;
	  break;
	case 'r':
	  dump_reloc_info = 1;
	  break;
	case 'a':
	  dump_ar_hdrs = 1;
	  break;
	case 'h':
	  dump_section_headers = 1;
	  break;
	case 'V':
	  show_version = 1;
	  break;
	default:
	  usage ();
	}
    }

  if (show_version)
    printf ("%s version %s\n", program_name, program_version);

  if (seenflag == false)
    usage ();

  if (info)
    {
      display_info ();
    }
  else
    {
      if (optind == argc)
	display_file ("a.out", target);
      else
	for (; optind < argc;)
	  display_file (argv[optind++], target);
    }
  return 0;
}
