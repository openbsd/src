/*                       ASSOCIATION LIST FOR STORING NAME-VALUE PAIRS

   Lookups from association list are not case-sensitive.

 */

#ifndef HTASSOC_H
#define HTASSOC_H

#include <HTList.h>

typedef HTList HTAssocList;

typedef struct {
    char * name;
    char * value;
} HTAssoc;


PUBLIC HTAssocList *HTAssocList_new NOPARAMS;
PUBLIC void HTAssocList_delete PARAMS((HTAssocList * alist));

PUBLIC void HTAssocList_add PARAMS((HTAssocList *       alist,
                                    CONST char *        name,
                                    CONST char *        value));

PUBLIC char *HTAssocList_lookup PARAMS((HTAssocList *   alist,
                                        CONST char *    name));

#endif /* not HTASSOC_H */
/*

   End of file HTAssoc.h.  */
