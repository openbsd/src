#ifndef LYMAINLOOP_H
#define LYMAINLOOP_H

#ifndef HTUTILS_H
#include <HTUtils.h>
#endif

extern BOOLEAN LYOpenTraceLog NOPARAMS;
extern int LYGetNewline NOPARAMS;
extern int mainloop NOPARAMS;
extern void HTAddGotoURL PARAMS((char *url));
extern void LYCloseTracelog NOPARAMS;
extern void LYMainLoop_pageDisplay PARAMS((int line_num));
extern void LYSetNewline PARAMS((int value));
extern void handle_LYK_TRACE_TOGGLE NOPARAMS;
extern void handle_LYK_WHEREIS PARAMS((int cmd, BOOLEAN *refresh_screen));
#ifdef SUPPORT_CHDIR
extern void handle_LYK_CHDIR NOPARAMS; 
#endif
extern void repaint_main_statusline PARAMS((int for_what));

#endif /* LYMAINLOOP_H */
