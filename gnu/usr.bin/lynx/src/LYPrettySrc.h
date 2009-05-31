#ifndef LYPrettySrc_H
#define LYPrettySrc_H

#ifdef USE_PRETTYSRC

#include <HTMLDTD.h>

#ifdef __cplusplus
extern "C" {
#endif
    extern BOOL psrc_convert_string;

    /*whether HTML_put_string should convert string passed with 
       TRANSLATE_AND_UNESCAPE_TO_STD */
    extern BOOL psrc_view;
    extern BOOL LYpsrc;

/*
 * This is used for tracking down whether the SGML engine was initialized
 * ==TRUE if yes.  It's value is meaningful if psrc_view = TRUE
 */
    extern BOOL sgml_in_psrc_was_initialized;

    extern BOOL psrc_nested_call;	/* this is used when distinguishing whether 

					   the current call is nested or not in HTML.c HTML_{start,end}_element.
					   It ==FALSE if psrc_view==FALSE || sgml_in_psrc_was_initialized==TRUE */

    extern BOOL psrc_first_tag;	/* this is also used in HTML.c to trigger the 

				   1st tag to preform special.
				 */

    extern BOOL mark_htext_as_source;

/* here is a list of lexeme codes. */
    typedef enum {
	HTL_comm = 0,
	HTL_tag,
	HTL_attrib,
	HTL_attrval,
	HTL_abracket,
	HTL_entity,
	HTL_href,
	HTL_entire,
	HTL_badseq,
	HTL_badtag,
	HTL_badattr,
	HTL_sgmlspecial,
	HTL_num_lexemes
    } HTlexeme;

    typedef struct _HT_tagspec {
	struct _HT_tagspec *next;	/* 0 at the last */
#ifdef USE_COLOR_STYLE
	int style;		/* precalculated value of the style */
	char *class_name;
#endif
	/* these will be passed to HTML_start_element */
	HTMLElement element;
	BOOL *present;
	char **value;

	BOOL start;		/* if true, then this starts element, otherwise - ends */
    } HT_tagspec;

    extern char *HTL_tagspecs[HTL_num_lexemes];
    extern HT_tagspec *lexeme_start[HTL_num_lexemes];
    extern HT_tagspec *lexeme_end[HTL_num_lexemes];

    extern int html_src_parse_tagspec(char *ts, HTlexeme lexeme,
				      BOOL checkonly, BOOL isstart);
    extern void HTMLSRC_init_caches(BOOL dont_exit);
    extern void html_src_clean_item(HTlexeme l);
    extern void html_src_clean_data(void);
    extern void html_src_on_lynxcfg_reload(void);

/* these 2 vars tell what kind of transform should be appiled to tag names
  and attribute names. 0 - lowercase, 1 - as is, 2 uppercase. */
    extern int tagname_transform;
    extern int attrname_transform;

    extern BOOL psrcview_no_anchor_numbering;

#ifdef __cplusplus
}
#endif
#endif				/* ifdef USE_PRETTYSRC */
#endif				/* LYPrettySrc_H */
