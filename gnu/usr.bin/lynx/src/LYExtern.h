/* $LynxId: LYExtern.h,v 1.14 2010/09/24 09:39:20 tom Exp $ */
#ifndef EXTERNALS_H
#define EXTERNALS_H

#ifndef LYSTRUCTS_H
#include <LYStructs.h>
#endif /* LYSTRUCTS_H */

#ifdef __cplusplus
extern "C" {
#endif
    extern BOOL run_external(char *c, int only_overriders);
#ifdef __cplusplus
}
#endif
#endif				/* EXTERNALS_H */
