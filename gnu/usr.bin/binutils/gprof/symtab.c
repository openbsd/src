#include "gprof.h"
#include "cg_arcs.h"
#include "corefile.h"
#include "symtab.h"

Sym_Table symtab;


/*
 * Initialize a symbol (so it's empty).
 */
void
DEFUN (sym_init, (sym), Sym * sym)
{
  memset (sym, 0, sizeof (*sym));
  /*
   * It is not safe to assume that a binary zero corresponds to
   * a floating-point 0.0, so initialize floats explicitly:
   */
  sym->hist.time = 0.0;
  sym->cg.child_time = 0.0;
  sym->cg.prop.fract = 0.0;
  sym->cg.prop.self = 0.0;
  sym->cg.prop.child = 0.0;
}


/*
 * Compare the function entry-point of two symbols and return <0, =0,
 * or >0 depending on whether the left value is smaller than, equal
 * to, or greater than the right value.  If two symbols are equal
 * but one has is_func set and the other doesn't, we make the
 * non-function symbol one "bigger" so that the function symbol will
 * survive duplicate removal.  Finally, if both symbols have the
 * same is_func value, we discriminate against is_static such that
 * the global symbol survives.
 */
static int
DEFUN (cmp_addr, (lp, rp), const PTR lp AND const PTR rp)
{
  Sym *left = (Sym *) lp;
  Sym *right = (Sym *) rp;

  if (left->addr > right->addr)
    {
      return 1;
    }
  else if (left->addr < right->addr)
    {
      return -1;
    }

  if (left->is_func != right->is_func)
    {
      return right->is_func - left->is_func;
    }

  return left->is_static - right->is_static;
}


void
DEFUN (symtab_finalize, (tab), Sym_Table * tab)
{
  Sym *src, *dst;
  bfd_vma prev_addr;

  if (!tab->len)
    {
      return;
    }

  /*
   * Sort symbol table in order of increasing function addresses:
   */
  qsort (tab->base, tab->len, sizeof (Sym), cmp_addr);

  /*
   * Remove duplicate entries to speed-up later processing and
   * set end_addr if its not set yet:
   */
  prev_addr = tab->base[0].addr + 1;
  for (src = dst = tab->base; src < tab->limit; ++src)
    {
      if (src->addr == prev_addr)
	{
	  /*
	   * If same address, favor global symbol over static one,
	   * then function over line number.  If both symbols are
	   * either static or global and either function or line, check
	   * whether one has name beginning with underscore while
	   * the other doesn't.  In such cases, keep sym without
	   * underscore.  This takes cares of compiler generated
	   * symbols (such as __gnu_compiled, __c89_used, etc.).
	   */
	  if ((!src->is_static && dst[-1].is_static)
	      || ((src->is_static == dst[-1].is_static)
		  && ((src->is_func && !dst[-1].is_func)
		      || ((src->is_func == dst[-1].is_func)
			  && ((src->name[0] != '_' && dst[-1].name[0] == '_')
			      || (src->name[0]
				  && src->name[1] != '_'
				  && dst[-1].name[1] == '_'))))))
	    {
	      DBG (AOUTDEBUG | IDDEBUG,
		   printf ("[symtab_finalize] favor %s@%c%c over %s@%c%c",
			   src->name, src->is_static ? 't' : 'T',
			   src->is_func ? 'F' : 'f',
			   dst[-1].name, dst[-1].is_static ? 't' : 'T',
			   dst[-1].is_func ? 'F' : 'f');
		   printf (" (addr=%lx)\n", (unsigned long) src->addr));
	      dst[-1] = *src;
	    }
	  else
	    {
	      DBG (AOUTDEBUG | IDDEBUG,
		   printf ("[symtab_finalize] favor %s@%c%c over %s@%c%c",
			   dst[-1].name, dst[-1].is_static ? 't' : 'T',
			   dst[-1].is_func ? 'F' : 'f',
			   src->name, src->is_static ? 't' : 'T',
			   src->is_func ? 'F' : 'f');
		   printf (" (addr=%lx)\n", (unsigned long) src->addr));
	    }
	}
      else
	{
	  if (dst > tab->base && dst[-1].end_addr == 0)
	    {
	      dst[-1].end_addr = src->addr - 1;
	    }

	  /* retain sym only if it has a non-empty address range: */
	  if (!src->end_addr || src->addr <= src->end_addr)
	    {
	      *dst = *src;
	      dst++;
	      prev_addr = src->addr;
	    }
	}
    }
  if (tab->len > 0 && dst[-1].end_addr == 0)
    {
      dst[-1].end_addr = core_text_sect->vma + core_text_sect->_raw_size - 1;
    }

  DBG (AOUTDEBUG | IDDEBUG,
       printf ("[symtab_finalize]: removed %d duplicate entries\n",
	       tab->len - (int) (dst - tab->base)));

  tab->limit = dst;
  tab->len = tab->limit - tab->base;

  DBG (AOUTDEBUG | IDDEBUG,
       unsigned int j;

       for (j = 0; j < tab->len; ++j)
       {
       printf ("[symtab_finalize] 0x%lx-0x%lx\t%s\n",
	       (long) tab->base[j].addr, (long) tab->base[j].end_addr,
	       tab->base[j].name);
       }
  );
}


#ifdef DEBUG

Sym *
DEFUN (dbg_sym_lookup, (symtab, address), Sym_Table * symtab AND bfd_vma address)
{
  long low, mid, high;
  Sym *sym;

  fprintf (stderr, "[dbg_sym_lookup] address 0x%lx\n",
	   (unsigned long) address);

  sym = symtab->base;
  for (low = 0, high = symtab->len - 1; low != high;)
    {
      mid = (high + low) >> 1;
      fprintf (stderr, "[dbg_sym_lookup] low=0x%lx, mid=0x%lx, high=0x%lx\n",
	       low, mid, high);
      fprintf (stderr, "[dbg_sym_lookup] sym[m]=0x%lx sym[m + 1]=0x%lx\n",
	       (unsigned long) sym[mid].addr,
	       (unsigned long) sym[mid + 1].addr);
      if (sym[mid].addr <= address && sym[mid + 1].addr > address)
	{
	  return &sym[mid];
	}
      if (sym[mid].addr > address)
	{
	  high = mid;
	}
      else
	{
	  low = mid + 1;
	}
    }
  fprintf (stderr, "[dbg_sym_lookup] binary search fails???\n");
  return 0;
}

#endif	/* DEBUG */


/*
 * Look up an address in the symbol-table that is sorted by address.
 * If address does not hit any symbol, 0 is returned.
 */
Sym *
DEFUN (sym_lookup, (symtab, address), Sym_Table * symtab AND bfd_vma address)
{
  long low, high;
  long mid = -1;
  Sym *sym;
#ifdef DEBUG
  int probes = 0;
#endif /* DEBUG */

  if (!symtab->len)
    {
      return 0;
    }

  sym = symtab->base;
  for (low = 0, high = symtab->len - 1; low != high;)
    {
      DBG (LOOKUPDEBUG, ++probes);
      mid = (high + low) / 2;
      if (sym[mid].addr <= address && sym[mid + 1].addr > address)
	{
	  if (address > sym[mid].end_addr)
	    {
	      /*
	       * Address falls into gap between sym[mid] and
	       * sym[mid + 1]:
	       */
	      return 0;
	    }
	  else
	    {
	      DBG (LOOKUPDEBUG,
		   printf ("[sym_lookup] %d probes (symtab->len=%u)\n",
			   probes, symtab->len - 1));
	      return &sym[mid];
	    }
	}
      if (sym[mid].addr > address)
	{
	  high = mid;
	}
      else
	{
	  low = mid + 1;
	}
    }
  if (sym[mid + 1].addr <= address)
    {
      if (address > sym[mid + 1].end_addr)
	{
	  /* address is beyond end of sym[mid + 1]: */
	  return 0;
	}
      else
	{
	  DBG (LOOKUPDEBUG, printf ("[sym_lookup] %d (%u) probes, fall off\n",
				    probes, symtab->len - 1));
	  return &sym[mid + 1];
	}
    }
  return 0;
}
