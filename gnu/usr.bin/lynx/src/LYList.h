/* $LynxId: LYList.h,v 1.12 2010/09/25 11:35:35 tom Exp $ */
#ifndef LYLIST_H
#define LYLIST_H

#include <LYStructs.h>

#ifdef __cplusplus
extern "C" {
#endif
    extern int showlist(DocInfo *newdoc, int titles);
    extern void printlist(FILE *fp, int titles);

#ifdef __cplusplus
}
#endif
#endif				/* LYLIST_H */
