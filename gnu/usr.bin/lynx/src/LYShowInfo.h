#ifndef LYSHOWINFO_H
#define LYSHOWINFO_H

#ifndef LYSTRUCTS_H
#include <LYStructs.h>
#endif /* LYSTRUCTS_H */

#ifdef __cplusplus
extern "C" {
#endif
    extern BOOL LYVersionIsRelease(void);
    extern const char *LYVersionStatus(void);
    extern const char *LYVersionDate(void);
    extern int LYShowInfo(DocInfo *doc,
			  DocInfo *newdoc,
			  char *owner_address);

#ifdef __cplusplus
}
#endif
#endif				/* LYSHOWINFO_H */
