///////////////////////////////////////////////////////////////////////////// 
//
// Menu
//

ID_SYM_FRAME_MAINFRAME MENU PRELOAD DISCARDABLE 
BEGIN
	#include "file.h"
	#include "window.h"
	#include "debug.h"
	#include "help.h"
END

ID_SYM_FRAME_CMDFRAME MENU PRELOAD DISCARDABLE 
BEGIN
	#include "file.h"
	#include "window.h"
END

ID_SYM_FRAME_LOGTYPE MENU DISCARDABLE 
BEGIN
	#include "file.h"
    POPUP "&Edit"
    BEGIN
        MENUITEM "&Undo\tCtrl+Z",               ID_EDIT_UNDO
        MENUITEM SEPARATOR
        MENUITEM "Cu&t\tCtrl+X",                ID_EDIT_CUT
        MENUITEM "&Copy\tCtrl+C",               ID_EDIT_COPY
        MENUITEM "&Paste\tCtrl+V",              ID_EDIT_PASTE
        MENUITEM "&Delete\tDel",                ID_EDIT_CLEAR
        MENUITEM SEPARATOR
        MENUITEM "&Find...",                    ID_EDIT_FIND
        MENUITEM "Find &Next\tF3",              ID_EDIT_REPEAT
        MENUITEM "&Replace...",                 ID_EDIT_REPLACE
        MENUITEM SEPARATOR
        MENUITEM "Select &All",                 ID_EDIT_SELECT_ALL
        MENUITEM "&Word Wrap",                  ID_CMD_BUTTON_WORD_WRAP
    END
	#include "window.h"

	#include "debug.h"
	#include "help.h"
END

ID_SYM_FRAME_CMDTYPE MENU DISCARDABLE 
BEGIN
	#include "file.h"
    POPUP "&Edit"
    BEGIN
        MENUITEM "&Undo\tCtrl+Z",               ID_EDIT_UNDO
        MENUITEM SEPARATOR
        MENUITEM "Cu&t\tCtrl+X",                ID_EDIT_CUT
        MENUITEM "&Copy\tCtrl+C",               ID_EDIT_COPY
        MENUITEM "&Paste\tCtrl+V",              ID_EDIT_PASTE
        MENUITEM "&Delete\tDel",                ID_EDIT_CLEAR
        MENUITEM SEPARATOR
        MENUITEM "&Find...",                    ID_EDIT_FIND
        MENUITEM "Find &Next\tF3",              ID_EDIT_REPEAT
        MENUITEM "&Replace...",                 ID_EDIT_REPLACE
        MENUITEM SEPARATOR
        MENUITEM "Select &All",                 ID_EDIT_SELECT_ALL
        MENUITEM "&Word Wrap",                  ID_CMD_BUTTON_WORD_WRAP
    END
	#include "window.h"
	#include "debug.h"
	#include "help.h"

END

ID_SYM_FRAME_REGTYPE MENU DISCARDABLE 
BEGIN
	#include "file.h"	
	#include "window.h"
    POPUP "&Base"
    BEGIN
        MENUITEM "&Decimal",                    ID_REAL_CMD_BUTTON_REG_DECIMAL
        MENUITEM "&Hex",                        ID_REAL_CMD_BUTTON_REG_HEX
        MENUITEM "&Octal",                      ID_REAL_CMD_BUTTON_REG_OCTAL
        MENUITEM "&Binary",                     ID_REAL_CMD_BUTTON_REG_BINARY
        MENUITEM "&Everything",                 ID_REAL_CMD_BUTTON_REG_EVERYTHING
    END
	#include "debug.h"
	#include "help.h"
END


ID_SYM_FRAME_SRCTYPE MENU PRELOAD DISCARDABLE 
BEGIN
	#include "srcfile.h"
	#include "srcpopup.h"
	#include "window.h"
	#include "debug.h"
	#include "help.h"
END


ID_SYM_FRAME_BPTTYPE MENU PRELOAD DISCARDABLE 
BEGIN
	#include "file.h"
	#include "window.h"

    POPUP "&Debug"
    BEGIN
        MENUITEM "&Set",                      ID_REAL_CMD_BUTTON_SET_BREAKPOINT
    END
	#include "help.h"
END

ID_SYM_FRAME_SRCBROWSER MENU PRELOAD DISCARDABLE 
BEGIN
	#include "file.h"
	#include "window.h"
	#include "debug.h"
	#include "help.h"
END

ID_SYM_FRAME_LOCALTYPE MENU PRELOAD DISCARDABLE 
BEGIN
	#include "file.h"
	#include "window.h"
	#include "debug.h"
	#include "help.h"
END

ID_SYM_FRAME_FLASHTYPE MENU PRELOAD DISCARDABLE 
BEGIN
	#include "file.h"
	#include "window.h"
	#include "debug.h"
	#include "help.h"
END

ID_SYM_FRAME_SPLITTYPE MENU PRELOAD DISCARDABLE 
BEGIN
	#include "file.h"
	#include "window.h"
	#include "debug.h"
	#include "help.h"

END

ID_SYM_FRAME_INFOTYPE MENU PRELOAD DISCARDABLE 
BEGIN
	#include "file.h"
    POPUP "&View"
    BEGIN
        MENUITEM "&Contents",                   ID_REAL_CMD_BUTTON_VIEW_CONTENTS
        MENUITEM "&Both",                       ID_REAL_CMD_BUTTON_VIEW_BOTH
        MENUITEM "&Page",                       ID_REAL_CMD_BUTTON_VIEW_PAGE
    END

	#include "window.h"
	#include "debug.h"

    POPUP "&Actions"
    BEGIN
        MENUITEM SEPARATOR
        MENUITEM "&Next Page",                  ID_REAL_CMD_BUTTON_NEXT_PAG
        MENUITEM "&Previous Page",              ID_REAL_CMD_BUTTON_PREV_PAGE
        MENUITEM "&Up",                         ID_REAL_CMD_BUTTON_UP
        MENUITEM SEPARATOR
        MENUITEM "N&ext Node",                  ID_REAL_CMD_BUTTON_ACTIONS_NEXT
        MENUITEM "P&revious Node",              ID_REAL_CMD_BUTTON_PREV
        MENUITEM SEPARATOR
        MENUITEM "&Back",                       ID_REAL_CMD_BUTTON_BACKWARD
        MENUITEM "&Forward",                    ID_REAL_CMD_BUTTON_ACTIONS_FORWARD
     END
	#include "help.h"
END




ID_SYM_FRAME_EXPTYPE MENU DISCARDABLE 
BEGIN
	#include "file.h"
    POPUP "&Edit"
    BEGIN
        MENUITEM "&Copy\tCtrl+C",               ID_EDIT_COPY
        MENUITEM "&Paste\tCtrl+V",              ID_EDIT_PASTE
        MENUITEM "&Delete\tDel",                ID_EDIT_CLEAR
        MENUITEM "&Add\tCtrl+A",              ID_REAL_CMD_BUTTON_EDIT_ADD
    END
	#include "window.h"
	#include "debug.h"
	#include "help.h"
END


ID_SYM_FRAME_SRCPOPUP MENU DISCARDABLE
BEGIN
#include "srcpopup.h"
END


ID_SYM_FRAME_INSPECTPOPUP MENU DISCARDABLE
BEGIN
#include "insppop.h"
END

ID_SYM_FRAME_TABTYPE MENU DISCARDABLE 
BEGIN
	#include "file.h"
    POPUP "&Edit"
    BEGIN
        MENUITEM "&Delete\tDel",                ID_EDIT_CLEAR
        MENUITEM "&Add\tCtrl+A",              ID_REAL_CMD_BUTTON_EDIT_ADD
    END
	#include "window.h"
	#include "debug.h"
	#include "help.h"
END

