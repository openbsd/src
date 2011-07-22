/*
 * $LynxId: LYCharSets.h,v 1.31 2009/05/25 13:48:24 tom Exp $
 */
#ifndef LYCHARSETS_H
#define LYCHARSETS_H

#ifndef HTUTILS_H
#include <HTUtils.h>
#endif

#include <UCDefs.h>

#ifndef UCMAP_H
#include <UCMap.h>
#endif /* !UCMAP_H */

#include <HTCJK.h>

#ifdef __cplusplus
extern "C" {
#endif
    extern BOOL HTPassEightBitRaw;
    extern BOOL HTPassEightBitNum;
    extern BOOL HTPassHighCtrlRaw;
    extern BOOL HTPassHighCtrlNum;
    extern BOOLEAN LYHaveCJKCharacterSet;
    extern BOOLEAN DisplayCharsetMatchLocale;

    extern HTkcode kanji_code;

/*
 *  currently active character set (internal handler)
 */
    extern int current_char_set;
/*
 *  character set where it is safe to draw lines on boxes.
 */
    extern int linedrawing_char_set;

/*
 *  Initializer, calls initialization function for the
 *  CHARTRANS handling. - KW
 */
    extern int LYCharSetsDeclared(void);

    extern const char **LYCharSets[];
    extern const char *SevenBitApproximations[];
    extern const char **p_entity_values;
    extern const char *LYchar_set_names[];	/* Full name, not MIME */
    extern int LYlowest_eightbit[];
    extern int LYNumCharsets;
    extern LYUCcharset LYCharSet_UC[];
    extern int UCGetLYhndl_byAnyName(char *value);
    extern void HTMLSetCharacterHandling(int i);
    extern void HTMLSetUseDefaultRawMode(int i, BOOLEAN modeflag);
    extern void HTMLUseCharacterSet(int i);
    extern UCode_t HTMLGetEntityUCValue(const char *name);
    extern void Set_HTCJK(const char *inMIMEname, const char *outMIMEname);

    extern const char *HTMLGetEntityName(UCode_t code);

    UCode_t LYcp1252ToUnicode(UCode_t code);

/*
 * HTMLGetEntityName calls LYEntityNames for iso-8859-1 entity names only. 
 * This is an obsolete technique but widely used in the code.  Note that
 * unicode number in general may have several equivalent entity names because
 * of synonyms.
 */
    extern BOOL force_old_UCLYhndl_on_reload;
    extern int forced_UCLYhdnl;

#ifndef  EXP_CHARSET_CHOICE
# define ALL_CHARSETS_IN_O_MENU_SCREEN 1
#endif

#ifdef EXP_CHARSET_CHOICE
    typedef struct {
	BOOL hide_display;	/* if FALSE, show in "display-charset" menu */
	BOOL hide_assumed;	/* if FALSE, show in "assumed-charset" menu */
#ifndef ALL_CHARSETS_IN_O_MENU_SCREEN
	int assumed_idx;	/* only this field is needed */
#endif
    } charset_subset_t;

/* each element corresponds to charset in LYCharSets */
    extern charset_subset_t charset_subsets[];

/* all zeros by default - i.e., all charsets allowed */

/*
 * true if the charset choices for display charset were requested by user via
 * lynx.cfg.  It will remain FALSE if no "display_charset_choice" settings were
 * encountered in lynx.cfg
 */
    extern BOOL custom_display_charset;

/* similar to custom_display_charset */
    extern BOOL custom_assumed_doc_charset;

#ifndef ALL_CHARSETS_IN_O_MENU_SCREEN

/* this stuff is initialized after reading lynx.cfg and .lynxrc */

/*
 * These arrays map index of charset shown in menu to the index in LYCharsets[]
 */
    extern int display_charset_map[];
    extern int assumed_doc_charset_map[];

/* these arrays are NULL terminated */
    extern const char *display_charset_choices[];
    extern const char *assumed_charset_choices[];

    extern int displayed_display_charset_idx;

#endif
/* this will be called after lynx.cfg and .lynxrc are read */
    extern void init_charset_subsets(void);
#endif				/* EXP_CHARSET_CHOICE */

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
    extern int Switch_Display_Charset(int ord, enum switch_display_charset_t really);
    extern int Find_Best_Display_Charset(int ord);
    extern char *charsets_directory;
    extern char *charset_switch_rules;
    extern int switch_display_charsets;
    extern int auto_other_display_charset;
    extern int codepages[2];
    extern int real_charsets[2];	/* Non "auto-" charsets for the codepages */
#endif

#ifdef __cplusplus
}
#endif
#endif				/* LYCHARSETS_H */
