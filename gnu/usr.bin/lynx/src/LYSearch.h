/*
 * $LynxId: LYSearch.h,v 1.12 2013/10/03 11:24:06 tom Exp $
 */
#ifndef LYSEARCH_H
#define LYSEARCH_H

#ifndef HTFORMS_H
#include <HTForms.h>
#endif

#ifndef LYSTRUCTS_H
#include <LYStructs.h>
#endif /* LYSTRUCT_H */

#ifdef __cplusplus
extern "C" {
#endif
    extern BOOL field_has_target(FormInfo * field, const char *target);
    extern BOOL textsearch(DocInfo *cur_doc,
			   bstring **prev_target,
			   int direction);

#define IN_FILE 1
#define IN_LINKS 2

#ifndef NOT_FOUND
#define NOT_FOUND 0
#endif				/* NOT_FOUND */

#ifdef __cplusplus
}
#endif
#endif				/* LYSEARCH_H */
