#ifndef LYREADCFG_H
#define LYREADCFG_H

#ifndef LYSTRUCTS_H
#include <LYStructs.h>
#endif /* LYSTRUCTS_H */

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
#endif /* USE_DEFAULT_COLORS */

extern int default_fg;
extern int default_bg;

extern int check_color PARAMS((char * color, int the_default));
#endif

extern void read_cfg PARAMS((char *cfg_filename, char *parent_filename, int nesting_level, FILE *fp0));
extern void free_lynx_cfg NOPARAMS;
extern BOOLEAN have_read_cfg;

extern int lynx_cfg_infopage PARAMS((document *newdoc));
extern int lynx_compile_opts PARAMS((document *newdoc));
extern void reload_read_cfg NOPARAMS; /* not implemented yet, in LYMain.c */

#endif /* LYREADCFG_H */
