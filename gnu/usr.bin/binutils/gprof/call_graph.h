#ifndef call_graph_h
#define call_graph_h

#include <stdio.h>
#include "gprof.h"
#include "symtab.h"

extern void cg_tally PARAMS ((bfd_vma from_pc, bfd_vma self_pc, int count));
extern void cg_read_rec PARAMS ((FILE * ifp, const char *filename));
extern void cg_write_arcs PARAMS ((FILE * ofp, const char *filename));

#endif /* call_graph_h */
