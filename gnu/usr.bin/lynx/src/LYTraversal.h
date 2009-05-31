/*	traversal.c function declarations
*/
#ifndef TRAVERSAL_H
#define TRAVERSAL_H

#ifndef HTUTILS_H
#include <HTUtils.h>		/* BOOL, ARGS */
#endif

#ifdef __cplusplus
extern "C" {
#endif
    extern BOOLEAN lookup_link(char *target);
    extern void add_to_table(char *target);
    extern void add_to_traverse_list(char *fname, char *prev_link_name);
    extern void dump_traversal_history(void);
    extern void add_to_reject_list(char *target);
    extern BOOLEAN lookup_reject(char *target);

#ifdef __cplusplus
}
#endif
#endif				/* TRAVERSAL_H */
