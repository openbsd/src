
#ifndef LYBOOKMARK_H
#define LYBOOKMARK_H

#ifndef LYSTRUCTS_H
#include "LYStructs.h"
#endif /* LYSTRUCTS_H */

extern char * get_bookmark_filename PARAMS((char **name));
extern void save_bookmark_link PARAMS((char *address, char *title));
extern void remove_bookmark_link PARAMS((int cur, char *cur_bookmark_page));
extern int select_multi_bookmarks NOPARAMS;
extern int select_menu_multi_bookmarks NOPARAMS;
extern BOOLEAN LYHaveSubBookmarks NOPARAMS;
extern void LYMBM_statusline PARAMS((char *text));

#endif /* LYBOOKMARK_H */

