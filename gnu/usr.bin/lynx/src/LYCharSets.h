#ifndef LYCHARSETS_H
#define LYCHARSETS_H

#ifndef HTUTILS_H
#include <HTUtils.h>
#endif

#include <UCDefs.h>

#ifndef UCMAP_H
#include <UCMap.h>
#endif /* !UCMAP_H */

extern BOOL HTPassEightBitRaw;
extern BOOL HTPassEightBitNum;
extern BOOL HTPassHighCtrlRaw;
extern BOOL HTPassHighCtrlNum;
extern BOOLEAN LYHaveCJKCharacterSet;
extern BOOLEAN DisplayCharsetMatchLocale;

#include <HTCJK.h>
extern HTkcode kanji_code;

/*
 *  currently active character set (internal handler)
 */
extern int current_char_set;

/*
 *  Initializer, calls initialization function for the
 *  CHARTRANS handling. - KW
 */
extern int LYCharSetsDeclared NOPARAMS;


extern CONST char ** LYCharSets[];
extern CONST char * SevenBitApproximations[];
extern CONST char ** p_entity_values;
extern CONST char * LYchar_set_names[];  /* Full name, not MIME */
extern int LYlowest_eightbit[];
extern int LYNumCharsets;
extern LYUCcharset LYCharSet_UC[];
extern int UCGetLYhndl_byAnyName PARAMS((char *value));
extern void HTMLSetCharacterHandling PARAMS((int i));
extern void HTMLSetUseDefaultRawMode PARAMS((int i, BOOLEAN modeflag));
extern void HTMLUseCharacterSet PARAMS((int i));
extern UCode_t HTMLGetEntityUCValue PARAMS((CONST char *name));
extern void Set_HTCJK PARAMS((CONST char *inMIMEname, CONST char *outMIMEname));

extern CONST char * HTMLGetEntityName PARAMS((UCode_t code));
		/*
		** HTMLGetEntityName calls LYEntityNames for iso-8859-1 entity
		** names only.	This is an obsolete technique but widely used in
		** the code.  Note that unicode number in general may have
		** several equivalent entity names because of synonyms.
		*/
extern BOOL force_old_UCLYhndl_on_reload;
extern int forced_UCLYhdnl;

#ifndef  EXP_CHARSET_CHOICE
# define ALL_CHARSETS_IN_O_MENU_SCREEN 1
#endif

#ifdef EXP_CHARSET_CHOICE
typedef struct {
    BOOL hide_display;		/* if FALSE, show in "display-charset" menu */
    BOOL hide_assumed;		/* if FALSE, show in "assumed-charset" menu */
#ifndef ALL_CHARSETS_IN_O_MENU_SCREEN
    int assumed_idx;		/* only this field is needed */
#endif
} charset_subset_t;
/* each element corresponds to charset in LYCharSets */
extern charset_subset_t charset_subsets[];
/* all zeros by default - i.e., all charsets allowed */

extern BOOL custom_display_charset; /* whether the charset choices for display
    charset were requested by user via lynx.cfg.  It will remain FALSE if no
    "display_charset_choice" settings were encountered in lynx.cfg */
extern BOOL custom_assumed_doc_charset; /* similar to custom_display_charset */

#ifndef ALL_CHARSETS_IN_O_MENU_SCREEN

/* this stuff is initialized after reading lynx.cfg and .lynxrc */

/* these arrays maps index of charset shown in menu to the index in LYCharsets[]*/
extern int display_charset_map[];
extern int assumed_doc_charset_map[];

/* these arrays are NULL terminated */
extern CONST char* display_charset_choices[];
extern CONST char* assumed_charset_choices[];

extern int displayed_display_charset_idx;

#endif
/* this will be called after lynx.cfg and .lynxrc are read */
extern void init_charset_subsets NOPARAMS;
#endif /* EXP_CHARSET_CHOICE */

#if !defined(NO_AUTODETECT_DISPLAY_CHARSET)
#  ifdef __EMX__
#    define CAN_AUTODETECT_DISPLAY_CHARSET
#    ifdef EXP_CHARTRANS_AUTOSWITCH
#      define CAN_SWITCH_DISPLAY_CHARSET
#    endif
#  endif
#endif

#ifdef CAN_AUTODETECT_DISPLAY_CHARSET
extern int auto_display_charset;
#endif

#ifdef CAN_SWITCH_DISPLAY_CHARSET
enum switch_display_charset_t {
    SWITCH_DISPLAY_CHARSET_MAYBE,
    SWITCH_DISPLAY_CHARSET_REALLY,
    SWITCH_DISPLAY_CHARSET_RESIZE
};
extern int Switch_Display_Charset PARAMS((int ord, enum switch_display_charset_t really));
extern int Find_Best_Display_Charset PARAMS((int ord));
extern char *charsets_directory;
extern char *charset_switch_rules;
extern int switch_display_charsets;
extern int auto_other_display_charset;
extern int codepages[2];
extern int real_charsets[2];	/* Non "auto-" charsets for the codepages */
#endif

#endif /* LYCHARSETS_H */
