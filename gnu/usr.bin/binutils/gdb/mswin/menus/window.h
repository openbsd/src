    POPUP "&Window"
    BEGIN
        MENUITEM "&New Window",                 ID_WINDOW_NEW
        MENUITEM "Cascade",                     ID_WINDOW_CASCADE
        MENUITEM "&Tile",                       ID_WINDOW_TILE_HORZ
        MENUITEM "&Arrange Icons",              ID_WINDOW_ARRANGE
        MENUITEM SEPARATOR
        MENUITEM "&Command input",          ID_REAL_CMD_BUTTON_NEW_CMDWIN
        MENUITEM "&Register",   ID_REAL_CMD_BUTTON_NEW_REGWIN
        MENUITEM "Break&point", ID_REAL_CMD_BUTTON_NEW_BPTWIN
        MENUITEM "&Source",     ID_REAL_CMD_BUTTON_NEW_SRC_WIN
        MENUITEM "&Browser",    ID_REAL_CMD_BUTTON_NEW_SRCBROWSER_WIN
        MENUITEM "&Locals",     ID_REAL_CMD_BUTTON_NEW_LOCAL_WIN
	MENUITEM "&Watch", 	ID_REAL_CMD_BUTTON_NEW_EXPRESSION_WIN
	MENUITEM "&Memory", 	ID_REAL_CMD_BUTTON_NEW_MEMORY_WIN
	MENUITEM "&IO", 	ID_REAL_CMD_BUTTON_NEW_IO_WIN
    END
