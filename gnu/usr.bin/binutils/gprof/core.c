#include "libiberty.h"
#include "gprof.h"
#include "core.h"
#include "symtab.h"

bfd *core_bfd;
int core_num_syms;
asymbol **core_syms;
asection *core_text_sect;
PTR core_text_space;


void
DEFUN (core_init, (a_out_name), const char *a_out_name)
{
  core_bfd = bfd_openr (a_out_name, 0);

  if (!core_bfd)
    {
      perror (a_out_name);
      done (1);
    }

  if (!bfd_check_format (core_bfd, bfd_object))
    {
      fprintf (stderr, "%s: %s: not in a.out format\n", whoami, a_out_name);
      done (1);
    }

  /* get core's text section: */
  core_text_sect = bfd_get_section_by_name (core_bfd, ".text");
  if (!core_text_sect)
    {
      core_text_sect = bfd_get_section_by_name (core_bfd, "$CODE$");
      if (!core_text_sect)
	{
	  fprintf (stderr, "%s: can't find .text section in %s\n",
		   whoami, a_out_name);
	  done (1);
	}
    }

  /* read core's symbol table: */

  /* this will probably give us more than we need, but that's ok:  */
  core_num_syms = bfd_get_symtab_upper_bound (core_bfd);
  if (core_num_syms < 0)
    {
      fprintf (stderr, "%s: %s: %s\n", whoami, a_out_name,
	       bfd_errmsg (bfd_get_error ()));
      done (1);
    }

  core_syms = (asymbol **) xmalloc (core_num_syms);
  core_num_syms = bfd_canonicalize_symtab (core_bfd, core_syms);
  if (core_num_syms < 0)
    {
      fprintf (stderr, "%s: %s: %s\n", whoami, a_out_name,
	       bfd_errmsg (bfd_get_error ()));
      done (1);
    }
}


/*
 * Read in the text space of an a.out file
 */
void
DEFUN (core_get_text_space, (core_bfd), bfd * core_bfd)
{
  core_text_space = (PTR) malloc (core_text_sect->_raw_size);

  if (!core_text_space)
    {
      fprintf (stderr, "%s: ran out room for %ld bytes of text space\n",
	       whoami, core_text_sect->_raw_size);
      done (1);
    }
  if (!bfd_get_section_contents (core_bfd, core_text_sect, core_text_space,
				 0, core_text_sect->_raw_size))
    {
      bfd_perror ("bfd_get_section_contents");
      free (core_text_space);
      core_text_space = 0;
    }
  if (!core_text_space)
    {
      fprintf (stderr, "%s: can't do -c\n", whoami);
    }
}


/*
 * Return class of symbol SYM.  The returned class can be any of:
 *      0   -> symbol is not interesting to us
 *      'T' -> symbol is a global name
 *      't' -> symbol is a local (static) name
 */
static int
DEFUN (core_sym_class, (sym), asymbol * sym)
{
  symbol_info syminfo;
  const char *name;
  char sym_prefix;
  int i;

  /*
   * Must be a text symbol, and static text symbols don't qualify if
   * ignore_static_funcs set.
   */
  if (!sym->section)
    {
      return 0;
    }

  if (ignore_static_funcs && (sym->flags & BSF_LOCAL))
    {
      DBG (AOUTDEBUG, printf ("[core_sym_class] %s: not a function\n",
			      sym->name));
      return 0;
    }

  bfd_get_symbol_info (core_bfd, sym, &syminfo);
  i = syminfo.type;

  if (i == 'T')
    {
      return i;			/* it's a global symbol */
    }

  if (i != 't')
    {
      /* not a static text symbol */
      DBG (AOUTDEBUG, printf ("[core_sym_class] %s is of class %c\n",
			      sym->name, i));
      return 0;
    }

  /* do some more filtering on static function-names: */

  if (ignore_static_funcs)
    {
      return 0;
    }
  /*
   * Can't zero-length name or funny characters in name, where
   * `funny' includes: `.' (.o file names) and `$' (Pascal labels).
   */
  if (!sym->name || sym->name[0] == '\0')
    {
      return 0;
    }

  for (name = sym->name; *name; ++name)
    {
      if (*name == '.' || *name == '$')
	{
	  return 0;
	}
    }
  /*
   * On systems where the C compiler adds an underscore to all
   * names, static names without underscores seem usually to be
   * labels in hand written assembler in the library.  We don't want
   * these names.  This is certainly necessary on a Sparc running
   * SunOS 4.1 (try profiling a program that does a lot of
   * division). I don't know whether it has harmful side effects on
   * other systems.  Perhaps it should be made configurable.
   */
  sym_prefix = bfd_get_symbol_leading_char (core_bfd);
  if (sym_prefix && sym_prefix != sym->name[0]
  /*
   * GCC may add special symbols to help gdb figure out the file
   * language.  We want to ignore these, since sometimes they mask
   * the real function.  (dj@ctron)
   */
      || !strncmp (sym->name, "__gnu_compiled", 14)
      || !strncmp (sym->name, "___gnu_compiled", 15))
    {
      return 0;
    }
  return 't';			/* it's a static text symbol */
}


/*
 * Get whatever source info we can get regarding address ADDR:
 */
static bool
DEFUN (get_src_info, (addr, filename, name, line_num),
       bfd_vma addr AND const char **filename AND const char **name
       AND int *line_num)
{
  const char *fname = 0, *func_name = 0;
  int l = 0;

  if (bfd_find_nearest_line (core_bfd, core_text_sect, core_syms,
			     addr - core_text_sect->vma,
			     &fname, &func_name, (unsigned int *) &l)
      && fname && func_name && l)
    {
      DBG (AOUTDEBUG, printf ("[get_src_info] 0x%lx -> %s:%d (%s)\n",
			      addr, fname, l, func_name));
      *filename = fname;
      *name = func_name;
      *line_num = l;
      return TRUE;
    }
  else
    {
      DBG (AOUTDEBUG, printf ("[get_src_info] no info for 0x%lx (%s:%d,%s)\n",
			      (long) addr, fname ? fname : "<unknown>", l,
			      func_name ? func_name : "<unknown>"));
      return FALSE;
    }
}


/*
 * Read in symbol table from core.  One symbol per function is
 * entered.
 */
void
DEFUN (core_create_function_syms, (core_bfd), bfd * core_bfd)
{
  bfd_vma min_vma = ~0, max_vma = 0;
  const char *filename, *func_name;
  int class;
  long i;

  /* pass 1 - determine upper bound on number of function names: */
  symtab.len = 0;
  for (i = 0; i < core_num_syms; ++i)
    {
      if (!core_sym_class (core_syms[i]))
	{
	  continue;
	}
      ++symtab.len;
    }

  if (symtab.len == 0)
    {
      fprintf (stderr, "%s: file `%s' has no symbols\n", whoami, a_out_name);
      done (1);
    }

  /* the "+ 2" is for the sentinels: */
  symtab.base = (Sym *) xmalloc ((symtab.len + 2) * sizeof (Sym));

  /* pass 2 - create symbols: */

  symtab.limit = symtab.base;
  for (i = 0; i < core_num_syms; ++i)
    {
      class = core_sym_class (core_syms[i]);
      if (!class)
	{
	  DBG (AOUTDEBUG,
	       printf ("[core_create_function_syms] rejecting: 0x%lx %s\n",
		       core_syms[i]->value, core_syms[i]->name));
	  continue;
	}

      sym_init (symtab.limit);

      /* symbol offsets are always section-relative: */

      symtab.limit->addr = core_syms[i]->value + core_syms[i]->section->vma;
      symtab.limit->name = core_syms[i]->name;

#ifdef __osf__
      /*
       * Suppress symbols that are not function names.  This is
       * useful to suppress code-labels and aliases.
       *
       * This is known to be useful under DEC's OSF/1.  Under SunOS 4.x,
       * labels do not appear in the symbol table info, so this isn't
       * necessary.
       */
      if (get_src_info (symtab.limit->addr, &filename, &func_name,
			&symtab.limit->line_num))
	{
	  symtab.limit->file = source_file_lookup_path (filename);

	  if (strcmp (symtab.limit->name, func_name) != 0)
	    {
	      /*
	       * The symbol's address maps to a different name, so
	       * it can't be a function-entry point.  This happens
	       * for labels, for example.
	       */
	      DBG (AOUTDEBUG,
		printf ("[core_create_function_syms: rej %s (maps to %s)\n",
			symtab.limit->name, func_name));
	      continue;
	    }
	}
#endif

      symtab.limit->is_func = TRUE;
      symtab.limit->is_bb_head = TRUE;
      if (class == 't')
	{
	  symtab.limit->is_static = TRUE;
	}

      min_vma = MIN (symtab.limit->addr, min_vma);
      max_vma = MAX (symtab.limit->addr, max_vma);

      /*
       * If we see "main" without an initial '_', we assume names
       * are *not* prefixed by '_'.
       */
      if (symtab.limit->name[0] == 'm' && discard_underscores
	  && strcmp (symtab.limit->name, "main") == 0)
	{
	  discard_underscores = 0;
	}

      DBG (AOUTDEBUG, printf ("[core_create_function_syms] %ld %s 0x%lx\n",
			      (long) (symtab.limit - symtab.base),
			      symtab.limit->name, symtab.limit->addr));
      ++symtab.limit;
    }

  /* create sentinels: */

  sym_init (symtab.limit);
  symtab.limit->name = "<locore>";
  symtab.limit->addr = 0;
  symtab.limit->end_addr = min_vma - 1;
  ++symtab.limit;

  sym_init (symtab.limit);
  symtab.limit->name = "<hicore>";
  symtab.limit->addr = max_vma + 1;
  symtab.limit->end_addr = ~0;
  ++symtab.limit;

  symtab.len = symtab.limit - symtab.base;
  symtab_finalize (&symtab);
}


/*
 * Read in symbol table from core.  One symbol per line of source code
 * is entered.
 */
void
DEFUN (core_create_line_syms, (core_bfd), bfd * core_bfd)
{
  char prev_name[PATH_MAX], prev_filename[PATH_MAX];
  bfd_vma vma, min_vma = ~0, max_vma = 0;
  bfd_vma offset, prev_offset, min_dist;
  Sym *prev, dummy, *sentinel, *sym;
  const char *filename;
  int prev_line_num, i;
  Sym_Table ltab;
  /*
   * Create symbols for functions as usual.  This is necessary in
   * cases where parts of a program were not compiled with -g.  For
   * those parts we still want to get info at the function level:
   */
  core_create_function_syms (core_bfd);

  /* pass 1 - counter number of symbols: */

  /*
   * To find all line information, walk through all possible
   * text-space addresses (one by one!) and get the debugging
   * info for each address.  When the debugging info changes,
   * it is time to create a new symbol.
   *
   * Of course, this is rather slow and it would be better if
   * bfd would provide an iterator for enumerating all line
   * infos, but for now, we try to speed up the second pass
   * by determining what the minimum code distance between two
   * lines is.
   */
  prev_name[0] = '\0';
  ltab.len = 0;
  min_dist = core_text_sect->_raw_size;
  prev_offset = -min_dist;
  prev_filename[0] = '\0';
  prev_line_num = 0;
  for (offset = 0; offset < core_text_sect->_raw_size; ++offset)
    {
      vma = core_text_sect->vma + offset;
      if (!get_src_info (vma, &filename, &dummy.name, &dummy.line_num)
	  || (prev_line_num == dummy.line_num &&
	      strcmp (prev_name, dummy.name) == 0
	      && strcmp (prev_filename, filename) == 0))
	{
	  continue;
	}

      ++ltab.len;
      prev_line_num = dummy.line_num;
      strcpy (prev_name, dummy.name);
      strcpy (prev_filename, filename);

      if (offset - prev_offset < min_dist)
	{
	  min_dist = offset - prev_offset;
	}
      prev_offset = offset;

      min_vma = MIN (vma, min_vma);
      max_vma = MAX (vma, max_vma);
    }

  DBG (AOUTDEBUG, printf ("[core_create_line_syms] min_dist=%lx\n", min_dist));

  /* make room for function symbols, too: */
  ltab.len += symtab.len;
  ltab.base = (Sym *) xmalloc (ltab.len * sizeof (Sym));
  ltab.limit = ltab.base;

  /* pass 2 - create symbols: */

  prev = 0;
  for (offset = 0; offset < core_text_sect->_raw_size; offset += min_dist)
    {
      sym_init (ltab.limit);
      if (!get_src_info (core_text_sect->vma + offset, &filename,
			 &ltab.limit->name, &ltab.limit->line_num)
	  || (prev && prev->line_num == ltab.limit->line_num
	      && strcmp (prev->name, ltab.limit->name) == 0
	      && strcmp (prev->file->name, filename) == 0))
	{
	  continue;
	}

      /* make name pointer a malloc'ed string: */
      ltab.limit->name = strdup (ltab.limit->name);
      ltab.limit->file = source_file_lookup_path (filename);

      ltab.limit->addr = core_text_sect->vma + offset;
      prev = ltab.limit;

      /*
       * If we see "main" without an initial '_', we assume names
       * are *not* prefixed by '_'.
       */
      if (ltab.limit->name[0] == 'm' && discard_underscores
	  && strcmp (ltab.limit->name, "main") == 0)
	{
	  discard_underscores = 0;
	}

      DBG (AOUTDEBUG, printf ("[core_create_line_syms] %d %s 0x%lx\n",
			      ltab.len, ltab.limit->name,
			      ltab.limit->addr));
      ++ltab.limit;
    }

  /* update sentinels: */

  sentinel = sym_lookup (&symtab, 0);
  if (strcmp (sentinel->name, "<locore>") == 0
      && min_vma <= sentinel->end_addr)
    {
      sentinel->end_addr = min_vma - 1;
    }

  sentinel = sym_lookup (&symtab, ~0);
  if (strcmp (sentinel->name, "<hicore>") == 0 && max_vma >= sentinel->addr)
    {
      sentinel->addr = max_vma + 1;
    }

  /* copy in function symbols: */
  memcpy (ltab.limit, symtab.base, symtab.len * sizeof (Sym));
  ltab.limit += symtab.len;

  if (ltab.limit - ltab.base != ltab.len)
    {
      fprintf (stderr,
	       "%s: somebody miscounted: ltab.len=%ld instead of %d\n",
	       whoami, (long) (ltab.limit - ltab.base), ltab.len);
      done (1);
    }

  /* finalize ltab and make it symbol table: */

  symtab_finalize (&ltab);
  free (symtab.base);
  symtab = ltab;

  /* now go through all core symbols and set is_static accordingly: */

  for (i = 0; i < core_num_syms; ++i)
    {
      if (core_sym_class (core_syms[i]) == 't')
	{
	  sym = sym_lookup (&symtab, core_syms[i]->value
			    + core_syms[i]->section->vma);
	  do
	    {
	      sym++->is_static = TRUE;
	    }
	  while (sym->file == sym[-1].file &&
		 strcmp (sym->name, sym[-1].name) == 0);
	}
    }

}
