#ifndef LYNEWSPOST_H
#define LYNEWSPOST_H

#ifndef LYSTRUCTS_H
#include <LYStructs.h>
#endif /* LYSTRUCTS_H */

#ifdef __cplusplus
extern "C" {
#endif
    extern BOOLEAN term_message;

    extern char *LYNewsPost(char *newsgroups, BOOLEAN followup);

#ifdef __cplusplus
}
#endif
#endif				/* LYNEWSPOST_H */
