#ifndef LYGETFILE_H
#define LYGETFILE_H

#include <LYStructs.h>

#define NOT_FOUND 0
#define NORMAL 1
#define NULLFILE 3

extern BOOLEAN getfile PARAMS((document *doc));
extern int follow_link_number PARAMS((
	int		c,
	int		cur,
	document *	doc,
	int *		num));
extern void add_trusted PARAMS((char *str, int type));
extern BOOLEAN exec_ok PARAMS((CONST char *source, CONST char *linkpath, int type));

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

#endif /* LYGETFILE_H */
