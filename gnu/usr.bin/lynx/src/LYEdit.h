#ifndef LYEDIT_H
#define LYEDIT_H

#ifndef HTUTILS_H
#include <HTUtils.h>
#endif

extern int edit_current_file PARAMS((char *newfile, int cur, int lineno));

extern BOOLEAN editor_can_position NOPARAMS;

#endif /* LYEDIT_H */
