#ifndef LYMAINLOOP_H
#define LYMAINLOOP_H

#ifndef HTUTILS_H
#include <HTUtils.h>
#endif

#define TEXTAREA_EXPAND_SIZE  5
#define AUTOGROW
#define AUTOEXTEDIT

extern BOOLEAN LYOpenTraceLog NOPARAMS;
extern void LYCloseTracelog NOPARAMS;
extern int mainloop NOPARAMS;

#endif /* LYMAINLOOP_H */
