#ifndef symtab_h
#define symtab_h

#include "bfd.h"
#include "gprof.h"

/*
 * For a profile to be intelligible to a human user, it is necessary
 * to map code-addresses into source-code information.  Source-code
 * information can be any combination of: (i) function-name, (ii)
 * source file-name, and (iii) source line number.
 *
 * The symbol table is used to map addresses into source-code
 * information.
 */

#include "source.h"

#define NBBS 10

/*
 * Symbol-entry.  For each external in the specified file we gather
 * its address, the number of calls and compute its share of cpu time.
 */
typedef struct sym
  {
    /*
     * Common information:
     *
     * In the symbol-table, fields ADDR and FUNC_NAME are guaranteed
     * to contain valid information.  FILE may be 0, if unknown and
     * LINE_NUM maybe 0 if unknown.
     */
    bfd_vma addr;		/* address of entry point */
    bfd_vma end_addr;		/* end-address */
    const char *name;		/* name of function this sym is from */
    Source_File *file;		/* source file symbol comes from */
    int line_num;		/* source line number */
    unsigned int is_func:1,	/* is this a function entry point? */
      is_static:1,		/* is this a local (static) symbol? */
      is_bb_head:1,		/* is this the head of a basic-blk? */
      mapped:1,			/* this symbol was mapped to another name */
      has_been_placed:1;	/* have we placed this symbol?  */
    unsigned long ncalls;	/* how many times executed */
    int nuses;			/* how many times this symbol appears in
				   a particular context */
    bfd_vma bb_addr[NBBS];	/* address of basic-block start */
    unsigned long bb_calls[NBBS]; /* how many times basic-block was called */
    struct sym *next;		/* for building chains of syms */
    struct sym *prev;		/* for building chains of syms */

    /* profile-specific information: */

    /* histogram specific info: */
    struct
      {
	double time;		/* (weighted) ticks in this routine */
	bfd_vma scaled_addr;	/* scaled entry point */
      }
    hist;

    /* call-graph specific info: */
    struct
      {
	unsigned long self_calls; /* how many calls to self */
	double child_time;	/* cumulative ticks in children */
	int index;		/* index in the graph list */
	int top_order;		/* graph call chain top-sort order */
	bool print_flag;	/* should this be printed? */
	struct
	  {
	    double fract;	/* what % of time propagates */
	    double self;	/* how much self time propagates */
	    double child;	/* how much child time propagates */
	  }
	prop;
	struct
	  {
	    int num;		/* internal number of cycle on */
	    struct sym *head;	/* head of cycle */
	    struct sym *next;	/* next member of cycle */
	  }
	cyc;
	struct arc *parents;	/* list of caller arcs */
	struct arc *children;	/* list of callee arcs */
      }
    cg;
  }
Sym;

/*
 * Symbol-tables are always assumed to be sorted in increasing order
 * of addresses:
 */
typedef struct
  {
    unsigned int len;		/* # of symbols in this table */
    Sym *base;			/* first element in symbol table */
    Sym *limit;			/* limit = base + len */
  }
Sym_Table;

extern Sym_Table symtab;	/* the symbol table */

extern void sym_init PARAMS ((Sym * sym));
extern void symtab_finalize PARAMS ((Sym_Table * symtab));
extern Sym *sym_lookup PARAMS ((Sym_Table * symtab, bfd_vma address));

extern void find_call PARAMS ((Sym *, bfd_vma, bfd_vma));

#endif /* symtab_h */
