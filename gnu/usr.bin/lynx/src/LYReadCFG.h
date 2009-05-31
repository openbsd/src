#ifndef LYREADCFG_H
#define LYREADCFG_H

#ifndef LYSTRUCTS_H
#include <LYStructs.h>
#endif /* LYSTRUCTS_H */

#ifdef __cplusplus
extern "C" {
#endif
#if defined(USE_COLOR_STYLE) || defined(USE_COLOR_TABLE)
#define DEFAULT_COLOR -1
#define NO_COLOR      -2
#define ERR_COLOR     -3
/* Note: the sense of colors that Lynx uses for defaults is the reverse of
 * the standard for color-curses.
 */
#ifdef USE_DEFAULT_COLORS
# ifdef USE_SLANG
#  define DEFAULT_FG "default"
#  define DEFAULT_BG "default"
# else
#  ifdef HAVE_USE_DEFAULT_COLORS
#   define DEFAULT_FG DEFAULT_COLOR
#   define DEFAULT_BG DEFAULT_COLOR
#  else
#   define DEFAULT_FG COLOR_BLACK
#   define DEFAULT_BG COLOR_WHITE
#  endif
# endif
#else
# ifdef USE_SLANG
#  define DEFAULT_FG "black"
#  define DEFAULT_BG "white"
# else
#  define DEFAULT_FG COLOR_BLACK
#  define DEFAULT_BG COLOR_WHITE
# endif
#endif				/* USE_DEFAULT_COLORS */
    extern int default_fg;
    extern int default_bg;
    extern BOOL default_color_reset;

    extern int check_color(char *color, int the_default);
    extern const char *lookup_color(int code);
#endif

    extern void read_cfg(const char *cfg_filename,
			 const char *parent_filename,
			 int nesting_level,
			 FILE *fp0);
    extern void free_lynx_cfg(void);
    extern BOOLEAN have_read_cfg;

    extern FILE *LYOpenCFG(const char *cfg_filename, const char
			   *parent_filename, const char *dft_filename);
    extern int lynx_cfg_infopage(DocInfo *newdoc);
    extern int lynx_compile_opts(DocInfo *newdoc);
    extern int match_item_by_name(lynx_list_item_type *ptr, char *name, BOOLEAN only_overriders);
    extern lynx_list_item_type *find_item_by_number(lynx_list_item_type *
						    list_ptr,
						    char *number);
    extern void reload_read_cfg(void);	/* implemented in LYMain.c */
    extern void LYSetConfigValue(char *name, char *value);

#ifdef __cplusplus
}
#endif
#endif				/* LYREADCFG_H */
