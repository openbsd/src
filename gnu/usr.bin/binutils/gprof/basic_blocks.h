#ifndef basic_blocks_h
#define basic_blocks_h

#include <stdio.h>
#include "gprof.h"
#include "source.h"
#include "symtab.h"

/*
 * Options:
 */
extern bool bb_annotate_all_lines;	/* force annotation of all lines? */
extern int bb_table_length;		/* length of most-used bb table */
extern unsigned long bb_min_calls;	/* minimum execution count */

extern void bb_read_rec PARAMS ((FILE * ifp, const char *filename));
extern void bb_write_blocks PARAMS ((FILE * ofp, const char *filename));
extern void bb_create_syms PARAMS ((void));

extern void print_annotated_source PARAMS ((void));
extern void print_exec_counts PARAMS ((void));

#endif /* basic_blocks_h */
