#ifndef LYPrettySrc_H
#define LYPrettySrc_H

#ifdef USE_PSRC

#include <HTMLDTD.h>

extern BOOL psrc_convert_string;
 /*whether HTML_put_string should convert string passed with 
   TRANSLATE_AND_UNESCAPE_TO_STD */
extern BOOL psrc_view;
extern BOOL LYpsrc;
extern BOOL sgml_in_psrc_was_initialized; 
 /*this is used for tracking down whether the SGML engine was initialized
  ==TRUE if yes. It's value is meaningful if psrc_view = TRUE */
  
extern BOOL psrc_nested_call;/* this is used when distinguishing whether 
 the current call is nested or not in HTML.c HTML_{start,end}_element.
 It ==FALSE if psrc_view==FALSE || sgml_in_psrc_was_initialized==TRUE */
 
extern BOOL psrc_first_tag; /* this is also used in HTML.c to trigger the 
 1st tag to preform special. */

/* here is a list of lexem codes. */
typedef enum _HTlexem {
  HTL_comm=0,
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
  HTL_num_lexems
} HTlexem;

typedef struct _HT_tagspec
{
    struct _HT_tagspec* next;/* 0 at the last */
#ifdef USE_COLOR_STYLE
    int style;/* precalculated value of the style */    
    char* class_name;
#endif    
        /* these will be passed to HTML_start_element*/
    HTMLElement element;
    BOOL* present;
    char** value;
    
    BOOL start; /* if true, then this starts element, otherwise - ends */    
} HT_tagspec;

extern char* HTL_tagspecs[HTL_num_lexems];
extern HT_tagspec* lexem_start[HTL_num_lexems];
extern HT_tagspec* lexem_end[HTL_num_lexems];

extern int html_src_parse_tagspec PARAMS((char* ts, HTlexem lexem,
                     BOOL checkonly,BOOL isstart));
extern void HTMLSRC_init_caches NOPARAMS;

/* these 2 vars tell what kind of transform should be appiled to tag names
  and attribute names. 0 - lowercase, 1 - as is, 2 uppercase. */
extern int tagname_transform;
extern int attrname_transform;


#endif /* ifdef USE_PSRC */


#endif /* LYPrettySrc_H */
