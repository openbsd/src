#ifndef cg_print_h
#define cg_print_h

#include "gprof.h"
#include "symtab.h"

extern double print_time;	/* total of time being printed */

extern void cg_print PARAMS ((Sym ** cg));
extern void cg_print_index PARAMS ((void));

#endif /* cg_print_h */
