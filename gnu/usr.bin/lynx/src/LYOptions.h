#ifndef LYOPTIONS_H
#define LYOPTIONS_H

#include <LYStructs.h>

extern BOOLEAN term_options; /* for LYgetstr() */

extern void edit_bookmarks NOPARAMS;
extern  int popup_choice PARAMS((
	int		cur_choice,
	int		line,
	int		column,
	char ** 	choices,
	int		i_length,
	int		disabled));

#ifndef NO_OPTION_FORMS
extern int postoptions PARAMS((document *newdoc));
#endif /* !NO_OPTION_FORMS */

#ifndef NO_OPTION_MENU
extern void LYoptions NOPARAMS;
#endif /* !NO_OPTION_MENU */

#endif /* LYOPTIONS_H */
