#ifndef LYLOCAL_H
#define LYLOCAL_H

#ifdef DIRED_SUPPORT

#include <HTUtils.h>

/* Special return code for LYMainLoop.c */
#define PERMIT_FORM_RESULT (-99)

extern int local_create PARAMS((document *doc));
extern int local_modify PARAMS((document *doc, char **newpath));
extern int local_remove PARAMS((document *doc));
#ifdef OK_INSTALL
extern BOOLEAN local_install PARAMS((char *destpath, char *srcpath, char **newpath));
#endif

/* MainLoop needs to know about this one for atexit cleanup */
extern void clear_tags NOPARAMS;

extern int dired_options PARAMS ((document *doc, char ** newfile));
extern int local_dired PARAMS((document *doc));
extern void add_menu_item PARAMS((char *str));
extern void reset_dired_menu NOPARAMS;
extern void showtags PARAMS((HTList *tag));
extern void tagflag PARAMS((int flag, int cur)); 

#endif /* DIRED_SUPPORT */

#endif /* LYLOCAL_H */
