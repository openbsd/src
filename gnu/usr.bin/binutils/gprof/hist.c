/*
 * Histogram related operations.
 */
#include <stdio.h>
#include "libiberty.h"
#include "gprof.h"
#include "corefile.h"
#include "gmon_io.h"
#include "gmon_out.h"
#include "hist.h"
#include "symtab.h"
#include "sym_ids.h"
#include "utils.h"

#define UNITS_TO_CODE (offset_to_code / sizeof(UNIT))

static void scale_and_align_entries PARAMS ((void));

/* declarations of automatically generated functions to output blurbs: */
extern void flat_blurb PARAMS ((FILE * fp));

bfd_vma s_lowpc;		/* lowest address in .text */
bfd_vma s_highpc = 0;		/* highest address in .text */
bfd_vma lowpc, highpc;		/* same, but expressed in UNITs */
int hist_num_bins = 0;		/* number of histogram samples */
int *hist_sample = 0;		/* histogram samples (shorts in the file!) */
double hist_scale;
char hist_dimension[sizeof (((struct gmon_hist_hdr *) 0)->dimen) + 1] =
  "seconds";
char hist_dimension_abbrev = 's';

static double accum_time;	/* accumulated time so far for print_line() */
static double total_time;	/* total time for all routines */
/*
 * Table of SI prefixes for powers of 10 (used to automatically
 * scale some of the values in the flat profile).
 */
const struct
  {
    char prefix;
    double scale;
  }
SItab[] =
{
  {
    'T', 1e-12
  }
  ,				/* tera */
  {
    'G', 1e-09
  }
  ,				/* giga */
  {
    'M', 1e-06
  }
  ,				/* mega */
  {
    'K', 1e-03
  }
  ,				/* kilo */
  {
    ' ', 1e-00
  }
  ,
  {
    'm', 1e+03
  }
  ,				/* milli */
  {
    'u', 1e+06
  }
  ,				/* micro */
  {
    'n', 1e+09
  }
  ,				/* nano */
  {
    'p', 1e+12
  }
  ,				/* pico */
  {
    'f', 1e+15
  }
  ,				/* femto */
  {
    'a', 1e+18
  }
  ,				/* ato */
};

/*
 * Read the histogram from file IFP.  FILENAME is the name of IFP and
 * is provided for formatting error messages only.
 */
void
DEFUN (hist_read_rec, (ifp, filename), FILE * ifp AND const char *filename)
{
  struct gmon_hist_hdr hdr;
  bfd_vma n_lowpc, n_highpc;
  int i, ncnt, profrate;
  UNIT count;

  if (fread (&hdr, sizeof (hdr), 1, ifp) != 1)
    {
      fprintf (stderr, _("%s: %s: unexpected end of file\n"),
	       whoami, filename);
      done (1);
    }

  n_lowpc = (bfd_vma) get_vma (core_bfd, (bfd_byte *) hdr.low_pc);
  n_highpc = (bfd_vma) get_vma (core_bfd, (bfd_byte *) hdr.high_pc);
  ncnt = bfd_get_32 (core_bfd, (bfd_byte *) hdr.hist_size);
  profrate = bfd_get_32 (core_bfd, (bfd_byte *) hdr.prof_rate);
  strncpy (hist_dimension, hdr.dimen, sizeof (hdr.dimen));
  hist_dimension[sizeof (hdr.dimen)] = '\0';
  hist_dimension_abbrev = hdr.dimen_abbrev;

  if (!s_highpc)
    {

      /* this is the first histogram record: */

      s_lowpc = n_lowpc;
      s_highpc = n_highpc;
      lowpc = (bfd_vma) n_lowpc / sizeof (UNIT);
      highpc = (bfd_vma) n_highpc / sizeof (UNIT);
      hist_num_bins = ncnt;
      hz = profrate;
    }

  DBG (SAMPLEDEBUG,
       printf ("[hist_read_rec] n_lowpc 0x%lx n_highpc 0x%lx ncnt %d\n",
	       (unsigned long) n_lowpc, (unsigned long) n_highpc, ncnt);
       printf ("[hist_read_rec] s_lowpc 0x%lx s_highpc 0x%lx nsamples %d\n",
	       (unsigned long) s_lowpc, (unsigned long) s_highpc,
	       hist_num_bins);
       printf ("[hist_read_rec]   lowpc 0x%lx   highpc 0x%lx\n",
	       (unsigned long) lowpc, (unsigned long) highpc));

  if (n_lowpc != s_lowpc || n_highpc != s_highpc
      || ncnt != hist_num_bins || hz != profrate)
    {
      fprintf (stderr, _("%s: `%s' is incompatible with first gmon file\n"),
	       whoami, filename);
      done (1);
    }

  if (!hist_sample)
    {
      hist_sample = (int *) xmalloc (hist_num_bins * sizeof (hist_sample[0]));
      memset (hist_sample, 0, hist_num_bins * sizeof (hist_sample[0]));
    }

  for (i = 0; i < hist_num_bins; ++i)
    {
      if (fread (&count[0], sizeof (count), 1, ifp) != 1)
	{
	  fprintf (stderr,
		   _("%s: %s: unexpected EOF after reading %d of %d samples\n"),
		   whoami, filename, i, hist_num_bins);
	  done (1);
	}
      hist_sample[i] += bfd_get_16 (core_bfd, (bfd_byte *) & count[0]);
    }
}


/*
 * Write execution histogram to file OFP.  FILENAME is the name
 * of OFP and is provided for formatting error-messages only.
 */
void
DEFUN (hist_write_hist, (ofp, filename), FILE * ofp AND const char *filename)
{
  struct gmon_hist_hdr hdr;
  unsigned char tag;
  UNIT count;
  int i;

  /* write header: */

  tag = GMON_TAG_TIME_HIST;
  put_vma (core_bfd, s_lowpc, (bfd_byte *) hdr.low_pc);
  put_vma (core_bfd, s_highpc, (bfd_byte *) hdr.high_pc);
  bfd_put_32 (core_bfd, hist_num_bins, (bfd_byte *) hdr.hist_size);
  bfd_put_32 (core_bfd, hz, (bfd_byte *) hdr.prof_rate);
  strncpy (hdr.dimen, hist_dimension, sizeof (hdr.dimen));
  hdr.dimen_abbrev = hist_dimension_abbrev;

  if (fwrite (&tag, sizeof (tag), 1, ofp) != 1
      || fwrite (&hdr, sizeof (hdr), 1, ofp) != 1)
    {
      perror (filename);
      done (1);
    }

  for (i = 0; i < hist_num_bins; ++i)
    {
      bfd_put_16 (core_bfd, hist_sample[i], (bfd_byte *) & count[0]);
      if (fwrite (&count[0], sizeof (count), 1, ofp) != 1)
	{
	  perror (filename);
	  done (1);
	}
    }
}


/*
 * Calculate scaled entry point addresses (to save time in
 * hist_assign_samples), and, on architectures that have procedure
 * entry masks at the start of a function, possibly push the scaled
 * entry points over the procedure entry mask, if it turns out that
 * the entry point is in one bin and the code for a routine is in the
 * next bin.
 */
static void
scale_and_align_entries ()
{
  Sym *sym;
  bfd_vma bin_of_entry;
  bfd_vma bin_of_code;

  for (sym = symtab.base; sym < symtab.limit; sym++)
    {
      sym->hist.scaled_addr = sym->addr / sizeof (UNIT);
      bin_of_entry = (sym->hist.scaled_addr - lowpc) / hist_scale;
      bin_of_code = (sym->hist.scaled_addr + UNITS_TO_CODE - lowpc) / hist_scale;
      if (bin_of_entry < bin_of_code)
	{
	  DBG (SAMPLEDEBUG,
	       printf ("[scale_and_align_entries] pushing 0x%lx to 0x%lx\n",
		       (unsigned long) sym->hist.scaled_addr,
		       (unsigned long) (sym->hist.scaled_addr
					+ UNITS_TO_CODE)));
	  sym->hist.scaled_addr += UNITS_TO_CODE;
	}
    }
}


/*
 * Assign samples to the symbol to which they belong.
 *
 * Histogram bin I covers some address range [BIN_LOWPC,BIN_HIGH_PC)
 * which may overlap one more symbol address ranges.  If a symbol
 * overlaps with the bin's address range by O percent, then O percent
 * of the bin's count is credited to that symbol.
 *
 * There are three cases as to where BIN_LOW_PC and BIN_HIGH_PC can be
 * with respect to the symbol's address range [SYM_LOW_PC,
 * SYM_HIGH_PC) as shown in the following diagram.  OVERLAP computes
 * the distance (in UNITs) between the arrows, the fraction of the
 * sample that is to be credited to the symbol which starts at
 * SYM_LOW_PC.
 *
 *        sym_low_pc                                      sym_high_pc
 *             |                                               |
 *             v                                               v
 *
 *             +-----------------------------------------------+
 *             |                                               |
 *        |  ->|    |<-         ->|         |<-         ->|    |<-  |
 *        |         |             |         |             |         |
 *        +---------+             +---------+             +---------+
 *
 *        ^         ^             ^         ^             ^         ^
 *        |         |             |         |             |         |
 *   bin_low_pc bin_high_pc  bin_low_pc bin_high_pc  bin_low_pc bin_high_pc
 *
 * For the VAX we assert that samples will never fall in the first two
 * bytes of any routine, since that is the entry mask, thus we call
 * scale_and_align_entries() to adjust the entry points if the entry
 * mask falls in one bin but the code for the routine doesn't start
 * until the next bin.  In conjunction with the alignment of routine
 * addresses, this should allow us to have only one sample for every
 * four bytes of text space and never have any overlap (the two end
 * cases, above).
 */
void
DEFUN_VOID (hist_assign_samples)
{
  bfd_vma bin_low_pc, bin_high_pc;
  bfd_vma sym_low_pc, sym_high_pc;
  bfd_vma overlap, addr;
  int bin_count, i;
  unsigned int j;
  double time, credit;

  /* read samples and assign to symbols: */
  hist_scale = highpc - lowpc;
  hist_scale /= hist_num_bins;
  scale_and_align_entries ();

  /* iterate over all sample bins: */

  for (i = 0, j = 1; i < hist_num_bins; ++i)
    {
      bin_count = hist_sample[i];
      if (!bin_count)
	{
	  continue;
	}
      bin_low_pc = lowpc + (bfd_vma) (hist_scale * i);
      bin_high_pc = lowpc + (bfd_vma) (hist_scale * (i + 1));
      time = bin_count;
      DBG (SAMPLEDEBUG,
	   printf (
      "[assign_samples] bin_low_pc=0x%lx, bin_high_pc=0x%lx, bin_count=%d\n",
		    (unsigned long) (sizeof (UNIT) * bin_low_pc),
		    (unsigned long) (sizeof (UNIT) * bin_high_pc),
		    bin_count));
      total_time += time;

      /* credit all symbols that are covered by bin I: */

      for (j = j - 1; j < symtab.len; ++j)
	{
	  sym_low_pc = symtab.base[j].hist.scaled_addr;
	  sym_high_pc = symtab.base[j + 1].hist.scaled_addr;
	  /*
	   * If high end of bin is below entry address, go for next
	   * bin:
	   */
	  if (bin_high_pc < sym_low_pc)
	    {
	      break;
	    }
	  /*
	   * If low end of bin is above high end of symbol, go for
	   * next symbol.
	   */
	  if (bin_low_pc >= sym_high_pc)
	    {
	      continue;
	    }
	  overlap =
	    MIN (bin_high_pc, sym_high_pc) - MAX (bin_low_pc, sym_low_pc);
	  if (overlap > 0)
	    {
	      DBG (SAMPLEDEBUG,
		   printf (
			    "[assign_samples] [0x%lx,0x%lx) %s gets %f ticks %ld overlap\n",
			    (unsigned long) symtab.base[j].addr,
			    (unsigned long) (sizeof (UNIT) * sym_high_pc),
			    symtab.base[j].name, overlap * time / hist_scale,
			    (long) overlap));
	      addr = symtab.base[j].addr;
	      credit = overlap * time / hist_scale;
	      /*
	       * Credit symbol if it appears in INCL_FLAT or that
	       * table is empty and it does not appear it in
	       * EXCL_FLAT.
	       */
	      if (sym_lookup (&syms[INCL_FLAT], addr)
		  || (syms[INCL_FLAT].len == 0
		      && !sym_lookup (&syms[EXCL_FLAT], addr)))
		{
		  symtab.base[j].hist.time += credit;
		}
	      else
		{
		  total_time -= credit;
		}
	    }
	}
    }
  DBG (SAMPLEDEBUG, printf ("[assign_samples] total_time %f\n",
			    total_time));
}


/*
 * Print header for flag histogram profile:
 */
static void
DEFUN (print_header, (prefix), const char prefix)
{
  char unit[64];

  sprintf (unit, _("%c%c/call"), prefix, hist_dimension_abbrev);

  if (bsd_style_output)
    {
      printf (_("\ngranularity: each sample hit covers %ld byte(s)"),
	      (long) hist_scale * sizeof (UNIT));
      if (total_time > 0.0)
	{
	  printf (_(" for %.2f%% of %.2f %s\n\n"),
		  100.0 / total_time, total_time / hz, hist_dimension);
	}
    }
  else
    {
      printf (_("\nEach sample counts as %g %s.\n"), 1.0 / hz, hist_dimension);
    }

  if (total_time <= 0.0)
    {
      printf (_(" no time accumulated\n\n"));
      /* this doesn't hurt since all the numerators will be zero: */
      total_time = 1.0;
    }

  printf ("%5.5s %10.10s %8.8s %8.8s %8.8s %8.8s  %-8.8s\n",
	  "%  ", _("cumulative"), _("self  "), "", _("self  "), _("total "), "");
  printf ("%5.5s %9.9s  %8.8s %8.8s %8.8s %8.8s  %-8.8s\n",
	  _("time"), hist_dimension, hist_dimension, _("calls"), unit, unit,
	  _("name"));
}


static void
DEFUN (print_line, (sym, scale), Sym * sym AND double scale)
{
  if (ignore_zeros && sym->ncalls == 0 && sym->hist.time == 0)
    {
      return;
    }

  accum_time += sym->hist.time;
  if (bsd_style_output)
    {
      printf ("%5.1f %10.2f %8.2f",
	      total_time > 0.0 ? 100 * sym->hist.time / total_time : 0.0,
	      accum_time / hz, sym->hist.time / hz);
    }
  else
    {
      printf ("%6.2f %9.2f %8.2f",
	      total_time > 0.0 ? 100 * sym->hist.time / total_time : 0.0,
	      accum_time / hz, sym->hist.time / hz);
    }
  if (sym->ncalls != 0)
    {
      printf (" %8lu %8.2f %8.2f  ",
	      sym->ncalls, scale * sym->hist.time / hz / sym->ncalls,
	  scale * (sym->hist.time + sym->cg.child_time) / hz / sym->ncalls);
    }
  else
    {
      printf (" %8.8s %8.8s %8.8s  ", "", "", "");
    }
  if (bsd_style_output)
    {
      print_name (sym);
    }
  else
    {
      print_name_only (sym);
    }
  printf ("\n");
}


/*
 * Compare LP and RP.  The primary comparison key is execution time,
 * the secondary is number of invocation, and the tertiary is the
 * lexicographic order of the function names.
 */
static int
DEFUN (cmp_time, (lp, rp), const PTR lp AND const PTR rp)
{
  const Sym *left = *(const Sym **) lp;
  const Sym *right = *(const Sym **) rp;
  double time_diff;

  time_diff = right->hist.time - left->hist.time;
  if (time_diff > 0.0)
    {
      return 1;
    }
  if (time_diff < 0.0)
    {
      return -1;
    }

  if (right->ncalls > left->ncalls)
    {
      return 1;
    }
  if (right->ncalls < left->ncalls)
    {
      return -1;
    }

  return strcmp (left->name, right->name);
}


/*
 * Print the flat histogram profile.
 */
void
DEFUN_VOID (hist_print)
{
  Sym **time_sorted_syms, *top_dog, *sym;
  unsigned int index;
  int log_scale;
  double top_time, time;
  bfd_vma addr;

  if (first_output)
    {
      first_output = FALSE;
    }
  else
    {
      printf ("\f\n");
    }

  accum_time = 0.0;
  if (bsd_style_output)
    {
      if (print_descriptions)
	{
	  printf (_("\n\n\nflat profile:\n"));
	  flat_blurb (stdout);
	}
    }
  else
    {
      printf (_("Flat profile:\n"));
    }
  /*
   * Sort the symbol table by time (call-count and name as secondary
   * and tertiary keys):
   */
  time_sorted_syms = (Sym **) xmalloc (symtab.len * sizeof (Sym *));
  for (index = 0; index < symtab.len; ++index)
    {
      time_sorted_syms[index] = &symtab.base[index];
    }
  qsort (time_sorted_syms, symtab.len, sizeof (Sym *), cmp_time);

  if (bsd_style_output)
    {
      log_scale = 5;		/* milli-seconds is BSD-default */
    }
  else
    {
      /*
       * Search for symbol with highest per-call execution time and
       * scale accordingly:
       */
      log_scale = 0;
      top_dog = 0;
      top_time = 0.0;
      for (index = 0; index < symtab.len; ++index)
	{
	  sym = time_sorted_syms[index];
	  if (sym->ncalls != 0)
	    {
	      time = (sym->hist.time + sym->cg.child_time) / sym->ncalls;
	      if (time > top_time)
		{
		  top_dog = sym;
		  top_time = time;
		}
	    }
	}
      if (top_dog && top_dog->ncalls != 0 && top_time > 0.0)
	{
	  top_time /= hz;
	  while (SItab[log_scale].scale * top_time < 1000.0
		 && ((size_t) log_scale
		     < sizeof (SItab) / sizeof (SItab[0]) - 1))
	    {
	      ++log_scale;
	    }
	}
    }

  /*
   * For now, the dimension is always seconds.  In the future, we
   * may also want to support other (pseudo-)dimensions (such as
   * I-cache misses etc.).
   */
  print_header (SItab[log_scale].prefix);
  for (index = 0; index < symtab.len; ++index)
    {
      addr = time_sorted_syms[index]->addr;
      /*
       * Print symbol if its in INCL_FLAT table or that table
       * is empty and the symbol is not in EXCL_FLAT.
       */
      if (sym_lookup (&syms[INCL_FLAT], addr)
	  || (syms[INCL_FLAT].len == 0
	      && !sym_lookup (&syms[EXCL_FLAT], addr)))
	{
	  print_line (time_sorted_syms[index], SItab[log_scale].scale);
	}
    }
  free (time_sorted_syms);

  if (print_descriptions && !bsd_style_output)
    {
      flat_blurb (stdout);
    }
}
