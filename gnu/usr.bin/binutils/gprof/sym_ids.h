#ifndef sym_ids_h
#define sym_ids_h

#include "symtab.h"

typedef enum
  {
    INCL_GRAPH = 0, EXCL_GRAPH,
    INCL_ARCS, EXCL_ARCS,
    INCL_FLAT, EXCL_FLAT,
    INCL_TIME, EXCL_TIME,
    INCL_ANNO, EXCL_ANNO,
    INCL_EXEC, EXCL_EXEC,
    NUM_TABLES
  }
Table_Id;

extern Sym_Table syms[NUM_TABLES];

extern void sym_id_add PARAMS ((const char *spec, Table_Id which_table));
extern void sym_id_parse PARAMS ((void));
extern bool sym_id_arc_is_present PARAMS ((Sym_Table * symtab,
					   Sym * from, Sym * to));

#endif /* sym_ids_h */
