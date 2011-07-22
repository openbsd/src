/* $LynxId: LYExtern.h,v 1.13 2008/12/29 18:59:39 tom Exp $ */
#ifndef EXTERNALS_H
#define EXTERNALS_H

#ifndef LYSTRUCTS_H
#include <LYStructs.h>
#endif /* LYSTRUCTS_H */

#ifdef __cplusplus
extern "C" {
#endif
    extern BOOL run_external(char *c, BOOL only_overriders);
#ifdef __cplusplus
}
#endif
#endif				/* EXTERNALS_H */
