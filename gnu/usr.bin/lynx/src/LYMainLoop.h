#ifndef LYMAINLOOP_H
#define LYMAINLOOP_H

#ifndef HTUTILS_H
#include <HTUtils.h>
#endif

#ifdef DISP_PARTIAL
extern BOOL LYMainLoop_pageDisplay PARAMS((int line_num));
#endif
extern BOOLEAN LYOpenTraceLog NOPARAMS;
extern char* LYDownLoadAddress NOPARAMS;
extern int LYGetNewline NOPARAMS;
extern int mainloop NOPARAMS;
extern void HTAddGotoURL PARAMS((char *url));
extern void LYCloseTracelog NOPARAMS;
extern void LYSetNewline PARAMS((int value));
extern void handle_LYK_TRACE_TOGGLE NOPARAMS;
extern void handle_LYK_WHEREIS PARAMS((int cmd, BOOLEAN *refresh_screen));
extern void repaint_main_statusline PARAMS((int for_what));

#ifdef SUPPORT_CHDIR
extern void handle_LYK_CHDIR NOPARAMS; 
#endif

#endif /* LYMAINLOOP_H */
