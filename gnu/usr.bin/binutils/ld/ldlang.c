/* Linker command language support.
   Copyright (C) 1991, 92, 93, 94, 1995 Free Software Foundation, Inc.

This file is part of GLD, the Gnu Linker.

GLD is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 1, or (at your option)
any later version.

GLD is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GLD; see the file COPYING.  If not, write to
the Free Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include "bfd.h"
#include "sysdep.h"
#include "libiberty.h"
#include "bfdlink.h"

#include "ld.h"
#include "ldmain.h"
#include "ldgram.h"
#include "ldexp.h"
#include "ldlang.h"
#include "ldemul.h"
#include "ldlex.h"
#include "ldmisc.h"
#include "ldctor.h"
#include "ldfile.h"

/* FORWARDS */
static void print_statements PARAMS ((void));
static void print_statement PARAMS ((lang_statement_union_type *,
				      lang_output_section_statement_type *));
static lang_statement_union_type *new_statement PARAMS ((enum statement_enum,
							 size_t,
							 lang_statement_list_type*));


/* LOCALS */
static struct obstack stat_obstack;

#define obstack_chunk_alloc xmalloc
#define obstack_chunk_free free
static CONST char *startup_file;
static lang_statement_list_type input_file_chain;
static boolean placed_commons = false;
static lang_output_section_statement_type *default_common_section;
static boolean map_option_f;
static bfd_vma print_dot;
static lang_input_statement_type *first_file;
static lang_statement_list_type lang_output_section_statement;
static CONST char *current_target;
static CONST char *output_target;
static int longest_section_name = 8;
static lang_statement_list_type statement_list;

static void print_size PARAMS ((size_t value));
static void print_alignment PARAMS ((unsigned int value));
static void print_fill PARAMS ((fill_type value));
static void print_section PARAMS ((const char *name));
static void lang_for_each_statement_worker
  PARAMS ((void (*func) (lang_statement_union_type *),
	   lang_statement_union_type *s));
static lang_input_statement_type *new_afile
  PARAMS ((const char *name, lang_input_file_enum_type file_type,
	   const char *target, boolean add_to_list));
static void print_flags PARAMS ((int *ignore_flags));
static void init_os PARAMS ((lang_output_section_statement_type *s));
static void wild_section PARAMS ((lang_wild_statement_type *ptr,
				  const char *section,
				  lang_input_statement_type *file,
				  lang_output_section_statement_type *output));
static lang_input_statement_type *lookup_name PARAMS ((const char *name));
static void load_symbols PARAMS ((lang_input_statement_type *entry,
				  lang_statement_list_type *));
static void wild PARAMS ((lang_wild_statement_type *s,
			  const char *section, const char *file,
			  const char *target,
			  lang_output_section_statement_type *output));
static bfd *open_output PARAMS ((const char *name));
static void ldlang_open_output PARAMS ((lang_statement_union_type *statement));
static void open_input_bfds
  PARAMS ((lang_statement_union_type *statement, boolean));
static void lang_reasonable_defaults PARAMS ((void));
static void lang_place_undefineds PARAMS ((void));
static void map_input_to_output_sections
  PARAMS ((lang_statement_union_type *s,
	   const char *target,
	   lang_output_section_statement_type *output_section_statement));
static void print_output_section_statement
  PARAMS ((lang_output_section_statement_type *output_section_statement));
static void print_assignment
  PARAMS ((lang_assignment_statement_type *assignment,
	   lang_output_section_statement_type *output_section));
static void print_input_statement PARAMS ((lang_input_statement_type *statm));
static void print_input_section PARAMS ((lang_input_section_type *in));
static void print_fill_statement PARAMS ((lang_fill_statement_type *fill));
static void print_data_statement PARAMS ((lang_data_statement_type *data));
static void print_reloc_statement PARAMS ((lang_reloc_statement_type *reloc));
static void print_padding_statement PARAMS ((lang_padding_statement_type *s));
static void print_wild_statement
  PARAMS ((lang_wild_statement_type *w,
	   lang_output_section_statement_type *os));
static void print_group
  PARAMS ((lang_group_statement_type *, lang_output_section_statement_type *));
static void print_statement PARAMS ((lang_statement_union_type *s,
				     lang_output_section_statement_type *os));
static void print_statements PARAMS ((void));
static bfd_vma insert_pad PARAMS ((lang_statement_union_type **this_ptr,
				   fill_type fill, unsigned int power,
				   asection *output_section_statement,
				   bfd_vma dot));
static bfd_vma size_input_section
  PARAMS ((lang_statement_union_type **this_ptr,
	   lang_output_section_statement_type *output_section_statement,
	   fill_type fill, bfd_vma dot, boolean relax));
static void lang_finish PARAMS ((void));
static void lang_check PARAMS ((void));
static void lang_common PARAMS ((void));
static boolean lang_one_common PARAMS ((struct bfd_link_hash_entry *, PTR));
static void lang_place_orphans PARAMS ((void));
static int topower PARAMS ((int));
static void lang_set_startof PARAMS ((void));
static void reset_memory_regions PARAMS ((void));

/* EXPORTS */
lang_output_section_statement_type *abs_output_section;
lang_statement_list_type *stat_ptr = &statement_list;
lang_statement_list_type file_chain = { 0 };
const char *entry_symbol = NULL;
boolean entry_from_cmdline;
boolean lang_has_input_file = false;
boolean had_output_filename = false;
boolean lang_float_flag = false;
boolean delete_output_file_on_failure = false;

etree_type *base; /* Relocation base - or null */


#if defined(__STDC__) || defined(ALMOST_STDC)
#define cat(a,b) a##b
#else
#define cat(a,b) a/**/b
#endif

#define new_stat(x,y) (cat(x,_type)*) new_statement(cat(x,_enum), sizeof(cat(x,_type)),y)

#define outside_section_address(q) ( (q)->output_offset + (q)->output_section->vma)

#define outside_symbol_address(q) ((q)->value +   outside_section_address(q->section))

PTR
stat_alloc (size)
     size_t size;
{
  return obstack_alloc (&stat_obstack, size);
}

static void
print_size (value)
     size_t value;
{
  fprintf (config.map_file, "%5x", (unsigned) value);
}

static void
print_alignment (value)
     unsigned int value;
{
  fprintf (config.map_file, "2**%1u", value);
}

static void
print_fill (value)
     fill_type value;
{
  fprintf (config.map_file, "%04x", (unsigned) value);
}

static void
print_section (name)
     CONST char *name;
{
  fprintf (config.map_file, "%*s", -longest_section_name, name);
}

/*----------------------------------------------------------------------
  lang_for_each_statement walks the parse tree and calls the provided
  function for each node
*/

static void
lang_for_each_statement_worker (func, s)
     void (*func) PARAMS ((lang_statement_union_type *));
     lang_statement_union_type *s;
{
  for (; s != (lang_statement_union_type *) NULL; s = s->next)
    {
      func (s);

      switch (s->header.type)
	{
	case lang_constructors_statement_enum:
	  lang_for_each_statement_worker (func, constructor_list.head);
	  break;
	case lang_output_section_statement_enum:
	  lang_for_each_statement_worker
	    (func,
	     s->output_section_statement.children.head);
	  break;
	case lang_wild_statement_enum:
	  lang_for_each_statement_worker
	    (func,
	     s->wild_statement.children.head);
	  break;
	case lang_group_statement_enum:
	  lang_for_each_statement_worker (func,
					  s->group_statement.children.head);
	  break;
	case lang_data_statement_enum:
	case lang_reloc_statement_enum:
	case lang_object_symbols_statement_enum:
	case lang_output_statement_enum:
	case lang_target_statement_enum:
	case lang_input_section_enum:
	case lang_input_statement_enum:
	case lang_assignment_statement_enum:
	case lang_padding_statement_enum:
	case lang_address_statement_enum:
	  break;
	default:
	  FAIL ();
	  break;
	}
    }
}

void
lang_for_each_statement (func)
     void (*func) PARAMS ((lang_statement_union_type *));
{
  lang_for_each_statement_worker (func,
				  statement_list.head);
}

/*----------------------------------------------------------------------*/
void
lang_list_init (list)
     lang_statement_list_type *list;
{
  list->head = (lang_statement_union_type *) NULL;
  list->tail = &list->head;
}

/*----------------------------------------------------------------------

  build a new statement node for the parse tree

 */

static
lang_statement_union_type *
new_statement (type, size, list)
     enum statement_enum type;
     size_t size;
     lang_statement_list_type * list;
{
  lang_statement_union_type *new = (lang_statement_union_type *)
  stat_alloc (size);

  new->header.type = type;
  new->header.next = (lang_statement_union_type *) NULL;
  lang_statement_append (list, new, &new->header.next);
  return new;
}

/*
  Build a new input file node for the language. There are several ways
  in which we treat an input file, eg, we only look at symbols, or
  prefix it with a -l etc.

  We can be supplied with requests for input files more than once;
  they may, for example be split over serveral lines like foo.o(.text)
  foo.o(.data) etc, so when asked for a file we check that we havn't
  got it already so we don't duplicate the bfd.

 */
static lang_input_statement_type *
new_afile (name, file_type, target, add_to_list)
     CONST char *name;
     lang_input_file_enum_type file_type;
     CONST char *target;
     boolean add_to_list;
{
  lang_input_statement_type *p;

  if (add_to_list)
    p = new_stat (lang_input_statement, stat_ptr);
  else
    {
      p = ((lang_input_statement_type *)
	   stat_alloc (sizeof (lang_input_statement_type)));
      p->header.next = NULL;
    }

  lang_has_input_file = true;
  p->target = target;
  p->complained = false;
  switch (file_type)
    {
    case lang_input_file_is_symbols_only_enum:
      p->filename = name;
      p->is_archive = false;
      p->real = true;
      p->local_sym_name = name;
      p->just_syms_flag = true;
      p->search_dirs_flag = false;
      break;
    case lang_input_file_is_fake_enum:
      p->filename = name;
      p->is_archive = false;
      p->real = false;
      p->local_sym_name = name;
      p->just_syms_flag = false;
      p->search_dirs_flag = false;
      break;
    case lang_input_file_is_l_enum:
      p->is_archive = true;
      p->filename = name;
      p->real = true;
      p->local_sym_name = concat ("-l", name, (const char *) NULL);
      p->just_syms_flag = false;
      p->search_dirs_flag = true;
      break;
    case lang_input_file_is_marker_enum:
      p->filename = name;
      p->is_archive = false;
      p->real = false;
      p->local_sym_name = name;
      p->just_syms_flag = false;
      p->search_dirs_flag = true;
      break;
    case lang_input_file_is_search_file_enum:
      p->filename = name;
      p->is_archive = false;
      p->real = true;
      p->local_sym_name = name;
      p->just_syms_flag = false;
      p->search_dirs_flag = true;
      break;
    case lang_input_file_is_file_enum:
      p->filename = name;
      p->is_archive = false;
      p->real = true;
      p->local_sym_name = name;
      p->just_syms_flag = false;
      p->search_dirs_flag = false;
      break;
    default:
      FAIL ();
    }
  p->the_bfd = (bfd *) NULL;
  p->asymbols = (asymbol **) NULL;
  p->next_real_file = (lang_statement_union_type *) NULL;
  p->next = (lang_statement_union_type *) NULL;
  p->symbol_count = 0;
  p->common_output_section = (asection *) NULL;
  p->dynamic = config.dynamic_link;
  p->loaded = false;
  lang_statement_append (&input_file_chain,
			 (lang_statement_union_type *) p,
			 &p->next_real_file);
  return p;
}

lang_input_statement_type *
lang_add_input_file (name, file_type, target)
     CONST char *name;
     lang_input_file_enum_type file_type;
     CONST char *target;
{
  lang_has_input_file = true;
  return new_afile (name, file_type, target, true);
}

/* Build enough state so that the parser can build its tree */
void
lang_init ()
{
  obstack_begin (&stat_obstack, 1000);

  stat_ptr = &statement_list;

  lang_list_init (stat_ptr);

  lang_list_init (&input_file_chain);
  lang_list_init (&lang_output_section_statement);
  lang_list_init (&file_chain);
  first_file = lang_add_input_file ((char *) NULL,
				    lang_input_file_is_marker_enum,
				    (char *) NULL);
  abs_output_section = lang_output_section_statement_lookup (BFD_ABS_SECTION_NAME);

  abs_output_section->bfd_section = bfd_abs_section_ptr;

}

/*----------------------------------------------------------------------
 A region is an area of memory declared with the
 MEMORY {  name:org=exp, len=exp ... }
 syntax.

 We maintain a list of all the regions here

 If no regions are specified in the script, then the default is used
 which is created when looked up to be the entire data space
*/

static lang_memory_region_type *lang_memory_region_list;
static lang_memory_region_type **lang_memory_region_list_tail = &lang_memory_region_list;

lang_memory_region_type *
lang_memory_region_lookup (name)
     CONST char *CONST name;
{

  lang_memory_region_type *p = lang_memory_region_list;

  for (p = lang_memory_region_list;
       p != (lang_memory_region_type *) NULL;
       p = p->next)
    {
      if (strcmp (p->name, name) == 0)
	{
	  return p;
	}
    }

#if 0
  /* This code used to always use the first region in the list as the
     default region.  I changed it to instead use a region
     encompassing all of memory as the default region.  This permits
     NOLOAD sections to work reasonably without requiring a region.
     People should specify what region they mean, if they really want
     a region.  */
  if (strcmp (name, "*default*") == 0)
    {
      if (lang_memory_region_list != (lang_memory_region_type *) NULL)
	{
	  return lang_memory_region_list;
	}
    }
#endif

  {
    lang_memory_region_type *new =
    (lang_memory_region_type *) stat_alloc (sizeof (lang_memory_region_type));

    new->name = buystring (name);
    new->next = (lang_memory_region_type *) NULL;

    *lang_memory_region_list_tail = new;
    lang_memory_region_list_tail = &new->next;
    new->origin = 0;
    new->length = ~(bfd_size_type)0;
    new->current = 0;
    new->had_full_message = false;

    return new;
  }
}


lang_output_section_statement_type *
lang_output_section_find (name)
     CONST char *CONST name;
{
  lang_statement_union_type *u;
  lang_output_section_statement_type *lookup;

  for (u = lang_output_section_statement.head;
       u != (lang_statement_union_type *) NULL;
       u = lookup->next)
    {
      lookup = &u->output_section_statement;
      if (strcmp (name, lookup->name) == 0)
	{
	  return lookup;
	}
    }
  return (lang_output_section_statement_type *) NULL;
}

lang_output_section_statement_type *
lang_output_section_statement_lookup (name)
     CONST char *CONST name;
{
  lang_output_section_statement_type *lookup;

  lookup = lang_output_section_find (name);
  if (lookup == (lang_output_section_statement_type *) NULL)
    {

      lookup = (lang_output_section_statement_type *)
	new_stat (lang_output_section_statement, stat_ptr);
      lookup->region = (lang_memory_region_type *) NULL;
      lookup->fill = 0;
      lookup->block_value = 1;
      lookup->name = name;

      lookup->next = (lang_statement_union_type *) NULL;
      lookup->bfd_section = (asection *) NULL;
      lookup->processed = false;
      lookup->loadable = 1;
      lookup->addr_tree = (etree_type *) NULL;
      lang_list_init (&lookup->children);

      lookup->memspec = (CONST char *) NULL;
      lookup->flags = 0;
      lookup->subsection_alignment = -1;
      lookup->section_alignment = -1;
      lookup->load_base = (union etree_union *) NULL;

      lang_statement_append (&lang_output_section_statement,
			     (lang_statement_union_type *) lookup,
			     &lookup->next);
    }
  return lookup;
}

/*ARGSUSED*/
static void
print_flags (ignore_flags)
     int *ignore_flags;
{
  fprintf (config.map_file, "(");
#if 0
  if (flags->flag_read)
    fprintf (outfile, "R");
  if (flags->flag_write)
    fprintf (outfile, "W");
  if (flags->flag_executable)
    fprintf (outfile, "X");
  if (flags->flag_loadable)
    fprintf (outfile, "L");
#endif
 fprintf (config.map_file, ")");
}

void
lang_map ()
{
  lang_memory_region_type *m;

  fprintf (config.map_file, "**MEMORY CONFIGURATION**\n\n");
#ifdef BFD64
  fprintf (config.map_file, "name\t\torigin\t\tlength\t\tattributes\n");
#else
  fprintf (config.map_file,
	   "name\t\torigin   length   r_size   c_size    is    attributes\n");

#endif
  for (m = lang_memory_region_list;
       m != (lang_memory_region_type *) NULL;
       m = m->next)
    {
      fprintf (config.map_file, "%-16s", m->name);
      print_address (m->origin);
      print_space ();
      print_address ((bfd_vma)m->length);
      print_space ();
      print_address ((bfd_vma)m->old_length);
      print_space();
      print_address (m->current - m->origin);
      print_space();
      if (m->old_length)
       fprintf (config.map_file, " %2d%%  ",
		(int) ((m->current - m->origin) * 100 / m->old_length));
      print_flags (&m->flags);
      fprintf (config.map_file, "\n");
    }
  fprintf (config.map_file, "\n\n**LINK EDITOR MEMORY MAP**\n\n");
  fprintf (config.map_file, "output   input     virtual\n");
  fprintf (config.map_file, "section  section   address    tsize\n\n");

  print_statements ();

}

/*
 *
 */
static void
init_os (s)
     lang_output_section_statement_type * s;
{
/*  asection *section = bfd_get_section_by_name(output_bfd, s->name);*/
  section_userdata_type *new =
  (section_userdata_type *)
  stat_alloc (sizeof (section_userdata_type));

  s->bfd_section = bfd_get_section_by_name (output_bfd, s->name);
  if (s->bfd_section == (asection *) NULL)
    s->bfd_section = bfd_make_section (output_bfd, s->name);
  if (s->bfd_section == (asection *) NULL)
    {
      einfo ("%P%F: output format %s cannot represent section called %s\n",
	     output_bfd->xvec->name, s->name);
    }
  s->bfd_section->output_section = s->bfd_section;

  /* We initialize an output sections output offset to minus its own */
  /* vma to allow us to output a section through itself */
  s->bfd_section->output_offset = 0;
  get_userdata (s->bfd_section) = (PTR) new;

}

/***********************************************************************
  The wild routines.

  These expand statements like *(.text) and foo.o to a list of
  explicit actions, like foo.o(.text), bar.o(.text) and
  foo.o(.text,.data) .

  The toplevel routine, wild, takes a statement, section, file and
  target. If either the section or file is null it is taken to be the
  wildcard. Seperate lang_input_section statements are created for
  each part of the expanstion, and placed after the statement provided.

*/

void
wild_doit (ptr, section, output, file)
     lang_statement_list_type * ptr;
     asection * section;
     lang_output_section_statement_type * output;
     lang_input_statement_type * file;
{
  if (output->bfd_section == (asection *) NULL)
    init_os (output);

  if (section != (asection *) NULL
      && section->output_section == (asection *) NULL)
  {
    /* Add a section reference to the list */
    lang_input_section_type *new = new_stat (lang_input_section, ptr);

    new->section = section;
    new->ifile = file;
    section->output_section = output->bfd_section;

    /* We don't copy the SEC_NEVER_LOAD flag from an input section to
       an output section, because we want to be able to include a
       SEC_NEVER_LOAD section in the middle of an otherwise loaded
       section (I don't know why we want to do this, but we do).
       build_link_order in ldwrite.c handles this case by turning the
       embedded SEC_NEVER_LOAD section into a fill.  */
    section->output_section->flags |=
      section->flags & (flagword) (~ SEC_NEVER_LOAD);

    if (!output->loadable) 
    {
      /* Turn off load flag */
      output->bfd_section->flags &= ~SEC_LOAD;
      output->bfd_section->flags |= SEC_NEVER_LOAD;
    }
    if (section->alignment_power > output->bfd_section->alignment_power)
    {
      output->bfd_section->alignment_power = section->alignment_power;
    }
    /* If supplied an aligmnet, then force it */
    if (output->section_alignment != -1)
    {
      output->bfd_section->alignment_power = output->section_alignment;
    }
  }
}

static void
wild_section (ptr, section, file, output)
     lang_wild_statement_type *ptr;
     const char *section;
     lang_input_statement_type *file;
     lang_output_section_statement_type *output;
{
  if (file->just_syms_flag == false)
    {
      register asection *s;

      for (s = file->the_bfd->sections; s != NULL; s = s->next)
	{
	  /* Attach all sections named SECTION.  If SECTION is NULL,
	     then attach all sections.

	     Previously, if SECTION was NULL, this code did not call
	     wild_doit if the SEC_IS_COMMON flag was set for the
	     section.  I did not understand that, and I took it out.
	     --ian@cygnus.com.  */

	  if (section == NULL
	      || strcmp (bfd_get_section_name (file->the_bfd, s),
			 section) == 0)
	    wild_doit (&ptr->children, s, output, file);
	}
    }
}

/* passed a file name (which must have been seen already and added to
   the statement tree. We will see if it has been opened already and
   had its symbols read. If not then we'll read it.

   Archives are pecuilar here. We may open them once, but if they do
   not define anything we need at the time, they won't have all their
   symbols read. If we need them later, we'll have to redo it.  */
static lang_input_statement_type *
lookup_name (name)
     CONST char *name;
{
  lang_input_statement_type *search;

  for (search = (lang_input_statement_type *) input_file_chain.head;
       search != (lang_input_statement_type *) NULL;
       search = (lang_input_statement_type *) search->next_real_file)
    {
      if (search->filename == (char *) NULL && name == (char *) NULL)
	return search;
      if (search->filename != (char *) NULL
	  && name != (char *) NULL
	  && strcmp (search->filename, name) == 0)
	break;
    }

  if (search == (lang_input_statement_type *) NULL)
    search = new_afile (name, lang_input_file_is_file_enum, default_target,
			false);

  /* If we have already added this file, or this file is not real
     (FIXME: can that ever actually happen?) or the name is NULL
     (FIXME: can that ever actually happen?) don't add this file.  */
  if (search->loaded
      || ! search->real
      || search->filename == (const char *) NULL)
    return search;

  load_symbols (search, (lang_statement_list_type *) NULL);

  return search;
}

/* Get the symbols for an input file.  */

static void
load_symbols (entry, place)
     lang_input_statement_type *entry;
     lang_statement_list_type *place;
{
  char **matching;

  if (entry->loaded)
    return;

  ldfile_open_file (entry);

  if (! bfd_check_format (entry->the_bfd, bfd_archive)
      && ! bfd_check_format_matches (entry->the_bfd, bfd_object, &matching))
    {
      bfd_error_type err;
      lang_statement_list_type *hold;

      err = bfd_get_error ();
      if (err == bfd_error_file_ambiguously_recognized)
	{
	  char **p;

	  einfo ("%B: file not recognized: %E\n", entry->the_bfd);
	  einfo ("%B: matching formats:", entry->the_bfd);
	  for (p = matching; *p != NULL; p++)
	    einfo (" %s", *p);
	  einfo ("%F\n");
	}
      else if (err != bfd_error_file_not_recognized
	       || place == NULL)
	einfo ("%F%B: file not recognized: %E\n", entry->the_bfd);

      /* Try to interpret the file as a linker script.  */

      bfd_close (entry->the_bfd);
      entry->the_bfd = NULL;

      ldfile_open_command_file (entry->filename);

      hold = stat_ptr;
      stat_ptr = place;

      ldfile_assumed_script = true;
      parser_input = input_script;
      yyparse ();
      ldfile_assumed_script = false;

      stat_ptr = hold;

      return;
    }

  /* We don't call ldlang_add_file for an archive.  Instead, the
     add_symbols entry point will call ldlang_add_file, via the
     add_archive_element callback, for each element of the archive
     which is used.  */
  switch (bfd_get_format (entry->the_bfd))
    {
    default:
      break;

    case bfd_object:
      ldlang_add_file (entry);
      if (trace_files || trace_file_tries)
	info_msg ("%I\n", entry);
      break;

    case bfd_archive:
      if (whole_archive)
	{
	  bfd *member = bfd_openr_next_archived_file (entry->the_bfd,
						      (bfd *) NULL);
	  while (member != NULL)
	    {
	      if (! bfd_check_format (member, bfd_object))
		einfo ("%F%B: object %B in archive is not object\n",
		       entry->the_bfd, member);
	      if (! ((*link_info.callbacks->add_archive_element)
		     (&link_info, member, "-whole-archive")))
		abort ();
	      if (! bfd_link_add_symbols (member, &link_info))
		einfo ("%F%B: could not read symbols: %E\n", member);
	      member = bfd_openr_next_archived_file (entry->the_bfd,
						     member);
	    }

	  entry->loaded = true;

	  return;
	}
    }

  if (! bfd_link_add_symbols (entry->the_bfd, &link_info))
    einfo ("%F%B: could not read symbols: %E\n", entry->the_bfd);

  entry->loaded = true;
}

static void
wild (s, section, file, target, output)
     lang_wild_statement_type * s;
     CONST char *section;
     CONST char *file;
     CONST char *target;
     lang_output_section_statement_type * output;
{
  lang_input_statement_type *f;

  if (file == (char *) NULL)
    {
      /* Perform the iteration over all files in the list */
      for (f = (lang_input_statement_type *) file_chain.head;
	   f != (lang_input_statement_type *) NULL;
	   f = (lang_input_statement_type *) f->next)
	{
	  wild_section (s, section, f, output);
	}
    }
  else
    {
      /* Perform the iteration over a single file */
      wild_section (s, section, lookup_name (file), output);
    }
  if (section != (char *) NULL
      && strcmp (section, "COMMON") == 0
   && default_common_section == (lang_output_section_statement_type *) NULL)
    {
      /* Remember the section that common is going to incase we later
         get something which doesn't know where to put it */
      default_common_section = output;
    }
}

/*
  read in all the files
  */

static bfd *
open_output (name)
     CONST char *name;
{
  bfd *output;

  if (output_target == (char *) NULL)
    {
      if (current_target != (char *) NULL)
	output_target = current_target;
      else
	output_target = default_target;
    }
  output = bfd_openw (name, output_target);

  if (output == (bfd *) NULL)
    {
      if (bfd_get_error () == bfd_error_invalid_target)
	{
	  einfo ("%P%F: target %s not found\n", output_target);
	}
      einfo ("%P%F: cannot open output file %s: %E\n", name);
    }

  delete_output_file_on_failure = 1;

  /*  output->flags |= D_PAGED;*/

  if (! bfd_set_format (output, bfd_object))
    einfo ("%P%F:%s: can not make object file: %E\n", name);
  if (! bfd_set_arch_mach (output,
			   ldfile_output_architecture,
			   ldfile_output_machine))
    einfo ("%P%F:%s: can not set architecture: %E\n", name);

  link_info.hash = bfd_link_hash_table_create (output);
  if (link_info.hash == (struct bfd_link_hash_table *) NULL)
    einfo ("%P%F: can not create link hash table: %E\n");

  bfd_set_gp_size (output, g_switch_value);
  return output;
}




static void
ldlang_open_output (statement)
     lang_statement_union_type * statement;
{
  switch (statement->header.type)
    {
    case lang_output_statement_enum:
      ASSERT (output_bfd == (bfd *) NULL);
      output_bfd = open_output (statement->output_statement.name);
      ldemul_set_output_arch ();
      if (config.magic_demand_paged && !link_info.relocateable)
	output_bfd->flags |= D_PAGED;
      else
	output_bfd->flags &= ~D_PAGED;
      if (config.text_read_only)
	output_bfd->flags |= WP_TEXT;
      else
	output_bfd->flags &= ~WP_TEXT;
      if (config.traditional_format)
	output_bfd->flags |= BFD_TRADITIONAL_FORMAT;
      else
	output_bfd->flags &= ~BFD_TRADITIONAL_FORMAT;
      break;

    case lang_target_statement_enum:
      current_target = statement->target_statement.target;
      break;
    default:
      break;
    }
}

/* Open all the input files.  */

static void
open_input_bfds (s, force)
     lang_statement_union_type *s;
     boolean force;
{
  for (; s != (lang_statement_union_type *) NULL; s = s->next)
    {
      switch (s->header.type)
	{
	case lang_constructors_statement_enum:
	  open_input_bfds (constructor_list.head, force);
	  break;
	case lang_output_section_statement_enum:
	  open_input_bfds (s->output_section_statement.children.head, force);
	  break;
	case lang_wild_statement_enum:
	  /* Maybe we should load the file's symbols */
	  if (s->wild_statement.filename)
	    (void) lookup_name (s->wild_statement.filename);
	  open_input_bfds (s->wild_statement.children.head, force);
	  break;
	case lang_group_statement_enum:
	  {
	    struct bfd_link_hash_entry *undefs;

	    /* We must continually search the entries in the group
               until no new symbols are added to the list of undefined
               symbols.  */

	    do
	      {
		undefs = link_info.hash->undefs_tail;
		open_input_bfds (s->group_statement.children.head, true);
	      }
	    while (undefs != link_info.hash->undefs_tail);
	  }
	  break;
	case lang_target_statement_enum:
	  current_target = s->target_statement.target;
	  break;
	case lang_input_statement_enum:
	  if (s->input_statement.real == true)
	    {
	      lang_statement_list_type add;

	      s->input_statement.target = current_target;

	      /* If we are being called from within a group, and this
                 is an archive which has already been searched, then
                 force it to be researched.  */
	      if (force
		  && s->input_statement.loaded
		  && bfd_check_format (s->input_statement.the_bfd,
				       bfd_archive))
		s->input_statement.loaded = false;

	      lang_list_init (&add);

	      load_symbols (&s->input_statement, &add);

	      if (add.head != NULL)
		{
		  *add.tail = s->next;
		  s->next = add.head;
		}
	    }
	  break;
	default:
	  break;
	}
    }
}

/* If there are [COMMONS] statements, put a wild one into the bss section */

static void
lang_reasonable_defaults ()
{
#if 0
  lang_output_section_statement_lookup (".text");
  lang_output_section_statement_lookup (".data");

  default_common_section =
    lang_output_section_statement_lookup (".bss");


  if (placed_commons == false)
    {
      lang_wild_statement_type *new =
      new_stat (lang_wild_statement,
		&default_common_section->children);

      new->section_name = "COMMON";
      new->filename = (char *) NULL;
      lang_list_init (&new->children);
    }
#endif

}

/*
 Add the supplied name to the symbol table as an undefined reference.
 Remove items from the chain as we open input bfds
 */
typedef struct ldlang_undef_chain_list
{
  struct ldlang_undef_chain_list *next;
  char *name;
}                       ldlang_undef_chain_list_type;

static ldlang_undef_chain_list_type *ldlang_undef_chain_list_head;

void
ldlang_add_undef (name)
     CONST char *CONST name;
{
  ldlang_undef_chain_list_type *new =
    ((ldlang_undef_chain_list_type *)
     stat_alloc (sizeof (ldlang_undef_chain_list_type)));

  new->next = ldlang_undef_chain_list_head;
  ldlang_undef_chain_list_head = new;

  new->name = buystring (name);
}

/* Run through the list of undefineds created above and place them
   into the linker hash table as undefined symbols belonging to the
   script file.
*/
static void
lang_place_undefineds ()
{
  ldlang_undef_chain_list_type *ptr;

  for (ptr = ldlang_undef_chain_list_head;
       ptr != (ldlang_undef_chain_list_type *) NULL;
       ptr = ptr->next)
    {
      struct bfd_link_hash_entry *h;

      h = bfd_link_hash_lookup (link_info.hash, ptr->name, true, false, true);
      if (h == (struct bfd_link_hash_entry *) NULL)
	einfo ("%P%F: bfd_link_hash_lookup failed: %E");
      if (h->type == bfd_link_hash_new)
	{
	  h->type = bfd_link_hash_undefined;
	  h->u.undef.abfd = NULL;
	  bfd_link_add_undef (link_info.hash, h);
	}
    }
}

/* Open input files and attatch to output sections */
static void
map_input_to_output_sections (s, target, output_section_statement)
     lang_statement_union_type * s;
     CONST char *target;
     lang_output_section_statement_type * output_section_statement;
{
  for (; s != (lang_statement_union_type *) NULL; s = s->next)
    {
      switch (s->header.type)
	{


	case lang_wild_statement_enum:
	  wild (&s->wild_statement, s->wild_statement.section_name,
		s->wild_statement.filename, target,
		output_section_statement);

	  break;
	case lang_constructors_statement_enum:
	  map_input_to_output_sections (constructor_list.head,
					target,
					output_section_statement);
	  break;
	case lang_output_section_statement_enum:
	  map_input_to_output_sections (s->output_section_statement.children.head,
					target,
					&s->output_section_statement);
	  break;
	case lang_output_statement_enum:
	  break;
	case lang_target_statement_enum:
	  target = s->target_statement.target;
	  break;
	case lang_group_statement_enum:
	  map_input_to_output_sections (s->group_statement.children.head,
					target,
					output_section_statement);
	  break;
	case lang_fill_statement_enum:
	case lang_input_section_enum:
	case lang_object_symbols_statement_enum:
	case lang_data_statement_enum:
	case lang_reloc_statement_enum:
	case lang_assignment_statement_enum:
	case lang_padding_statement_enum:
	case lang_input_statement_enum:
	  if (output_section_statement != NULL
	      && output_section_statement->bfd_section == NULL)
	    init_os (output_section_statement);
	  break;
	case lang_afile_asection_pair_statement_enum:
	  FAIL ();
	  break;
	case lang_address_statement_enum:
	  /* Mark the specified section with the supplied address */
	  {
	    lang_output_section_statement_type *os =
	      lang_output_section_statement_lookup
		(s->address_statement.section_name);

	    if (os->bfd_section == NULL)
	      init_os (os);
	    os->addr_tree = s->address_statement.address;
	  }
	  break;
	}
    }
}

static void
print_output_section_statement (output_section_statement)
     lang_output_section_statement_type * output_section_statement;
{
  asection *section = output_section_statement->bfd_section;

  print_nl ();
  print_section (output_section_statement->name);


  if (section)
  {
    print_dot = section->vma;
    print_space ();
    print_section ("");
    print_space ();
    print_address (section->vma);
    print_space ();
    print_size (section->_raw_size);
    print_space ();
    print_size(section->_cooked_size);
    print_space ();
    print_alignment (section->alignment_power);
    print_space ();
#if 0
    fprintf (config.map_file, "%s flags", output_section_statement->region->name);
    print_flags (stdout, &output_section_statement->flags);
#endif
    if (section->flags & SEC_LOAD)
     fprintf (config.map_file, "load ");
    if (section->flags & SEC_ALLOC)
     fprintf (config.map_file, "alloc ");
    if (section->flags & SEC_RELOC)
     fprintf (config.map_file, "reloc ");
    if (section->flags & SEC_HAS_CONTENTS)
     fprintf (config.map_file, "contents ");

  }
  else
  {
    fprintf (config.map_file, " (no attached output section)");
  }
  print_nl ();
  if (output_section_statement->load_base)
    {
      bfd_vma b = exp_get_abs_int(output_section_statement->load_base,
				0, "output base", lang_final_phase_enum);
      fprintf (config.map_file, "Output address   ");
      fprintf_vma (config.map_file, b);
      fprintf (config.map_file, "\n");
    }
  if (output_section_statement->section_alignment >= 0
      || output_section_statement->subsection_alignment >= 0) 
  {
    fprintf (config.map_file, "\t\t\t\t\tforced alignment ");
    if (output_section_statement->section_alignment >= 0) 
    {
      fprintf (config.map_file, "section 2**%d ",output_section_statement->section_alignment );
    }
    if ( output_section_statement->subsection_alignment >= 0) 
    {
      fprintf (config.map_file, "subsection 2**%d ",output_section_statement->subsection_alignment );
    }
  
    print_nl ();
  }
  print_statement (output_section_statement->children.head,
		   output_section_statement);

}

static void
print_assignment (assignment, output_section)
     lang_assignment_statement_type * assignment;
     lang_output_section_statement_type * output_section;
{
  etree_value_type result;

  print_section ("");
  print_space ();
  print_section ("");
  print_space ();
  print_address (print_dot);
  print_space ();
  result = exp_fold_tree (assignment->exp->assign.src,
			  output_section,
			  lang_final_phase_enum,
			  print_dot,
			  &print_dot);

  if (result.valid)
    {
      print_address (result.value);
    }
  else
    {
      fprintf (config.map_file, "*undefined*");
    }
  print_space ();
  exp_print_tree (assignment->exp);

  fprintf (config.map_file, "\n");
}

static void
print_input_statement (statm)
     lang_input_statement_type * statm;
{
  if (statm->filename != (char *) NULL)
    {
      fprintf (config.map_file, "LOAD %s\n", statm->filename);
    }
}

/* Print all the defined symbols for the abfd provided by in the supplied
   section.
*/

static boolean 
print_one_symbol (hash_entry, ptr)
struct  bfd_link_hash_entry *hash_entry;
PTR ptr;
{
  asection * sec = (asection *)ptr;

  if (hash_entry->type == bfd_link_hash_defined
      || hash_entry->type == bfd_link_hash_defweak)
    {
      if (sec == hash_entry->u.def.section) {
	print_section ("");
	fprintf (config.map_file, " ");
	print_section ("");
	fprintf (config.map_file, " ");
	print_address (hash_entry->u.def.value + outside_section_address (sec));
	fprintf (config.map_file, "              %s", hash_entry->root.string);
	print_nl ();
      }
    }

  return true;
}

static void
print_input_section (in)
     lang_input_section_type * in;
{
  asection *i = in->section;
  bfd_size_type size = i->_cooked_size != 0 ? i->_cooked_size : i->_raw_size;

  if (size != 0)
    {
      print_section ("");
      fprintf (config.map_file, " ");
      print_section (i->name);
      fprintf (config.map_file, " ");
      if (i->output_section)
	{
	  print_address (i->output_section->vma + i->output_offset);
	  fprintf (config.map_file, " ");
	  print_size (i->_raw_size);
	  fprintf (config.map_file, " ");
	  print_size(i->_cooked_size);
	  fprintf (config.map_file, " ");
	  print_alignment (i->alignment_power);
	  fprintf (config.map_file, " ");
	  if (in->ifile)
	    {

	      bfd *abfd = in->ifile->the_bfd;

	      if (in->ifile->just_syms_flag == true)
		{
		  fprintf (config.map_file, "symbols only ");
		}

	      fprintf (config.map_file, " %s ", abfd->xvec->name);
	      if (abfd->my_archive != (bfd *) NULL)
		{
		  fprintf (config.map_file, "[%s]%s", abfd->my_archive->filename,
			   abfd->filename);
		}
	      else
		{
		  fprintf (config.map_file, "%s", abfd->filename);
		}
	      fprintf (config.map_file, "(overhead %d bytes)", (int) bfd_alloc_size (abfd));
	      print_nl ();

	      /* Print all the symbols */
	      bfd_link_hash_traverse (link_info.hash, print_one_symbol, (PTR) i);
	    }
	  else
	    {
	      print_nl ();
	    }


	  print_dot = outside_section_address (i) + size;
	}
      else
	{
	  fprintf (config.map_file, "No output section allocated\n");
	}
    }
}

static void
print_fill_statement (fill)
     lang_fill_statement_type * fill;
{
  fprintf (config.map_file, "FILL mask ");
  print_fill (fill->fill);
}

static void
print_data_statement (data)
     lang_data_statement_type * data;
{
/*  bfd_vma value; */
  print_section ("");
  print_space ();
  print_section ("");
  print_space ();
/*  ASSERT(print_dot == data->output_vma);*/

  print_address (data->output_vma + data->output_section->vma);
  print_space ();
  print_address (data->value);
  print_space ();
  switch (data->type)
    {
    case BYTE:
      fprintf (config.map_file, "BYTE ");
      print_dot += BYTE_SIZE;
      break;
    case SHORT:
      fprintf (config.map_file, "SHORT ");
      print_dot += SHORT_SIZE;
      break;
    case LONG:
      fprintf (config.map_file, "LONG ");
      print_dot += LONG_SIZE;
      break;
    case QUAD:
      fprintf (config.map_file, "QUAD ");
      print_dot += QUAD_SIZE;
      break;
    }

  exp_print_tree (data->exp);

  fprintf (config.map_file, "\n");
}

/* Print a reloc statement.  */

static void
print_reloc_statement (reloc)
     lang_reloc_statement_type *reloc;
{
  print_section ("");
  print_space ();
  print_section ("");
  print_space ();

/*  ASSERT(print_dot == data->output_vma);*/

  print_address (reloc->output_vma + reloc->output_section->vma);
  print_space ();
  print_address (reloc->addend_value);
  print_space ();

  fprintf (config.map_file, "RELOC %s ", reloc->howto->name);

  print_dot += bfd_get_reloc_size (reloc->howto);

  exp_print_tree (reloc->addend_exp);

  fprintf (config.map_file, "\n");
}  

static void
print_padding_statement (s)
     lang_padding_statement_type * s;
{
  print_section ("");
  print_space ();
  print_section ("*fill*");
  print_space ();
  print_address (s->output_offset + s->output_section->vma);
  print_space ();
  print_size (s->size);
  print_space ();
  print_fill (s->fill);
  print_nl ();

  print_dot = s->output_offset + s->output_section->vma + s->size;

}

static void
print_wild_statement (w, os)
     lang_wild_statement_type * w;
     lang_output_section_statement_type * os;
{
  fprintf (config.map_file, " from ");
  if (w->filename != (char *) NULL)
    {
      fprintf (config.map_file, "%s", w->filename);
    }
  else
    {
      fprintf (config.map_file, "*");
    }
  if (w->section_name != (char *) NULL)
    {
      fprintf (config.map_file, "(%s)", w->section_name);
    }
  else
    {
      fprintf (config.map_file, "(*)");
    }
  print_nl ();
  print_statement (w->children.head, os);

}

/* Print a group statement.  */

static void
print_group (s, os)
     lang_group_statement_type *s;
     lang_output_section_statement_type *os;
{
  fprintf (config.map_file, "START GROUP\n");
  print_statement (s->children.head, os);
  fprintf (config.map_file, "END GROUP\n");
}

static void
print_statement (s, os)
     lang_statement_union_type * s;
     lang_output_section_statement_type * os;
{
  while (s)
    {
      switch (s->header.type)
	{
	  case lang_constructors_statement_enum:
	  fprintf (config.map_file, "constructors:\n");
	  print_statement (constructor_list.head, os);
	  break;
	case lang_wild_statement_enum:
	  print_wild_statement (&s->wild_statement, os);
	  break;
	default:
	  fprintf (config.map_file, "Fail with %d\n", s->header.type);
	  FAIL ();
	  break;
	case lang_address_statement_enum:
	  fprintf (config.map_file, "address\n");
	  break;
	case lang_object_symbols_statement_enum:
	  fprintf (config.map_file, "object symbols\n");
	  break;
	case lang_fill_statement_enum:
	  print_fill_statement (&s->fill_statement);
	  break;
	case lang_data_statement_enum:
	  print_data_statement (&s->data_statement);
	  break;
	case lang_reloc_statement_enum:
	  print_reloc_statement (&s->reloc_statement);
	  break;
	case lang_input_section_enum:
	  print_input_section (&s->input_section);
	  break;
	case lang_padding_statement_enum:
	  print_padding_statement (&s->padding_statement);
	  break;
	case lang_output_section_statement_enum:
	  print_output_section_statement (&s->output_section_statement);
	  break;
	case lang_assignment_statement_enum:
	  print_assignment (&s->assignment_statement,
			    os);
	  break;
	case lang_target_statement_enum:
	  fprintf (config.map_file, "TARGET(%s)\n", s->target_statement.target);
	  break;
	case lang_output_statement_enum:
	  fprintf (config.map_file, "OUTPUT(%s %s)\n",
		   s->output_statement.name,
		   output_target ? output_target : "");
	  break;
	case lang_input_statement_enum:
	  print_input_statement (&s->input_statement);
	  break;
	case lang_group_statement_enum:
	  print_group (&s->group_statement, os);
	  break;
	case lang_afile_asection_pair_statement_enum:
	  FAIL ();
	  break;
	}
      s = s->next;
    }
}


static void
print_statements ()
{
  print_statement (statement_list.head,
		   abs_output_section);

}

static bfd_vma
insert_pad (this_ptr, fill, power, output_section_statement, dot)
     lang_statement_union_type ** this_ptr;
     fill_type fill;
     unsigned int power;
     asection * output_section_statement;
     bfd_vma dot;
{
  /* Align this section first to the
     input sections requirement, then
     to the output section's requirement.
     If this alignment is > than any seen before,
     then record it too. Perform the alignment by
     inserting a magic 'padding' statement.
     */

  unsigned int alignment_needed = align_power (dot, power) - dot;

  if (alignment_needed != 0)
    {
      lang_statement_union_type *new =
	((lang_statement_union_type *)
	 stat_alloc (sizeof (lang_padding_statement_type)));

      /* Link into existing chain */
      new->header.next = *this_ptr;
      *this_ptr = new;
      new->header.type = lang_padding_statement_enum;
      new->padding_statement.output_section = output_section_statement;
      new->padding_statement.output_offset =
	dot - output_section_statement->vma;
      new->padding_statement.fill = fill;
      new->padding_statement.size = alignment_needed;
    }


  /* Remember the most restrictive alignment */
  if (power > output_section_statement->alignment_power)
    {
      output_section_statement->alignment_power = power;
    }
  output_section_statement->_raw_size += alignment_needed;
  return alignment_needed + dot;

}

/* Work out how much this section will move the dot point */
static bfd_vma
size_input_section (this_ptr, output_section_statement, fill, dot, relax)
     lang_statement_union_type ** this_ptr;
     lang_output_section_statement_type * output_section_statement;
     fill_type fill;
     bfd_vma dot;
     boolean relax;
{
  lang_input_section_type *is = &((*this_ptr)->input_section);
  asection *i = is->section;

  if (is->ifile->just_syms_flag == false)
    {
      if (output_section_statement->subsection_alignment != -1)
       i->alignment_power =
	output_section_statement->subsection_alignment;

      dot = insert_pad (this_ptr, fill, i->alignment_power,
			output_section_statement->bfd_section, dot);

      /* Remember where in the output section this input section goes */

      i->output_offset = dot - output_section_statement->bfd_section->vma;

      /* Mark how big the output section must be to contain this now
	 */
      if (i->_cooked_size != 0)
	dot += i->_cooked_size;
      else
	dot += i->_raw_size;
      output_section_statement->bfd_section->_raw_size = dot - output_section_statement->bfd_section->vma;
    }
  else
    {
      i->output_offset = i->vma - output_section_statement->bfd_section->vma;
    }

  return dot;
}

/* This variable indicates whether bfd_relax_section should be called
   again.  */

static boolean relax_again;

/* Set the sizes for all the output sections.  */

bfd_vma
lang_size_sections (s, output_section_statement, prev, fill, dot, relax)
     lang_statement_union_type * s;
     lang_output_section_statement_type * output_section_statement;
     lang_statement_union_type ** prev;
     fill_type fill;
     bfd_vma dot;
     boolean relax;
{
  /* Size up the sections from their constituent parts */
  for (; s != (lang_statement_union_type *) NULL; s = s->next)
  {
    switch (s->header.type)
    {

     case lang_output_section_statement_enum:
     {
       bfd_vma after;
       lang_output_section_statement_type *os = &s->output_section_statement;

       if (os->bfd_section == NULL)
	 {
	   /* This section was never actually created.  */
	   break;
	 }

       /* If this is a COFF shared library section, use the size and
	  address from the input section.  FIXME: This is COFF
	  specific; it would be cleaner if there were some other way
	  to do this, but nothing simple comes to mind.  */
       if ((os->bfd_section->flags & SEC_COFF_SHARED_LIBRARY) != 0)
	 {
	   asection *input;

	   if (os->children.head == NULL
	       || os->children.head->next != NULL
	       || os->children.head->header.type != lang_input_section_enum)
	     einfo ("%P%X: Internal error on COFF shared library section %s",
		    os->name);

	   input = os->children.head->input_section.section;
	   bfd_set_section_vma (os->bfd_section->owner,
				os->bfd_section,
				bfd_section_vma (input->owner, input));
	   os->bfd_section->_raw_size = input->_raw_size;
	   break;
	 }

       if (bfd_is_abs_section (os->bfd_section))
       {
	 /* No matter what happens, an abs section starts at zero */
	 ASSERT (os->bfd_section->vma == 0);
       }
       else
       {
	 if (os->addr_tree == (etree_type *) NULL)
	 {
	   /* No address specified for this section, get one
	      from the region specification
	      */
	   if (os->region == (lang_memory_region_type *) NULL)
	   {
	     os->region = lang_memory_region_lookup ("*default*");
	   }
	   dot = os->region->current;
	   if (os->section_alignment == -1)
	     dot = align_power (dot, os->bfd_section->alignment_power);
	 }
	 else
	 {
	   etree_value_type r;

	   r = exp_fold_tree (os->addr_tree,
			      abs_output_section,
			      lang_allocating_phase_enum,
			      dot, &dot);
	   if (r.valid == false)
	   {
	     einfo ("%F%S: non constant address expression for section %s\n",
		    os->name);
	   }
	   dot = r.value;
	 }
	 /* The section starts here */
	 /* First, align to what the section needs */

	 if (os->section_alignment != -1)
	   dot = align_power (dot, os->section_alignment);

	 bfd_set_section_vma (0, os->bfd_section, dot);
	 
	 os->bfd_section->output_offset = 0;
       }

       (void) lang_size_sections (os->children.head, os, &os->children.head,
				  os->fill, dot, relax);
       /* Ignore the size of the input sections, use the vma and size to */
       /* align against */

       after = ALIGN_N (os->bfd_section->vma +
			os->bfd_section->_raw_size,
			/* The coercion here is important, see ld.h.  */
			(bfd_vma) os->block_value);

       if (bfd_is_abs_section (os->bfd_section))
	 ASSERT (after == os->bfd_section->vma);
       else
	 os->bfd_section->_raw_size = after - os->bfd_section->vma;
       dot = os->bfd_section->vma + os->bfd_section->_raw_size;
       os->processed = true;

       /* Replace into region ? */
       if (os->region != (lang_memory_region_type *) NULL)
	 {
	   os->region->current = dot;
	   /* Make sure this isn't silly.  */
	   if ((os->region->current < os->region->origin)
	       || (os->region->current
		   > os->region->origin + os->region->length))
	     {
	       if (os->addr_tree != (etree_type *) NULL)
		 {
		   einfo ("%X%P: address 0x%v of %B section %s is not within region %s\n",
			  os->region->current,
			  os->bfd_section->owner,
			  os->bfd_section->name,
			  os->region->name);
		 }
	       else
		 {
		   einfo ("%X%P: region %s is full (%B section %s)\n",
			  os->region->name,
			  os->bfd_section->owner,
			  os->bfd_section->name);
		 }
	       /* Reset the region pointer.  */
	       os->region->current = os->region->origin;
	     }
	 }
     }
     break;

     case lang_constructors_statement_enum:
      dot = lang_size_sections (constructor_list.head,
				output_section_statement,
				&s->wild_statement.children.head,
				fill,
				dot, relax);
      break;

     case lang_data_statement_enum:
     {
       unsigned int size = 0;

       s->data_statement.output_vma = dot - output_section_statement->bfd_section->vma;
       s->data_statement.output_section =
	output_section_statement->bfd_section;

       switch (s->data_statement.type)
       {
        case QUAD:
	 size = QUAD_SIZE;
	 break;
	case LONG:
	 size = LONG_SIZE;
	 break;
	case SHORT:
	 size = SHORT_SIZE;
	 break;
	case BYTE:
	 size = BYTE_SIZE;
	 break;

       }
       dot += size;
       output_section_statement->bfd_section->_raw_size += size;
       /* The output section gets contents, and then we inspect for
	  any flags set in the input script which override any ALLOC */
       output_section_statement->bfd_section->flags |= SEC_HAS_CONTENTS;
       if (!(output_section_statement->flags & SEC_NEVER_LOAD)) {
	 output_section_statement->bfd_section->flags |= SEC_ALLOC | SEC_LOAD;
       }
     }
      break;

     case lang_reloc_statement_enum:
     {
       int size;

       s->reloc_statement.output_vma =
	 dot - output_section_statement->bfd_section->vma;
       s->reloc_statement.output_section =
	 output_section_statement->bfd_section;
       size = bfd_get_reloc_size (s->reloc_statement.howto);
       dot += size;
       output_section_statement->bfd_section->_raw_size += size;
     }
     break;
     
     case lang_wild_statement_enum:

      dot = lang_size_sections (s->wild_statement.children.head,
				output_section_statement,
				&s->wild_statement.children.head,

				fill, dot, relax);

      break;

     case lang_object_symbols_statement_enum:
      link_info.create_object_symbols_section =
	output_section_statement->bfd_section;
      break;
     case lang_output_statement_enum:
     case lang_target_statement_enum:
      break;
     case lang_input_section_enum:
      {
	asection *i;

	i = (*prev)->input_section.section;
	if (! relax)
	  i->_cooked_size = i->_raw_size;
	else
	  {
	    boolean again;

	    if (! bfd_relax_section (i->owner, i, &link_info, &again))
	      einfo ("%P%F: can't relax section: %E\n");
	    if (again)
	      relax_again = true;
	  }
	dot = size_input_section (prev,
				  output_section_statement,
				  output_section_statement->fill,
				  dot, relax);
      }
      break;
     case lang_input_statement_enum:
      break;
     case lang_fill_statement_enum:
      s->fill_statement.output_section = output_section_statement->bfd_section;

      fill = s->fill_statement.fill;
      break;
     case lang_assignment_statement_enum:
     {
       bfd_vma newdot = dot;

       exp_fold_tree (s->assignment_statement.exp,
		      output_section_statement,
		      lang_allocating_phase_enum,
		      dot,
		      &newdot);

       if (newdot != dot && !relax)
	 {
	   /* The assignment changed dot.  Insert a pad.  */
	   if (output_section_statement == abs_output_section)
	     {
	       /* If we don't have an output section, then just adjust
		  the default memory address.  */
	       lang_memory_region_lookup ("*default*")->current = newdot;
	     }
	   else
	     {
	       lang_statement_union_type *new =
		 ((lang_statement_union_type *)
		  stat_alloc (sizeof (lang_padding_statement_type)));

	       /* Link into existing chain */
	       new->header.next = *prev;
	       *prev = new;
	       new->header.type = lang_padding_statement_enum;
	       new->padding_statement.output_section =
		 output_section_statement->bfd_section;
	       new->padding_statement.output_offset =
		 dot - output_section_statement->bfd_section->vma;
	       new->padding_statement.fill = fill;
	       new->padding_statement.size = newdot - dot;
	       output_section_statement->bfd_section->_raw_size +=
		 new->padding_statement.size;
	     }

	   dot = newdot;
	 }
     }
     break;

   case lang_padding_statement_enum:
     /* If we are relaxing, and this is not the first pass, some
	padding statements may have been inserted during previous
	passes.  We may have to move the padding statement to a new
	location if dot has a different value at this point in this
	pass than it did at this point in the previous pass.  */
     s->padding_statement.output_offset =
       dot - output_section_statement->bfd_section->vma;
     dot += s->padding_statement.size;
     break;

     case lang_group_statement_enum:
       dot = lang_size_sections (s->group_statement.children.head,
				 output_section_statement,
				 &s->group_statement.children.head,
				 fill, dot, relax);
       break;

     default:
      FAIL ();
      break;

      /* This can only get here when relaxing is turned on */

     case lang_address_statement_enum:
      break;
    }
    prev = &s->header.next;
  }
  return dot;
}

bfd_vma
lang_do_assignments (s, output_section_statement, fill, dot)
     lang_statement_union_type * s;
     lang_output_section_statement_type * output_section_statement;
     fill_type fill;
     bfd_vma dot;
{
  for (; s != (lang_statement_union_type *) NULL; s = s->next)
    {
      switch (s->header.type)
	{
	case lang_constructors_statement_enum:
	  dot = lang_do_assignments (constructor_list.head,
				     output_section_statement,
				     fill,
				     dot);
	  break;

	case lang_output_section_statement_enum:
	  {
	    lang_output_section_statement_type *os =
	      &(s->output_section_statement);

	    if (os->bfd_section != NULL)
	      {
		dot = os->bfd_section->vma;
		(void) lang_do_assignments (os->children.head, os,
					    os->fill, dot);
		dot = os->bfd_section->vma + os->bfd_section->_raw_size;
	      }
	    if (os->load_base) 
	      {
		/* If nothing has been placed into the output section then
		   it won't have a bfd_section. */
		if (os->bfd_section) 
		  {
		    os->bfd_section->lma 
		      = exp_get_abs_int(os->load_base, 0,"load base", lang_final_phase_enum);
		  }
	      }
	  }
	  break;
	case lang_wild_statement_enum:

	  dot = lang_do_assignments (s->wild_statement.children.head,
				     output_section_statement,
				     fill, dot);

	  break;

	case lang_object_symbols_statement_enum:
	case lang_output_statement_enum:
	case lang_target_statement_enum:
#if 0
	case lang_common_statement_enum:
#endif
	  break;
	case lang_data_statement_enum:
	  {
	    etree_value_type value;

	    value = exp_fold_tree (s->data_statement.exp,
				   abs_output_section,
				   lang_final_phase_enum, dot, &dot);
	    s->data_statement.value = value.value;
	    if (value.valid == false)
	      einfo ("%F%P: invalid data statement\n");
	  }
	  switch (s->data_statement.type)
	    {
	    case QUAD:
	      dot += QUAD_SIZE;
	      break;
	    case LONG:
	      dot += LONG_SIZE;
	      break;
	    case SHORT:
	      dot += SHORT_SIZE;
	      break;
	    case BYTE:
	      dot += BYTE_SIZE;
	      break;
	    }
	  break;

	case lang_reloc_statement_enum:
	  {
	    etree_value_type value;

	    value = exp_fold_tree (s->reloc_statement.addend_exp,
				   abs_output_section,
				   lang_final_phase_enum, dot, &dot);
	    s->reloc_statement.addend_value = value.value;
	    if (value.valid == false)
	      einfo ("%F%P: invalid reloc statement\n");
	  }
	  dot += bfd_get_reloc_size (s->reloc_statement.howto);
	  break;

	case lang_input_section_enum:
	  {
	    asection *in = s->input_section.section;

	    if (in->_cooked_size != 0)
	      dot += in->_cooked_size;
	    else
	      dot += in->_raw_size;
	  }
	  break;

	case lang_input_statement_enum:
	  break;
	case lang_fill_statement_enum:
	  fill = s->fill_statement.fill;
	  break;
	case lang_assignment_statement_enum:
	  {
	    exp_fold_tree (s->assignment_statement.exp,
			   output_section_statement,
			   lang_final_phase_enum,
			   dot,
			   &dot);
	  }

	  break;
	case lang_padding_statement_enum:
	  dot += s->padding_statement.size;
	  break;

	case lang_group_statement_enum:
	  dot = lang_do_assignments (s->group_statement.children.head,
				     output_section_statement,
				     fill, dot);

	  break;

	default:
	  FAIL ();
	  break;
	case lang_address_statement_enum:
	  break;
	}

    }
  return dot;
}

/* Fix any .startof. or .sizeof. symbols.  When the assemblers see the
   operator .startof. (section_name), it produces an undefined symbol
   .startof.section_name.  Similarly, when it sees
   .sizeof. (section_name), it produces an undefined symbol
   .sizeof.section_name.  For all the output sections, we look for
   such symbols, and set them to the correct value.  */

static void
lang_set_startof ()
{
  asection *s;

  for (s = output_bfd->sections; s != NULL; s = s->next)
    {
      const char *secname;
      char *buf;
      struct bfd_link_hash_entry *h;

      secname = bfd_get_section_name (output_bfd, s);
      buf = xmalloc (10 + strlen (secname));

      sprintf (buf, ".startof.%s", secname);
      h = bfd_link_hash_lookup (link_info.hash, buf, false, false, true);
      if (h != NULL && h->type == bfd_link_hash_undefined)
	{
	  h->type = bfd_link_hash_defined;
	  h->u.def.value = bfd_get_section_vma (output_bfd, s);
	  h->u.def.section = bfd_abs_section_ptr;
	}

      sprintf (buf, ".sizeof.%s", secname);
      h = bfd_link_hash_lookup (link_info.hash, buf, false, false, true);
      if (h != NULL && h->type == bfd_link_hash_undefined)
	{
	  h->type = bfd_link_hash_defined;
	  if (s->_cooked_size != 0)
	    h->u.def.value = s->_cooked_size;
	  else
	    h->u.def.value = s->_raw_size;
	  h->u.def.section = bfd_abs_section_ptr;
	}

      free (buf);
    }
}

static void
lang_finish ()
{
  struct bfd_link_hash_entry *h;
  boolean warn;

  if (link_info.relocateable || link_info.shared)
    warn = false;
  else
    warn = true;

  if (entry_symbol == (char *) NULL)
    {
      /* No entry has been specified.  Look for start, but don't warn
	 if we don't find it.  */
      entry_symbol = "start";
      warn = false;
    }

  h = bfd_link_hash_lookup (link_info.hash, entry_symbol, false, false, true);
  if (h != (struct bfd_link_hash_entry *) NULL
      && (h->type == bfd_link_hash_defined
	  || h->type == bfd_link_hash_defweak)
      && h->u.def.section->output_section != NULL)
    {
      bfd_vma val;

      val = (h->u.def.value
	     + bfd_get_section_vma (output_bfd,
				    h->u.def.section->output_section)
	     + h->u.def.section->output_offset);
      if (! bfd_set_start_address (output_bfd, val))
	einfo ("%P%F:%s: can't set start address\n", entry_symbol);
    }
  else
    {
      asection *ts;

      /* Can't find the entry symbol.  Use the first address in the
	 text section.  */
      ts = bfd_get_section_by_name (output_bfd, ".text");
      if (ts != (asection *) NULL)
	{
	  if (warn)
	    einfo ("%P: warning: cannot find entry symbol %s; defaulting to %V\n",
		   entry_symbol, bfd_get_section_vma (output_bfd, ts));
	  if (! bfd_set_start_address (output_bfd,
				       bfd_get_section_vma (output_bfd, ts)))
	    einfo ("%P%F: can't set start address\n");
	}
      else
	{
	  if (warn)
	    einfo ("%P: warning: cannot find entry symbol %s; not setting start address\n",
		   entry_symbol);
	}
    }
}

/* Check that the architecture of all the input files is compatible
   with the output file.  Also call the backend to let it do any
   other checking that is needed.  */

static void
lang_check ()
{
  lang_statement_union_type *file;
  bfd *input_bfd;
  CONST bfd_arch_info_type *compatible;

  for (file = file_chain.head;
       file != (lang_statement_union_type *) NULL;
       file = file->input_statement.next)
    {
      input_bfd = file->input_statement.the_bfd;
      compatible = bfd_arch_get_compatible (input_bfd,
					    output_bfd);
      if (compatible == NULL)
	einfo ("%P: warning: %s architecture of input file `%B' is incompatible with %s output\n",
	       bfd_printable_name (input_bfd), input_bfd,
	       bfd_printable_name (output_bfd));

      else
	bfd_merge_private_bfd_data (input_bfd, output_bfd);
    }
}

/* Look through all the global common symbols and attach them to the
   correct section.  The -sort-common command line switch may be used
   to roughly sort the entries by size.  */

static void
lang_common ()
{
  if (link_info.relocateable
      && ! command_line.force_common_definition)
    return;

  if (! config.sort_common)
    bfd_link_hash_traverse (link_info.hash, lang_one_common, (PTR) NULL);
  else
    {
      int power;

      for (power = 4; power >= 0; power--)
	bfd_link_hash_traverse (link_info.hash, lang_one_common,
				(PTR) &power);
    }
}

/* Place one common symbol in the correct section.  */

static boolean
lang_one_common (h, info)
     struct bfd_link_hash_entry *h;
     PTR info;
{
  unsigned int power_of_two;
  bfd_vma size;
  asection *section;

  if (h->type != bfd_link_hash_common)
    return true;

  size = h->u.c.size;
  power_of_two = h->u.c.p->alignment_power;

  if (config.sort_common
      && power_of_two < (unsigned int) *(int *) info)
    return true;

  section = h->u.c.p->section;

  /* Increase the size of the section.  */
  section->_raw_size = ALIGN_N (section->_raw_size,
				(bfd_size_type) (1 << power_of_two));

  /* Adjust the alignment if necessary.  */
  if (power_of_two > section->alignment_power)
    section->alignment_power = power_of_two;

  /* Change the symbol from common to defined.  */
  h->type = bfd_link_hash_defined;
  h->u.def.section = section;
  h->u.def.value = section->_raw_size;

  /* Increase the size of the section.  */
  section->_raw_size += size;

  /* Make sure the section is allocated in memory.  */
  section->flags |= SEC_ALLOC;

  if (config.map_file != NULL)
    fprintf (config.map_file, "Allocating common %s: %lx at %lx %s\n",
	     h->root.string, (unsigned long) size,
	     (unsigned long) h->u.def.value, section->owner->filename);

  return true;
}

/*
run through the input files and ensure that every input
section has somewhere to go. If one is found without
a destination then create an input request and place it
into the statement tree.
*/

static void
lang_place_orphans ()
{
  lang_input_statement_type *file;

  for (file = (lang_input_statement_type *) file_chain.head;
       file != (lang_input_statement_type *) NULL;
       file = (lang_input_statement_type *) file->next)
    {
      asection *s;

      for (s = file->the_bfd->sections;
	   s != (asection *) NULL;
	   s = s->next)
	{
	  if (s->output_section == (asection *) NULL)
	    {
	      /* This section of the file is not attatched, root
	         around for a sensible place for it to go */

	      if (file->just_syms_flag)
		{
		  /* We are only retrieving symbol values from this
                     file.  We want the symbols to act as though the
                     values in the file are absolute.  */
		  s->output_section = bfd_abs_section_ptr;
		  s->output_offset = s->vma;
		}
	      else if (file->common_section == s)
		{
		  /* This is a lonely common section which must
		     have come from an archive. We attatch to the
		     section with the wildcard  */
		  if (! link_info.relocateable
		      && ! command_line.force_common_definition)
		    {
		      if (default_common_section ==
			  (lang_output_section_statement_type *) NULL)
			{
			  info_msg ("%P: no [COMMON] command, defaulting to .bss\n");

			  default_common_section =
			    lang_output_section_statement_lookup (".bss");

			}
		      wild_doit (&default_common_section->children, s,
				 default_common_section, file);
		    }
		}
	      else if (ldemul_place_orphan (file, s))
		;
	      else
		{
		  lang_output_section_statement_type *os =
		  lang_output_section_statement_lookup (s->name);

		  wild_doit (&os->children, s, os, file);
		}
	    }
	}
    }
}


void
lang_set_flags (ptr, flags)
     int *ptr;
     CONST char *flags;
{
  boolean state = false;

  *ptr = 0;
  while (*flags)
    {
      if (*flags == '!')
	{
	  state = false;
	  flags++;
	}
      else
	state = true;
      switch (*flags)
	{
	case 'R':
	  /*	  ptr->flag_read = state; */
	  break;
	case 'W':
	  /*	  ptr->flag_write = state; */
	  break;
	case 'X':
	  /*	  ptr->flag_executable= state;*/
	  break;
	case 'L':
	case 'I':
	  /*	  ptr->flag_loadable= state;*/
	  break;
	default:
	  einfo ("%P%F: invalid syntax in flags\n");
	  break;
	}
      flags++;
    }
}

/* Call a function on each input file.  This function will be called
   on an archive, but not on the elements.  */

void
lang_for_each_input_file (func)
     void (*func) PARAMS ((lang_input_statement_type *));
{
  lang_input_statement_type *f;

  for (f = (lang_input_statement_type *) input_file_chain.head;
       f != NULL;
       f = (lang_input_statement_type *) f->next_real_file)
    func (f);
}

/* Call a function on each file.  The function will be called on all
   the elements of an archive which are included in the link, but will
   not be called on the archive file itself.  */

void
lang_for_each_file (func)
     void (*func) PARAMS ((lang_input_statement_type *));
{
  lang_input_statement_type *f;

  for (f = (lang_input_statement_type *) file_chain.head;
       f != (lang_input_statement_type *) NULL;
       f = (lang_input_statement_type *) f->next)
    {
      func (f);
    }
}

#if 0

/* Not used.  */

void
lang_for_each_input_section (func)
     void (*func) PARAMS ((bfd * ab, asection * as));
{
  lang_input_statement_type *f;

  for (f = (lang_input_statement_type *) file_chain.head;
       f != (lang_input_statement_type *) NULL;
       f = (lang_input_statement_type *) f->next)
    {
      asection *s;

      for (s = f->the_bfd->sections;
	   s != (asection *) NULL;
	   s = s->next)
	{
	  func (f->the_bfd, s);
	}
    }
}

#endif

void
ldlang_add_file (entry)
     lang_input_statement_type * entry;
{
  bfd **pp;

  lang_statement_append (&file_chain,
			 (lang_statement_union_type *) entry,
			 &entry->next);

  /* The BFD linker needs to have a list of all input BFDs involved in
     a link.  */
  ASSERT (entry->the_bfd->link_next == (bfd *) NULL);
  ASSERT (entry->the_bfd != output_bfd);
  for (pp = &link_info.input_bfds;
       *pp != (bfd *) NULL;
       pp = &(*pp)->link_next)
    ;
  *pp = entry->the_bfd;
  entry->the_bfd->usrdata = (PTR) entry;
  bfd_set_gp_size (entry->the_bfd, g_switch_value);
}

void
lang_add_output (name, from_script)
     CONST char *name;
     int from_script;
{
  /* Make -o on command line override OUTPUT in script.  */
  if (had_output_filename == false || !from_script)
    {
      output_filename = name;
      had_output_filename = true;
    }
}


static lang_output_section_statement_type *current_section;

static int
topower (x)
     int x;
{
  unsigned int i = 1;
  int l;

  if (x < 0)
    return -1;

  for (l = 0; l < 32; l++) 
    {
      if (i >= (unsigned int) x)
	return l;
      i <<= 1;
    }

  return 0;
}
void
lang_enter_output_section_statement (output_section_statement_name,
				     address_exp, flags, block_value,
				     align, subalign, ebase)
     const char *output_section_statement_name;
     etree_type * address_exp;
     int flags;
     bfd_vma block_value;
     etree_type *align;
     etree_type *subalign;
     etree_type *ebase;
{
  lang_output_section_statement_type *os;

  current_section =
   os =
    lang_output_section_statement_lookup (output_section_statement_name);



  /* Add this statement to tree */
  /*  add_statement(lang_output_section_statement_enum,
      output_section_statement);*/
  /* Make next things chain into subchain of this */

  if (os->addr_tree ==
      (etree_type *) NULL)
  {
    os->addr_tree =
     address_exp;
  }
  os->flags = flags;
  if (flags & SEC_NEVER_LOAD)
   os->loadable = 0;
  else
   os->loadable = 1;
  os->block_value = block_value ? block_value : 1;
  stat_ptr = &os->children;

  os->subsection_alignment = topower(
   exp_get_value_int(subalign, -1,
		     "subsection alignment",
		     0));
  os->section_alignment = topower(
   exp_get_value_int(align, -1,
		     "section alignment", 0));

  os->load_base = ebase;
}


void
lang_final ()
{
  lang_output_statement_type *new =
    new_stat (lang_output_statement, stat_ptr);

  new->name = output_filename;
}

/* Reset the current counters in the regions */
static void
reset_memory_regions ()
{
  lang_memory_region_type *p = lang_memory_region_list;

  for (p = lang_memory_region_list;
       p != (lang_memory_region_type *) NULL;
       p = p->next)
    {
      p->old_length = (bfd_size_type) (p->current - p->origin);
      p->current = p->origin;
    }
}

void
lang_process ()
{
  lang_reasonable_defaults ();
  current_target = default_target;

  lang_for_each_statement (ldlang_open_output);	/* Open the output file */

  ldemul_create_output_section_statements ();

  /* Add to the hash table all undefineds on the command line */
  lang_place_undefineds ();

  /* Create a bfd for each input file */
  current_target = default_target;
  open_input_bfds (statement_list.head, false);

  ldemul_after_open ();

  /* Build all sets based on the information gathered from the input
     files.  */
  ldctor_build_sets ();

  /* Size up the common data */
  lang_common ();

  /* Run through the contours of the script and attatch input sections
     to the correct output sections
     */
  map_input_to_output_sections (statement_list.head, (char *) NULL,
				(lang_output_section_statement_type *) NULL);


  /* Find any sections not attatched explicitly and handle them */
  lang_place_orphans ();

  ldemul_before_allocation ();

  /* Now run around and relax if we can */
  if (command_line.relax)
    {
      /* First time round is a trial run to get the 'worst case'
	 addresses of the objects if there was no relaxing.  */
      lang_size_sections (statement_list.head,
			  abs_output_section,
			  &(statement_list.head), 0, (bfd_vma) 0, false);

      /* Keep relaxing until bfd_relax_section gives up.  */
      do
	{
	  reset_memory_regions ();

	  relax_again = false;

	  /* Do all the assignments with our current guesses as to
	     section sizes.  */
	  lang_do_assignments (statement_list.head,
			       abs_output_section,
			       (fill_type) 0, (bfd_vma) 0);

	  /* Perform another relax pass - this time we know where the
	     globals are, so can make better guess.  */
	  lang_size_sections (statement_list.head,
			      abs_output_section,
			      &(statement_list.head), 0, (bfd_vma) 0, true);
	}
      while (relax_again);
    }
  else
    {
      /* Size up the sections.  */
      lang_size_sections (statement_list.head,
			  abs_output_section,
			  &(statement_list.head), 0, (bfd_vma) 0, false);
    }

  /* See if anything special should be done now we know how big
     everything is.  */
  ldemul_after_allocation ();

  /* Fix any .startof. or .sizeof. symbols.  */
  lang_set_startof ();

  /* Do all the assignments, now that we know the final restingplaces
     of all the symbols */

  lang_do_assignments (statement_list.head,
		       abs_output_section,
		       (fill_type) 0, (bfd_vma) 0);

  /* Make sure that we're not mixing architectures */

  lang_check ();

  /* Final stuffs */

  ldemul_finish ();
  lang_finish ();
}

/* EXPORTED TO YACC */

void
lang_add_wild (section_name, filename)
     CONST char *CONST section_name;
     CONST char *CONST filename;
{
  lang_wild_statement_type *new = new_stat (lang_wild_statement,
					    stat_ptr);

  if (section_name != (char *) NULL && strcmp (section_name, "COMMON") == 0)
    {
      placed_commons = true;
    }
  if (filename != (char *) NULL)
    {
      lang_has_input_file = true;
    }
  new->section_name = section_name;
  new->filename = filename;
  lang_list_init (&new->children);
}

void
lang_section_start (name, address)
     CONST char *name;
     etree_type * address;
{
  lang_address_statement_type *ad = new_stat (lang_address_statement, stat_ptr);

  ad->section_name = name;
  ad->address = address;
}

/* Set the start symbol to NAME.  CMDLINE is nonzero if this is called
   because of a -e argument on the command line, or zero if this is
   called by ENTRY in a linker script.  Command line arguments take
   precedence.  */

/* WINDOWS_NT.  When an entry point has been specified, we will also force
   this symbol to be defined by calling ldlang_add_undef (equivalent to 
   having switch -u entry_name on the command line).  The reason we do
   this is so that the user doesn't have to because they would have to use
   the -u switch if they were specifying an entry point other than 
   _mainCRTStartup.  Specifically, if creating a windows application, entry
   point _WinMainCRTStartup must be specified.
     What I have found for non console applications (entry not _mainCRTStartup)
   is that the .obj that contains mainCRTStartup is brought in since it is
   the first encountered in libc.lib and it has other symbols in it which will
   be pulled in by the link process.  To avoid this, adding -u with the entry
   point name specified forces the correct .obj to be used.  We can avoid
   making the user do this by always adding the entry point name as an
   undefined symbol.  */

void
lang_add_entry (name, cmdline)
     CONST char *name;
     boolean cmdline;
{
  if (entry_symbol == NULL
      || cmdline
      || ! entry_from_cmdline)
    {
      entry_symbol = name;
      entry_from_cmdline = cmdline;
    }
#if 0 
  /* don't do this yet.  It seems to work (the executables run), but the 
     image created is very different from what I was getting before indicating
     that something else is being pulled in.  When everything else is working,
     then try to put this back in to see if it will do the right thing for
     other more complicated applications */
  ldlang_add_undef (name);
#endif
}

void
lang_add_target (name)
     CONST char *name;
{
  lang_target_statement_type *new = new_stat (lang_target_statement,
					      stat_ptr);

  new->target = name;

}

void
lang_add_map (name)
     CONST char *name;
{
  while (*name)
    {
      switch (*name)
	{
	  case 'F':
	  map_option_f = true;
	  break;
	}
      name++;
    }
}

void
lang_add_fill (exp)
     int exp;
{
  lang_fill_statement_type *new = new_stat (lang_fill_statement,
					    stat_ptr);

  new->fill = exp;
}

void
lang_add_data (type, exp)
     int type;
     union etree_union *exp;
{

  lang_data_statement_type *new = new_stat (lang_data_statement,
					    stat_ptr);

  new->exp = exp;
  new->type = type;

}

/* Create a new reloc statement.  RELOC is the BFD relocation type to
   generate.  HOWTO is the corresponding howto structure (we could
   look this up, but the caller has already done so).  SECTION is the
   section to generate a reloc against, or NAME is the name of the
   symbol to generate a reloc against.  Exactly one of SECTION and
   NAME must be NULL.  ADDEND is an expression for the addend.  */

void
lang_add_reloc (reloc, howto, section, name, addend)
     bfd_reloc_code_real_type reloc;
     reloc_howto_type *howto;
     asection *section;
     const char *name;
     union etree_union *addend;
{
  lang_reloc_statement_type *p = new_stat (lang_reloc_statement, stat_ptr);
  
  p->reloc = reloc;
  p->howto = howto;
  p->section = section;
  p->name = name;
  p->addend_exp = addend;

  p->addend_value = 0;
  p->output_section = NULL;
  p->output_vma = 0;
}

void
lang_add_assignment (exp)
     etree_type * exp;
{
  lang_assignment_statement_type *new = new_stat (lang_assignment_statement,
						  stat_ptr);

  new->exp = exp;
}

void
lang_add_attribute (attribute)
     enum statement_enum attribute;
{
  new_statement (attribute, sizeof (lang_statement_union_type), stat_ptr);
}

void
lang_startup (name)
     CONST char *name;
{
  if (startup_file != (char *) NULL)
    {
      einfo ("%P%Fmultiple STARTUP files\n");
    }
  first_file->filename = name;
  first_file->local_sym_name = name;
  first_file->real = true;

  startup_file = name;
}

void
lang_float (maybe)
     boolean maybe;
{
  lang_float_flag = maybe;
}

void
lang_leave_output_section_statement (fill, memspec)
     bfd_vma fill;
     CONST char *memspec;
{
  current_section->fill = fill;
  current_section->region = lang_memory_region_lookup (memspec);
  stat_ptr = &statement_list;
}

/*
 Create an absolute symbol with the given name with the value of the
 address of first byte of the section named.

 If the symbol already exists, then do nothing.
*/
void
lang_abs_symbol_at_beginning_of (secname, name)
     const char *secname;
     const char *name;
{
  struct bfd_link_hash_entry *h;

  h = bfd_link_hash_lookup (link_info.hash, name, true, true, true);
  if (h == (struct bfd_link_hash_entry *) NULL)
    einfo ("%P%F: bfd_link_hash_lookup failed: %E\n");

  if (h->type == bfd_link_hash_new
      || h->type == bfd_link_hash_undefined)
    {
      asection *sec;

      h->type = bfd_link_hash_defined;

      sec = bfd_get_section_by_name (output_bfd, secname);
      if (sec == (asection *) NULL)
	h->u.def.value = 0;
      else
	h->u.def.value = bfd_get_section_vma (output_bfd, sec);

      h->u.def.section = bfd_abs_section_ptr;
    }
}

/*
 Create an absolute symbol with the given name with the value of the
 address of the first byte after the end of the section named.

 If the symbol already exists, then do nothing.
*/
void
lang_abs_symbol_at_end_of (secname, name)
     const char *secname;
     const char *name;
{
  struct bfd_link_hash_entry *h;

  h = bfd_link_hash_lookup (link_info.hash, name, true, true, true);
  if (h == (struct bfd_link_hash_entry *) NULL)
    einfo ("%P%F: bfd_link_hash_lookup failed: %E\n");

  if (h->type == bfd_link_hash_new
      || h->type == bfd_link_hash_undefined)
    {
      asection *sec;

      h->type = bfd_link_hash_defined;

      sec = bfd_get_section_by_name (output_bfd, secname);
      if (sec == (asection *) NULL)
	h->u.def.value = 0;
      else
	h->u.def.value = (bfd_get_section_vma (output_bfd, sec)
			  + bfd_section_size (output_bfd, sec));

      h->u.def.section = bfd_abs_section_ptr;
    }
}

void
lang_statement_append (list, element, field)
     lang_statement_list_type * list;
     lang_statement_union_type * element;
     lang_statement_union_type ** field;
{
  *(list->tail) = element;
  list->tail = field;
}

/* Set the output format type.  -oformat overrides scripts.  */

void
lang_add_output_format (format, big, little, from_script)
     const char *format;
     const char *big;
     const char *little;
     int from_script;
{
  if (output_target == NULL || !from_script)
    {
      if (command_line.endian == ENDIAN_BIG
	  && big != NULL)
	format = big;
      else if (command_line.endian == ENDIAN_LITTLE
	       && little != NULL)
	format = little;

      output_target = format;
    }
}

/* Enter a group.  This creates a new lang_group_statement, and sets
   stat_ptr to build new statements within the group.  */

void
lang_enter_group ()
{
  lang_group_statement_type *g;

  g = new_stat (lang_group_statement, stat_ptr);
  lang_list_init (&g->children);
  stat_ptr = &g->children;
}

/* Leave a group.  This just resets stat_ptr to start writing to the
   regular list of statements again.  Note that this will not work if
   groups can occur inside anything else which can adjust stat_ptr,
   but currently they can't.  */

void
lang_leave_group ()
{
  stat_ptr = &statement_list;
}
