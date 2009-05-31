#ifndef LYBOOKMARK_H
#define LYBOOKMARK_H

#ifndef LYSTRUCTS_H
#include <LYStructs.h>
#endif /* LYSTRUCTS_H */

#ifdef __cplusplus
extern "C" {
#endif
    extern BOOLEAN LYHaveSubBookmarks(void);
    extern const char *get_bookmark_filename(char **name);
    extern int LYMBM2index(int ch);
    extern int LYindex2MBM(int n);
    extern int select_menu_multi_bookmarks(void);
    extern int select_multi_bookmarks(void);
    extern void LYMBM_statusline(const char *text);
    extern void remove_bookmark_link(int cur, char *cur_bookmark_page);
    extern void save_bookmark_link(const char *address, const char *title);
    extern void set_default_bookmark_page(char *value);

#ifdef __cplusplus
}
#endif
#endif				/* LYBOOKMARK_H */
