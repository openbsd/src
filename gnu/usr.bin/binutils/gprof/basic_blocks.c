/*
 * Basic-block level related code: reading/writing of basic-block info
 * to/from gmon.out; computing and formatting of basic-block related
 * statistics.
 */
#include <stdio.h>
#include <unistd.h>
#include "basic_blocks.h"
#include "core.h"
#include "gmon_io.h"
#include "gmon_out.h"
#include "gprof.h"
#include "libiberty.h"
#include "source.h"
#include "sym_ids.h"


/*
 * Default option values:
 */
bool bb_annotate_all_lines = FALSE;
int bb_min_calls = 1;
int bb_table_length = 10;

/*
 * Variables used to compute annotated source listing stats:
 */
static long num_executable_lines;
static long num_lines_executed;


/*
 * Helper for sorting.  Compares two basic-blocks and returns result
 * such that sorting will be increasing according to filename, line
 * number, and basic-block address (in that order).
 */
static int
DEFUN (cmp_bb, (lp, rp), const void *lp AND const void *rp)
{
  int r;
  const Sym *left = *(const Sym **) lp;
  const Sym *right = *(const Sym **) rp;

  if (left->file && right->file)
    {
      r = strcmp (left->file->name, right->file->name);
      if (r)
	{
	  return r;
	}

      if (left->line_num != right->line_num)
	{
	  return left->line_num - right->line_num;
	}
    }

  if (left->addr < right->addr)
    {
      return -1;
    }
  else if (left->addr > right->addr)
    {
      return 1;
    }
  else
    {
      return 0;
    }
}


/*
 * Helper for sorting.  Order basic blocks in decreasing number of
 * calls, ties are broken in increasing order of line numbers.
 */
static int
DEFUN (cmp_ncalls, (lp, rp), const void *lp AND const void *rp)
{
  const Sym *left = *(const Sym **) lp;
  const Sym *right = *(const Sym **) rp;

  if (!left)
    {
      return 1;
    }
  else if (!right)
    {
      return -1;
    }

  if (right->ncalls != left->ncalls)
    {
      return right->ncalls - left->ncalls;
    }

  return left->line_num - right->line_num;
}


/*
 * Skip over variable length string.
 */
static void
DEFUN (fskip_string, (fp), FILE * fp)
{
  int ch;

  while ((ch = fgetc (fp)) != EOF)
    {
      if (ch == '\0')
	{
	  break;
	}
    }
}


/*
 * Read a basic-block record from file IFP.  FILENAME is the name
 * of file IFP and is provided for formatting error-messages only.
 */
void
DEFUN (bb_read_rec, (ifp, filename), FILE * ifp AND const char *filename)
{
  int nblocks, b;
  bfd_vma addr;
  long ncalls;
  Sym *sym;

  if (fread (&nblocks, sizeof (nblocks), 1, ifp) != 1)
    {
      fprintf (stderr, "%s: %s: unexpected end of file\n", whoami, filename);
      done (1);
    }

  nblocks = bfd_get_32 (core_bfd, (bfd_byte *) & nblocks);
  if (gmon_file_version == 0)
    {
      fskip_string (ifp);
    }

  for (b = 0; b < nblocks; ++b)
    {
      if (gmon_file_version == 0)
	{
	  int line_num;
	  /*
	   * Version 0 had lots of extra stuff that we don't
	   * care about anymore.
	   */
	  if ((fread (&ncalls, sizeof (ncalls), 1, ifp) != 1)
	      || (fread (&addr, sizeof (addr), 1, ifp) != 1)
	      || (fskip_string (ifp), FALSE)
	      || (fskip_string (ifp), FALSE)
	      || (fread (&line_num, sizeof (line_num), 1, ifp) != 1))
	    {
	      perror (filename);
	      done (1);
	    }
	}
      else
	{
	  if (fread (&addr, sizeof (addr), 1, ifp) != 1
	      || fread (&ncalls, sizeof (ncalls), 1, ifp) != 1)
	    {
	      perror (filename);
	      done (1);
	    }
	}

      /*
       * Basic-block execution counts are meaningful only if we're
       * profiling at the line-by-line level:
       */
      if (line_granularity)
	{

	  /* convert from target to host endianness: */

	  addr = get_vma (core_bfd, (bfd_byte *) & addr);

	  sym = sym_lookup (&symtab, addr);
	  sym->is_bb_head = TRUE;
	  sym->ncalls += bfd_get_32 (core_bfd, (bfd_byte *) & ncalls);

	  DBG (BBDEBUG, printf ("[bb_read_rec] 0x%lx->0x%lx (%s) cnt=%d\n",
				addr, sym->addr, sym->name, sym->ncalls));
	}
      else
	{
	  static bool user_warned = FALSE;

	  if (!user_warned)
	    {
	      user_warned = TRUE;
	      fprintf (stderr,
		       "%s: warning: ignoring basic-block exec counts (use -l or --line)\n",
		       whoami);
	    }
	}
    }
  return;
}


/*
 * Write all basic-blocks with non-zero counts to file OFP.  FILENAME
 * is the name of OFP and is provided for producing error-messages
 * only.
 */
void
DEFUN (bb_write_blocks, (ofp, filename), FILE * ofp AND const char *filename)
{
  const unsigned char tag = GMON_TAG_BB_COUNT;
  int nblocks = 0;
  bfd_vma addr;
  long ncalls;
  Sym *sym;

  /* count how many non-zero blocks with have: */

  for (sym = symtab.base; sym < symtab.limit; ++sym)
    {
      if (sym->ncalls > 0)
	{
	  ++nblocks;
	}
    }

  /* write header: */
  bfd_put_32 (core_bfd, nblocks, (bfd_byte *) & nblocks);
  if (fwrite (&tag, sizeof (tag), 1, ofp) != 1
      || fwrite (&nblocks, sizeof (nblocks), 1, ofp) != 1)
    {
      perror (filename);
      done (1);
    }

  /* write counts: */
  for (sym = symtab.base; sym < symtab.limit; ++sym)
    {
      if (sym->ncalls == 0)
	{
	  continue;
	}

      put_vma (core_bfd, sym->addr, (bfd_byte *) & addr);
      bfd_put_32 (core_bfd, sym->ncalls, (bfd_byte *) & ncalls);

      if (fwrite (&addr, sizeof (addr), 1, ofp) != 1
	  || fwrite (&ncalls, sizeof (ncalls), 1, ofp) != 1)
	{
	  perror (filename);
	  done (1);
	}
    }
}


/*
 * Output basic-block statistics in a format that is easily parseable.
 * Current the format is:
 *
 *      <filename>:<line-number>: (<function-name>:<bb-addr): <ncalls>
 */
void
DEFUN_VOID (print_exec_counts)
{
  Sym **sorted_bbs, *sym;
  int i, len;

  if (first_output)
    {
      first_output = FALSE;
    }
  else
    {
      printf ("\f\n");
    }

  /* sort basic-blocks according to function name and line number: */

  sorted_bbs = (Sym **) xmalloc (symtab.len * sizeof (sorted_bbs[0]));
  len = 0;
  for (sym = symtab.base; sym < symtab.limit; ++sym)
    {
      /*
       * Accept symbol if it's the start of a basic-block and it is
       * called at least bb_min_calls times and if it's in the
       * INCL_EXEC table or there is no INCL_EXEC table and it does
       * not appear in the EXCL_EXEC table.
       */
      if (sym->is_bb_head && sym->ncalls >= bb_min_calls
	  && (sym_lookup (&syms[INCL_EXEC], sym->addr)
	      || (syms[INCL_EXEC].len == 0
		  && !sym_lookup (&syms[EXCL_EXEC], sym->addr))))
	{
	  sorted_bbs[len++] = sym;
	}
    }
  qsort (sorted_bbs, len, sizeof (sorted_bbs[0]), cmp_bb);

  /* output basic-blocks: */

  for (i = 0; i < len; ++i)
    {
      sym = sorted_bbs[i];
      printf ("%s:%d: (%s:0x%lx) %d executions\n",
	      sym->file ? sym->file->name : "<unknown>", sym->line_num,
	      sym->name, sym->addr, sym->ncalls);
    }
  free (sorted_bbs);
}


/*
 * Helper for bb_annotated_source: format annotation containing
 * number of line executions.
 */
static void
DEFUN (annotate_with_count, (buf, width, line_num, arg),
       char *buf AND int width AND int line_num AND void *arg)
{
  Source_File *sf = arg;
  Sym *b;
  long cnt;
  static long last_count;

  if (line_num == 1)
    {
      last_count = -1;
    }

  b = 0;
  if (line_num <= sf->num_lines)
    {
      b = sf->line[line_num - 1];
    }
  if (!b)
    {
      cnt = -1;
    }
  else
    {
      ++num_executable_lines;
      cnt = b->ncalls;
    }
  if (cnt > 0)
    {
      ++num_lines_executed;
    }
  if (cnt < 0 && bb_annotate_all_lines)
    {
      cnt = last_count;
    }

  if (cnt < 0)
    {
      strcpy (buf, "\t\t");
    }
  else if (cnt < bb_min_calls)
    {
      strcpy (buf, "       ##### -> ");
    }
  else
    {
      sprintf (buf, "%12ld -> ", cnt);
    }
  last_count = cnt;
}


/*
 * Annotate the files named in SOURCE_FILES with basic-block statistics
 * (execution counts).  After each source files, a few statistics
 * regarding that source file are printed.
 */
void
DEFUN_VOID (print_annotated_source)
{
  Sym *sym, *line_stats, *new_line;
  Source_File *sf;
  int i, table_len;
  FILE *ofp;

  /*
   * Find maximum line number for each source file that user is
   * interested in:
   */
  for (sym = symtab.base; sym < symtab.limit; ++sym)
    {
      /*
       * Accept symbol if it's file is known, its line number is
       * bigger than anything we have seen for that file so far and
       * if it's in the INCL_ANNO table or there is no INCL_ANNO
       * table and it does not appear in the EXCL_ANNO table.
       */
      if (sym->file && sym->line_num > sym->file->num_lines
	  && (sym_lookup (&syms[INCL_ANNO], sym->addr)
	      || (syms[INCL_ANNO].len == 0
		  && !sym_lookup (&syms[EXCL_ANNO], sym->addr))))
	{
	  sym->file->num_lines = sym->line_num;
	}
    }

  /* allocate line descriptors: */

  for (sf = first_src_file; sf; sf = sf->next)
    {
      if (sf->num_lines > 0)
	{
	  sf->line = (void *) xmalloc (sf->num_lines * sizeof (sf->line[0]));
	  memset (sf->line, 0, sf->num_lines * sizeof (sf->line[0]));
	}
    }

  /* count executions per line: */

  for (sym = symtab.base; sym < symtab.limit; ++sym)
    {
      if (sym->is_bb_head && sym->file && sym->file->num_lines
	  && (sym_lookup (&syms[INCL_ANNO], sym->addr)
	      || (syms[INCL_ANNO].len == 0
		  && !sym_lookup (&syms[EXCL_ANNO], sym->addr))))
	{
	  sym->file->ncalls += sym->ncalls;
	  line_stats = sym->file->line[sym->line_num - 1];
	  if (!line_stats)
	    {
	      /* common case has at most one basic-block per source line: */
	      sym->file->line[sym->line_num - 1] = sym;
	    }
	  else if (!line_stats->addr)
	    {
	      /* sym is the 3rd .. nth basic block for this line: */
	      line_stats->ncalls += sym->ncalls;
	    }
	  else
	    {
	      /* sym is the second basic block for this line */
	      new_line = (Sym *) xmalloc (sizeof (*new_line));
	      *new_line = *line_stats;
	      new_line->addr = 0;
	      new_line->ncalls += sym->ncalls;
	      sym->file->line[sym->line_num - 1] = new_line;
	    }
	}
    }

  /* plod over source files, annotating them: */

  for (sf = first_src_file; sf; sf = sf->next)
    {
      if (!sf->num_lines || (ignore_zeros && sf->ncalls == 0))
	{
	  continue;
	}

      num_executable_lines = num_lines_executed = 0;
      ofp = annotate_source (sf, 16, annotate_with_count, sf);
      if (!ofp)
	{
	  continue;
	}

      if (bb_table_length > 0)
	{
	  fprintf (ofp, "\n\nTop %d Lines:\n\n     Line      Count\n\n",
		   bb_table_length);

	  /* abuse line arrays---it's not needed anymore: */
	  qsort (sf->line, sf->num_lines, sizeof (sf->line[0]), cmp_ncalls);
	  table_len = bb_table_length;
	  if (table_len > sf->num_lines)
	    {
	      table_len = sf->num_lines;
	    }
	  for (i = 0; i < table_len; ++i)
	    {
	      sym = sf->line[i];
	      if (!sym || sym->ncalls <= 0)
		{
		  break;
		}
	      fprintf (ofp, "%9d %10d\n", sym->line_num, sym->ncalls);
	    }
	}

      free (sf->line);
      sf->line = 0;

      fprintf (ofp, "\nExecution Summary:\n\n");
      fprintf (ofp, "%9ld   Executable lines in this file\n",
	       num_executable_lines);
      fprintf (ofp, "%9ld   Lines executed\n", num_lines_executed);
      fprintf (ofp, "%9.2f   Percent of the file executed\n",
	       num_executable_lines
	       ? 100.0 * num_lines_executed / (double) num_executable_lines
	       : 100.0);
      fprintf (ofp, "\n%9d   Total number of line executions\n", sf->ncalls);
      fprintf (ofp, "%9.2f   Average executions per line\n",
	       num_executable_lines
	       ? sf->ncalls / (double) num_executable_lines
	       : 0.0);
      if (ofp != stdout)
	{
	  fclose (ofp);
	}
    }
}
