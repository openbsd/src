#ifndef LYOPTIONS_H
#define LYOPTIONS_H

#include <LYStructs.h>
#include <LYStrings.h>

extern BOOLEAN term_options; /* for LYgetstr() */

extern BOOLEAN LYCheckUserAgent NOPARAMS;
extern void edit_bookmarks NOPARAMS;
extern  int popup_choice PARAMS((
	int		cur_choice,
	int		line,
	int		column,
	CONST char ** 	choices,
	int		length,
	int		disabled,
	BOOLEAN		mouse));

#define LYChoosePopup(cur, line, column, choices, length, disabled, mouse) \
	popup_choice(cur, line, column, (CONST char **)choices, length, disabled, mouse)

#ifndef NO_OPTION_FORMS
extern int postoptions PARAMS((document *newdoc));
#endif /* !NO_OPTION_FORMS */

#ifndef NO_OPTION_MENU
extern void LYoptions NOPARAMS;
#endif /* !NO_OPTION_MENU */

#endif /* LYOPTIONS_H */
