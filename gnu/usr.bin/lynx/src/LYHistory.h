
#ifndef LYHISTORY_H
#define LYHISTORY_H

#ifndef LYSTRUCTS_H
#include "LYStructs.h"
#endif /* LYSTRUCTS_H */

extern void LYAddVisitedLink PARAMS((document *doc));
extern void LYpush PARAMS((document *doc, BOOLEAN force_push));
extern void LYpop PARAMS((document *doc));
extern void LYpop_num PARAMS((int number, document *doc));
extern int showhistory PARAMS((char **newfile));
extern BOOLEAN historytarget PARAMS((document *newdoc));
extern int LYShowVisitedLinks PARAMS((char **newfile));

#define HISTORY_PAGE_TITLE  "Lynx History Page"
#define VISITED_LINKS_TITLE  "Lynx Visited Links Page"

#endif /* LYHISTORY_H */
