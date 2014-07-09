/* $LynxId: LYOptions.h,v 1.31 2013/10/22 00:37:32 tom Exp $ */
#ifndef LYOPTIONS_H
#define LYOPTIONS_H

#include <LYStructs.h>
#include <LYStrings.h>

#ifdef __cplusplus
extern "C" {
#endif
    extern BOOLEAN term_options;	/* for LYgetstr() */

    extern BOOLEAN LYCheckUserAgent(void);
    extern void edit_bookmarks(void);
    extern int popup_choice(int cur_choice,
			    int line,
			    int column,
			    STRING2PTR choices,
			    int length,
			    int disabled,
			    int mouse);

#define LYChoosePopup(cur, line, column, choices, length, disabled, mouse) \
	popup_choice(cur, line, column, (STRING2PTR) choices, length, disabled, mouse)

#ifndef NO_OPTION_FORMS
    extern void LYMenuVisitedLinks(FILE *fp0, int disable_all);
    extern int postoptions(DocInfo *newdoc);
#endif				/* !NO_OPTION_FORMS */

#ifndef NO_OPTION_MENU
    extern void LYoptions(void);
#endif				/* !NO_OPTION_MENU */

#ifdef USE_COLOR_STYLE
    extern void build_lss_enum(HTList *);
#endif

#ifdef __cplusplus
}
#endif
#endif				/* LYOPTIONS_H */
