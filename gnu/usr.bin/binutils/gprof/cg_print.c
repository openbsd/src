#include "libiberty.h"
#include "cg_arcs.h"
#include "cg_print.h"
#include "hist.h"
#include "utils.h"

/*
 * Return value of comparison functions used to sort tables:
 */
#define	LESSTHAN	-1
#define	EQUALTO		0
#define	GREATERTHAN	1

/* declarations of automatically generated functions to output blurbs: */
extern void bsd_callg_blurb PARAMS ((FILE * fp));
extern void fsf_callg_blurb PARAMS ((FILE * fp));

double print_time = 0.0;


static void
DEFUN_VOID (print_header)
{
  if (first_output)
    {
      first_output = FALSE;
    }
  else
    {
      printf ("\f\n");
    }
  if (!bsd_style_output)
    {
      if (print_descriptions)
	{
	  printf ("\t\t     Call graph (explanation follows)\n\n");
	}
      else
	{
	  printf ("\t\t\tCall graph\n\n");
	}
    }
  printf ("\ngranularity: each sample hit covers %ld byte(s)",
	  (long) hist_scale * sizeof (UNIT));
  if (print_time > 0.0)
    {
      printf (" for %.2f%% of %.2f seconds\n\n",
	      100.0 / print_time, print_time / hz);
    }
  else
    {
      printf (" no time propagated\n\n");
      /*
       * This doesn't hurt, since all the numerators will be 0.0:
       */
      print_time = 1.0;
    }
  if (bsd_style_output)
    {
      printf ("%6.6s %5.5s %7.7s %11.11s %7.7s/%-7.7s     %-8.8s\n",
	      "", "", "", "", "called", "total", "parents");
      printf ("%-6.6s %5.5s %7.7s %11.11s %7.7s+%-7.7s %-8.8s\t%5.5s\n",
	      "index", "%time", "self", "descendents",
	      "called", "self", "name", "index");
      printf ("%6.6s %5.5s %7.7s %11.11s %7.7s/%-7.7s     %-8.8s\n",
	      "", "", "", "", "called", "total", "children");
      printf ("\n");
    }
  else
    {
      printf ("index %% time    self  children    called     name\n");
    }
}


/*
 * Print a cycle header.
 */
static void
DEFUN (print_cycle, (cyc), Sym * cyc)
{
  char buf[BUFSIZ];

  sprintf (buf, "[%d]", cyc->cg.index);
  printf (bsd_style_output
	  ? "%-6.6s %5.1f %7.2f %11.2f %7d"
	  : "%-6.6s %5.1f %7.2f %7.2f %7d", buf,
	  100 * (cyc->cg.prop.self + cyc->cg.prop.child) / print_time,
	  cyc->cg.prop.self / hz, cyc->cg.prop.child / hz, cyc->ncalls);
  if (cyc->cg.self_calls != 0)
    {
      printf ("+%-7d", cyc->cg.self_calls);
    }
  else
    {
      printf (" %7.7s", "");
    }
  printf (" <cycle %d as a whole> [%d]\n", cyc->cg.cyc.num, cyc->cg.index);
}


/*
 * Compare LEFT and RIGHT membmer.  Major comparison key is
 * CG.PROP.SELF+CG.PROP.CHILD, secondary key is NCALLS+CG.SELF_CALLS.
 */
static int
DEFUN (cmp_member, (left, right), Sym * left AND Sym * right)
{
  double left_time = left->cg.prop.self + left->cg.prop.child;
  double right_time = right->cg.prop.self + right->cg.prop.child;
  long left_calls = left->ncalls + left->cg.self_calls;
  long right_calls = right->ncalls + right->cg.self_calls;

  if (left_time > right_time)
    {
      return GREATERTHAN;
    }
  if (left_time < right_time)
    {
      return LESSTHAN;
    }

  if (left_calls > right_calls)
    {
      return GREATERTHAN;
    }
  if (left_calls < right_calls)
    {
      return LESSTHAN;
    }
  return EQUALTO;
}


/*
 * Sort members of a cycle.
 */
static void
DEFUN (sort_members, (cyc), Sym * cyc)
{
  Sym *todo, *doing, *prev;
  /*
   * Detach cycle members from cyclehead, and insertion sort them
   * back on.
   */
  todo = cyc->cg.cyc.next;
  cyc->cg.cyc.next = 0;
  for (doing = todo; doing && doing->cg.cyc.next; doing = todo)
    {
      todo = doing->cg.cyc.next;
      for (prev = cyc; prev->cg.cyc.next; prev = prev->cg.cyc.next)
	{
	  if (cmp_member (doing, prev->cg.cyc.next) == GREATERTHAN)
	    {
	      break;
	    }
	}
      doing->cg.cyc.next = prev->cg.cyc.next;
      prev->cg.cyc.next = doing;
    }
}


/*
 * Print the members of a cycle.
 */
static void
DEFUN (print_members, (cyc), Sym * cyc)
{
  Sym *member;

  sort_members (cyc);
  for (member = cyc->cg.cyc.next; member; member = member->cg.cyc.next)
    {
      printf (bsd_style_output
	      ? "%6.6s %5.5s %7.2f %11.2f %7d"
	      : "%6.6s %5.5s %7.2f %7.2f %7d",
	      "", "", member->cg.prop.self / hz, member->cg.prop.child / hz,
	      member->ncalls);
      if (member->cg.self_calls != 0)
	{
	  printf ("+%-7d", member->cg.self_calls);
	}
      else
	{
	  printf (" %7.7s", "");
	}
      printf ("     ");
      print_name (member);
      printf ("\n");
    }
}


/*
 * Compare two arcs to/from the same child/parent.
 *      - if one arc is a self arc, it's least.
 *      - if one arc is within a cycle, it's less than.
 *      - if both arcs are within a cycle, compare arc counts.
 *      - if neither arc is within a cycle, compare with
 *              time + child_time as major key
 *              arc count as minor key
 */
static int
DEFUN (cmp_arc, (left, right), Arc * left AND Arc * right)
{
  Sym *left_parent = left->parent;
  Sym *left_child = left->child;
  Sym *right_parent = right->parent;
  Sym *right_child = right->child;
  double left_time, right_time;

  DBG (TIMEDEBUG,
       printf ("[cmp_arc] ");
       print_name (left_parent);
       printf (" calls ");
       print_name (left_child);
       printf (" %f + %f %d/%d\n", left->time, left->child_time,
	       left->count, left_child->ncalls);
       printf ("[cmp_arc] ");
       print_name (right_parent);
       printf (" calls ");
       print_name (right_child);
       printf (" %f + %f %d/%d\n", right->time, right->child_time,
	       right->count, right_child->ncalls);
       printf ("\n");
    );
  if (left_parent == left_child)
    {
      return LESSTHAN;		/* left is a self call */
    }
  if (right_parent == right_child)
    {
      return GREATERTHAN;	/* right is a self call */
    }

  if (left_parent->cg.cyc.num != 0 && left_child->cg.cyc.num != 0
      && left_parent->cg.cyc.num == left_child->cg.cyc.num)
    {
      /* left is a call within a cycle */
      if (right_parent->cg.cyc.num != 0 && right_child->cg.cyc.num != 0
	  && right_parent->cg.cyc.num == right_child->cg.cyc.num)
	{
	  /* right is a call within the cycle, too */
	  if (left->count < right->count)
	    {
	      return LESSTHAN;
	    }
	  if (left->count > right->count)
	    {
	      return GREATERTHAN;
	    }
	  return EQUALTO;
	}
      else
	{
	  /* right isn't a call within the cycle */
	  return LESSTHAN;
	}
    }
  else
    {
      /* left isn't a call within a cycle */
      if (right_parent->cg.cyc.num != 0 && right_child->cg.cyc.num != 0
	  && right_parent->cg.cyc.num == right_child->cg.cyc.num)
	{
	  /* right is a call within a cycle */
	  return GREATERTHAN;
	}
      else
	{
	  /* neither is a call within a cycle */
	  left_time = left->time + left->child_time;
	  right_time = right->time + right->child_time;
	  if (left_time < right_time)
	    {
	      return LESSTHAN;
	    }
	  if (left_time > right_time)
	    {
	      return GREATERTHAN;
	    }
	  if (left->count < right->count)
	    {
	      return LESSTHAN;
	    }
	  if (left->count > right->count)
	    {
	      return GREATERTHAN;
	    }
	  return EQUALTO;
	}
    }
}


static void
DEFUN (sort_parents, (child), Sym * child)
{
  Arc *arc, *detached, sorted, *prev;

  /*
   * Unlink parents from child, then insertion sort back on to
   * sorted's parents.
   *      *arc        the arc you have detached and are inserting.
   *      *detached   the rest of the arcs to be sorted.
   *      sorted      arc list onto which you insertion sort.
   *      *prev       arc before the arc you are comparing.
   */
  sorted.next_parent = 0;
  for (arc = child->cg.parents; arc; arc = detached)
    {
      detached = arc->next_parent;

      /* consider *arc as disconnected; insert it into sorted: */
      for (prev = &sorted; prev->next_parent; prev = prev->next_parent)
	{
	  if (cmp_arc (arc, prev->next_parent) != GREATERTHAN)
	    {
	      break;
	    }
	}
      arc->next_parent = prev->next_parent;
      prev->next_parent = arc;
    }

  /* reattach sorted arcs to child: */
  child->cg.parents = sorted.next_parent;
}


static void
DEFUN (print_parents, (child), Sym * child)
{
  Sym *parent;
  Arc *arc;
  Sym *cycle_head;

  if (child->cg.cyc.head != 0)
    {
      cycle_head = child->cg.cyc.head;
    }
  else
    {
      cycle_head = child;
    }
  if (!child->cg.parents)
    {
      printf (bsd_style_output
	      ? "%6.6s %5.5s %7.7s %11.11s %7.7s %7.7s     <spontaneous>\n"
	      : "%6.6s %5.5s %7.7s %7.7s %7.7s %7.7s     <spontaneous>\n",
	      "", "", "", "", "", "");
      return;
    }
  sort_parents (child);
  for (arc = child->cg.parents; arc; arc = arc->next_parent)
    {
      parent = arc->parent;
      if (child == parent || (child->cg.cyc.num != 0
			      && parent->cg.cyc.num == child->cg.cyc.num))
	{
	  /* selfcall or call among siblings: */
	  printf (bsd_style_output
		  ? "%6.6s %5.5s %7.7s %11.11s %7d %7.7s     "
		  : "%6.6s %5.5s %7.7s %7.7s %7d %7.7s     ",
		  "", "", "", "",
		  arc->count, "");
	  print_name (parent);
	  printf ("\n");
	}
      else
	{
	  /* regular parent of child: */
	  printf (bsd_style_output
		  ? "%6.6s %5.5s %7.2f %11.2f %7d/%-7d     "
		  : "%6.6s %5.5s %7.2f %7.2f %7d/%-7d     ",
		  "", "",
		  arc->time / hz, arc->child_time / hz,
		  arc->count, cycle_head->ncalls);
	  print_name (parent);
	  printf ("\n");
	}
    }
}


static void
DEFUN (sort_children, (parent), Sym * parent)
{
  Arc *arc, *detached, sorted, *prev;
  /*
   * Unlink children from parent, then insertion sort back on to
   * sorted's children.
   *      *arc        the arc you have detached and are inserting.
   *      *detached   the rest of the arcs to be sorted.
   *      sorted      arc list onto which you insertion sort.
   *      *prev       arc before the arc you are comparing.
   */
  sorted.next_child = 0;
  for (arc = parent->cg.children; arc; arc = detached)
    {
      detached = arc->next_child;

      /* consider *arc as disconnected; insert it into sorted: */
      for (prev = &sorted; prev->next_child; prev = prev->next_child)
	{
	  if (cmp_arc (arc, prev->next_child) != LESSTHAN)
	    {
	      break;
	    }
	}
      arc->next_child = prev->next_child;
      prev->next_child = arc;
    }

  /* reattach sorted children to parent: */
  parent->cg.children = sorted.next_child;
}


static void
DEFUN (print_children, (parent), Sym * parent)
{
  Sym *child;
  Arc *arc;

  sort_children (parent);
  arc = parent->cg.children;
  for (arc = parent->cg.children; arc; arc = arc->next_child)
    {
      child = arc->child;
      if (child == parent || (child->cg.cyc.num != 0
			      && child->cg.cyc.num == parent->cg.cyc.num))
	{
	  /* self call or call to sibling: */
	  printf (bsd_style_output
		  ? "%6.6s %5.5s %7.7s %11.11s %7d %7.7s     "
		  : "%6.6s %5.5s %7.7s %7.7s %7d %7.7s     ",
		  "", "", "", "", arc->count, "");
	  print_name (child);
	  printf ("\n");
	}
      else
	{
	  /* regular child of parent: */
	  printf (bsd_style_output
		  ? "%6.6s %5.5s %7.2f %11.2f %7d/%-7d     "
		  : "%6.6s %5.5s %7.2f %7.2f %7d/%-7d     ",
		  "", "",
		  arc->time / hz, arc->child_time / hz,
		  arc->count, child->cg.cyc.head->ncalls);
	  print_name (child);
	  printf ("\n");
	}
    }
}


static void
DEFUN (print_line, (np), Sym * np)
{
  char buf[BUFSIZ];

  sprintf (buf, "[%d]", np->cg.index);
  printf (bsd_style_output
	  ? "%-6.6s %5.1f %7.2f %11.2f"
	  : "%-6.6s %5.1f %7.2f %7.2f", buf,
	  100 * (np->cg.prop.self + np->cg.prop.child) / print_time,
	  np->cg.prop.self / hz, np->cg.prop.child / hz);
  if ((np->ncalls + np->cg.self_calls) != 0)
    {
      printf (" %7d", np->ncalls);
      if (np->cg.self_calls != 0)
	{
	  printf ("+%-7d ", np->cg.self_calls);
	}
      else
	{
	  printf (" %7.7s ", "");
	}
    }
  else
    {
      printf (" %7.7s %7.7s ", "", "");
    }
  print_name (np);
  printf ("\n");
}


/*
 * Print dynamic call graph.
 */
void
DEFUN (cg_print, (timesortsym), Sym ** timesortsym)
{
  int index;
  Sym *parent;

  if (print_descriptions && bsd_style_output)
    {
      bsd_callg_blurb (stdout);
    }

  print_header ();

  for (index = 0; index < symtab.len + num_cycles; ++index)
    {
      parent = timesortsym[index];
      if ((ignore_zeros && parent->ncalls == 0
	   && parent->cg.self_calls == 0 && parent->cg.prop.self == 0
	   && parent->cg.prop.child == 0)
	  || !parent->cg.print_flag)
	{
	  continue;
	}
      if (!parent->name && parent->cg.cyc.num != 0)
	{
	  /* cycle header: */
	  print_cycle (parent);
	  print_members (parent);
	}
      else
	{
	  print_parents (parent);
	  print_line (parent);
	  print_children (parent);
	}
      if (bsd_style_output)
	printf ("\n");
      printf ("-----------------------------------------------\n");
      if (bsd_style_output)
	printf ("\n");
    }
  free (timesortsym);
  if (print_descriptions && !bsd_style_output)
    {
      fsf_callg_blurb (stdout);
    }
}


static int
DEFUN (cmp_name, (left, right), const PTR left AND const PTR right)
{
  const Sym **npp1 = (const Sym **) left;
  const Sym **npp2 = (const Sym **) right;

  return strcmp ((*npp1)->name, (*npp2)->name);
}


void
DEFUN_VOID (cg_print_index)
{
  int index, nnames, todo, i, j, col, starting_col;
  Sym **name_sorted_syms, *sym;
  const char *filename;
  char buf[20];
  int column_width = (output_width - 1) / 3;	/* don't write in last col! */
  /*
   * Now, sort regular function name alphabetically to create an
   * index:
   */
  name_sorted_syms = (Sym **) xmalloc ((symtab.len + num_cycles) * sizeof (Sym *));
  for (index = 0, nnames = 0; index < symtab.len; index++)
    {
      if (ignore_zeros && symtab.base[index].ncalls == 0
	  && symtab.base[index].hist.time == 0)
	{
	  continue;
	}
      name_sorted_syms[nnames++] = &symtab.base[index];
    }
  qsort (name_sorted_syms, nnames, sizeof (Sym *), cmp_name);
  for (index = 1, todo = nnames; index <= num_cycles; index++)
    {
      name_sorted_syms[todo++] = &cycle_header[index];
    }
  printf ("\f\nIndex by function name\n\n");
  index = (todo + 2) / 3;
  for (i = 0; i < index; i++)
    {
      col = 0;
      starting_col = 0;
      for (j = i; j < todo; j += index)
	{
	  sym = name_sorted_syms[j];
	  if (sym->cg.print_flag)
	    {
	      sprintf (buf, "[%d]", sym->cg.index);
	    }
	  else
	    {
	      sprintf (buf, "(%d)", sym->cg.index);
	    }
	  if (j < nnames)
	    {
	      if (bsd_style_output)
		{
		  printf ("%6.6s %-19.19s", buf, sym->name);
		}
	      else
		{
		  col += strlen (buf);
		  for (; col < starting_col + 5; ++col)
		    {
		      putchar (' ');
		    }
		  printf (" %s ", buf);
		  col += print_name_only (sym);
		  if (!line_granularity && sym->is_static && sym->file)
		    {
		      filename = sym->file->name;
		      if (!print_path)
			{
			  filename = strrchr (filename, '/');
			  if (filename)
			    {
			      ++filename;
			    }
			  else
			    {
			      filename = sym->file->name;
			    }
			}
		      printf (" (%s)", filename);
		      col += strlen (filename) + 3;
		    }
		}
	    }
	  else
	    {
	      if (bsd_style_output)
		{
		  printf ("%6.6s ", buf);
		  sprintf (buf, "<cycle %d>", sym->cg.cyc.num);
		  printf ("%-19.19s", buf);
		}
	      else
		{
		  col += strlen (buf);
		  for (; col < starting_col + 5; ++col)
		    putchar (' ');
		  printf (" %s ", buf);
		  sprintf (buf, "<cycle %d>", sym->cg.cyc.num);
		  printf ("%s", buf);
		  col += strlen (buf);
		}
	    }
	  starting_col += column_width;
	}
      printf ("\n");
    }
  free (name_sorted_syms);
}
