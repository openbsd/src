/*
 * Input and output from/to gmon.out files.
 */
#include "cg_arcs.h"
#include "basic_blocks.h"
#include "bfd.h"
#include "core.h"
#include "call_graph.h"
#include "gmon_io.h"
#include "gmon_out.h"
#include "gmon.h"		/* fetch header for old format */
#include "gprof.h"
#include "hertz.h"
#include "hist.h"
#include "libiberty.h"

int gmon_input = 0;
int gmon_file_version = 0;	/* 0 == old (non-versioned) file format */

/*
 * This probably ought to be in libbfd.
 */
bfd_vma
DEFUN (get_vma, (abfd, addr), bfd * abfd AND bfd_byte * addr)
{
  switch (sizeof (char*))
    {
    case 4:
      return bfd_get_32 (abfd, addr);
    case 8:
      return bfd_get_64 (abfd, addr);
    default:
      fprintf (stderr, "%s: bfd_vma has unexpected size of %ld bytes\n",
	       whoami, (long) sizeof (char*));
      done (1);
    }
}


/*
 * This probably ought to be in libbfd.
 */
void
DEFUN (put_vma, (abfd, val, addr), bfd * abfd AND bfd_vma val AND bfd_byte * addr)
{
  switch (sizeof (char*))
    {
    case 4:
      bfd_put_32 (abfd, val, addr);
      break;
    case 8:
      bfd_put_64 (abfd, val, addr);
      break;
    default:
      fprintf (stderr, "%s: bfd_vma has unexpected size of %ld bytes\n",
	       whoami, (long) sizeof (char*));
      done (1);
    }
}


void
DEFUN (gmon_out_read, (filename), const char *filename)
{
  FILE *ifp;
  struct gmon_hdr ghdr;
  unsigned char tag;
  int nhist = 0, narcs = 0, nbbs = 0;

  /* open gmon.out file: */

  if (strcmp (filename, "-") == 0)
    {
      ifp = stdin;
    }
  else
    {
      ifp = fopen (filename, FOPEN_RB);
      if (!ifp)
	{
	  perror (filename);
	  done (1);
	}
    }
  if (fread (&ghdr, sizeof (struct gmon_hdr), 1, ifp) != 1)
    {
      fprintf (stderr, "%s: file too short to be a gmon file\n",
	       filename);
      done (1);
    }

  if ((file_format == FF_MAGIC) ||
      (file_format == FF_AUTO && !strncmp (&ghdr.cookie[0], GMON_MAGIC, 4)))
    {
      if (file_format == FF_MAGIC && strncmp (&ghdr.cookie[0], GMON_MAGIC, 4))
	{
	  fprintf (stderr, "%s: file `%s' has bad magic cookie\n",
		   whoami, filename);
	  done (1);
	}

      /* right magic, so it's probably really a new gmon.out file */

      gmon_file_version = bfd_get_32 (core_bfd, (bfd_byte *) ghdr.version);
      if (gmon_file_version != GMON_VERSION && gmon_file_version != 0)
	{
	  fprintf (stderr,
		   "%s: file `%s' has unsupported version %d\n",
		   whoami, filename, gmon_file_version);
	  done (1);
	}

      /* read in all the records: */
      while (fread (&tag, sizeof (tag), 1, ifp) == 1)
	{
	  switch (tag)
	    {
	    case GMON_TAG_TIME_HIST:
	      ++nhist;
	      gmon_input |= INPUT_HISTOGRAM;
	      hist_read_rec (ifp, filename);
	      break;

	    case GMON_TAG_CG_ARC:
	      ++narcs;
	      gmon_input |= INPUT_CALL_GRAPH;
	      cg_read_rec (ifp, filename);
	      break;

	    case GMON_TAG_BB_COUNT:
	      ++nbbs;
	      gmon_input |= INPUT_BB_COUNTS;
	      bb_read_rec (ifp, filename);
	      break;

	    default:
	      fprintf (stderr,
		       "%s: %s: found bad tag %d (file corrupted?)\n",
		       whoami, filename, tag);
	      done (1);
	    }
	}
    }
  else if (file_format == FF_AUTO || file_format == FF_BSD)
    {
      struct hdr
      {
	bfd_vma low_pc;
	bfd_vma high_pc;
	int ncnt;
      };
      int i, samp_bytes, count;
      bfd_vma from_pc, self_pc;
      struct raw_arc raw_arc;
      struct raw_phdr raw;
      static struct hdr h;
      UNIT raw_bin_count;
      struct hdr tmp;

      /*
       * Information from a gmon.out file is in two parts: an array of
       * sampling hits within pc ranges, and the arcs.
       */
      gmon_input = INPUT_HISTOGRAM | INPUT_CALL_GRAPH;

      /*
       * This fseek() ought to work even on stdin as long as it's
       * not an interactive device (heck, is there anybody who would
       * want to type in a gmon.out at the terminal?).
       */
      if (fseek (ifp, 0, SEEK_SET) < 0)
	{
	  perror (filename);
	  done (1);
	}
      if (fread (&raw, 1, sizeof (struct raw_phdr), ifp)
	  != sizeof (struct raw_phdr))
	{
	  fprintf (stderr, "%s: file too short to be a gmon file\n",
		   filename);
	  done (1);
	}
      tmp.low_pc = get_vma (core_bfd, (bfd_byte *) & raw.low_pc[0]);
      tmp.high_pc = get_vma (core_bfd, (bfd_byte *) & raw.high_pc[0]);
      tmp.ncnt = bfd_get_32 (core_bfd, (bfd_byte *) & raw.ncnt[0]);
      if (s_highpc && (tmp.low_pc != h.low_pc ||
		       tmp.high_pc != h.high_pc || tmp.ncnt != h.ncnt))
	{
	  fprintf (stderr, "%s: incompatible with first gmon file\n",
		   filename);
	  done (1);
	}
      h = tmp;
      s_lowpc = (bfd_vma) h.low_pc;
      s_highpc = (bfd_vma) h.high_pc;
      lowpc = (bfd_vma) h.low_pc / sizeof (UNIT);
      highpc = (bfd_vma) h.high_pc / sizeof (UNIT);
      samp_bytes = h.ncnt - sizeof (struct raw_phdr);
      hist_num_bins = samp_bytes / sizeof (UNIT);
      DBG (SAMPLEDEBUG,
	   printf ("[gmon_out_read] lowpc 0x%lx highpc 0x%lx ncnt %d\n",
		   h.low_pc, h.high_pc, h.ncnt);
	   printf ("[gmon_out_read]   s_lowpc 0x%lx   s_highpc 0x%lx\n",
		   s_lowpc, s_highpc);
	   printf ("[gmon_out_read]     lowpc 0x%lx     highpc 0x%lx\n",
		   lowpc, highpc);
	   printf ("[gmon_out_read] samp_bytes %d hist_num_bins %d\n",
		   samp_bytes, hist_num_bins));

      if (hist_num_bins)
	{
	  ++nhist;
	}

      if (!hist_sample)
	{
	  hist_sample =
	    (int *) xmalloc (hist_num_bins * sizeof (hist_sample[0]));
	  memset (hist_sample, 0, hist_num_bins * sizeof (hist_sample[0]));
	}

      for (i = 0; i < hist_num_bins; ++i)
	{
	  if (fread (raw_bin_count, sizeof (raw_bin_count), 1, ifp) != 1)
	    {
	      fprintf (stderr,
		       "%s: unexpected EOF after reading %d/%d bins\n",
		       whoami, --i, hist_num_bins);
	      done (1);
	    }
	  hist_sample[i] += bfd_get_16 (core_bfd, (bfd_byte *) raw_bin_count);
	}

      /*
       * The rest of the file consists of a bunch of <from,self,count>
       * tuples:
       */
      while (fread (&raw_arc, sizeof (raw_arc), 1, ifp) == 1)
	{
	  ++narcs;
	  from_pc = get_vma (core_bfd, (bfd_byte *) raw_arc.from_pc);
	  self_pc = get_vma (core_bfd, (bfd_byte *) raw_arc.self_pc);
	  count = bfd_get_32 (core_bfd, (bfd_byte *) raw_arc.count);
	  DBG (SAMPLEDEBUG,
	     printf ("[gmon_out_read] frompc 0x%lx selfpc 0x%lx count %d\n",
		     from_pc, self_pc, count));
	  /* add this arc: */
	  cg_tally (from_pc, self_pc, count);
	}
      fclose (ifp);

      if (hz == HZ_WRONG)
	{
	  /*
	   * How many ticks per second?  If we can't tell, report
	   * time in ticks.
	   */
	  hz = hertz ();
	  if (hz == HZ_WRONG)
	    {
	      hz = 1;
	      fprintf (stderr, "time is in ticks, not seconds\n");
	    }
	}
    }
  else
    {
      fprintf (stderr, "%s: don't know how to deal with file format %d\n",
	       whoami, file_format);
      done (1);
    }

  if (output_style & STYLE_GMON_INFO)
    {
      printf ("File `%s' (version %d) contains:\n",
	      filename, gmon_file_version);
      printf ("\t%d histogram record%s\n",
	      nhist, nhist == 1 ? "" : "s");
      printf ("\t%d call-graph record%s\n",
	      narcs, narcs == 1 ? "" : "s");
      printf ("\t%d basic-block count record%s\n",
	      nbbs, nbbs == 1 ? "" : "s");
      first_output = FALSE;
    }
}


void
DEFUN (gmon_out_write, (filename), const char *filename)
{
  FILE *ofp;
  struct gmon_hdr ghdr;

  ofp = fopen (filename, FOPEN_WB);
  if (!ofp)
    {
      perror (filename);
      done (1);
    }

  if (file_format == FF_AUTO || file_format == FF_MAGIC)
    {
      /* write gmon header: */

      memcpy (&ghdr.cookie[0], GMON_MAGIC, 4);
      bfd_put_32 (core_bfd, GMON_VERSION, (bfd_byte *) ghdr.version);
      if (fwrite (&ghdr, sizeof (ghdr), 1, ofp) != 1)
	{
	  perror (filename);
	  done (1);
	}

      /* write execution time histogram if we have one: */
      if (gmon_input & INPUT_HISTOGRAM)
	{
	  hist_write_hist (ofp, filename);
	}

      /* write call graph arcs if we have any: */
      if (gmon_input & INPUT_CALL_GRAPH)
	{
	  cg_write_arcs (ofp, filename);
	}

      /* write basic-block info if we have it: */
      if (gmon_input & INPUT_BB_COUNTS)
	{
	  bb_write_blocks (ofp, filename);
	}
    }
  else if (file_format == FF_BSD)
    {
      struct raw_arc raw_arc;
      UNIT raw_bin_count;
      bfd_vma lpc, hpc;
      int i, ncnt;
      Arc *arc;
      Sym *sym;

      put_vma (core_bfd, s_lowpc, (bfd_byte *) & lpc);
      put_vma (core_bfd, s_highpc, (bfd_byte *) & hpc);
      bfd_put_32 (core_bfd,
		  hist_num_bins * sizeof (UNIT) + sizeof (struct raw_phdr),
		    (bfd_byte *) & ncnt);

      /* write header: */
      if (fwrite (&lpc, sizeof (lpc), 1, ofp) != 1
	  || fwrite (&hpc, sizeof (hpc), 1, ofp) != 1
	  || fwrite (&ncnt, sizeof (ncnt), 1, ofp) != 1)
	{
	  perror (filename);
	  done (1);
	}

      /* dump the samples: */

      for (i = 0; i < hist_num_bins; ++i)
	{
	  bfd_put_16 (core_bfd, hist_sample[i], (bfd_byte *) & raw_bin_count[0]);
	  if (fwrite (&raw_bin_count[0], sizeof (raw_bin_count), 1, ofp) != 1)
	    {
	      perror (filename);
	      done (1);
	    }
	}

      /* dump the normalized raw arc information: */

      for (sym = symtab.base; sym < symtab.limit; ++sym)
	{
	  for (arc = sym->cg.children; arc; arc = arc->next_child)
	    {
	      put_vma (core_bfd, arc->parent->addr,
		       (bfd_byte *) raw_arc.from_pc);
	      put_vma (core_bfd, arc->child->addr,
		       (bfd_byte *) raw_arc.self_pc);
	      bfd_put_32 (core_bfd, arc->count, (bfd_byte *) raw_arc.count);
	      if (fwrite (&raw_arc, sizeof (raw_arc), 1, ofp) != 1)
		{
		  perror (filename);
		  done (1);
		}
	      DBG (SAMPLEDEBUG,
		   printf ("[dumpsum] frompc 0x%lx selfpc 0x%lx count %d\n",
			   arc->parent->addr, arc->child->addr, arc->count));
	    }
	}
      fclose (ofp);
    }
  else
    {
      fprintf (stderr, "%s: don't know how to deal with file format %d\n",
	       whoami, file_format);
      done (1);
    }
}
