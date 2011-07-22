/*
 * $LynxId: HTForms.h,v 1.27 2009/05/28 22:49:51 tom Exp $
 */
#ifndef HTFORMS_H
#define HTFORMS_H

#ifndef LYSTRUCTS_H
#include <LYStructs.h>
#endif /* LYSTRUCTS_H */

#ifdef __cplusplus
extern "C" {
#endif
/* change_form_link() calls change_form_link_ex() with all its args and FALSE
 * as last arg
 */ extern int change_form_link(int cur,
				DocInfo *newdoc,
				BOOLEAN *refresh_screen,
				BOOLEAN use_last_tfpos,
				BOOLEAN immediate_submit);

    extern int change_form_link_ex(int cur,
				   DocInfo *newdoc,
				   BOOLEAN *refresh_screen,
				   BOOLEAN use_last_tfpos,
				   BOOLEAN immediate_submit,
				   BOOLEAN draw_only);

/* InputFieldData is used to pass the info between HTML.c and Gridtext.c in
 * HText_beginInput()
 */
    typedef struct _InputFieldData {
	const char *accept;
	const char *align;
	int checked;
	const char *iclass;
	int disabled;
	const char *error;
	const char *height;
	const char *id;
	const char *lang;
	const char *max;
	const char *maxlength;
	const char *md;
	const char *min;
	const char *name;
	int size;
	const char *src;
	const char *type;
	char *value;
	const char *width;
	int name_cs;		/* charset handle for name */
	int value_cs;		/* charset handle for value */
	const char *accept_cs;
    } InputFieldData;

/* The OptionType structure is for a linked list of option entries
 */
    typedef struct _OptionType {
	char *name;		/* the name of the entry */
	char *cp_submit_value;	/* the value to submit   */
	int value_cs;		/* charset value is in   */
	struct _OptionType *next;	/* the next entry        */
    } OptionType;

/*
 * The FormInfo structure is used to contain the form field data within each
 * anchor.  A pointer to this structure is in the TextAnchor struct.
 */
    typedef struct _FormInfo {
	char *name;		/* the name of the link */
	int number;		/* which form is the link within */
	int type;		/* string, int, etc. */
	char *value;		/* user entered string data */
	char *orig_value;	/* the original value */
	int size;		/* width on the screen */
	unsigned maxlength;	/* max width of data */
	int group;		/* a group associated with the link
				 *  this is used for select's
				 */
	int num_value;		/* value of the numerical fields */
	int hrange;		/* high numerical range */
	int lrange;		/* low numerical range */
	OptionType *select_list;	/* array of option choices */
	char *submit_action;	/* form's action */
	int submit_method;	/* form's method */
	char *submit_enctype;	/* form's entype */
	char *submit_title;	/* form's title */
	BOOL no_cache;		/* Always resubmit? */
	char *cp_submit_value;	/* option value to submit */
	char *orig_submit_value;	/* original submit value */
	int size_l;		/* The length of the option list */
	int disabled;		/* If YES, can't change values */
	int name_cs;
	int value_cs;
	char *accept_cs;
    } FormInfo;

/*
 * As structure for info associated with a form.  There is some redundancy
 * here, this shouldn't waste too much memory since the total number of forms
 * (as opposed to form fields) per doc is expected to be rather small.  More
 * things which are per form rather than per field could be moved here.  - kw
 */
    typedef struct _PerFormInfo {
	int number;		/* form number, see GridText.c */
	/* except for the last two, the following fields aren't actually used.. */
	int disabled;		/* If YES, can't change values */
	struct _PerFormInfo *next;	/* pointer to next form in doc */
	int nfields;		/* number of fields */
	FormInfo *first_field;
	FormInfo *last_field;	/* pointer to last field in form */
	char *accept_cs;
	char *thisacceptcs;	/* used during submit */
    } PerFormInfo;

#define HYPERTEXT_ANCHOR 1
#define INPUT_ANCHOR     2	/* forms mode input fields */
#define INTERNAL_LINK_ANCHOR 5	/* 1+4, can be used as bitflag... - kw */

    typedef enum {
	F_UNKNOWN = 0,
	F_TEXT_TYPE,
	F_PASSWORD_TYPE,
	F_CHECKBOX_TYPE,
	F_RADIO_TYPE,
	F_SUBMIT_TYPE,
	F_RESET_TYPE,
	F_OPTION_LIST_TYPE,
	F_HIDDEN_TYPE,
	F_TEXTAREA_TYPE,
	F_RANGE_TYPE,
	F_FILE_TYPE,
	F_TEXT_SUBMIT_TYPE,
	F_IMAGE_SUBMIT_TYPE,
	F_KEYGEN_TYPE,
	F_BUTTON_TYPE
    } FieldTypes;

#define F_TEXTLIKE(type) ((type) == F_TEXT_TYPE || \
			  (type) == F_TEXT_SUBMIT_TYPE || \
			  (type) == F_PASSWORD_TYPE || \
			  (type) == F_FILE_TYPE || \
			  (type) == F_TEXTAREA_TYPE)

#define WWW_FORM_LINK_TYPE  1
#define WWW_LINK_TYPE   2
#define WWW_INTERN_LINK_TYPE   6	/* can be used as a bitflag... - kw */
#define LINK_LINE_FOUND	8	/* used in follow_link_number, others - kw */
#define LINK_DO_ARROWUP	16	/* returned by HTGetLinkOrFieldStart - kw */

/* #define different lynx modes */
#define NORMAL_LYNX_MODE 1
#define FORMS_LYNX_MODE  2

#define FIRST_ORDER  1
#define MIDDLE_ORDER 2
#define LAST_ORDER   3

/* in LYForms.c */
    extern void show_formlink_statusline(const FormInfo * form,
					 int for_what);
#ifdef __cplusplus
}
#endif
#endif				/* HTFORMS_H */
