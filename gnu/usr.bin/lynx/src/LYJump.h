#ifndef LYJUMP_H
#define LYJUMP_H

#include <HTList.h>

typedef struct _JumpDatum {
    char *key;
    char *url;
} JumpDatum;

struct JumpTable { 
    int key;
    int nel;
    char *msg;
    char *file;
    char *shortcut;
    HTList *history;
    JumpDatum *table;
    struct JumpTable *next;
    char *mp;
};

extern struct JumpTable *JThead;
extern void LYJumpTable_free NOPARAMS;
extern void LYAddJumpShortcut PARAMS((HTList *the_history, char *shortcut));
extern BOOL LYJumpInit PARAMS((char *config));
extern char *LYJump PARAMS((int key));

#endif /* LYJUMP_H */
