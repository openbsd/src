#ifndef LYHISTORY_H
#define LYHISTORY_H

#ifndef LYSTRUCTS_H
#include <LYStructs.h>
#endif /* LYSTRUCTS_H */

extern BOOLEAN LYwouldPush PARAMS((CONST char *title, CONST char *docurl));
extern BOOLEAN historytarget PARAMS((document *newdoc));
extern int LYShowVisitedLinks PARAMS((char **newfile));
extern int LYhist_next PARAMS((document *doc, document *newdoc));
extern int LYpush PARAMS((document *doc, BOOLEAN force_push));
extern int showhistory PARAMS((char **newfile));
extern void LYAddVisitedLink PARAMS((document *doc));
extern void LYhist_prev PARAMS((document *doc));
extern void LYhist_prev_register PARAMS((document *doc));
extern void LYpop PARAMS((document *doc));
extern void LYpop_num PARAMS((int number, document *doc));
extern void LYstatusline_messages_on_exit PARAMS((char **buf));
extern void LYstore_message PARAMS((CONST char *message));
extern void LYstore_message2 PARAMS((CONST char *message, CONST char *argument));

extern int nhist_extra;

#endif /* LYHISTORY_H */
