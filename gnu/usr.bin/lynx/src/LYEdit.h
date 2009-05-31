#ifndef LYEDIT_H
#define LYEDIT_H

#ifndef HTUTILS_H
#include <HTUtils.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif
    extern BOOLEAN editor_can_position(void);
    extern int edit_current_file(char *newfile, int cur, int lineno);
    extern void edit_temporary_file(char *filename, const char *position, const char *message);

#ifdef __cplusplus
}
#endif
#endif				/* LYEDIT_H */
