/*	$OpenBSD: gui_athena.pro,v 1.1.1.1 1996/09/07 21:40:29 downsj Exp $	*/
/* gui_athena.c */
void gui_mch_create_widgets __PARMS((void));
int gui_mch_get_winsize __PARMS((void));
void gui_mch_set_winsize __PARMS((void));
void gui_mch_add_menu __PARMS((GuiMenu *menu, GuiMenu *parent));
void gui_mch_add_menu_item __PARMS((GuiMenu *menu, GuiMenu *parent));
void gui_mch_destroy_menu __PARMS((GuiMenu *menu));
void gui_mch_create_which_components __PARMS((void));
void gui_mch_update_scrollbars __PARMS((int worst_update, int which_sb));
void gui_mch_reorder_scrollbars __PARMS((int which_sb));
void gui_mch_destroy_scrollbar __PARMS((WIN *wp));
void gui_mch_update_horiz_scrollbar __PARMS((int value, int size, int max));
Window gui_mch_get_wid __PARMS((void));
