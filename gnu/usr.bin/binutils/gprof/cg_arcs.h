#ifndef cg_arcs_h
#define cg_arcs_h

#include "gprof.h"
#include "symtab.h"

/*
 * Arc structure for call-graph.
 *
 * With pointers to the symbols of the parent and the child, a count
 * of how many times this arc was traversed, and pointers to the next
 * parent of this child and the next child of this parent.
 */
typedef struct arc
  {
    Sym *parent;		/* source vertice of arc */
    Sym *child;			/* dest vertice of arc */
    int count;			/* # of calls from parent to child */
    double time;		/* time inherited along arc */
    double child_time;		/* child-time inherited along arc */
    struct arc *next_parent;	/* next parent of CHILD */
    struct arc *next_child;	/* next child of PARENT */
  }
Arc;

extern int num_cycles;		/* number of cycles discovered */
extern Sym *cycle_header;	/* cycle headers */

extern void arc_add PARAMS ((Sym * parent, Sym * child, int count));
extern Arc *arc_lookup PARAMS ((Sym * parent, Sym * child));
extern Sym **cg_assemble PARAMS ((void));

#endif /* cg_arcs_h */
