
#ifndef LYBOOKMARK_H
#define LYBOOKMARK_H

#ifndef LYSTRUCTS_H
#include <LYStructs.h>
#endif /* LYSTRUCTS_H */

extern BOOLEAN LYHaveSubBookmarks NOPARAMS;
extern char * get_bookmark_filename PARAMS((char **name));
extern int LYMBM2index PARAMS((int ch));
extern int LYindex2MBM PARAMS((int n));
extern int select_menu_multi_bookmarks NOPARAMS;
extern int select_multi_bookmarks NOPARAMS;
extern void LYMBM_statusline PARAMS((char *text));
extern void remove_bookmark_link PARAMS((int cur, char *cur_bookmark_page));
extern void save_bookmark_link PARAMS((char *address, char *title));
extern void set_default_bookmark_page PARAMS((char * value));

#endif /* LYBOOKMARK_H */

