#ifndef LYLOCAL_H
#define LYLOCAL_H

#ifdef DIRED_SUPPORT

#ifdef VMS
#include <types.h>
#include <stat.h>
#endif /* VMS */

#ifndef S_IRWXU 
#define S_IRWXU         0000700 /* rwx, owner */
#define         S_IRUSR 0000400 /* read permission, owner */
#define         S_IWUSR 0000200 /* write permission, owner */
#define         S_IXUSR 0000100 /* execute/search permission, owner */
#define S_IRWXG         0000070 /* rwx, group */
#define         S_IRGRP 0000040 /* read permission, group */
#define         S_IWGRP 0000020 /* write permission, grougroup */
#define         S_IXGRP 0000010 /* execute/search permission, group */
#define S_IRWXO         0000007 /* rwx, other */
#define         S_IROTH 0000004 /* read permission, other */
#define         S_IWOTH 0000002 /* write permission, other */
#define         S_IXOTH 0000001 /* execute/search permission, other */
#endif /* !S_IRWXU  */

#ifndef S_ISUID                 /* Unusual modes */
#define S_ISUID         0x800   /* set user id on execution */
#define S_ISGID         0x400   /* set group id on execution */
#define S_ISVTX         0x200   /* save swapped text even after use */
#endif /* !S_ISUID */

/* Special return code for LYMainLoop.c */
#define PERMIT_FORM_RESULT (-99)

extern char LYPermitFileURL[];
extern char LYDiredFileURL[];
extern char LYUploadFileURL[];

extern BOOLEAN local_create PARAMS((document *doc));
extern BOOLEAN local_modify PARAMS((document *doc, char **newpath));
extern BOOLEAN local_remove PARAMS((document *doc));
extern BOOLEAN local_install PARAMS((char *destpath, char *srcpath, char **newpath));

/* MainLoop needs to know about this one for atexit cleanup */
extern void clear_tags NOPARAMS;

/* Define the PRIVATE routines in case they ever go PUBLIC

extern BOOLEAN modify_name PARAMS((char *testpath));
extern BOOLEAN modify_location PARAMS((char *testpath));
extern BOOLEAN create_file PARAMS((char *testpath));
extern BOOLEAN create_directory PARAMS((char *testpath));
extern BOOLEAN modify_tagged PARAMS((char *testpath));
extern BOOLEAN remove_tagged NOPARAMS;
extern BOOLEAN remove_single PARAMS ((char *testpath));
extern BOOLEAN is_a_file PARAMS((char *testname));
*/
extern void tagflag PARAMS((int flag, int cur)); 
extern void showtags PARAMS((HTList *tag));
extern int local_dired PARAMS((document *doc));
extern int dired_options PARAMS ((document *doc, char ** newfile));

extern void add_menu_item PARAMS((char *str));

#endif /* DIRED_SUPPORT */

#endif /* LYLOCAL_H */
