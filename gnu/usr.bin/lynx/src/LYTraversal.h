/*	traversal.c function declarations
*/
#ifndef TRAVERSAL_H
#define TRAVERSAL_H

#include "HTUtils.h"            /* BOOL, PARAMS, ARGS */

extern BOOLEAN lookup PARAMS((char * target));
extern void add_to_table PARAMS((char * target));
extern void add_to_traverse_list PARAMS((char * fname, char * prev_link_name));
extern void dump_traversal_history NOPARAMS;
extern void add_to_reject_list PARAMS((char * target));
extern BOOLEAN lookup_reject PARAMS((char * target));

#endif /* TRAVERSAL_H */
