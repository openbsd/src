//Microsoft Visual C++ generated resource script.
//
#include "resource.h"

#define APSTUDIO_READONLY_SYMBOLS
/////////////////////////////////////////////////////////////////////////////
//
// Generated from the TEXTINCLUDE 2 resource.
//
#include "afxres.h"

/////////////////////////////////////////////////////////////////////////////
#undef APSTUDIO_READONLY_SYMBOLS


#ifdef APSTUDIO_INVOKED
/////////////////////////////////////////////////////////////////////////////
//
// TEXTINCLUDE
//

1 TEXTINCLUDE DISCARDABLE 
BEGIN
    "resource.h\0"
END

2 TEXTINCLUDE DISCARDABLE 
BEGIN
    "#include ""afxres.h""\r\n"
    "\0"
END

3 TEXTINCLUDE DISCARDABLE 
BEGIN
    "#include ""res\\gui.rc2""  // non-Microsoft Visual C++ edited resources\r\n"
    "#include ""menus\\menus.rc"" // non-Microsoft Visual C++ edited resources\r\n"
    "#define _AFX_NO_SPLITTER_RESOURCES\r\n"
    "#define _AFX_NO_OLE_RESOURCES\r\n"
    "#define _AFX_NO_TRACKER_RESOURCES\r\n"
    "#include ""afxres.rc""  \011// Standard components\r\n"
    "#include ""afxprint.rc""\011// printing/print preview resources\r\n"
END

/////////////////////////////////////////////////////////////////////////////
#endif    // APSTUDIO_INVOKED


/////////////////////////////////////////////////////////////////////////////
//
// Icon
//

ID_SYM_FRAME_LOGTYPE             ICON    DISCARDABLE     "res\\guidoc.ico"
ID_SYM_FRAME_BPTOKMARK           ICON    DISCARDABLE     "res\\idi_.ico"
ID_SYM_FRAME_BPTHEREMARK         ICON    DISCARDABLE     "res\\icon1.ico"
ID_SYM_FRAME_CMDFRAME            ICON    DISCARDABLE     "res\\idr_cmdf.ico"
ID_SYM_FRAME_CMDTYPE             ICON    DISCARDABLE     "res\\idr_guit.ico"
ID_SYM_FRAME_REGTYPE             ICON    DISCARDABLE     "res\\idr_main.ico"
ID_SYM_FRAME_MAINFRAME           ICON    DISCARDABLE     "res\\ico00001.ico"
ID_ICON_ICON1               ICON    DISCARDABLE     "res\\ico00002.ico"
ID_SYM_FRAME_SRCSTYPE            ICON    DISCARDABLE     "res\\ico00003.ico"
ID_ICON_PC_HERE             ICON    DISCARDABLE     "res\\icon2.ico"
ID_SYM_FRAME_SRCTYPE             ICON    DISCARDABLE     "res\\idr_srct.ico"
ID_SYM_FRAME_BPTTYPE             ICON    DISCARDABLE     "res\\ico00004.ico"
ID_SYM_FRAME_BPTHEREDISABLEDMARK ICON    DISCARDABLE     "res\\idr_bpth.ico"
ID_SYM_FRAME_ASMTYPE             ICON    DISCARDABLE     "res\\idr_asmt.ico"
ID_SYM_FRAME_SRCBROWSER          ICON    DISCARDABLE     "res\\idr_srcb.ico"
ID_SYM_FRAME_LOCALTYPE           ICON    DISCARDABLE     "res\\idr_regt.ico"
ID_SYM_FRAME_EXPTYPE             ICON    DISCARDABLE     "res\\idr_main.ico"
ID_SYM_FRAME_CMDTYPE1            ICON    DISCARDABLE     "res\\idr_cmdt.ico"
ID_SYM_FRAME_SPLITTYPE           ICON    DISCARDABLE     "res\\ico00005.ico"
ID_SYM_FRAME_INFOTYPE            ICON    DISCARDABLE     "res\\infodoc.ico"

/////////////////////////////////////////////////////////////////////////////
//
// Bitmap
//

ID_SYM_FRAME_MAINFRAME           BITMAP  MOVEABLE PURE   "res\\toolbar.bmp"
ID_SYM_BITMAP_SAC                 BITMAP  DISCARDABLE     "res\\bitmap1.bmp"
ID_SYM_FRAME_GDBTOOLBAR          BITMAP  MOVEABLE PURE   "res\\idr_main.bmp"
ID_SYM_FRAME_WINTOOLBAR          BITMAP  MOVEABLE PURE   "res\\idr_wint.bmp"
ID_SYM_FRAME_INFO_BITMAP         BITMAP  DISCARDABLE     "res\\bmp00007.bmp"
ID_SYM_BITMAP_SPLIT        BITMAP  DISCARDABLE     "res\\bmp00008.bmp"
ID_SYM_BITMAP_SPLIT1       BITMAP  DISCARDABLE     "res\\split_bi.bmp"
ID_SYM_BITMAP_ON                  BITMAP  DISCARDABLE     "res\\bmp00001.bmp"
ID_SYM_BITMAP_OFF                 BITMAP  DISCARDABLE     "res\\bmp00002.bmp"
ID_SYM_BITMAP_CUP                 BITMAP  DISCARDABLE     "res\\bmp00003.bmp"
ID_SYM_BITMAP_CDOWN               BITMAP  DISCARDABLE     "res\\cdown.bmp"
ID_SYM_BITMAP_CFOCUS              BITMAP  DISCARDABLE     "res\\cf.bmp"
ID_SYM_BITMAP_SRCWIN        BITMAP  DISCARDABLE     "res\\bmp00004.bmp"

/////////////////////////////////////////////////////////////////////////////
//
// Accelerator
//

ID_SYM_FRAME_MAINFRAME ACCELERATORS PRELOAD MOVEABLE PURE 
BEGIN
    "C",            ID_EDIT_COPY,           VIRTKEY, CONTROL, NOINVERT
    "N",            ID_FILE_NEW,            VIRTKEY, CONTROL, NOINVERT
    "O",            ID_FILE_OPEN,           VIRTKEY, CONTROL, NOINVERT
    "P",            ID_FILE_PRINT,          VIRTKEY, CONTROL, NOINVERT
    "S",            ID_FILE_SAVE,           VIRTKEY, CONTROL, NOINVERT
    "V",            ID_EDIT_PASTE,          VIRTKEY, CONTROL, NOINVERT
    VK_BACK,        ID_EDIT_UNDO,           VIRTKEY, ALT, NOINVERT
    VK_DELETE,      ID_CMD_BUTTON_TAB_REMOVE,          VIRTKEY, NOINVERT
    VK_DELETE,      ID_EDIT_CUT,            VIRTKEY, SHIFT, NOINVERT
    VK_F1,          ID_HELP,                VIRTKEY, NOINVERT
    VK_F1,          ID_REAL_CMD_BUTTON_CONTEXT_HELP,        VIRTKEY, SHIFT, NOINVERT
    VK_F10,         ID_REAL_CMD_BUTTON_N,                   VIRTKEY, NOINVERT
    VK_F5,          ID_REAL_CMD_BUTTON_CONT,                VIRTKEY, NOINVERT
    VK_F6,          ID_CMD_BUTTON_VIEW_DISASSEMBLE,    VIRTKEY, NOINVERT
    VK_F6,          ID_PREV_PANE,           VIRTKEY, SHIFT, NOINVERT
    VK_F7,          ID_REAL_CMD_BUTTON_FINISH,              VIRTKEY, NOINVERT
    VK_F8,          ID_REAL_CMD_BUTTON_S,                   VIRTKEY, NOINVERT
    VK_INSERT,      ID_EDIT_COPY,           VIRTKEY, CONTROL, NOINVERT
    VK_INSERT,      ID_EDIT_PASTE,          VIRTKEY, SHIFT, NOINVERT
    "X",            ID_EDIT_CUT,            VIRTKEY, CONTROL, NOINVERT
    "Z",            ID_EDIT_UNDO,           VIRTKEY, CONTROL, NOINVERT
END

ID_SYM_FRAME_SRCSTYPE ACCELERATORS PRELOAD MOVEABLE PURE 
BEGIN
    "C",            ID_REAL_CMD_BUTTON_CONT,                VIRTKEY, ALT, NOINVERT
    "N",            ID_REAL_CMD_BUTTON_N,                   VIRTKEY, ALT, NOINVERT
    "N",            ID_REAL_CMD_BUTTON_NI,                  VIRTKEY, SHIFT, ALT, NOINVERT
    "R",            ID_REAL_CMD_BUTTON_RUN,                 VIRTKEY, ALT, NOINVERT
    "S",            ID_REAL_CMD_BUTTON_S,                   VIRTKEY, ALT, NOINVERT
    "S",            ID_REAL_CMD_BUTTON_SI,                  VIRTKEY, SHIFT, ALT, NOINVERT
END


/////////////////////////////////////////////////////////////////////////////
//
// Dialog
//

ID_SYM_DIALOG_MYOBJEDITDLG DIALOG DISCARDABLE  0, 0, 185, 66
STYLE DS_MODALFRAME | WS_POPUP | WS_VISIBLE | WS_CAPTION | WS_SYSMENU
CAPTION "MyObj Data"
FONT 8, "MS Sans Serif"
BEGIN
    DEFPUSHBUTTON   "OK",IDOK,24,44,50,14
    PUSHBUTTON      "Cancel",IDCANCEL,112,44,50,14
    LTEXT           "Text:",IDC_STATIC,8,8,20,8
    EDITTEXT        ID_CMD_BUTTON_TEXT,8,20,172,12,ES_AUTOHSCROLL
END

ID_SYM_DIALOG_SET_TABSTOPS DIALOG DISCARDABLE  3, 4, 161, 46
STYLE DS_MODALFRAME | WS_POPUP | WS_VISIBLE | WS_CAPTION | WS_SYSMENU
CAPTION "Set Tab Stops"
FONT 8, "Helv"
BEGIN
    LTEXT           "&Tab Stops:",IDC_STATIC,6,14,37,8
    EDITTEXT        ID_CMD_BUTTON_EDIT_TABS,50,12,49,12,ES_AUTOHSCROLL
    DEFPUSHBUTTON   "OK",IDOK,111,4,40,15
    PUSHBUTTON      "Cancel",IDCANCEL,111,23,40,14
END

ID_SYM_DIALOG_COMMAND DIALOG DISCARDABLE  0, 0, 156, 46
STYLE WS_CHILD | WS_BORDER
FONT 8, "MS Sans Serif"
BEGIN
    DEFPUSHBUTTON   "OK",IDOK,30,30,10,6,NOT WS_VISIBLE | NOT WS_TABSTOP
    EDITTEXT        ID_CMD_BUTTON_COMMAND,5,5,140,20,ES_AUTOHSCROLL
END

ID_SYM_DIALOG_REGINSIDE DIALOG DISCARDABLE  0, 0, 186, 94
STYLE DS_MODALFRAME | WS_POPUP | WS_VISIBLE
FONT 8, "MS Sans Serif"
BEGIN
    DEFPUSHBUTTON   "OK",IDOK,60,5,50,14
    PUSHBUTTON      "Cancel",IDCANCEL,120,5,50,14
    CONTROL         "Check1",ID_CMD_BUTTON_CHECK1,"Button",BS_AUTOCHECKBOX | 
                    WS_TABSTOP,125,50,34,10
    CONTROL         "Check2",ID_CMD_BUTTON_CHECK2,"Button",BS_AUTOCHECKBOX | 
                    WS_TABSTOP,50,45,35,10
END

ID_SYM_DIALOG_SPLASH DIALOG DISCARDABLE  0, 0, 253, 92
STYLE DS_MODALFRAME | WS_POPUP
FONT 8, "Helv"
BEGIN
    CONTROL         "",ID_CMD_BUTTON_BIGICON,"Button",BS_OWNERDRAW | WS_DISABLED,6,6,
                    38,40
    LTEXT           "GDB for Windows",-1,60,6,164,8,NOT WS_GROUP
    LTEXT           "Cygnus Q4-94",-1,61,16,164,8,NOT WS_GROUP
    CONTROL         "",-1,"Static",SS_BLACKRECT,60,30,188,1
    LTEXT           "Warning: This computer program is protected by the Copyleft.  Reproduction or distribution of this program and source code, or any portion of it is encouraged.",
                    -1,60,40,185,45
END

ID_SYM_DIALOG_ABOUTBOX DIALOG DISCARDABLE  0, 0, 191, 114
STYLE DS_MODALFRAME | WS_POPUP | WS_CAPTION | WS_SYSMENU
CAPTION "About GDB"
FONT 8, "Helv"
BEGIN
    CONTROL         "",ID_CMD_BUTTON_BIGICON,"Button",BS_OWNERDRAW | WS_DISABLED,5,5,
                    38,40
    LTEXT           "GDB for windows",-1,60,5,65,8,NOT WS_GROUP
    LTEXT           "Cygnus support",-1,60,15,55,10,NOT WS_GROUP
    DEFPUSHBUTTON   "Steve, I'll buy you a beer.",IDOK,3,64,180,15
    CONTROL         "ID_SYM_BITMAP_SAC",ID_CMD_BUTTON_MYFACE,"Button",BS_OWNERDRAW | WS_TABSTOP,
                    135,5,45,30
END

ID_SYM_DIALOG_BREAKPOINT DIALOG DISCARDABLE  0, 0, 198, 117
STYLE DS_MODALFRAME | WS_CHILD
MENU 107
FONT 8, "MS Sans Serif"
BEGIN
    DEFPUSHBUTTON   "Set ",ID_REAL_CMD_BUTTON_SET_BREAKPOINT,150,5,40,15
    PUSHBUTTON      "&Clear",ID_CMD_BUTTON_CLEAR_BREAKPOINT,150,50,40,15,WS_DISABLED
    LISTBOX         ID_CMD_BUTTON_BREAKPOINT_LIST,5,25,135,90,LBS_USETABSTOPS | 
                    WS_VSCROLL | WS_HSCROLL | WS_TABSTOP
    EDITTEXT        ID_CMD_BUTTON_BREAKPOINT,5,5,135,15,ES_AUTOHSCROLL
    PUSHBUTTON      "&Disable",ID_CMD_BUTTON_DISABLE,150,70,40,15,WS_DISABLED
    PUSHBUTTON      "Clear &All",ID_CMD_BUTTON_CLEAR_ALL,150,90,40,15,WS_DISABLED
END

ID_SYM_DIALOG_SRCBROWSER DIALOG DISCARDABLE  0, 0, 157, 210
STYLE DS_MODALFRAME | WS_CHILD
MENU 107
FONT 8, "MS Sans Serif"
BEGIN
    LISTBOX         ID_CMD_BUTTON_BROWSE_LIST,5,25,145,150,LBS_OWNERDRAWFIXED | 
                    LBS_USETABSTOPS | WS_VSCROLL | WS_HSCROLL | WS_TABSTOP
    DEFPUSHBUTTON   "&View",ID_CMD_BUTTON_GOTO,5,180,40,15
    PUSHBUTTON      "&Break at",ID_CMD_BUTTON_BREAKPOINT,55,180,40,15
    PUSHBUTTON      "&Explode",ID_CMD_BUTTON_EXPLODE,110,180,40,15
    EDITTEXT        ID_CMD_BUTTON_FILTER,5,5,145,15,ES_AUTOHSCROLL
END

ID_SYM_DIALOG_LOCAL DIALOG DISCARDABLE  0, 0, 162, 137
STYLE WS_CHILD
MENU 133
FONT 8, "MS Sans Serif"
BEGIN
    LISTBOX         ID_CMD_BUTTON_FRAMEDATA,5,40,155,95,LBS_USETABSTOPS | 
                    LBS_NOINTEGRALHEIGHT | WS_VSCROLL | WS_TABSTOP
    LISTBOX         ID_CMD_BUTTON_FRAMENAMES,5,5,150,30,LBS_OWNERDRAWFIXED | 
                    WS_VSCROLL | WS_TABSTOP
END

ID_SYM_DIALOG_SRCWINIP DIALOG DISCARDABLE  0, 0, 131, 128
STYLE WS_CHILD | WS_VISIBLE | WS_DISABLED | WS_CAPTION
CAPTION "Source window settings"
FONT 8, "MS Sans Serif"
BEGIN
    GROUPBOX        "                           ",IDC_STATIC,5,20,85,40
    GROUPBOX        "                     ",IDC_STATIC,5,70,85,50
    CONTROL         "Line numbers",ID_CMD_BUTTON_LINENUMBERS,"Button",BS_AUTOCHECKBOX | 
                    WS_TABSTOP,15,35,55,10
    CONTROL         "Addresses",ID_CMD_BUTTON_ADDRESSES,"Button",BS_AUTOCHECKBOX | 
                    WS_TABSTOP,15,105,45,10
    CONTROL         "Breakpoint candidates",ID_CMD_BUTTON_BREAKPOINT_OK,"Button",
                    BS_AUTOCHECKBOX | WS_TABSTOP,5,5,84,10
    CONTROL         "Source",ID_REAL_CMD_BUTTON_SOURCE,"Button",BS_AUTOCHECKBOX | 
                    WS_TABSTOP,10,20,34,10
    CONTROL         "Disassembly",ID_CMD_BUTTON_DISASSEMBLY,"Button",BS_AUTOCHECKBOX | 
                    WS_TABSTOP,10,70,51,10
    CONTROL         "Instruction data",ID_REAL_CMD_BUTTON_INSTRUCTION_DATA,"Button",
                    BS_AUTOCHECKBOX | WS_TABSTOP,15,90,62,10
END

ID_SYM_DIALOG_SRCWINF DIALOG DISCARDABLE  0, 0, 106, 92
STYLE WS_CHILD | WS_VISIBLE | WS_DISABLED | WS_CAPTION
CAPTION "Fonts and Colors"
FONT 8, "MS Sans Serif"
BEGIN
    PUSHBUTTON      "Source color",ID_REAL_CMD_BUTTON_SET_SRC_COLOR,15,13,65,14
    PUSHBUTTON      "Assembler color",ID_REAL_CMD_BUTTON_SET_ASM_COLOR,16,31,65,14
    PUSHBUTTON      "Source font",ID_REAL_CMD_BUTTON_SET_FONT,15,50,65,14
    PUSHBUTTON      "Background",ID_REAL_CMD_BUTTON_SET_BCK_COLOR,15,70,65,15
END

ID_SYM_FRAME_EXPTYPE DIALOG DISCARDABLE  0, 0, 191, 134
STYLE DS_MODALFRAME | WS_POPUP | WS_VISIBLE | WS_CAPTION | WS_SYSMENU
CAPTION "Watch"
FONT 8, "MS Sans Serif"
BEGIN
    LISTBOX         ID_CMD_BUTTON_MYLIST,10,10,170,85,LBS_OWNERDRAWFIXED | WS_VSCROLL | 
                    WS_TABSTOP
    EDITTEXT        ID_CMD_BUTTON_EDIT,10,110,170,15,ES_AUTOHSCROLL
END

ID_SYM_DIALOG_DIALOG3 DIALOG DISCARDABLE  0, 0, 185, 92
STYLE DS_MODALFRAME | WS_POPUP | WS_VISIBLE | WS_CAPTION | WS_SYSMENU
CAPTION "Dialog"
FONT 8, "MS Sans Serif"
BEGIN
    DEFPUSHBUTTON   "OK",IDOK,129,6,50,14
    PUSHBUTTON      "Cancel",IDCANCEL,129,23,50,14
END

ID_SYM_DIALOG_SRCDLG DIALOG DISCARDABLE  0, 0, 121, 15
STYLE WS_CHILD | WS_VISIBLE | WS_DISABLED | WS_CAPTION
CAPTION "file"
FONT 8, "MS Sans Serif"
BEGIN
    LTEXT           "Static",IDC_STATIC,5,5,100,10
END

ID_SYM_DIALOG_COLOR_DIALOG DIALOG DISCARDABLE  0, 0, 276, 159
STYLE WS_CHILD | WS_DISABLED | WS_CAPTION
CAPTION "Select Colors"
FONT 8, "MS Sans Serif"
BEGIN
    CONTROL         "v",ID_CMD_BUTTON_C0,"Button",BS_OWNERDRAW | WS_TABSTOP,112,24,20,
                    16
    CONTROL         "v",ID_CMD_BUTTON_C1,"Button",BS_OWNERDRAW | WS_TABSTOP,136,24,20,
                    16
    CONTROL         "v",ID_CMD_BUTTON_C2,"Button",BS_OWNERDRAW | WS_TABSTOP,160,24,20,
                    16
    CONTROL         "v",ID_CMD_BUTTON_C3,"Button",BS_OWNERDRAW | WS_TABSTOP,184,24,20,
                    16
    CONTROL         "v",ID_CMD_BUTTON_C4,"Button",BS_OWNERDRAW | WS_TABSTOP,112,44,20,
                    16
    CONTROL         "v",ID_CMD_BUTTON_C5,"Button",BS_OWNERDRAW | WS_TABSTOP,136,44,20,
                    16
    CONTROL         "v",ID_CMD_BUTTON_C6,"Button",BS_OWNERDRAW | WS_TABSTOP,160,44,20,
                    16
    CONTROL         "v",ID_CMD_BUTTON_C7,"Button",BS_OWNERDRAW | WS_TABSTOP,184,44,20,
                    16
    CONTROL         "v",ID_CMD_BUTTON_C8,"Button",BS_OWNERDRAW | WS_TABSTOP,112,64,20,
                    16
    CONTROL         "v",ID_CMD_BUTTON_C9,"Button",BS_OWNERDRAW | WS_TABSTOP,136,64,20,
                    16
    CONTROL         "v",ID_CMD_BUTTON_C10,"Button",BS_OWNERDRAW | WS_TABSTOP,160,64,20,
                    16
    CONTROL         "v",ID_CMD_BUTTON_C11,"Button",BS_OWNERDRAW | WS_TABSTOP,184,64,20,
                    16
    CONTROL         "v",ID_CMD_BUTTON_C12,"Button",BS_OWNERDRAW | WS_TABSTOP,112,84,20,
                    16
    CONTROL         "v",ID_CMD_BUTTON_C13,"Button",BS_OWNERDRAW | WS_TABSTOP,136,84,20,
                    16
    CONTROL         "v",ID_CMD_BUTTON_C14,"Button",BS_OWNERDRAW | WS_TABSTOP,160,84,20,
                    16
    CONTROL         "v",ID_CMD_BUTTON_C15,"Button",BS_OWNERDRAW | WS_TABSTOP,184,84,20,
                    16
    CONTROL         "v",ID_CMD_BUTTON_C16,"Button",BS_OWNERDRAW | WS_TABSTOP,112,104,
                    20,16
    CONTROL         "v",ID_CMD_BUTTON_C17,"Button",BS_OWNERDRAW | WS_TABSTOP,136,104,
                    20,16
    CONTROL         "v",ID_CMD_BUTTON_C18,"Button",BS_OWNERDRAW | WS_TABSTOP,160,104,
                    20,16
    CONTROL         "v",ID_CMD_BUTTON_C19,"Button",BS_OWNERDRAW | WS_TABSTOP,184,104,
                    20,16
    LTEXT           "&Window Name",IDC_STATIC,8,8,68,11
    LISTBOX         ID_CMD_BUTTON_WINDS,8,20,84,104,LBS_NOINTEGRALHEIGHT | WS_VSCROLL | 
                    WS_TABSTOP
END

ID_SYM_DIALOG_FONT_DIALOG DIALOG DISCARDABLE  0, 0, 276, 159
STYLE WS_CHILD | WS_DISABLED | WS_CAPTION
CAPTION "Select Fonts"
FONT 8, "MS Sans Serif"
BEGIN
    EDITTEXT        ID_CMD_BUTTON_FONT_DEMO,92,100,176,40,ES_CENTER | ES_MULTILINE | 
                    ES_AUTOHSCROLL
    LISTBOX         ID_CMD_BUTTON_FONT_WINDOW,8,24,76,116,LBS_NOINTEGRALHEIGHT | 
                    WS_VSCROLL | WS_TABSTOP
    LISTBOX         ID_CMD_BUTTON_FONT_NAME,92,24,76,68,LBS_NOINTEGRALHEIGHT | 
                    WS_VSCROLL | WS_TABSTOP
    LISTBOX         ID_CMD_BUTTON_FONT_STYLE,178,24,48,68,LBS_NOINTEGRALHEIGHT | 
                    WS_VSCROLL | WS_TABSTOP
    LISTBOX         ID_CMD_BUTTON_FONT_SIZE,236,24,32,68,LBS_NOINTEGRALHEIGHT | 
                    WS_VSCROLL | WS_TABSTOP
    LTEXT           "&Window ",IDC_STATIC,8,12,28,8
    LTEXT           "&Font",IDC_STATIC,96,12,28,8
    LTEXT           "&Style",IDC_STATIC,180,12,28,8
    LTEXT           "Si&ze",IDC_STATIC,236,12,28,8
END

ID_SYM_DIALOG_DIALOG4 DIALOG DISCARDABLE  0, 0, 185, 92
STYLE DS_MODALFRAME | WS_POPUP | WS_VISIBLE | WS_CAPTION | WS_SYSMENU
CAPTION "Dialog"
FONT 8, "MS Sans Serif"
BEGIN
    DEFPUSHBUTTON   "OK",IDOK,129,6,50,14
    PUSHBUTTON      "Cancel",IDCANCEL,129,23,50,14
END

ID_SYM_DIALOG_DIR_DIALOG DIALOG DISCARDABLE  0, 0, 276, 175
STYLE WS_CHILD | WS_DISABLED | WS_CAPTION
CAPTION "Path List"
FONT 8, "MS Sans Serif"
BEGIN
    LTEXT           "&Path",IDC_STATIC,16,24,68,8
    LISTBOX         ID_CMD_BUTTON_PATH,16,40,184,104,LBS_SORT | LBS_NOINTEGRALHEIGHT | 
                    WS_VSCROLL | WS_HSCROLL | WS_TABSTOP
    PUSHBUTTON      "&Add ...",ID_CMD_BUTTON_ADD,208,40,48,16
    PUSHBUTTON      "&Remove",ID_CMD_BUTTON_REMOVE,208,62,48,16
    PUSHBUTTON      "Move &Up",ID_REAL_CMD_BUTTON_UP,208,84,48,16
    PUSHBUTTON      "Move &Down",ID_CMD_BUTTON_DOWN,208,106,48,16
END

ID_SYM_DIALOG_ADD_PATH DIALOG DISCARDABLE  0, 0, 185, 92
STYLE DS_MODALFRAME | WS_POPUP | WS_VISIBLE | WS_CAPTION | WS_SYSMENU
CAPTION "Add Path"
FONT 8, "MS Sans Serif"
BEGIN
    DEFPUSHBUTTON   "OK",IDOK,16,52,52,16
    PUSHBUTTON      "Cancel",IDCANCEL,72,52,52,16
    PUSHBUTTON      "&Browse ...",ID_CMD_BUTTON_BROWSE,128,52,52,16
    EDITTEXT        ID_REAL_CMD_BUTTON_NEW_PATH,16,24,164,12,ES_AUTOHSCROLL
    LTEXT           "&Path",IDC_STATIC,16,8,68,7
END

ID_SYM_DIALOG_BROWSE_DIR DIALOG DISCARDABLE  0, 0, 175, 150
STYLE DS_MODALFRAME | WS_POPUP | WS_VISIBLE | WS_CAPTION | WS_SYSMENU
CAPTION "Browse For Directory"
FONT 8, "MS Sans Serif"
BEGIN
    LTEXT           "",stc3,132,100,12,10
    EDITTEXT        edt1,124,100,12,12,ES_AUTOHSCROLL | ES_OEMCONVERT
    LISTBOX         lst1,120,108,40,9,LBS_SORT | LBS_OWNERDRAWFIXED | 
                    LBS_HASSTRINGS | LBS_DISABLENOSCROLL | WS_VSCROLL | 
                    WS_TABSTOP
    LTEXT           "&Directories:",-1,7,6,92,9
    LTEXT           "",stc1,7,18,92,9,SS_NOPREFIX
    LISTBOX         lst2,7,32,92,68,LBS_SORT | LBS_OWNERDRAWFIXED | 
                    LBS_HASSTRINGS | LBS_DISABLENOSCROLL | WS_VSCROLL | 
                    WS_TABSTOP
    LTEXT           "List Files of &Type:",stc2,128,108,26,9
    COMBOBOX        cmb1,116,100,38,36,CBS_DROPDOWNLIST | CBS_AUTOHSCROLL | 
                    WS_BORDER | WS_VSCROLL | WS_TABSTOP
    LTEXT           "Dri&ves:",stc4,4,108,92,9
    COMBOBOX        cmb2,8,120,92,68,CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | 
                    CBS_AUTOHSCROLL | CBS_SORT | CBS_HASSTRINGS | WS_BORDER | 
                    WS_VSCROLL | WS_TABSTOP
    DEFPUSHBUTTON   "OK",IDOK,105,6,50,14,WS_GROUP
    PUSHBUTTON      "Cancel",IDCANCEL,105,24,50,14,WS_GROUP
    PUSHBUTTON      "&Help",psh15,105,46,50,14,WS_GROUP
    CONTROL         "&Read Only",chx1,"Button",BS_AUTOCHECKBOX | WS_GROUP | 
                    WS_TABSTOP,105,68,50,12
END

ID_SYM_DIALOG_SRC_INFO DIALOG DISCARDABLE  0, 0, 336, 19
STYLE WS_CHILD | WS_BORDER
FONT 8, "MS Sans Serif"
BEGIN
    EDITTEXT        ID_CMD_BUTTON_EDIT1,115,3,215,12,ES_AUTOHSCROLL
    CONTROL         "Assembly",ID_REAL_CMD_BUTTON_SHOW_ASM1,"Button",BS_AUTOCHECKBOX | 
                    WS_TABSTOP,9,2,48,8
    CONTROL         "Source",ID_REAL_CMD_BUTTON_SHOW_SRC1,"Button",BS_AUTOCHECKBOX | 
                    WS_TABSTOP,74,1,36,8
    CONTROL         "",ID_REAL_CMD_BUTTON_SHOW_SRC2,"Button",BS_AUTOCHECKBOX | WS_TABSTOP,
                    74,10,36,8
    CONTROL         "",ID_REAL_CMD_BUTTON_SHOW_ASM2,"Button",BS_AUTOCHECKBOX | WS_TABSTOP,9,
                    10,48,8
END


/////////////////////////////////////////////////////////////////////////////
//
// Version
//

VS_VERSION_INFO VERSIONINFO
 FILEVERSION 1,0,0,1
 PRODUCTVERSION 1,0,0,1
 FILEFLAGSMASK 0x3fL
#ifdef _DEBUG
 FILEFLAGS 0x1L
#else
 FILEFLAGS 0x0L
#endif
 FILEOS 0x4L
 FILETYPE 0x1L
 FILESUBTYPE 0x0L
BEGIN
    BLOCK "StringFileInfo"
    BEGIN
        BLOCK "040904B0"
        BEGIN
            VALUE "CompanyName", "\0"
            VALUE "FileDescription", "GUI MFC Application\0"
            VALUE "FileVersion", "1, 0, 0, 1\0"
            VALUE "InternalName", "GUI\0"
            VALUE "LegalCopyright", "Copyright \251 1994\0"
            VALUE "LegalTrademarks", "\0"
            VALUE "OriginalFilename", "GUI.EXE\0"
            VALUE "ProductName", "GUI Application\0"
            VALUE "ProductVersion", "1, 0, 0, 1\0"
        END
    END
    BLOCK "VarFileInfo"
    BEGIN
        VALUE "Translation", 0x409, 1200
    END
END



/////////////////////////////////////////////////////////////////////////////
//
// Cursor
//

ID_CMD_BUTTON_BPT_CURSOR          CURSOR  DISCARDABLE     "res\\cursor1.cur"
ID_REAL_CMD_BUTTON_SRC_CURSOR          CURSOR  DISCARDABLE     "res\\cur00001.cur"
ID_REAL_CMD_BUTTON_NOTBPT_CURSOR       CURSOR  DISCARDABLE     "res\\bpt_curs.cur"

/////////////////////////////////////////////////////////////////////////////
//
// String Table
//

STRINGTABLE PRELOAD DISCARDABLE 
BEGIN
    ID_SYM_FRAME_MAINFRAME           "\nGui\n\n\n\n\nGui.Document\nGui Document"
    ID_SYM_FRAME_LOGTYPE             "\nCommand Log\n\n\n\nGDB Command Log.Document.3\nGDB Command Log"
    ID_SYM_FRAME_CMDFRAME            "\nGui\n\n\n\nGui.Document\nGui Document"
    ID_SYM_FRAME_SPLITTYPE           "\nspliti\n\n\n\n\nsplit.Document\nsplit Document"
END

STRINGTABLE PRELOAD DISCARDABLE 
BEGIN
    ID_SYM_FRAME_CMDTYPE             "\nCmd\n\n\n\nCmd.Document\nCmd Document"
    ID_SYM_FRAME_REGTYPE             "\nReg\n\n\n\nReg.Document\nReg Document"
    ID_SYM_FRAME_SRCTYPE             "Src\nSource \nSrck Book\nC Source II (*.c)\n.c\nSrc type\nSource File Type\nCHsBK\nsource files"
END

STRINGTABLE PRELOAD DISCARDABLE 
BEGIN
    ID_SYM_FRAME_BPTTYPE             "\nBreakpoint\n\n\n\nBreakpoint .Document\nBreakpoint Document"
    ID_SYM_FRAME_SRCBROWSER          "Browser\nBrowser\nbrowser  Book\nBrowser File (*.c)\n.c\nBrowser file type\nBrowser Book File Type\nbrowCHsBK\nbrowser Book Files"
    ID_SYM_FRAME_LOCALTYPE           "Locals\nLocals\nbrowser  Book\nBrowser File (*.c)\n.c\nBrowser file type\nBrowser Book File Type\nbrowCHsBK\nbrowser Book Files"
    ID_SYM_FRAME_FLASHTYPE           "\nFlash\n\n\n\nFlash .Document\nFlash Document"
    ID_SYM_FRAME_INFOTYPE            "Info\ninfo\nInfo browser\nInfo files (*.inf)\n.inf\nInfoFileType\nInfo File Type\nINF\nInfo Browser Files"
END

STRINGTABLE PRELOAD DISCARDABLE 
BEGIN
    ID_SYM_FRAME_EXPTYPE             "\nWatch\n\n\n\nWatch.Document\nWatch Document"
END

STRINGTABLE PRELOAD DISCARDABLE 
BEGIN
    AFX_IDS_APP_TITLE       "Cygnus GDB "
    AFX_IDS_IDLEMESSAGE     "For Help, press F1"
    AFX_IDS_HELPMODEMESSAGE "Select an object on which to get Help"
END

STRINGTABLE DISCARDABLE 
BEGIN
    ID_REAL_CMD_BUTTON_INDICATOR_EXT        "EXT"
    ID_REAL_CMD_BUTTON_INDICATOR_CAPS       "CAP"
    ID_REAL_CMD_BUTTON_INDICATOR_NUM        "NUM"
    ID_REAL_CMD_BUTTON_INDICATOR_SCRL       "SCRL"
    ID_REAL_CMD_BUTTON_INDICATOR_OVR        "OVR"
    ID_REAL_CMD_BUTTON_INDICATOR_REC        "REC"
END

STRINGTABLE DISCARDABLE 
BEGIN
    ID_FILE_NEW             "Create a new document\nNew"
    ID_FILE_OPEN            "Open an existing document\nOpen"
    ID_FILE_CLOSE           "Close the active document\nClose"
    ID_FILE_SAVE            "Save the active document\nSave"
    ID_FILE_SAVE_AS         "Save the active document with a new name\nSave As"
    ID_FILE_PAGE_SETUP      "Change the printing options\nPage Setup"
    ID_FILE_PRINT_SETUP     "Change the printer and printing options\nPrint Setup"
    ID_FILE_PRINT           "Print the active document\nPrint"
    ID_FILE_PRINT_PREVIEW   "Display full pages\nPrint Preview"
END

STRINGTABLE DISCARDABLE 
BEGIN
    ID_APP_ABOUT            "Display program information, version number and copyright\nAbout"
    ID_APP_EXIT             "Quit the application; prompts to save documents\nExit"
    ID_HELP_INDEX           "List Help topics\nHelp Index"
    ID_HELP_USING           "Display instructions about how to use help\nHelp"
    ID_REAL_CMD_BUTTON_CONTEXT_HELP         "Display help for clicked on buttons, menus and windows\nHelp"
    ID_HELP                 "Display help browser"
END

STRINGTABLE DISCARDABLE 
BEGIN
    ID_FILE_MRU_FILE1       "Open this document"
    ID_FILE_MRU_FILE2       "Open this document"
    ID_FILE_MRU_FILE3       "Open this document"
    ID_FILE_MRU_FILE4       "Open this document"
END

STRINGTABLE DISCARDABLE 
BEGIN
    ID_REAL_CMD_BUTTON_NEXT_PANE            "Switch to the next window pane\nNext Pane"
    ID_PREV_PANE            "Switch back to the previous window pane\nPrevious Pane"
END

STRINGTABLE DISCARDABLE 
BEGIN
    ID_WINDOW_NEW           "Open another window for the active document\nNew Window"
    ID_WINDOW_ARRANGE       "Arrange icons at the bottom of the window\nArrange Icons"
    ID_WINDOW_CASCADE       "Arrange windows so they overlap\nCascade Windows"
    ID_WINDOW_TILE_HORZ     "Arrange windows as non-overlapping tiles\nTile Windows"
    ID_WINDOW_TILE_VERT     "Arrange windows as non-overlapping tiles\nTile Windows"
    ID_WINDOW_SPLIT         "Split the active window into panes\nSplit"
END

STRINGTABLE DISCARDABLE 
BEGIN
    ID_EDIT_CLEAR           "Erase the selection\nErase"
    ID_EDIT_CLEAR_ALL       "Erase everything\nErase All"
    ID_EDIT_COPY            "Copy the selection and put it on the Clipboard\nCopy"
    ID_EDIT_CUT             "Cut the selection and put it on the Clipboard\nCut"
    ID_EDIT_FIND            "Find the specified text\nFind"
    ID_EDIT_PASTE           "Insert Clipboard contents\nPaste"
    ID_EDIT_REPEAT          "Repeat the last action\nRepeat"
    ID_EDIT_REPLACE         "Replace specific text with different text\nReplace"
    ID_EDIT_SELECT_ALL      "Select the entire document\nSelect All"
    ID_EDIT_UNDO            "Undo the last action\nUndo"
    ID_EDIT_REDO            "Redo the previously undone action\nRedo"
END

STRINGTABLE DISCARDABLE 
BEGIN
    ID_VIEW_TOOLBAR         "Show or hide the toolbar\nToggle ToolBar"
    ID_VIEW_STATUS_BAR      "Show or hide the status bar\nToggle StatusBar"
END

STRINGTABLE DISCARDABLE 
BEGIN
    AFX_IDS_SCSIZE          "Change the window size"
    AFX_IDS_SCMOVE          "Change the window position"
    AFX_IDS_SCMINIMIZE      "Reduce the window to an icon"
    AFX_IDS_SCMAXIMIZE      "Enlarge the window to full size"
    AFX_IDS_SCNEXTWINDOW    "Switch to the next document window"
    AFX_IDS_SCPREVWINDOW    "Switch to the previous document window"
    AFX_IDS_SCCLOSE         "Close the active window and prompts to save the documents"
END

STRINGTABLE DISCARDABLE 
BEGIN
    AFX_IDS_SCRESTORE       "Restore the window to normal size"
    AFX_IDS_SCTASKLIST      "Activate Task List"
    AFX_IDS_MDICHILD        "Activate this window"
END

STRINGTABLE DISCARDABLE 
BEGIN
    AFX_IDS_PREVIEW_CLOSE   "Close print preview mode\nCancel Preview"
END

STRINGTABLE DISCARDABLE 
BEGIN
    ID_CMD_BUTTON_CHOOSE_FONT          "Select the font doode"
    ID_REAL_CMD_BUTTON_NEXT2        "Goto next node in tree"
    ID_REAL_CMD_BUTTON_REGISTER             "Create new register window"
    ID_REAL_CMD_BUTTON_REG_DECIMAL          "Display register contents in decimal"
    ID_REAL_CMD_BUTTON_REG_HEX              "Show register contents in Hex"
    ID_REAL_CMD_BUTTON_REG_OCTAL            "Show register contents in octal"
    ID_REAL_CMD_BUTTON_REG_BINARY           "Show register contents in Binary"
    ID_REAL_CMD_BUTTON_REG_EVERYTHING       "Show register contents every way"
END

STRINGTABLE DISCARDABLE 
BEGIN
    ID_REAL_CMD_BUTTON_PREV2        "Goto previous node in tree"
    ID_REAL_CMD_BUTTON_SI                   "Step by one instruction"
    ID_DEBUG_NEXTINSTRUCTION "Step instruction and over calls"
    ID_REAL_CMD_BUTTON_UP2          "Move up one level in the tree"
    ID_ACTIONS_FORWARD2     "Move forwards though the history"
    ID_REAL_CMD_BUTTON_NEW_CMDWIN           "Command Window\nCommand Window"
    ID_REAL_CMD_BUTTON_BACKWARD2    "Move back though the history"
    ID_REAL_CMD_BUTTON_NEW_REGWIN           "\nRegister Window"
    ID_REAL_CMD_BUTTON_NEXT_PAGE     "Turn to the next page in the document"
    ID_REAL_CMD_BUTTON_S                    "Step one source line\nStep"
END

STRINGTABLE DISCARDABLE 
BEGIN
    ID_REAL_CMD_BUTTON_STRING_DISK_SPACE_UNAVAIL  "b"
    ID_REAL_CMD_BUTTON_STRING_DISK_SPACE          "%lu KB Free"
    ID_REAL_CMD_BUTTON_STRING_MATH_COPR_NOTPRESENT "Not present"
    ID_REAL_CMD_BUTTON_STRING_MATH_COPR_PRESENT   "Present"
    ID_REAL_CMD_BUTTON_STRING_PHYSICAL_MEM        "%lu KB"
END

STRINGTABLE DISCARDABLE 
BEGIN
    ID_REAL_CMD_BUTTON_N                    "Step to next source line\nNext"
    ID_REAL_CMD_BUTTON_NEW_BPTWIN           "\nBreakpoint Window"
    ID_REAL_CMD_BUTTON_CONT                 "Continue\nCont"
    ID_REAL_CMD_BUTTON_RUN                  "Run program\nRun"
    ID_CMD_BUTTON_WINDOW_ASM           "Open a disassembly window for the current document"
    ID_REAL_CMD_BUTTON_SET_FONT_S           "Select font to use for the source code in the window"
    ID_REAL_CMD_BUTTON_SET_FONT_A           "Select the font for disassembly"
    ID_REAL_CMD_BUTTON_SET_COLOR_A          "Setlect thge disassembly color"
    ID_REAL_CMD_BUTTON_NEW_SRCBROWSER_WIN   "Create a source browser window\nBrowser Window"
    ID_REAL_CMD_BUTTON_NEW_LOCAL_WIN        "Create new locals window\nLocals Window"
    ID_CMD_BUTTON_VIEW_OPTIONS         "Select GDB view options"
END

STRINGTABLE DISCARDABLE 
BEGIN
    ID_REAL_CMD_BUTTON_NEW_SRC_WIN          "\nSource Window"
    ID_REAL_CMD_BUTTON_NEW_EXPRESSION_WIN   "\nWatch Window"
    ID_REAL_CMD_BUTTON_FINISH               "\nFinish Function"
END

STRINGTABLE DISCARDABLE 
BEGIN
    ID_REAL_CMD_BUTTON_SRCWIN_SHOWBPT       "\nShow Break Locs"
    ID_REAL_CMD_BUTTON_SRCWIN_SHOWSOURCE    "\nShow Source"
    ID_REAL_CMD_BUTTON_SRCWIN_SHOWASM       "\nShow Assembly"
    ID_REAL_CMD_BUTTON_SRCWIN_SHOWLINES     "\nShow Line Numbers"
	ID_REAL_CMD_BUTTON_IN "\nIn one level"
	ID_REAL_CMD_BUTTON_OUT "\nOut one level"
END


#ifndef APSTUDIO_INVOKED
/////////////////////////////////////////////////////////////////////////////
//
// Generated from the TEXTINCLUDE 3 resource.
//
#include "res\gui.rc2"  // non-Microsoft Visual C++ edited resources
#include "menus\menus.rc" // non-Microsoft Visual C++ edited resources
#define _AFX_NO_SPLITTER_RESOURCES
#define _AFX_NO_OLE_RESOURCES
#define _AFX_NO_TRACKER_RESOURCES
#include "afxres.rc"  	// Standard components
#include "afxprint.rc"	// printing/print preview resources

/////////////////////////////////////////////////////////////////////////////
#endif    // not APSTUDIO_INVOKED

