#ifndef LYEDIT_H
#define LYEDIT_H

#ifndef HTUTILS_H
#include <HTUtils.h>
#endif

extern BOOLEAN editor_can_position NOPARAMS;
extern int edit_current_file PARAMS((char *newfile, int cur, int lineno));
extern void edit_temporary_file PARAMS((char * filename, char * position, char * message));

#endif /* LYEDIT_H */
