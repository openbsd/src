/* objdump.c -- dump information about an object file.
   Copyright 1990, 1991, 1992, 1993, 1994 Free Software Foundation, Inc.

This file is part of GNU Binutils.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include "bfd.h"
#include "getopt.h"
#include "progress.h"
#include "bucomm.h"
#include <ctype.h>
#include "dis-asm.h"
#include "libiberty.h"

/* Internal headers for the ELF .stab-dump code - sorry.  */
#define	BYTES_IN_WORD	32
#include "aout/aout64.h"

#ifdef NEED_DECLARATION_FPRINTF
/* This is needed by INIT_DISASSEMBLE_INFO.  */
extern int fprintf ();
#endif

char *default_target = NULL;	/* default at runtime */

extern char *program_version;

int show_version = 0;		/* show the version number */
int dump_section_contents;	/* -s */
int dump_section_headers;	/* -h */
boolean dump_file_header;	/* -f */
int dump_symtab;		/* -t */
int dump_dynamic_symtab;	/* -T */
int dump_reloc_info;		/* -r */
int dump_dynamic_reloc_info;	/* -R */
int dump_ar_hdrs;		/* -a */
int dump_private_headers;	/* -p */
int with_line_numbers;		/* -l */
boolean with_source_code;	/* -S */
int dump_stab_section_info;	/* --stabs */
boolean disassemble;		/* -d */
boolean disassemble_all;	/* -D */
boolean formats_info;		/* -i */
char *only;			/* -j secname */
int wide_output;		/* -w */
bfd_vma start_address = (bfd_vma) -1; /* --start-address */
bfd_vma stop_address = (bfd_vma) -1;  /* --stop-address */

/* Extra info to pass to the disassembler address printing function.  */
struct objdump_disasm_info {
  bfd *abfd;
  asection *sec;
  boolean require_sec;
};

/* Architecture to disassemble for, or default if NULL.  */
char *machine = (char *) NULL;

/* The symbol table.  */
asymbol **syms;

/* Number of symbols in `syms'.  */
long symcount = 0;

/* The sorted symbol table.  */
asymbol **sorted_syms;

/* Number of symbols in `sorted_syms'.  */
long sorted_symcount = 0;

/* The dynamic symbol table.  */
asymbol **dynsyms;

/* Number of symbols in `dynsyms'.  */
long dynsymcount = 0;

/* Forward declarations.  */

static void
display_file PARAMS ((char *filename, char *target));

static void
dump_data PARAMS ((bfd *abfd));

static void
dump_relocs PARAMS ((bfd *abfd));

static void
dump_dynamic_relocs PARAMS ((bfd * abfd));

static void
dump_reloc_set PARAMS ((bfd *, arelent **, long));

static void
dump_symbols PARAMS ((bfd *abfd, boolean dynamic));

static void
display_bfd PARAMS ((bfd *abfd));

static void
objdump_print_value PARAMS ((bfd_vma, FILE *));

static void
objdump_print_address PARAMS ((bfd_vma, struct disassemble_info *));

static void
show_line PARAMS ((bfd *, asection *, bfd_vma));

void
usage (stream, status)
     FILE *stream;
     int status;
{
  fprintf (stream, "\
Usage: %s [-ahifdDprRtTxsSlw] [-b bfdname] [-m machine] [-j section-name]\n\
       [--archive-headers] [--target=bfdname] [--disassemble]\n\
       [--disassemble-all] [--file-headers] [--section-headers] [--headers]\n\
       [--info] [--section=section-name] [--line-numbers] [--source]\n",
	   program_name);
  fprintf (stream, "\
       [--architecture=machine] [--reloc] [--full-contents] [--stabs]\n\
       [--syms] [--all-headers] [--dynamic-syms] [--dynamic-reloc]\n\
       [--wide] [--version] [--help] [--private-headers]\n\
       [--start-address=addr] [--stop-address=addr] objfile...\n\
at least one option besides -l (--line-numbers) must be given\n");
  list_supported_targets (program_name, stream);
  exit (status);
}

/* 150 isn't special; it's just an arbitrary non-ASCII char value.  */

#define OPTION_START_ADDRESS (150)
#define OPTION_STOP_ADDRESS (OPTION_START_ADDRESS + 1)

static struct option long_options[]=
{
  {"all-headers", no_argument, NULL, 'x'},
  {"private-headers", no_argument, NULL, 'p'},
  {"architecture", required_argument, NULL, 'm'},
  {"archive-headers", no_argument, NULL, 'a'},
  {"disassemble", no_argument, NULL, 'd'},
  {"disassemble-all", no_argument, NULL, 'D'},
  {"dynamic-reloc", no_argument, NULL, 'R'},
  {"dynamic-syms", no_argument, NULL, 'T'},
  {"file-headers", no_argument, NULL, 'f'},
  {"full-contents", no_argument, NULL, 's'},
  {"headers", no_argument, NULL, 'h'},
  {"help", no_argument, NULL, 'H'},
  {"info", no_argument, NULL, 'i'},
  {"line-numbers", no_argument, NULL, 'l'},
  {"reloc", no_argument, NULL, 'r'},
  {"section", required_argument, NULL, 'j'},
  {"section-headers", no_argument, NULL, 'h'},
  {"source", no_argument, NULL, 'S'},
  {"stabs", no_argument, &dump_stab_section_info, 1},
  {"start-address", required_argument, NULL, OPTION_START_ADDRESS},
  {"stop-address", required_argument, NULL, OPTION_STOP_ADDRESS},
  {"syms", no_argument, NULL, 't'},
  {"target", required_argument, NULL, 'b'},
  {"version", no_argument, &show_version, 1},
  {"wide", no_argument, &wide_output, 'w'},
  {0, no_argument, 0, 0}
};

static void
dump_section_header (abfd, section, ignored)
     bfd *abfd;
     asection *section;
     PTR ignored;
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
  printf (" lma ");
  printf_vma (section->lma);
  printf (" align 2**%u%s ",
	  section->alignment_power, (wide_output) ? "" : "\n");
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
  PF (SEC_DEBUGGING, "DEBUGGING");
  PF (SEC_NEVER_LOAD, "NEVER_LOAD");
  printf ("\n");
#undef PF
}

static void
dump_headers (abfd)
     bfd *abfd;
{
  bfd_map_over_sections (abfd, dump_section_header, (PTR) NULL);
}

static asymbol **
slurp_symtab (abfd)
     bfd *abfd;
{
  asymbol **sy = (asymbol **) NULL;
  long storage;

  if (!(bfd_get_file_flags (abfd) & HAS_SYMS))
    {
      printf ("No symbols in \"%s\".\n", bfd_get_filename (abfd));
      symcount = 0;
      return NULL;
    }

  storage = bfd_get_symtab_upper_bound (abfd);
  if (storage < 0)
    bfd_fatal (bfd_get_filename (abfd));

  if (storage)
    {
      sy = (asymbol **) xmalloc (storage);
    }
  symcount = bfd_canonicalize_symtab (abfd, sy);
  if (symcount < 0)
    bfd_fatal (bfd_get_filename (abfd));
  if (symcount == 0)
    fprintf (stderr, "%s: %s: No symbols\n",
	     program_name, bfd_get_filename (abfd));
  return sy;
}

/* Read in the dynamic symbols.  */

static asymbol **
slurp_dynamic_symtab (abfd)
     bfd *abfd;
{
  asymbol **sy = (asymbol **) NULL;
  long storage;

  storage = bfd_get_dynamic_symtab_upper_bound (abfd);
  if (storage < 0)
    {
      if (!(bfd_get_file_flags (abfd) & DYNAMIC))
	{
	  fprintf (stderr, "%s: %s: not a dynamic object\n",
		   program_name, bfd_get_filename (abfd));
	  dynsymcount = 0;
	  return NULL;
	}

      bfd_fatal (bfd_get_filename (abfd));
    }

  if (storage)
    {
      sy = (asymbol **) xmalloc (storage);
    }
  dynsymcount = bfd_canonicalize_dynamic_symtab (abfd, sy);
  if (dynsymcount < 0)
    bfd_fatal (bfd_get_filename (abfd));
  if (dynsymcount == 0)
    fprintf (stderr, "%s: %s: No dynamic symbols\n",
	     program_name, bfd_get_filename (abfd));
  return sy;
}

/* Filter out (in place) symbols that are useless for disassembly.
   COUNT is the number of elements in SYMBOLS.
   Return the number of useful symbols. */

long
remove_useless_symbols (symbols, count)
     asymbol **symbols;
     long count;
{
  register asymbol **in_ptr = symbols, **out_ptr = symbols;

  while (--count >= 0)
    {
      asymbol *sym = *in_ptr++;

      if (sym->name == NULL || sym->name[0] == '\0')
	continue;
      if (sym->flags & (BSF_DEBUGGING))
	continue;
      if (bfd_is_und_section (sym->section)
	  || bfd_is_com_section (sym->section))
	continue;

      *out_ptr++ = sym;
    }
  return out_ptr - symbols;
}

/* Sort symbols into value order.  */

static int 
compare_symbols (ap, bp)
     const PTR ap;
     const PTR bp;
{
  const asymbol *a = *(const asymbol **)ap;
  const asymbol *b = *(const asymbol **)bp;
  const char *an, *bn;
  size_t anl, bnl;
  boolean af, bf;
  flagword aflags, bflags;

  if (bfd_asymbol_value (a) > bfd_asymbol_value (b))
    return 1;
  else if (bfd_asymbol_value (a) < bfd_asymbol_value (b))
    return -1;

  if (a->section > b->section)
    return 1;
  else if (a->section < b->section)
    return -1;

  an = bfd_asymbol_name (a);
  bn = bfd_asymbol_name (b);
  anl = strlen (an);
  bnl = strlen (bn);

  /* The symbols gnu_compiled and gcc2_compiled convey no real
     information, so put them after other symbols with the same value.  */

  af = (strstr (an, "gnu_compiled") != NULL
	|| strstr (an, "gcc2_compiled") != NULL);
  bf = (strstr (bn, "gnu_compiled") != NULL
	|| strstr (bn, "gcc2_compiled") != NULL);

  if (af && ! bf)
    return 1;
  if (! af && bf)
    return -1;

  /* We use a heuristic for the file name, to try to sort it after
     more useful symbols.  It may not work on non Unix systems, but it
     doesn't really matter; the only difference is precisely which
     symbol names get printed.  */

#define file_symbol(s, sn, snl)			\
  (((s)->flags & BSF_FILE) != 0			\
   || ((sn)[(snl) - 2] == '.'			\
       && ((sn)[(snl) - 1] == 'o'		\
	   || (sn)[(snl) - 1] == 'a')))

  af = file_symbol (a, an, anl);
  bf = file_symbol (b, bn, bnl);

  if (af && ! bf)
    return 1;
  if (! af && bf)
    return -1;

  /* Finally, try to sort global symbols before local symbols before
     debugging symbols.  */

  aflags = a->flags;
  bflags = b->flags;

  if ((aflags & BSF_DEBUGGING) != (bflags & BSF_DEBUGGING))
    {
      if ((aflags & BSF_DEBUGGING) != 0)
	return 1;
      else
	return -1;
    }
  if ((aflags & BSF_LOCAL) != (bflags & BSF_LOCAL))
    {
      if ((aflags & BSF_LOCAL) != 0)
	return 1;
      else
	return -1;
    }

  return 0;
}

/* Sort relocs into address order.  */

static int
compare_relocs (ap, bp)
     const PTR ap;
     const PTR bp;
{
  const arelent *a = *(const arelent **)ap;
  const arelent *b = *(const arelent **)bp;

  if (a->address > b->address)
    return 1;
  else if (a->address < b->address)
    return -1;

  /* So that associated relocations tied to the same address show up
     in the correct order, we don't do any further sorting.  */
  if (a > b)
    return 1;
  else if (a < b)
    return -1;
  else
    return 0;
}

/* Print VMA to STREAM with no leading zeroes.  */

static void
objdump_print_value (vma, stream)
     bfd_vma vma;
     FILE *stream;
{
  char buf[30];
  char *p;

  sprintf_vma (buf, vma);
  for (p = buf; *p == '0'; ++p)
    ;
  fprintf (stream, "%s", p);
}

/* Print VMA symbolically to INFO if possible.  */

static void
objdump_print_address (vma, info)
     bfd_vma vma;
     struct disassemble_info *info;
{
  /* @@ Would it speed things up to cache the last two symbols returned,
     and maybe their address ranges?  For many processors, only one memory
     operand can be present at a time, so the 2-entry cache wouldn't be
     constantly churned by code doing heavy memory accesses.  */

  /* Indices in `sorted_syms'.  */
  long min = 0;
  long max = sorted_symcount;
  long thisplace;

  fprintf_vma (info->stream, vma);

  if (sorted_symcount < 1)
    return;

  /* Perform a binary search looking for the closest symbol to the
     required value.  We are searching the range (min, max].  */
  while (min + 1 < max)
    {
      asymbol *sym;

      thisplace = (max + min) / 2;
      sym = sorted_syms[thisplace];

      if (bfd_asymbol_value (sym) > vma)
	max = thisplace;
      else if (bfd_asymbol_value (sym) < vma)
	min = thisplace;
      else
	{
	  min = thisplace;
	  break;
	}
    }

  /* The symbol we want is now in min, the low end of the range we
     were searching.  If there are several symbols with the same
     value, we want the first one.  */
  thisplace = min;
  while (thisplace > 0
	 && (bfd_asymbol_value (sorted_syms[thisplace])
	     == bfd_asymbol_value (sorted_syms[thisplace - 1])))
    --thisplace;

  {
    /* If the file is relocateable, and the symbol could be from this
       section, prefer a symbol from this section over symbols from
       others, even if the other symbol's value might be closer.
       
       Note that this may be wrong for some symbol references if the
       sections have overlapping memory ranges, but in that case there's
       no way to tell what's desired without looking at the relocation
       table.  */
    struct objdump_disasm_info *aux;
    long i;

    aux = (struct objdump_disasm_info *) info->application_data;
    if (sorted_syms[thisplace]->section != aux->sec
	&& (aux->require_sec
	    || ((aux->abfd->flags & HAS_RELOC) != 0
		&& vma >= bfd_get_section_vma (aux->abfd, aux->sec)
		&& vma < (bfd_get_section_vma (aux->abfd, aux->sec)
			  + bfd_section_size (aux->abfd, aux->sec)))))
      {
	for (i = thisplace + 1; i < sorted_symcount; i++)
	  {
	    if (bfd_asymbol_value (sorted_syms[i])
		!= bfd_asymbol_value (sorted_syms[thisplace]))
	      break;
	  }
	--i;
	for (; i >= 0; i--)
	  {
	    if (sorted_syms[i]->section == aux->sec
		&& (i == 0
		    || sorted_syms[i - 1]->section != aux->sec
		    || (bfd_asymbol_value (sorted_syms[i])
			!= bfd_asymbol_value (sorted_syms[i - 1]))))
	      {
		thisplace = i;
		break;
	      }
	  }

	if (sorted_syms[thisplace]->section != aux->sec)
	  {
	    /* We didn't find a good symbol with a smaller value.
               Look for one with a larger value.  */
	    for (i = thisplace + 1; i < sorted_symcount; i++)
	      {
		if (sorted_syms[i]->section == aux->sec)
		  {
		    thisplace = i;
		    break;
		  }
	      }
	  }

	if (sorted_syms[thisplace]->section != aux->sec
	    && (aux->require_sec
		|| ((aux->abfd->flags & HAS_RELOC) != 0
		    && vma >= bfd_get_section_vma (aux->abfd, aux->sec)
		    && vma < (bfd_get_section_vma (aux->abfd, aux->sec)
			      + bfd_section_size (aux->abfd, aux->sec)))))
	  {
	    bfd_vma secaddr;

	    fprintf (info->stream, " <%s",
		     bfd_get_section_name (aux->abfd, aux->sec));
	    secaddr = bfd_get_section_vma (aux->abfd, aux->sec);
	    if (vma < secaddr)
	      {
		fprintf (info->stream, "-");
		objdump_print_value (secaddr - vma, info->stream);
	      }
	    else if (vma > secaddr)
	      {
		fprintf (info->stream, "+");
		objdump_print_value (vma - secaddr, info->stream);
	      }
	    fprintf (info->stream, ">");
	    return;
	  }
      }
  }

  fprintf (info->stream, " <%s", sorted_syms[thisplace]->name);
  if (bfd_asymbol_value (sorted_syms[thisplace]) > vma)
    {
      fprintf (info->stream, "-");
      objdump_print_value (bfd_asymbol_value (sorted_syms[thisplace]) - vma,
			   info->stream);
    }
  else if (vma > bfd_asymbol_value (sorted_syms[thisplace]))
    {
      fprintf (info->stream, "+");
      objdump_print_value (vma - bfd_asymbol_value (sorted_syms[thisplace]),
			   info->stream);
    }
  fprintf (info->stream, ">");
}

/* Hold the last function name and the last line number we displayed
   in a disassembly.  */

static char *prev_functionname;
static unsigned int prev_line;

/* We keep a list of all files that we have seen when doing a
   dissassembly with source, so that we know how much of the file to
   display.  This can be important for inlined functions.  */

struct print_file_list
{
  struct print_file_list *next;
  char *filename;
  unsigned int line;
  FILE *f;
};

static struct print_file_list *print_files;

/* The number of preceding context lines to show when we start
   displaying a file for the first time.  */

#define SHOW_PRECEDING_CONTEXT_LINES (5)

/* Skip ahead to a given line in a file, optionally printing each
   line.  */

static void
skip_to_line PARAMS ((struct print_file_list *, unsigned int, boolean));

static void
skip_to_line (p, line, show)
     struct print_file_list *p;
     unsigned int line;
     boolean show;
{
  while (p->line < line)
    {
      char buf[100];

      if (fgets (buf, sizeof buf, p->f) == NULL)
	{
	  fclose (p->f);
	  p->f = NULL;
	  break;
	}

      if (show)
	printf ("%s", buf);

      if (strchr (buf, '\n') != NULL)
	++p->line;
    }
}  

/* Show the line number, or the source line, in a dissassembly
   listing.  */

static void
show_line (abfd, section, off)
     bfd *abfd;
     asection *section;
     bfd_vma off;
{
  CONST char *filename;
  CONST char *functionname;
  unsigned int line;

  if (! with_line_numbers && ! with_source_code)
    return;

  if (! bfd_find_nearest_line (abfd, section, syms, off, &filename,
			       &functionname, &line))
    return;

  if (filename != NULL && *filename == '\0')
    filename = NULL;
  if (functionname != NULL && *functionname == '\0')
    functionname = NULL;

  if (with_line_numbers)
    {
      if (functionname != NULL
	  && (prev_functionname == NULL
	      || strcmp (functionname, prev_functionname) != 0))
	printf ("%s():\n", functionname);
      if (line > 0 && line != prev_line)
	printf ("%s:%u\n", filename == NULL ? "???" : filename, line);
    }

  if (with_source_code
      && filename != NULL
      && line > 0)
    {
      struct print_file_list **pp, *p;

      for (pp = &print_files; *pp != NULL; pp = &(*pp)->next)
	if (strcmp ((*pp)->filename, filename) == 0)
	  break;
      p = *pp;

      if (p != NULL)
	{
	  if (p != print_files)
	    {
	      int l;

	      /* We have reencountered a file name which we saw
		 earlier.  This implies that either we are dumping out
		 code from an included file, or the same file was
		 linked in more than once.  There are two common cases
		 of an included file: inline functions in a header
		 file, and a bison or flex skeleton file.  In the
		 former case we want to just start printing (but we
		 back up a few lines to give context); in the latter
		 case we want to continue from where we left off.  I
		 can't think of a good way to distinguish the cases,
		 so I used a heuristic based on the file name.  */
	      if (strcmp (p->filename + strlen (p->filename) - 2, ".h") != 0)
		l = p->line;
	      else
		{
		  l = line - SHOW_PRECEDING_CONTEXT_LINES;
		  if (l <= 0)
		    l = 1;
		}

	      if (p->f == NULL)
		{
		  p->f = fopen (p->filename, "r");
		  p->line = 0;
		}
	      if (p->f != NULL)
		skip_to_line (p, l, false);

	      if (print_files->f != NULL)
		{
		  fclose (print_files->f);
		  print_files->f = NULL;
		}
	    }

	  if (p->f != NULL)
	    {
	      skip_to_line (p, line, true);
	      *pp = p->next;
	      p->next = print_files;
	      print_files = p;
	    }
	}
      else
	{
	  FILE *f;

	  f = fopen (filename, "r");
	  if (f != NULL)
	    {
	      int l;

	      p = ((struct print_file_list *)
		   xmalloc (sizeof (struct print_file_list)));
	      p->filename = xmalloc (strlen (filename) + 1);
	      strcpy (p->filename, filename);
	      p->line = 0;
	      p->f = f;

	      if (print_files != NULL && print_files->f != NULL)
		{
		  fclose (print_files->f);
		  print_files->f = NULL;
		}
	      p->next = print_files;
	      print_files = p;

	      l = line - SHOW_PRECEDING_CONTEXT_LINES;
	      if (l <= 0)
		l = 1;
	      skip_to_line (p, l, false);
	      if (p->f != NULL)
		skip_to_line (p, line, true);
	    }
	}
    }

  if (functionname != NULL
      && (prev_functionname == NULL
	  || strcmp (functionname, prev_functionname) != 0))
    {
      if (prev_functionname != NULL)
	free (prev_functionname);
      prev_functionname = xmalloc (strlen (functionname) + 1);
      strcpy (prev_functionname, functionname);
    }

  if (line > 0 && line != prev_line)
    prev_line = line;
}

void
disassemble_data (abfd)
     bfd *abfd;
{
  long i;
  disassembler_ftype disassemble_fn = 0; /* New style */
  struct disassemble_info disasm_info;
  struct objdump_disasm_info aux;
  asection *section;
  boolean done_dot = false;

  print_files = NULL;
  prev_functionname = NULL;
  prev_line = -1;

  /* We make a copy of syms to sort.  We don't want to sort syms
     because that will screw up the relocs.  */
  sorted_syms = (asymbol **) xmalloc (symcount * sizeof (asymbol *));
  memcpy (sorted_syms, syms, symcount * sizeof (asymbol *));

  sorted_symcount = remove_useless_symbols (sorted_syms, symcount);

  /* Sort the symbols into section and symbol order */
  qsort (sorted_syms, sorted_symcount, sizeof (asymbol *), compare_symbols);

  INIT_DISASSEMBLE_INFO(disasm_info, stdout);
  disasm_info.application_data = (PTR) &aux;
  aux.abfd = abfd;
  disasm_info.print_address_func = objdump_print_address;

  if (machine != (char *) NULL)
    {
      const bfd_arch_info_type *info = bfd_scan_arch (machine);
      if (info == NULL)
	{
	  fprintf (stderr, "%s: Can't use supplied machine %s\n",
		   program_name,
		   machine);
	  exit (1);
	}
      abfd->arch_info = info;
    }

  disassemble_fn = disassembler (abfd);
  if (!disassemble_fn)
    {
      fprintf (stderr, "%s: Can't disassemble for architecture %s\n",
	       program_name,
	       bfd_printable_arch_mach (bfd_get_arch (abfd), 0));
      exit (1);
    }

  for (section = abfd->sections;
       section != (asection *) NULL;
       section = section->next)
    {
      bfd_byte *data = NULL;
      bfd_size_type datasize = 0;
      arelent **relbuf = NULL;
      arelent **relpp = NULL;
      arelent **relppend = NULL;
      long stop;

      if ((section->flags & SEC_LOAD) == 0
	  || (! disassemble_all
	      && only == NULL
	      && (section->flags & SEC_CODE) == 0))
	continue;
      if (only != (char *) NULL && strcmp (only, section->name) != 0)
	continue;

      if (dump_reloc_info
	  && (section->flags & SEC_RELOC) != 0)
	{
	  long relsize;

	  relsize = bfd_get_reloc_upper_bound (abfd, section);
	  if (relsize < 0)
	    bfd_fatal (bfd_get_filename (abfd));

	  if (relsize > 0)
	    {
	      long relcount;

	      relbuf = (arelent **) xmalloc (relsize);
	      relcount = bfd_canonicalize_reloc (abfd, section, relbuf, syms);
	      if (relcount < 0)
		bfd_fatal (bfd_get_filename (abfd));

	      /* Sort the relocs by address.  */
	      qsort (relbuf, relcount, sizeof (arelent *), compare_relocs);

	      relpp = relbuf;
	      relppend = relpp + relcount;
	    }
	}

      printf ("Disassembly of section %s:\n", section->name);

      datasize = bfd_get_section_size_before_reloc (section);
      if (datasize == 0)
	continue;

      data = (bfd_byte *) xmalloc ((size_t) datasize);

      bfd_get_section_contents (abfd, section, data, 0, datasize);

      aux.sec = section;
      disasm_info.buffer = data;
      disasm_info.buffer_vma = section->vma;
      disasm_info.buffer_length = datasize;
      if (start_address == (bfd_vma) -1
	  || start_address < disasm_info.buffer_vma)
	i = 0;
      else
	i = start_address - disasm_info.buffer_vma;
      if (stop_address == (bfd_vma) -1)
	stop = datasize;
      else
	{
	  if (stop_address < disasm_info.buffer_vma)
	    stop = 0;
	  else
	    stop = stop_address - disasm_info.buffer_vma;
	  if (stop > disasm_info.buffer_length)
	    stop = disasm_info.buffer_length;
	}
      while (i < stop)
	{
	  int bytes;
	  boolean need_nl = false;

	  if (data[i] == 0 && data[i + 1] == 0 && data[i + 2] == 0 &&
	      data[i + 3] == 0)
	    {
	      if (done_dot == false)
		{
		  printf ("...\n");
		  done_dot = true;
		}
	      bytes = 4;
	    }
	  else
	    {
	      done_dot = false;
	      if (with_line_numbers || with_source_code)
		show_line (abfd, section, i);
	      aux.require_sec = true;
	      objdump_print_address (section->vma + i, &disasm_info);
	      aux.require_sec = false;
	      putchar (' ');

	      bytes = (*disassemble_fn) (section->vma + i, &disasm_info);
	      if (bytes < 0)
		break;

	      if (!wide_output)
		putchar ('\n');
	      else
		need_nl = true;
	    }

	  if (dump_reloc_info
	      && (section->flags & SEC_RELOC) != 0)
	    {
	      while (relpp < relppend
		     && ((*relpp)->address >= (bfd_vma) i
			 && (*relpp)->address < (bfd_vma) i + bytes))
		{
		  arelent *q;
		  const char *sym_name;

		  q = *relpp;

		  printf ("\t\tRELOC: ");

		  printf_vma (section->vma + q->address);

		  printf (" %s ", q->howto->name);

		  if (q->sym_ptr_ptr != NULL
		      && *q->sym_ptr_ptr != NULL)
		    {
		      sym_name = bfd_asymbol_name (*q->sym_ptr_ptr);
		      if (sym_name == NULL || *sym_name == '\0')
			{
			  asection *sym_sec;

			  sym_sec = bfd_get_section (*q->sym_ptr_ptr);
			  sym_name = bfd_get_section_name (abfd, sym_sec);
			  if (sym_name == NULL || *sym_name == '\0')
			    sym_name = "*unknown*";
			}
		    }

		  printf ("%s", sym_name);

		  if (q->addend)
		    {
		      printf ("+0x");
		      printf_vma (q->addend);
		    }

		  printf ("\n");
		  need_nl = false;
		  ++relpp;
		}
	    }

	  if (need_nl)
	    printf ("\n");

	  i += bytes;
	}

      free (data);
      if (relbuf != NULL)
	free (relbuf);
    }
  free (sorted_syms);
}


/* Define a table of stab values and print-strings.  We wish the initializer
   could be a direct-mapped table, but instead we build one the first
   time we need it.  */

char **stab_name;

struct stab_print {
  int value;
  char *string;
};

struct stab_print stab_print[] = {
#define __define_stab(NAME, CODE, STRING) {CODE, STRING},
#include "aout/stab.def"
#undef __define_stab
  {0, ""}
};

void dump_section_stabs PARAMS ((bfd *abfd, char *stabsect_name,
				 char *strsect_name));

/* Dump the stabs sections from an object file that has a section that
   uses Sun stabs encoding.  It has to use some hooks into BFD because
   string table sections are not normally visible to BFD callers.  */

void
dump_stabs (abfd)
     bfd *abfd;
{
  /* Allocate and initialize stab name array if first time.  */
  if (stab_name == NULL) 
    {
      int i;

      stab_name = (char **) xmalloc (256 * sizeof(char *));
      /* Clear the array. */
      for (i = 0; i < 256; i++)
	stab_name[i] = NULL;
      /* Fill in the defined stabs. */
      for (i = 0; *stab_print[i].string; i++)
	stab_name[stab_print[i].value] = stab_print[i].string;
    }

  dump_section_stabs (abfd, ".stab", ".stabstr");
  dump_section_stabs (abfd, ".stab.excl", ".stab.exclstr");
  dump_section_stabs (abfd, ".stab.index", ".stab.indexstr");
  dump_section_stabs (abfd, "$GDB_SYMBOLS$", "$GDB_STRINGS$");
}

static struct internal_nlist *stabs;
static bfd_size_type stab_size;

static char *strtab;
static bfd_size_type stabstr_size;

/* Read ABFD's stabs section STABSECT_NAME into `stabs'
   and string table section STRSECT_NAME into `strtab'.
   If the section exists and was read, allocate the space and return true.
   Otherwise return false.  */

boolean
read_section_stabs (abfd, stabsect_name, strsect_name)
     bfd *abfd;
     char *stabsect_name;
     char *strsect_name;
{
  asection *stabsect, *stabstrsect;

  stabsect = bfd_get_section_by_name (abfd, stabsect_name);
  if (0 == stabsect)
    {
      printf ("No %s section present\n\n", stabsect_name);
      return false;
    }

  stabstrsect = bfd_get_section_by_name (abfd, strsect_name);
  if (0 == stabstrsect)
    {
      fprintf (stderr, "%s: %s has no %s section\n", program_name,
	       bfd_get_filename (abfd), strsect_name);
      return false;
    }
 
  stab_size    = bfd_section_size (abfd, stabsect);
  stabstr_size = bfd_section_size (abfd, stabstrsect);

  stabs  = (struct internal_nlist *) xmalloc (stab_size);
  strtab = (char *) xmalloc (stabstr_size);
  
  if (! bfd_get_section_contents (abfd, stabsect, (PTR) stabs, 0, stab_size))
    {
      fprintf (stderr, "%s: Reading %s section of %s failed: %s\n",
	       program_name, stabsect_name, bfd_get_filename (abfd),
	       bfd_errmsg (bfd_get_error ()));
      free (stabs);
      free (strtab);
      return false;
    }

  if (! bfd_get_section_contents (abfd, stabstrsect, (PTR) strtab, 0,
				  stabstr_size))
    {
      fprintf (stderr, "%s: Reading %s section of %s failed: %s\n",
	       program_name, strsect_name, bfd_get_filename (abfd),
	       bfd_errmsg (bfd_get_error ()));
      free (stabs);
      free (strtab);
      return false;
    }

  return true;
}

#define SWAP_SYMBOL(symp, abfd) \
{ \
    (symp)->n_strx = bfd_h_get_32(abfd,			\
				  (unsigned char *)&(symp)->n_strx);	\
    (symp)->n_desc = bfd_h_get_16 (abfd,			\
				   (unsigned char *)&(symp)->n_desc);  	\
    (symp)->n_value = bfd_h_get_32 (abfd,			\
				    (unsigned char *)&(symp)->n_value);	\
}

/* Print ABFD's stabs section STABSECT_NAME (in `stabs'),
   using string table section STRSECT_NAME (in `strtab').  */

void
print_section_stabs (abfd, stabsect_name, strsect_name)
     bfd *abfd;
     char *stabsect_name;
     char *strsect_name;
{
  int i;
  unsigned file_string_table_offset = 0, next_file_string_table_offset = 0;
  struct internal_nlist *stabp = stabs,
  *stabs_end = (struct internal_nlist *) (stab_size + (char *) stabs);

  printf ("Contents of %s section:\n\n", stabsect_name);
  printf ("Symnum n_type n_othr n_desc n_value  n_strx String\n");

  /* Loop through all symbols and print them.

     We start the index at -1 because there is a dummy symbol on
     the front of stabs-in-{coff,elf} sections that supplies sizes.  */

  for (i = -1; stabp < stabs_end; stabp++, i++)
    {
      SWAP_SYMBOL (stabp, abfd);
      printf ("\n%-6d ", i);
      /* Either print the stab name, or, if unnamed, print its number
	 again (makes consistent formatting for tools like awk). */
      if (stab_name[stabp->n_type])
	printf ("%-6s", stab_name[stabp->n_type]);
      else if (stabp->n_type == N_UNDF)
	printf ("HdrSym");
      else
	printf ("%-6d", stabp->n_type);
      printf (" %-6d %-6d ", stabp->n_other, stabp->n_desc);
      printf_vma (stabp->n_value);
      printf (" %-6lu", stabp->n_strx);

      /* Symbols with type == 0 (N_UNDF) specify the length of the
	 string table associated with this file.  We use that info
	 to know how to relocate the *next* file's string table indices.  */

      if (stabp->n_type == N_UNDF)
	{
	  file_string_table_offset = next_file_string_table_offset;
	  next_file_string_table_offset += stabp->n_value;
	}
      else
	{
	  /* Using the (possibly updated) string table offset, print the
	     string (if any) associated with this symbol.  */

	  if ((stabp->n_strx + file_string_table_offset) < stabstr_size)
	    printf (" %s", &strtab[stabp->n_strx + file_string_table_offset]);
	  else
	    printf (" *");
	}
    }
  printf ("\n\n");
}

void
dump_section_stabs (abfd, stabsect_name, strsect_name)
     bfd *abfd;
     char *stabsect_name;
     char *strsect_name;
{
  asection *s;

  /* Check for section names for which stabsect_name is a prefix, to
     handle .stab0, etc.  */
  for (s = abfd->sections;
       s != NULL;
       s = s->next)
    {
      if (strncmp (stabsect_name, s->name, strlen (stabsect_name)) == 0
	  && strncmp (strsect_name, s->name, strlen (strsect_name)) != 0)
	{
	  if (read_section_stabs (abfd, s->name, strsect_name))
	    {
	      print_section_stabs (abfd, s->name, strsect_name);
	      free (stabs);
	      free (strtab);
	    }
	}
    }
}

static void
dump_bfd_header (abfd)
     bfd *abfd;
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

static void
dump_bfd_private_header (abfd)
bfd *abfd;
{
  bfd_print_private_bfd_data (abfd, stdout);
}
static void
display_bfd (abfd)
     bfd *abfd;
{
  char **matching;

  if (!bfd_check_format_matches (abfd, bfd_object, &matching))
    {
      bfd_nonfatal (bfd_get_filename (abfd));
      if (bfd_get_error () == bfd_error_file_ambiguously_recognized)
	{
	  list_matching_formats (matching);
	  free (matching);
	}
      return;
    }

  printf ("\n%s:     file format %s\n", bfd_get_filename (abfd),
	  abfd->xvec->name);
  if (dump_ar_hdrs)
    print_arelt_descr (stdout, abfd, true);
  if (dump_file_header)
    dump_bfd_header (abfd);
  if (dump_private_headers)
    dump_bfd_private_header (abfd);
  putchar ('\n');
  if (dump_section_headers)
    dump_headers (abfd);
  if (dump_symtab || dump_reloc_info || disassemble)
    {
      syms = slurp_symtab (abfd);
    }
  if (dump_dynamic_symtab || dump_dynamic_reloc_info)
    {
      dynsyms = slurp_dynamic_symtab (abfd);
    }
  if (dump_symtab)
    dump_symbols (abfd, false);
  if (dump_dynamic_symtab)
    dump_symbols (abfd, true);
  if (dump_stab_section_info)
    dump_stabs (abfd);
  if (dump_reloc_info && ! disassemble)
    dump_relocs (abfd);
  if (dump_dynamic_reloc_info)
    dump_dynamic_relocs (abfd);
  if (dump_section_contents)
    dump_data (abfd);
  if (disassemble)
    disassemble_data (abfd);
  if (syms)
    {
      free (syms);
      syms = NULL;
    }
  if (dynsyms)
    {
      free (dynsyms);
      dynsyms = NULL;
    }
}

static void
display_file (filename, target)
     char *filename;
     char *target;
{
  bfd *file, *arfile = (bfd *) NULL;

  file = bfd_openr (filename, target);
  if (file == NULL)
    {
      bfd_nonfatal (filename);
      return;
    }

  if (bfd_check_format (file, bfd_archive) == true)
    {
      bfd *last_arfile = NULL;

      printf ("In archive %s:\n", bfd_get_filename (file));
      for (;;)
	{
	  bfd_set_error (bfd_error_no_error);

	  arfile = bfd_openr_next_archived_file (file, arfile);
	  if (arfile == NULL)
	    {
	      if (bfd_get_error () != bfd_error_no_more_archived_files)
		{
		  bfd_nonfatal (bfd_get_filename (file));
		}
	      break;
	    }

	  display_bfd (arfile);

	  if (last_arfile != NULL)
	    bfd_close (last_arfile);
	  last_arfile = arfile;
	}

      if (last_arfile != NULL)
	bfd_close (last_arfile);
    }
  else
    display_bfd (file);

  bfd_close (file);
}

/* Actually display the various requested regions */

static void
dump_data (abfd)
     bfd *abfd;
{
  asection *section;
  bfd_byte *data = 0;
  bfd_size_type datasize = 0;
  bfd_size_type i;
  bfd_size_type start, stop;

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

	      if (bfd_section_size (abfd, section) == 0)
		continue;
	      data = (bfd_byte *) xmalloc ((size_t) bfd_section_size (abfd, section));
	      datasize = bfd_section_size (abfd, section);


	      bfd_get_section_contents (abfd, section, (PTR) data, 0, bfd_section_size (abfd, section));

	      if (start_address == (bfd_vma) -1
		  || start_address < section->vma)
		start = 0;
	      else
		start = start_address - section->vma;
	      if (stop_address == (bfd_vma) -1)
		stop = bfd_section_size (abfd, section);
	      else
		{
		  if (stop_address < section->vma)
		    stop = 0;
		  else
		    stop = stop_address - section->vma;
		  if (stop > bfd_section_size (abfd, section))
		    stop = bfd_section_size (abfd, section);
		}
	      for (i = start; i < stop; i += onaline)
		{
		  bfd_size_type j;

		  printf (" %04lx ", (unsigned long int) (i + section->vma));
		  for (j = i; j < i + onaline; j++)
		    {
		      if (j < stop)
			printf ("%02x", (unsigned) (data[j]));
		      else
			printf ("  ");
		      if ((j & 3) == 3)
			printf (" ");
		    }

		  printf (" ");
		  for (j = i; j < i + onaline; j++)
		    {
		      if (j >= stop)
			printf (" ");
		      else
			printf ("%c", isprint (data[j]) ? data[j] : '.');
		    }
		  putchar ('\n');
		}
	      free (data);
	    }
	}
    }
}

/* Should perhaps share code and display with nm? */
static void
dump_symbols (abfd, dynamic)
     bfd *abfd;
     boolean dynamic;
{
  asymbol **current;
  long max;
  long count;

  if (dynamic)
    {
      current = dynsyms;
      max = dynsymcount;
      if (max == 0)
	return;
      printf ("DYNAMIC SYMBOL TABLE:\n");
    }
  else
    {
      current = syms;
      max = symcount;
      if (max == 0)
	return;
      printf ("SYMBOL TABLE:\n");
    }

  for (count = 0; count < max; count++)
    {
      if (*current)
	{
	  bfd *cur_bfd = bfd_asymbol_bfd(*current);
	  if (cur_bfd)
	    {
	      bfd_print_symbol (cur_bfd,
				stdout,
				*current, bfd_print_symbol_all);
	      printf ("\n");
	    }
	}
      current++;
    }
  printf ("\n");
  printf ("\n");
}

static void
dump_relocs (abfd)
     bfd *abfd;
{
  arelent **relpp;
  long relcount;
  asection *a;

  for (a = abfd->sections; a != (asection *) NULL; a = a->next)
    {
      long relsize;

      if (bfd_is_abs_section (a))
	continue;
      if (bfd_is_und_section (a))
	continue;
      if (bfd_is_com_section (a))
	continue;

      if (only)
	{
	  if (strcmp (only, a->name))
	    continue;
	}
      else if ((a->flags & SEC_RELOC) == 0)
	continue;

      relsize = bfd_get_reloc_upper_bound (abfd, a);
      if (relsize < 0)
	bfd_fatal (bfd_get_filename (abfd));

      printf ("RELOCATION RECORDS FOR [%s]:", a->name);

      if (relsize == 0)
	{
	  printf (" (none)\n\n");
	}
      else
	{
	  relpp = (arelent **) xmalloc (relsize);
	  relcount = bfd_canonicalize_reloc (abfd, a, relpp, syms);
	  if (relcount < 0)
	    bfd_fatal (bfd_get_filename (abfd));
	  else if (relcount == 0)
	    {
	      printf (" (none)\n\n");
	    }
	  else
	    {
	      printf ("\n");
	      dump_reloc_set (abfd, relpp, relcount);
	      printf ("\n\n");
	    }
	  free (relpp);
	}
    }
}

static void
dump_dynamic_relocs (abfd)
     bfd *abfd;
{
  long relsize;
  arelent **relpp;
  long relcount;

  relsize = bfd_get_dynamic_reloc_upper_bound (abfd);
  if (relsize < 0)
    bfd_fatal (bfd_get_filename (abfd));

  printf ("DYNAMIC RELOCATION RECORDS");

  if (relsize == 0)
    {
      printf (" (none)\n\n");
    }
  else
    {
      relpp = (arelent **) xmalloc (relsize);
      relcount = bfd_canonicalize_dynamic_reloc (abfd, relpp, dynsyms);
      if (relcount < 0)
	bfd_fatal (bfd_get_filename (abfd));
      else if (relcount == 0)
	{
	  printf (" (none)\n\n");
	}
      else
	{
	  printf ("\n");
	  dump_reloc_set (abfd, relpp, relcount);
	  printf ("\n\n");
	}
      free (relpp);
    }
}

static void
dump_reloc_set (abfd, relpp, relcount)
     bfd *abfd;
     arelent **relpp;
     long relcount;
{
  arelent **p;

  /* Get column headers lined up reasonably.  */
  {
    static int width;
    if (width == 0)
      {
	char buf[30];
	sprintf_vma (buf, (bfd_vma) -1);
	width = strlen (buf) - 7;
      }
    printf ("OFFSET %*s TYPE %*s VALUE \n", width, "", 12, "");
  }

  for (p = relpp; relcount && *p != (arelent *) NULL; p++, relcount--)
    {
      arelent *q = *p;
      CONST char *sym_name;
      CONST char *section_name;

      if (start_address != (bfd_vma) -1
	  && q->address < start_address)
	continue;
      if (stop_address != (bfd_vma) -1
	  && q->address > stop_address)
	continue;

      if (q->sym_ptr_ptr && *q->sym_ptr_ptr)
	{
	  sym_name = (*(q->sym_ptr_ptr))->name;
	  section_name = (*(q->sym_ptr_ptr))->section->name;
	}
      else
	{
	  sym_name = NULL;
	  section_name = NULL;
	}
      if (sym_name)
	{
	  printf_vma (q->address);
	  printf (" %-16s  %s",
		  q->howto->name,
		  sym_name);
	}
      else
	{
	  if (section_name == (CONST char *) NULL)
	    section_name = "*unknown*";
	  printf_vma (q->address);
	  printf (" %-16s  [%s]",
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
}

/* The length of the longest architecture name + 1.  */
#define LONGEST_ARCH sizeof("rs6000:6000")

#ifndef L_tmpnam
#define L_tmpnam 25
#endif

/* List the targets that BFD is configured to support, each followed
   by its endianness and the architectures it supports.  */

static void
display_target_list ()
{
  extern char *tmpnam ();
  extern bfd_target *bfd_target_vector[];
  char tmparg[L_tmpnam];
  char *dummy_name;
  int t;

  dummy_name = tmpnam (tmparg);
  for (t = 0; bfd_target_vector[t]; t++)
    {
      bfd_target *p = bfd_target_vector[t];
      bfd *abfd = bfd_openw (dummy_name, p->name);
      int a;

      printf ("%s\n (header %s, data %s)\n", p->name,
	      p->header_byteorder_big_p ? "big endian" : "little endian",
	      p->byteorder_big_p ? "big endian" : "little endian");

      if (abfd == NULL)
	{
	  bfd_nonfatal (dummy_name);
	  continue;
	}

      if (! bfd_set_format (abfd, bfd_object))
	{
	  if (bfd_get_error () != bfd_error_invalid_operation)
	    bfd_nonfatal (p->name);
	  continue;
	}

      for (a = (int) bfd_arch_obscure + 1; a < (int) bfd_arch_last; a++)
	if (bfd_set_arch_mach (abfd, (enum bfd_architecture) a, 0))
	  printf ("  %s\n",
		  bfd_printable_arch_mach ((enum bfd_architecture) a, 0));
    }
  unlink (dummy_name);
}

/* Print a table showing which architectures are supported for entries
   FIRST through LAST-1 of bfd_target_vector (targets across,
   architectures down).  */

static void
display_info_table (first, last)
     int first;
     int last;
{
  extern bfd_target *bfd_target_vector[];
  extern char *tmpnam ();
  char tmparg[L_tmpnam];
  int t, a;
  char *dummy_name;

  /* Print heading of target names.  */
  printf ("\n%*s", (int) LONGEST_ARCH, " ");
  for (t = first; t < last && bfd_target_vector[t]; t++)
    printf ("%s ", bfd_target_vector[t]->name);
  putchar ('\n');

  dummy_name = tmpnam (tmparg);
  for (a = (int) bfd_arch_obscure + 1; a < (int) bfd_arch_last; a++)
    if (strcmp (bfd_printable_arch_mach (a, 0), "UNKNOWN!") != 0)
      {
	printf ("%*s ", (int) LONGEST_ARCH - 1,
		bfd_printable_arch_mach (a, 0));
	for (t = first; t < last && bfd_target_vector[t]; t++)
	  {
	    bfd_target *p = bfd_target_vector[t];
	    boolean ok = true;
	    bfd *abfd = bfd_openw (dummy_name, p->name);

	    if (abfd == NULL)
	      {
		bfd_nonfatal (p->name);
		ok = false;
	      }

	    if (ok)
	      {
		if (! bfd_set_format (abfd, bfd_object))
		  {
		    if (bfd_get_error () != bfd_error_invalid_operation)
		      bfd_nonfatal (p->name);
		    ok = false;
		  }
	      }

	    if (ok)
	      {
		if (! bfd_set_arch_mach (abfd, a, 0))
		  ok = false;
	      }

	    if (ok)
	      printf ("%s ", p->name);
	    else
	      {
		int l = strlen (p->name);
		while (l--)
		  putchar ('-');
		putchar (' ');
	      }
	  }
	putchar ('\n');
      }
  unlink (dummy_name);
}

/* Print tables of all the target-architecture combinations that
   BFD has been configured to support.  */

static void
display_target_tables ()
{
  int t, columns;
  extern bfd_target *bfd_target_vector[];
  char *colum;
  extern char *getenv ();

  columns = 0;
  colum = getenv ("COLUMNS");
  if (colum != NULL)
    columns = atoi (colum);
  if (columns == 0)
    columns = 80;

  t = 0;
  while (bfd_target_vector[t] != NULL)
    {
      int oldt = t, wid;

      wid = LONGEST_ARCH + strlen (bfd_target_vector[t]->name) + 1;
      ++t;
      while (wid < columns && bfd_target_vector[t] != NULL)
	{
	  int newwid;

	  newwid = wid + strlen (bfd_target_vector[t]->name) + 1;
	  if (newwid >= columns)
	    break;
	  wid = newwid;
	  ++t;
	}
      display_info_table (oldt, t);
    }
}

static void
display_info ()
{
  printf ("BFD header file version %s\n", BFD_VERSION);
  display_target_list ();
  display_target_tables ();
}

int
main (argc, argv)
     int argc;
     char **argv;
{
  int c;
  char *target = default_target;
  boolean seenflag = false;

  program_name = *argv;
  xmalloc_set_program_name (program_name);

  START_PROGRESS (program_name, 0);

  bfd_init ();

  while ((c = getopt_long (argc, argv, "pib:m:VdDlfahrRtTxsSj:w", long_options,
			   (int *) 0))
	 != EOF)
    {
      if (c != 'l' && c != OPTION_START_ADDRESS && c != OPTION_STOP_ADDRESS)
	seenflag = true;
      switch (c)
	{
	case 0:
	  break;		/* we've been given a long option */
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
	  formats_info = true;
	  break;
	case 'p':
	  dump_private_headers = 1;
	  break;
	case 'x':
	  dump_private_headers = 1;
	  dump_symtab = 1;
	  dump_reloc_info = 1;
	  dump_file_header = true;
	  dump_ar_hdrs = 1;
	  dump_section_headers = 1;
	  break;
	case 't':
	  dump_symtab = 1;
	  break;
	case 'T':
	  dump_dynamic_symtab = 1;
	  break;
	case 'd':
	  disassemble = true;
	  break;
	case 'D':
	  disassemble = disassemble_all = true;
	  break;
	case 'S':
	  disassemble = true;
	  with_source_code = true;
	  break;
	case 's':
	  dump_section_contents = 1;
	  break;
	case 'r':
	  dump_reloc_info = 1;
	  break;
	case 'R':
	  dump_dynamic_reloc_info = 1;
	  break;
	case 'a':
	  dump_ar_hdrs = 1;
	  break;
	case 'h':
	  dump_section_headers = 1;
	  break;
	case 'H':
	  usage (stdout, 0);
	case 'V':
	  show_version = 1;
	  break;
	case 'w':
	  wide_output = 1;
	  break;
	case OPTION_START_ADDRESS:
	  start_address = parse_vma (optarg, "--start-address");
	  break;
	case OPTION_STOP_ADDRESS:
	  stop_address = parse_vma (optarg, "--stop-address");
	  break;
	default:
	  usage (stderr, 1);
	}
    }

  if (show_version)
    {
      printf ("GNU %s version %s\n", program_name, program_version);
      exit (0);
    }

  if (seenflag == false)
    usage (stderr, 1);

  if (formats_info)
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

  END_PROGRESS (program_name);

  return 0;
}
