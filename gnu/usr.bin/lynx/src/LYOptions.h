#ifndef LYOPTIONS_H
#define LYOPTIONS_H

extern BOOLEAN term_options;

extern void options NOPARAMS;
extern void edit_bookmarks NOPARAMS;

/*
 *  Values for the options menu. - FM
 *
 *  L_foo values are the Y coordinates for the menu item.
 *  B_foo values are the X coordinates for the item's prompt string.
 *  C_foo values are the X coordinates for the item's value string.
 */
#define L_EDITOR	 2
#define L_DISPLAY	 3

#define L_HOME		 4
#define C_MULTI		24
#define B_BOOK		34
#define C_DEFAULT	50

#define L_FTPSTYPE	 5
#define L_MAIL_ADDRESS	 6
#define L_SSEARCH	 7
#define L_LANGUAGE	 8
#define L_PREF_CHARSET	 9
#define L_ASSUME_CHARSET (L_PREF_CHARSET + 1)
#define L_CHARSET	10
#define L_RAWMODE	11

#define L_COLOR		L_RAWMODE
#define B_COLOR		44
#define C_COLOR		62

#define L_BOOL_A	12
#define B_VIKEYS	5
#define C_VIKEYS	15
#define B_EMACSKEYS	22
#define C_EMACSKEYS	36
#define B_SHOW_DOTFILES	44
#define C_SHOW_DOTFILES	62

#define L_BOOL_B	13
#define B_SELECT_POPUPS	5
#define C_SELECT_POPUPS	36
#define B_SHOW_CURSOR	44
#define C_SHOW_CURSOR	62

#define L_KEYPAD	14 
#define L_LINEED	15

#ifdef DIRED_SUPPORT
#define L_DIRED		16
#define L_USER_MODE	17
#define L_USER_AGENT	18
#define L_EXEC		19
#else
#define L_USER_MODE	16
#define L_USER_AGENT	17
#define L_EXEC		18
#endif /* DIRED_SUPPORT */

#endif /* LYOPTIONS_H */
