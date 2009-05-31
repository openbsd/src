#ifndef LYGETFILE_H
#define LYGETFILE_H

#include <LYStructs.h>

#ifdef __cplusplus
extern "C" {
#endif
#define NOT_FOUND 0
#define NORMAL 1
#define NULLFILE 3
    extern int getfile(DocInfo *doc, int *target);
    extern void srcmode_for_next_retrieval(int);
    extern int follow_link_number(int c,
				  int cur,
				  DocInfo *doc,
				  int *num);
    extern void add_trusted(char *str, int type);
    extern BOOLEAN exec_ok(const char *source, const char *linkpath, int type);

    extern char *WWW_Download_File;

/* values for follow_link_number() */
#define DO_LINK_STUFF		1
#define DO_GOTOLINK_STUFF	2
#define DO_GOTOPAGE_STUFF	3
#define DO_FORMS_STUFF		4
#define PRINT_ERROR		5

/* values for add_trusted() and exec_ok() */
#define EXEC_PATH 0
#define ALWAYS_EXEC_PATH  1
#define CGI_PATH  2

#ifdef __cplusplus
}
#endif
#endif				/* LYGETFILE_H */
