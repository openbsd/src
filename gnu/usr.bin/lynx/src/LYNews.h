/* $LynxId: LYNews.h,v 1.10 2010/09/25 11:35:12 tom Exp $ */
#ifndef LYNEWSPOST_H
#define LYNEWSPOST_H

#ifndef LYSTRUCTS_H
#include <LYStructs.h>
#endif /* LYSTRUCTS_H */

#ifdef __cplusplus
extern "C" {
#endif
    extern BOOLEAN term_message;

    extern char *LYNewsPost(char *newsgroups, int followup);

#ifdef __cplusplus
}
#endif
#endif				/* LYNEWSPOST_H */
