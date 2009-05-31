#ifndef LYLIST_H
#define LYLIST_H

#include <LYStructs.h>

#ifdef __cplusplus
extern "C" {
#endif
    extern int showlist(DocInfo *newdoc, BOOLEAN titles);
    extern void printlist(FILE *fp, BOOLEAN titles);

#ifdef __cplusplus
}
#endif
#endif				/* LYLIST_H */
