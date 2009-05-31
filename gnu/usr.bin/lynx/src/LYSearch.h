#ifndef LYSEARCH_H
#define LYSEARCH_H

#ifndef LYSTRUCTS_H
#include <LYStructs.h>
#endif /* LYSTRUCT_H */

#ifdef __cplusplus
extern "C" {
#endif
    extern BOOL textsearch(DocInfo *cur_doc,
			   char *prev_target,
			   int target_size,
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
