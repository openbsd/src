#ifndef LYSHOWINFO_H
#define LYSHOWINFO_H

#ifndef LYSTRUCTS_H
#include <LYStructs.h>
#endif /* LYSTRUCTS_H */

extern BOOL LYVersionIsRelease NOPARAMS;
extern char *LYVersionStatus NOPARAMS;
extern char *LYVersionDate NOPARAMS;
extern int LYShowInfo PARAMS((DocInfo *doc, int size_of_file, DocInfo *newdoc,
							char *owner_address));

#endif /* LYSHOWINFO_H */
