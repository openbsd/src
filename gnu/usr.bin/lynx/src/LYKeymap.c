#include <HTUtils.h>
#include <LYUtils.h>
#include <LYGlobalDefs.h>
#include <LYKeymap.h>
#include <LYCharSets.h>		/* for LYlowest_eightbit - kw */
#include <HTAccess.h>
#include <HTFormat.h>
#include <HTAlert.h>
#include <LYStrings.h>		/* for USE_KEYMAP stuff - kw */

#include <LYLeaks.h>

#ifdef EXP_KEYBOARD_LAYOUT
#include <jcuken_kb.h>
#include <yawerty_kb.h>
#include <rot13_kb.h>
#endif

#ifdef EXP_KEYBOARD_LAYOUT
PUBLIC int current_layout = 0;  /* Index into LYKbLayouts[]   */

PUBLIC LYKbLayout_t * LYKbLayouts[]={
	kb_layout_rot13,
	kb_layout_jcuken,
	kb_layout_yawerty
};

PUBLIC char * LYKbLayoutNames[]={
	"ROT13'd keyboard layout",
	"JCUKEN Cyrillic, for AT 101-key kbd",
	"YAWERTY Cyrillic, for DEC LK201 kbd",
        (char *) 0
};
#endif /* EXP_KEYBOARD_LAYOUT */

struct _HTStream
{
  HTStreamClass * isa;
};

/* * *  Tables mapping LynxKeyCodes to LynxActionCodes  * * */

/*
 *  Lynxkeycodes include all single-byte keys as well as codes
 *  for function keys and some special purposes.  See LYStrings.h.
 *  Extended lynxkeycode values can also contain flags for modifiers
 *  and other purposes, but here only the base values are mapped to
 *  lynxactioncodes.  They are called `keystrokes' in lynx.cfg.
 *
 *  Lynxactioncodes (confusingly, constants are named LYK_foo and
 *  typed as LYKeymapCode) specify key `functions', see LYKeymap.h.
 */

/* the character gets 1 added to it before lookup,
 * so that EOF maps to 0
 */
LYKeymap_t keymap[KEYMAP_SIZE] = {

0,
/* EOF */

LYK_DO_NOTHING,     LYK_HOME,       LYK_PREV_PAGE,     0,
/* nul */           /* ^A */        /* ^B */       /* ^C */

LYK_ABORT,          LYK_END,        LYK_NEXT_PAGE,     0,
/* ^D */            /* ^E */        /* ^F */       /* ^G */

LYK_HISTORY,    LYK_FASTFORW_LINK,  LYK_ACTIVATE,  LYK_COOKIE_JAR,
/* bs */            /* ht */        /* nl */       /* ^K */

#ifdef KANJI_CODE_OVERRIDE
LYK_CHG_KCODE,    LYK_ACTIVATE,     LYK_DOWN_TWO,      0,
/* ^L */            /* cr */        /* ^N */       /* ^O */

#else
LYK_REFRESH,      LYK_ACTIVATE,     LYK_DOWN_TWO,      0,
/* ^L */            /* cr */        /* ^N */       /* ^O */
#endif

LYK_UP_TWO,       LYK_CHG_CENTER,   LYK_RELOAD,    LYK_TO_CLIPBOARD,
/* ^P */            /* XON */       /* ^R */       /* ^S */

LYK_TRACE_TOGGLE,       0,        LYK_SWITCH_DTD,  LYK_REFRESH,
/* ^T */            /* ^U */        /* ^V */       /* ^W */

0,                      0,              0,             0,
/* ^X */            /* ^Y */        /* ^Z */       /* ESC */

0,                      0,              0,             0,
/* ^\ */            /* ^] */        /* ^^ */       /* ^_ */

LYK_NEXT_PAGE,       LYK_SHELL,  LYK_SOFT_DQUOTES,  LYK_TOOLBAR,
/* sp */             /* ! */         /* " */        /* # */

LYK_LAST_LINK,          0,              0,          LYK_HISTORICAL,
/* $ */              /* % */         /* & */        /* ' */

LYK_UP_HALF,      LYK_DOWN_HALF, LYK_IMAGE_TOGGLE,  LYK_NEXT_PAGE,
/* ( */              /* ) */         /* * */        /* + */

LYK_EXTERN_PAGE,  LYK_PREV_PAGE, LYK_EXTERN_LINK,   LYK_WHEREIS,
/* , */              /* - */         /* . */        /* / */

LYK_F_LINK_NUM,      LYK_1,          LYK_2,         LYK_3,
/* 0 */              /* 1 */         /* 2 */        /* 3 */

LYK_4,               LYK_5,          LYK_6,         LYK_7,
/* 4 */              /* 5 */         /* 6 */        /* 7 */

LYK_8,               LYK_9,         LYK_COMMAND,    LYK_TRACE_LOG,
/* 8 */              /* 9 */         /* : */        /* ; */

LYK_UP_LINK,         LYK_INFO,     LYK_DOWN_LINK,   LYK_HELP,
/* < */              /* = */         /* > */        /* ? */

#ifndef SUPPORT_CHDIR
LYK_RAW_TOGGLE,      LYK_ADDRLIST, LYK_PREV_PAGE,   LYK_COMMENT,
/* @ */              /* A */         /* B */        /* C */
#else
LYK_RAW_TOGGLE,      LYK_ADDRLIST, LYK_PREV_PAGE,   LYK_CHDIR,
/* @ */              /* A */         /* B */        /* C */
#endif

LYK_DOWNLOAD,        LYK_ELGOTO,  LYK_DIRED_MENU,   LYK_ECGOTO,
/* D */              /* E */         /* F */        /* G */

LYK_HELP,            LYK_INDEX,      LYK_JUMP,      LYK_KEYMAP,
/* H */              /* I */         /* J */        /* K */

LYK_LIST,          LYK_MAIN_MENU,    LYK_PREV,      LYK_OPTIONS,
/* L */              /* M */         /* N */        /* O */

LYK_PRINT,          LYK_ABORT,    LYK_DEL_BOOKMARK, LYK_INDEX_SEARCH,
/* P */              /* Q */         /* R */        /* S */

LYK_TAG_LINK,      LYK_PREV_DOC,    LYK_VLINKS,         0,
/* T */              /* U */         /* V */        /* W */

LYK_NOCACHE,            0,        LYK_INTERRUPT,    LYK_INLINE_TOGGLE,
/* X */              /* Y */         /* Z */        /* [ */

LYK_SOURCE,          LYK_HEAD,    LYK_FIRST_LINK,   LYK_CLEAR_AUTH,
/* \ */              /* ] */         /* ^ */        /* _ */

LYK_MINIMAL,   LYK_ADD_BOOKMARK,  LYK_PREV_PAGE,    LYK_COMMENT,
/* ` */              /* a */         /* b */        /* c */

LYK_DOWNLOAD,        LYK_EDIT,    LYK_DIRED_MENU,   LYK_GOTO,
/* d */              /* e */         /* f */        /* g */

LYK_HELP,            LYK_INDEX,      LYK_JUMP,      LYK_KEYMAP,
/* h */              /* i */         /* j */        /* k */

LYK_LIST,         LYK_MAIN_MENU,     LYK_NEXT,      LYK_OPTIONS,
/* l */              /* m */         /* n */        /* o */

LYK_PRINT,           LYK_QUIT,    LYK_DEL_BOOKMARK, LYK_INDEX_SEARCH,
/* p */              /* q */         /* r */        /* s */

LYK_TAG_LINK,     LYK_PREV_DOC,   LYK_VIEW_BOOKMARK,   0,
/* t */              /* u */         /* v */        /* w */

LYK_NOCACHE,            0,          LYK_INTERRUPT, LYK_SHIFT_LEFT,
/* x */              /* y */          /* z */       /* { */

LYK_LINEWRAP_TOGGLE, LYK_SHIFT_RIGHT, LYK_NESTED_TABLES, LYK_HISTORY,
/* | */               /* } */         /* ~ */       /* del */


/* 80..9F (illegal ISO-8859-1) 8-bit characters. */
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,

/* A0..FF (permissible ISO-8859-1) 8-bit characters. */
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,

/* 100..10F function key definitions in LYStrings.h */
LYK_PREV_LINK,    LYK_NEXT_LINK,    LYK_ACTIVATE,   LYK_PREV_DOC,
/* UPARROW */     /* DNARROW */     /* RTARROW */   /* LTARROW */

LYK_NEXT_PAGE,    LYK_PREV_PAGE,    LYK_HOME,       LYK_END,
/* PGDOWN */      /* PGUP */        /* HOME */      /* END */

#if (defined(_WINDOWS) || defined(__DJGPP__))

LYK_DWIMHELP,          0,              0,             0,
/* F1*/
#else

LYK_DWIMHELP,     LYK_ACTIVATE,     LYK_HOME,       LYK_END,
/* F1*/ 	  /* Do key */      /* Find key */  /* Select key */

#endif /* _WINDOWS || __DJGPP__ */

LYK_UP_TWO,       LYK_DOWN_TWO,     LYK_DO_NOTHING, LYK_FASTBACKW_LINK,
/* Insert key */  /* Remove key */  /* DO_NOTHING*/ /* Back tab */

/* 110..18F */

   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,             LYK_DO_NOTHING,      0,             0,
               /* 0x11d: MOUSE_KEY */
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
#ifdef DJGPP_KEYHANDLER
   0,                  LYK_ABORT,      0,             0,
                       /* ALT_X */
#else
   0,                  0,              0,             0,
#endif /* DJGPP_KEYHANDLER */
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
/* 190..20F */

   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
#if (defined(_WINDOWS) || defined(__DJGPP__) || defined(__CYGWIN__)) && !defined(USE_SLANG) /* PDCurses */
   LYK_ABORT,          0,              0,             0,
   /* ALT_X */
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              LYK_WHEREIS,   0,
                                       /* KP_SLASH */
   0,                  0,              0,           LYK_IMAGE_TOGGLE,
                                                    /* KP_* */
   LYK_PREV_PAGE,      LYK_NEXT_PAGE,  0,             0,
   /* KP_- */          /* KP_+ */
#else
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
#endif /* (_WINDOWS || __DJGPP__ || __CYGWIN__) && !USE_SLANG */
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
/* 210..28F */

   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   /* 290...293 */
   LYK_CHANGE_LINK,    0,              0,             0,
};

#if defined(DIRED_SUPPORT) && defined(OK_OVERRIDE)
/*
 * This table is used to override the standard keyboard assignments
 * when lynx_edit_mode is in effect and keyboard overrides have been
 * allowed at compile time.
 */

LYKeymap_t key_override[KEYMAP_SIZE] = {

    0,
/* EOF */

    0,                  0,              0,            0,
/* nul */           /* ^A */        /* ^B */      /* ^C */

    0,                  0,              0,            0,
/* ^D */            /* ^E */        /* ^F */      /* ^G */

    0,                  0,              0,            0,
/* bs */            /* ht */        /* nl */      /* ^K */

    0,                  0,              0,            0,
/* ^L */            /* cr */        /* ^N */      /* ^O */

    0,                  0,              0,            0,
/* ^P */            /* XON */       /* ^R */      /* XOFF */

    0,            LYK_PREV_DOC,         0,            0,
/* ^T */            /* ^U */        /* ^V */      /* ^W */

    0,                  0,              0,            0,
/* ^X */            /* ^Y */        /* ^Z */      /* ESC */

    0,                  0,              0,            0,
/* ^\ */            /* ^] */        /* ^^ */      /* ^_ */

    0,                 0,              0,            0,
/* sp */            /* ! */         /* " */       /* # */

   0,                  0,              0,            0,
/* $ */             /* % */         /* & */       /* ' */

    0,                 0,              0,            0,
/* ( */             /* ) */         /* * */       /* + */

    0,                 0,         LYK_TAG_LINK,      0,
/* , */             /* - */         /* . */       /* / */

   0,                  0,              0,            0,
/* 0 */             /* 1 */         /* 2 */       /* 3 */

   0,                  0,              0,            0,
/* 4 */             /* 5 */         /* 6 */       /* 7 */

   0,                  0,              0,             0,
/* 8 */             /* 9 */         /* : */        /* ; */

   0,                  0,              0,             0,
/* < */             /* = */         /* > */        /* ? */
#ifndef SUPPORT_CHDIR
   0,                  0,              0,         LYK_CREATE,
/* @ */             /* A */         /* B */        /* C */
#else
   0,                  0,              0,         LYK_CHDIR,
/* @ */             /* A */         /* B */        /* C */
#endif

   0,                  0,        LYK_DIRED_MENU,       0,
/* D */             /* E */         /* F */        /* G */

   0,                  0,              0,             0,
/* H */             /* I */         /* J */        /* K */

   0,             LYK_MODIFY,          0,             0,
/* L */             /* M */         /* N */        /* O */

   0,                  0,         LYK_REMOVE,         0,
/* P */             /* Q */         /* R */        /* S */

LYK_TAG_LINK,     LYK_UPLOAD,          0,             0,
/* T */             /* U */         /* V */        /* W */

   0,                  0,              0,             0,
/* X */             /* Y */         /* Z */        /* [ */

   0,                  0,              0,             0,
/* \ */             /* ] */         /* ^ */        /* _ */

0,                     0,              0,         LYK_CREATE,
/* ` */             /* a */         /* b */        /* c */

   0,                  0,       LYK_DIRED_MENU,       0,
/* d */             /* e */         /* f */        /* g */

   0,                  0,              0,             0,
/* h */             /* i */         /* j */        /* k */

0,                LYK_MODIFY,          0,             0,
/* l */             /* m */         /* n */        /* o */

   0,                  0,          LYK_REMOVE,        0,
/* p */             /* q */         /* r */        /* s */

LYK_TAG_LINK,      LYK_UPLOAD,         0,             0,
/* t */             /* u */         /* v */         /* w */

   0,                  0,               0,            0,
/* x */             /* y */          /* z */       /* { */

   0,                   0,             0,              0,
/* | */              /* } */         /* ~ */       /* del */

/* 80..9F (illegal ISO-8859-1) 8-bit characters. */
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,

/* A0..FF (permissible ISO-8859-1) 8-bit characters. */
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,

/* 100..10F function key definitions in LYStrings.h */
   0,                   0,             0,              0,
/* UPARROW */     /* DNARROW */     /* RTARROW */   /* LTARROW */

   0,                  0,              0,              0,
/* PGDOWN */      /* PGUP */        /* HOME */      /* END */

   0,                  0,              0,              0,
/* F1*/ 	  /* Do key */      /* Find key */  /* Select key */

   0,                  0,           LYK_DO_NOTHING,    0,
/* Insert key */  /* Remove key */  /* DO_NOTHING */ /* Back tab */

/* 110..18F */

   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
/* 190..20F */

   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
/* 210..28F */

   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   /* 290...293 */
   0,                  0,              0,             0,
};
#endif /* DIRED_SUPPORT && OK_OVERRIDE */

#define DATA(code, name, doc) { code, name, doc }
/* The order of this array must match the LYKeymapCode enum in LYKeymap.h */
PRIVATE Kcmd revmap[] = {
    DATA(
	LYK_UNKNOWN, "UNMAPPED",
	NULL ),
    DATA(
	LYK_COMMAND, "COMMAND",
	"prompt for, execute a command" ),
    DATA(
	LYK_1, "1",
	NULL ),
    DATA(
	LYK_2, "2",
	NULL ),
    DATA(
	LYK_3, "3",
	NULL ),
    DATA(
	LYK_4, "4",
	NULL ),
    DATA(
	LYK_5, "5",
	NULL ),
    DATA(
	LYK_6, "6",
	NULL ),
    DATA(
	LYK_7, "7",
	NULL ),
    DATA(
	LYK_8, "8",
	NULL ),
    DATA(
	LYK_9, "9",
	NULL ),
    DATA(
	LYK_SOURCE, "SOURCE",
	"toggle source/presentation for current document" ),
    DATA(
	LYK_RELOAD, "RELOAD",
	"reload the current document" ),
    DATA(
	LYK_QUIT, "QUIT",
	"quit the browser" ),
    DATA(
	LYK_ABORT, "ABORT",
	"quit the browser unconditionally" ),
    DATA(
	LYK_NEXT_PAGE, "NEXT_PAGE",
	"view the next page of the document" ),
    DATA(
	LYK_PREV_PAGE, "PREV_PAGE",
	"view the previous page of the document" ),
    DATA(
	LYK_UP_TWO, "UP_TWO",
	"go back two lines in the document" ),
    DATA(
	LYK_DOWN_TWO, "DOWN_TWO",
	"go forward two lines in the document" ),
    DATA(
	LYK_UP_HALF, "UP_HALF",
	"go back half a page in the document" ),
    DATA(
	LYK_DOWN_HALF, "DOWN_HALF",
	"go forward half a page in the document" ),
    DATA(
	LYK_REFRESH, "REFRESH",
	"refresh the screen to clear garbled text" ),
    DATA(
	LYK_HOME, "HOME",
	"go to the beginning of the current document" ),
    DATA(
	LYK_END, "END",
	"go to the end of the current document" ),
    DATA(
	LYK_FIRST_LINK, "FIRST_LINK",
	"make the first link on the line current" ),
    DATA(
	LYK_LAST_LINK, "LAST_LINK",
	"make the last link on the line current" ),
    DATA(
	LYK_PREV_LINK, "PREV_LINK",
	"make the previous link current" ),
    DATA(
	LYK_NEXT_LINK, "NEXT_LINK",
	"make the next link current" ),
    DATA(
	LYK_LPOS_PREV_LINK, "LPOS_PREV_LINK",
	"make previous link current, same column for input" ),
    DATA(
	LYK_LPOS_NEXT_LINK, "LPOS_NEXT_LINK",
	"make next link current, same column for input" ),
    DATA(
	LYK_FASTBACKW_LINK, "FASTBACKW_LINK",
	"previous link or text area, only stops on links" ),
    DATA(
	LYK_FASTFORW_LINK, "FASTFORW_LINK",
	"next link or text area, only stops on links" ),
    DATA(
	LYK_UP_LINK, "UP_LINK",
	"move up the page to a previous link" ),
    DATA(
	LYK_DOWN_LINK, "DOWN_LINK",
	"move down the page to another link" ),
    DATA(
	LYK_RIGHT_LINK, "RIGHT_LINK",
	"move right to another link" ),
    DATA(
	LYK_LEFT_LINK, "LEFT_LINK",
	"move left to a previous link" ),
    DATA(
	LYK_HISTORY, "HISTORY",
	"display stack of currently-suspended documents" ),
    DATA(
	LYK_PREV_DOC, "PREV_DOC",
	"go back to the previous document" ),
    DATA(
	LYK_NEXT_DOC, "NEXT_DOC",
	"undo going back to the previous document" ),
    DATA(
	LYK_ACTIVATE, "ACTIVATE",
	"go to the document given by the current link" ),
    DATA(
	LYK_SUBMIT, "MOUSE_SUBMIT",
	"DO NOT MAP:  follow current link, submit" ),
    DATA(
	LYK_GOTO, "GOTO",
	"go to a document given as a URL" ),
    DATA(
	LYK_ECGOTO, "ECGOTO",
	"edit the current document's URL and go to it" ),
    DATA(
	LYK_HELP, "HELP",
	"display help on using the browser" ),
    DATA(
	LYK_DWIMHELP, "DWIMHELP",
	"display help page that may depend on context" ),
    DATA(
	LYK_INDEX, "INDEX",
	"display an index of potentially useful documents" ),
    DATA(
	LYK_NOCACHE, "NOCACHE",
	"force submission of form or link with no-cache" ),
    DATA(
	LYK_INTERRUPT, "INTERRUPT",
	"interrupt network connection or transmission" ),
    DATA(
	LYK_MAIN_MENU, "MAIN_MENU",
	"return to the first screen (home page)" ),
    DATA(
	LYK_OPTIONS, "OPTIONS",
	"display and change option settings" ),
    DATA(
	LYK_INDEX_SEARCH, "INDEX_SEARCH",
	"allow searching of an index" ),
    DATA(
	LYK_WHEREIS, "WHEREIS",
	"search within the current document" ),
    DATA(
	LYK_PREV, "PREV",
	"search for the previous occurence" ),
    DATA(
	LYK_NEXT, "NEXT",
	"search for the next occurence" ),
    DATA(
	LYK_COMMENT, "COMMENT",
	"send a comment to the author of the current document" ),
    DATA(
	LYK_EDIT, "EDIT",
	"edit the current document or a form's textarea" ),
    DATA(
	LYK_INFO, "INFO",
	"display information on the current document and link" ),
    DATA(
	LYK_PRINT, "PRINT",
	"display choices for printing the current document" ),
    DATA(
	LYK_ADD_BOOKMARK, "ADD_BOOKMARK",
	"add to your personal bookmark list" ),
    DATA(
	LYK_DEL_BOOKMARK, "DEL_BOOKMARK",
	"delete from your personal bookmark list" ),
    DATA(
	LYK_VIEW_BOOKMARK, "VIEW_BOOKMARK",
	"view your personal bookmark list" ),
    DATA(
	LYK_VLINKS, "VLINKS",
	"list links visited during the current Lynx session" ),
    DATA(
	LYK_SHELL, "SHELL",
	"escape from the browser to the system" ),
    DATA(
	LYK_DOWNLOAD, "DOWNLOAD",
	"download the current link to your computer" ),
    DATA(
	LYK_TRACE_TOGGLE, "TRACE_TOGGLE",
	"toggle tracing of browser operations" ),
    DATA(
	LYK_TRACE_LOG, "TRACE_LOG",
	"view trace log if started in the current session" ),
    DATA(
	LYK_IMAGE_TOGGLE, "IMAGE_TOGGLE",
	"toggle handling of all images as links" ),
    DATA(
	LYK_INLINE_TOGGLE, "INLINE_TOGGLE",
	"toggle pseudo-ALTs for inlines with no ALT string" ),
    DATA(
	LYK_HEAD, "HEAD",
	"send a HEAD request for the current document or link" ),
    DATA(
	LYK_DO_NOTHING, "DO_NOTHING",
	NULL ),
    DATA(
	LYK_TOGGLE_HELP, "TOGGLE_HELP",
	"show other commands in the novice help menu" ),
    DATA(
	LYK_JUMP, "JUMP",
	"go directly to a target document or action" ),
    DATA(
	LYK_KEYMAP, "KEYMAP",
	"display the current key map" ),
    DATA(
	LYK_LIST, "LIST",
	"list the references (links) in the current document" ),
    DATA(
	LYK_TOOLBAR, "TOOLBAR",
	"go to Toolbar or Banner in the current document" ),
    DATA(
	LYK_HISTORICAL, "HISTORICAL",
	"toggle historical vs.  valid/minimal comment parsing" ),
    DATA(
	LYK_MINIMAL, "MINIMAL",
	"toggle minimal vs.  valid comment parsing" ),
    DATA(
	LYK_SOFT_DQUOTES, "SOFT_DQUOTES",
	"toggle valid vs.  soft double-quote parsing" ),
    DATA(
	LYK_RAW_TOGGLE, "RAW_TOGGLE",
	"toggle raw 8-bit translations or CJK mode ON or OFF" ),
    DATA(
	LYK_COOKIE_JAR, "COOKIE_JAR",
	"examine the Cookie Jar" ),
    DATA(
	LYK_F_LINK_NUM, "F_LINK_NUM",
	"invoke the 'Follow link (or page) number:' prompt" ),
    DATA(
	LYK_CLEAR_AUTH, "CLEAR_AUTH",
	"clear all authorization info for this session" ),
    DATA(
	LYK_SWITCH_DTD, "SWITCH_DTD",
	"switch between two ways of parsing HTML" ),
    DATA(
	LYK_ELGOTO, "ELGOTO",
	"edit the current link's URL or ACTION and go to it" ),
    DATA(
	LYK_CHANGE_LINK, "CHANGE_LINK",
	"force reset of the current link on the page" ),
    DATA(
	LYK_DWIMEDIT, "DWIMEDIT",
	"use external editor for context-dependent purpose" ),
    DATA(
	LYK_EDIT_TEXTAREA, "EDITTEXTAREA",
	"use an external editor to edit a form's textarea" ),
    DATA(
	LYK_GROW_TEXTAREA, "GROWTEXTAREA",
	"add 5 new blank lines to the bottom of a textarea" ),
    DATA(
	LYK_INSERT_FILE, "INSERTFILE",
	"insert file into a textarea (just above cursorline)" ),
#ifdef EXP_ADDRLIST_PAGE
    DATA(
	LYK_ADDRLIST, "ADDRLIST",
	"like LIST command, but always shows the links' URLs" ),
#endif
#ifdef USE_EXTERNALS
    DATA(
	LYK_EXTERN_LINK, "EXTERN_LINK",
	"run external program with current link" ),
    DATA(
	LYK_EXTERN_PAGE, "EXTERN_PAGE",
	"run external program with current page" ),
#endif
#ifdef VMS
    DATA(
	LYK_DIRED_MENU, "DIRED_MENU",
	"invoke File/Directory Manager, if available" ),
#else
#ifdef DIRED_SUPPORT
    DATA(
	LYK_DIRED_MENU, "DIRED_MENU",
	"display a full menu of file operations" ),
    DATA(
	LYK_CREATE, "CREATE",
	"create a new file or directory" ),
    DATA(
	LYK_REMOVE, "REMOVE",
	"remove a file or directory" ),
    DATA(
	LYK_MODIFY, "MODIFY",
	"modify the name or location of a file or directory" ),
    DATA(
	LYK_TAG_LINK, "TAG_LINK",
	"tag a file or directory for later action" ),
    DATA(
	LYK_UPLOAD, "UPLOAD",
	"upload from your computer to the current directory" ),
    DATA(
	LYK_INSTALL, "INSTALL",
	"install file or tagged files into a system area" ),
#endif /* DIRED_SUPPORT */
    DATA(
	LYK_CHG_CENTER, "CHANGE_CENTER",
	"toggle center alignment in HTML TABLE" ),
#ifdef KANJI_CODE_OVERRIDE
    DATA(
	LYK_CHG_KCODE, "CHANGE_KCODE",
	"Change Kanji code" ),
#endif
#endif /* VMS */
#ifdef SUPPORT_CHDIR
    DATA(
	LYK_CHDIR, "CHDIR",
	"change current directory" ),
#endif
#ifdef USE_CURSES_PADS
    DATA(
	LYK_SHIFT_LEFT, "SHIFT_LEFT",
	"shift the screen left" ),
    DATA(
	LYK_SHIFT_RIGHT, "SHIFT_RIGHT",
	"shift the screen right" ),
    DATA(
	LYK_LINEWRAP_TOGGLE, "LINEWRAP_TOGGLE",
	"toggle linewrap on/off" ),
#endif
#ifdef CAN_CUT_AND_PASTE
    DATA(
	LYK_PASTE_URL, "PASTE_URL",
	"Goto the URL in the clipboard" ),
    DATA(
	LYK_TO_CLIPBOARD, "TO_CLIPBOARD",
	"link's URL to Clip Board" ),
#endif
#ifdef EXP_NESTED_TABLES
    DATA(
	LYK_NESTED_TABLES, "NESTED_TABLES",
	"toggle nested-table parsing on/off" ),
#endif
    DATA(
	LYK_UNKNOWN, NULL,
	"" )
};
#undef DATA

PRIVATE CONST struct {
    int key;
    CONST char *name;
} named_keys[] = {
    { '\t',		"<tab>" },
    { '\r',		"<return>" },
    { CH_ESC,		"ESC" },
    { ' ',		"<space>" },
    { '<',		"<" },
    { '>',		">" },
    { CH_DEL,		"<delete>" },
    { UPARROW,		"Up Arrow" },
    { DNARROW,		"Down Arrow" },
    { RTARROW,		"Right Arrow" },
    { LTARROW,		"Left Arrow" },
    { PGDOWN,		"Page Down" },
    { PGUP,		"Page Up" },
    { HOME,		"Home" },
    { END_KEY,		"End" },
    { F1,		"F1" },
    { DO_KEY,		"Do key" },
    { FIND_KEY,		"Find key" },
    { SELECT_KEY,	"Select key" },
    { INSERT_KEY,	"Insert key" },
    { REMOVE_KEY,	"Remove key" },
    { DO_NOTHING,	"(DO_NOTHING)" },
    { BACKTAB_KEY,	"Back Tab" },
    { MOUSE_KEY,	"mouse pseudo key" },
};

struct emap {
	CONST char *name;
	CONST int   code;
	CONST char *descr;
};

PRIVATE struct emap ekmap[] = {
  {"NOP",	LYE_NOP,	"Do Nothing"},
  {"CHAR",	LYE_CHAR,	"Insert printable char"},
  {"ENTER",	LYE_ENTER,	"Input complete, return char/lynxkeycode"},
  {"TAB",	LYE_TAB,	"Input complete, return TAB"},
  {"STOP",	LYE_STOP,	"Input deactivated"},
  {"ABORT",	LYE_ABORT,	"Input cancelled"},

  {"PASS",	LYE_FORM_PASS,  "In fields: input complete, or Do Nothing"},

  {"DELN",	LYE_DELN,	"Delete next/curr char"},
  {"DELP",	LYE_DELP,	"Delete prev      char"},
  {"DELNW",	LYE_DELNW,	"Delete next word"},
  {"DELPW",	LYE_DELPW,	"Delete prev word"},

  {"ERASE",	LYE_ERASE,	"Erase the line"},

  {"BOL",	LYE_BOL,	"Go to begin of line"},
  {"EOL",	LYE_EOL,	"Go to end   of line"},
  {"FORW",	LYE_FORW,	"Cursor forwards"},
  {"FORW_RL",	LYE_FORW_RL,	"Cursor forwards or right link"},
  {"BACK",	LYE_BACK,	"Cursor backwards"},
  {"BACK_LL",	LYE_BACK_LL,	"Cursor backwards or left link"},
  {"FORWW",	LYE_FORWW,	"Word forward"},
  {"BACKW",	LYE_BACKW,	"Word back"},

  {"LOWER",	LYE_LOWER,	"Lower case the line"},
  {"UPPER",	LYE_UPPER,	"Upper case the line"},

  {"LKCMD",	LYE_LKCMD,	"Invoke command prompt"},

  {"AIX",	LYE_AIX,	"Hex 97"},

  {"DELBL",	LYE_DELBL,	"Delete back to BOL"},
  {"DELEL",	LYE_DELEL,	"Delete thru EOL"},

  {"SWMAP",	LYE_SWMAP,	"Switch input keymap"},

  {"TPOS",	LYE_TPOS,	"Transpose characters"},

  {"SETM1",	LYE_SETM1,	"Set modifier 1 flag"},
  {"SETM2",	LYE_SETM2,	"Set modifier 2 flag"},
  {"UNMOD",	LYE_UNMOD,	"Fall back to no-modifier command"},

  {"C1CHAR",	LYE_C1CHAR,	"Insert C1 char if printable"},

  {"SETMARK",	LYE_SETMARK,	"emacs-like set-mark-command"},
  {"XPMARK",	LYE_XPMARK,	"emacs-like exchange-point-and-mark"},
  {"KILLREG",	LYE_KILLREG,	"emacs-like kill-region"},
  {"YANK",	LYE_YANK,	"emacs-like yank"},
#ifdef CAN_CUT_AND_PASTE
  {"PASTE",	LYE_PASTE,	"ClipBoard to Lynx"},
#endif
};

/*
 * Build a list of Lynx's commands, for use in the tab-completion in LYgetstr.
 */
PUBLIC HTList *LYcommandList NOARGS
{
    static HTList *myList = NULL;

    if (myList == NULL) {
	unsigned j;
	myList = HTList_new();
	for (j = 0; revmap[j].name != 0; j++) {
	    if (revmap[j].doc != 0)
		HTList_addObject(myList, (char *)revmap[j].name);
	}
    }
    return myList;
}

/*
 * Find the given keycode.
 */
PUBLIC Kcmd * LYKeycodeToKcmd ARGS1(
	LYKeymapCode,	code)
{
    unsigned j;
    Kcmd *result = 0;

    if (code > LYK_UNKNOWN) {
	for (j = 0; revmap[j].name != 0; j++) {
	    if (revmap[j].code == code) {
		result = revmap + j;
		break;
	    }
	}
    }
    return result;
}

/*
 * Find the given command-name, accepting an abbreviation if it is unique.
 */
PUBLIC Kcmd * LYStringToKcmd ARGS1(
	CONST char *,	name)
{
    unsigned need = strlen(name);
    unsigned j;
    BOOL exact = FALSE;
    Kcmd *result = 0;
    Kcmd *maybe = 0;

    if (name != 0 && *name != 0) {
	for (j = 0; revmap[j].name != 0; j++) {
	    if (!strcasecomp(revmap[j].name, name)) {
		result = revmap + j;
		break;
	    } else if (!exact
		&& !strncasecomp(revmap[j].name, name, need)) {
		if (maybe == 0) {
		    maybe = revmap + j;
		} else {
		    if (revmap[j].name[need] != 0
		     && maybe->name[need] != 0) {
			maybe = 0;
			exact = TRUE;
		    }
		}
	    }
	}
    }
    return (result != 0) ? result : maybe;
}

PUBLIC char *LYKeycodeToString ARGS2 (
	int,		c,
	BOOLEAN,	upper8)
{
    static char buf[30];
    unsigned n;
    BOOLEAN named = FALSE;

    for (n = 0; n < TABLESIZE(named_keys); n++) {
	if (named_keys[n].key == c) {
	    named = TRUE;
	    strcpy(buf, named_keys[n].name);
	    break;
	}
    }

    if (!named) {
	if (c > ' '
	 && c < 0177)
	    sprintf(buf, "%c", c);
	else if (upper8
	 && c > ' '
	 && c <= 0377
	 && c <= LYlowest_eightbit[current_char_set])
	    sprintf(buf, "%c", c);
	else if (c < ' ')
	    sprintf(buf, "^%c", c|0100);
	else if (c >= 0400)
	    sprintf(buf, "key-0x%x", c);
	else
	    sprintf(buf, "0x%x", c);
    }
    return buf;
}

PUBLIC int LYStringToKeycode ARGS1 (
	char *,		src)
{
    unsigned n;
    int key = -1;
    int len = strlen(src);

    if (len == 1) {
	key = *src;
    } else if (len == 2 && *src == '^') {
	key = src[1] & 0x1f;
    } else if (len > 2 && !strncasecomp(src, "0x", 2)) {
	char *dst = 0;
	key = strtol(src, &dst, 0);
	if (isEmpty(dst))
	    key = -1;
    } else if (len > 6 && !strncasecomp(src, "key-", 4)) {
	char *dst = 0;
	key = strtol(src + 4, &dst, 0);
	if (isEmpty(dst))
	    key = -1;
    }
    if (key < 0) {
	for (n = 0; n < TABLESIZE(named_keys); n++) {
	    if (!strcasecomp(named_keys[n].name, src)) {
		key = named_keys[n].key;
		break;
	    }
	}
    }
    return key;
}

#define PRETTY_LEN 11

PRIVATE char *pretty_html ARGS1 (int, c)
{
    char *src = LYKeycodeToString(c, TRUE);

    if (src != 0) {
	static CONST struct {
	    int	code;
	    CONST char *name;
	} table[] = {
	    { '<',	"&lt;" },
	    { '>',	"&gt;" },
	    { '"',	"&quot;" },
	    { '&',	"&amp;" }
	};

	static char buf[30];
	char *dst = buf;
	int adj = 0;
	unsigned n;
	BOOLEAN found;

	while ((c = *src++) != 0) {
	    found = FALSE;
	    for (n = 0; n < TABLESIZE(table); n++) {
		if (c == table[n].code) {
		    found = TRUE;
		    strcpy(dst, table[n].name);
		    adj += strlen(dst) - 1;
		    dst += strlen(dst);
		    break;
		}
	    }
	    if (!found) {
		*dst++ = (char) c;
	    }
	}
	adj -= (dst - buf) - PRETTY_LEN;
	while (adj-- > 0)
	    *dst++ = ' ';
	*dst = 0;
	return buf;
    }

    return 0;
}

PRIVATE char * format_binding ARGS2(
	LYKeymap_t *,	table,
	int,		i)
{
    LYKeymap_t the_key = table[i];
    char *buf = 0;
    char *formatted;
    Kcmd *rmap = LYKeycodeToKcmd(the_key);

    if (rmap != 0
     && rmap->name != 0
     && rmap->doc != 0
     && (formatted = pretty_html(i-1)) != 0) {
	HTSprintf0(&buf, "%-*s %-13s %s\n",
		   PRETTY_LEN, formatted,
		   rmap->name,
		   rmap->doc);
	return buf;
    }
    return 0;
}

/* if both is true, produce an additional line for the corresponding
   uppercase key if its binding is different. - kw */
PRIVATE void print_binding ARGS3(
    HTStream *,	target,
    int,	i,
    BOOLEAN, 	both)
{
    char *buf;
    LYKeymapCode lac1 = LYK_UNKNOWN; /* 0 */

#if defined(DIRED_SUPPORT) && defined(OK_OVERRIDE)
    if (prev_lynx_edit_mode && !no_dired_support &&
	(lac1 = key_override[i]) != LYK_UNKNOWN) {
	if ((buf = format_binding(key_override, i)) != 0) {
	    (*target->isa->put_block)(target, buf, strlen(buf));
	    FREE(buf);
	}
    } else
#endif /* DIRED_SUPPORT && OK_OVERRIDE */
    if ((buf = format_binding(keymap, i)) != 0) {
	lac1 = keymap[i];
	(*target->isa->put_block)(target, buf, strlen(buf));
	FREE(buf);
    }

    if (!both)
	return;
    i -= ' ';			/* corresponding uppercase key */

#if defined(DIRED_SUPPORT) && defined(OK_OVERRIDE)
    if (prev_lynx_edit_mode && !no_dired_support && key_override[i]) {
	if (key_override[i] != lac1 &&
	    (buf = format_binding(key_override, i)) != 0) {
	    (*target->isa->put_block)(target, buf, strlen(buf));
	    FREE(buf);
	}
    } else
#endif /* DIRED_SUPPORT && OK_OVERRIDE */
    if (keymap[i] != lac1 && (buf = format_binding(keymap, i)) != 0) {
	(*target->isa->put_block)(target, buf, strlen(buf));
	FREE(buf);
    }
}

/*
 *  Return lynxactioncode whose name is the string func.
 *  returns -1 if not found. - kw
 */
PUBLIC int lacname_to_lac ARGS1(
	CONST char *,	func)
{
    Kcmd *mp = LYStringToKcmd(func);

    return (mp != 0) ? (int) mp->code : -1;
}

/*
 *  Return editactioncode whose name is the string func.
 *  func must be present in the ekmap table.
 *  returns -1 if not found. - kw
 */
PUBLIC int lecname_to_lec ARGS1(
	CONST char *,	func)
{
    int i;
    struct emap *mp;

    if (func != NULL && *func != '\0') {
	for (i = 0, mp = ekmap; (*mp).name != NULL; mp++, i++) {
	    if (strcmp((*mp).name, func) == 0) {
		return (*mp).code;
	    }
	}
    }
    return (-1);
}

/*
 *  Return lynxkeycode represented by string src.
 *  returns -1 if not valid.
 *  This is simpler than what map_string_to_keysym() does for
 *  USE_KEYMAP, but compatible with revmap() used for processing
 *  KEYMAP options in the configuration file. - kw
 */
PUBLIC int lkcstring_to_lkc ARGS1(
	CONST char *,	src)
{
    int c = -1;

    if (strlen(src) == 1)
	c = *src;
    else if (strlen(src) == 2 && *src == '^')
	c = src[1] & 037;
    else if (strlen(src) >= 2 && isdigit(UCH(*src))) {
	if (sscanf(src, "%i", &c) != 1)
	    return (-1);
#ifdef USE_KEYMAPS
    } else {
	map_string_to_keysym(src, &c);
#ifndef USE_SLANG
	if (c >= 0) {
	    if ((c&LKC_MASK) > 255 && !(c & LKC_ISLKC))
		return (-1);	/* Don't accept untranslated curses KEY_* */
	    else
		c &= ~LKC_ISLKC;
	}
#endif
#endif
    }
    if (c == CH_ESC)
	escape_bound = 1;
    if (c < -1)
	return (-1);
    else
	return c;
}

PRIVATE int LYLoadKeymap ARGS4 (
	CONST char *, 		arg GCC_UNUSED,
	HTParentAnchor *,	anAnchor,
	HTFormat,		format_out,
	HTStream*,		sink)
{
    HTFormat format_in = WWW_HTML;
    HTStream *target;
    char *buf = 0;
    int i;

    /*
     *  Set up the stream. - FM
     */
    target = HTStreamStack(format_in, format_out, sink, anAnchor);
    if (!target || target == NULL) {
	HTSprintf0(&buf, CANNOT_CONVERT_I_TO_O,
			 HTAtom_name(format_in), HTAtom_name(format_out));
	HTAlert(buf);
	FREE(buf);
	return(HT_NOT_LOADED);
    }
    anAnchor->no_cache = TRUE;

#define PUTS(buf)    (*target->isa->put_block)(target, buf, strlen(buf))

    HTSprintf0(&buf, "<html>\n<head>\n<title>%s</title>\n</head>\n<body>\n",
		     CURRENT_KEYMAP_TITLE);
    PUTS(buf);
    HTSprintf0(&buf, "<pre>\n");
    PUTS(buf);

    for (i = 'a'+1; i <= 'z'+1; i++) {
	print_binding(target, i, TRUE);
    }
    for (i = 1; i < KEYMAP_SIZE; i++) {
	/*
	 *  Don't show CHANGE_LINK if mouse not enabled.
	 */
	if ((i >= 0200 || i <= ' ' || !isalpha(i-1)) &&
	    (LYUseMouse || (keymap[i] != LYK_CHANGE_LINK))) {
	    print_binding(target, i, FALSE);
	}
    }

    HTSprintf0(&buf,"</pre>\n</body>\n</html>\n");
    PUTS(buf);

    (*target->isa->_free)(target);
    FREE(buf);
    return(HT_LOADED);
}

#ifdef GLOBALDEF_IS_MACRO
#define _LYKEYMAP_C_GLOBALDEF_1_INIT { "LYNXKEYMAP", LYLoadKeymap, 0}
GLOBALDEF (HTProtocol,LYLynxKeymap,_LYKEYMAP_C_GLOBALDEF_1_INIT);
#else
GLOBALDEF PUBLIC HTProtocol LYLynxKeymap = {"LYNXKEYMAP", LYLoadKeymap, 0};
#endif /* GLOBALDEF_IS_MACRO */

/*
 * Install func as the mapping for key.
 * If for_dired is TRUE, install it in the key_override[] table
 * for Dired mode, otherwise in the general keymap[] table.
 * If DIRED_SUPPORT or OK_OVERRIDE is not defined, don't do anything
 * when for_dired is requested.
 * returns lynxkeycode value != 0 if the mapping was made, 0 if not.
 */
PUBLIC int remap ARGS3(
	char *,		key,
	char *,		func,
	BOOLEAN,	for_dired)
{
    Kcmd *mp;
    int c;

#if !defined(DIRED_SUPPORT) || !defined(OK_OVERRIDE)
    if (for_dired)
	return 0;
#endif
    if (func == NULL)
	return 0;
    c = lkcstring_to_lkc(key);
    if (c <= -1)
	return 0;
    else if (c >= 0) {
	/* Remapping of key actions is supported only for basic
	 * lynxkeycodes, without modifiers etc.!  If we get somehow
	 * called for an invalid lynxkeycode, fail or silently ignore
	 * modifiers. - kw
	 */
	if (c & (LKC_ISLECLAC|LKC_ISLAC))
	   return 0;
	if ((c & LKC_MASK) != c)
	   c &= LKC_MASK;
    }
    if (c + 1 >= KEYMAP_SIZE)
	return 0;
    if ((mp = LYStringToKcmd(func)) != 0) {
#if defined(DIRED_SUPPORT) && defined(OK_OVERRIDE)
	if (for_dired)
	    key_override[c+1] = mp->code;
	else
#endif
	    keymap[c+1] = (LYKeymap_t) mp->code;
	return (c ? c : (int) LAC_TO_LKC0(mp->code)); /* don't return 0, successful */
    }
    return 0;
}

typedef struct {
    int	code;
    LYKeymap_t map;
    LYKeymap_t save;
} ANY_KEYS;

/*
 * Save the given keys in the table, setting them to the map'd value.
 */
PRIVATE void set_any_keys ARGS2(
	ANY_KEYS *,	table,
	int,		size)
{
    int j, k;

    for (j = 0; j < size; ++j) {
	k = table[j].code + 1;
	table[j].save = keymap[k];
	keymap[k] = table[j].map;
    }
}

/*
 * Restore the given keys from the table.
 */
PRIVATE void reset_any_keys ARGS2(
	ANY_KEYS *,	table,
	int,		size)
{
    int j, k;

    for (j = 0; j < size; ++j) {
	k = table[j].code + 1;
	keymap[k] = table[j].save;
    }
}

static ANY_KEYS vms_keys_table[] = {
    { 26,   LYK_ABORT,   0 },	/* control-Z */
    { '$',  LYK_SHELL,   0 },
};

PUBLIC void set_vms_keys NOARGS
{
    set_any_keys(vms_keys_table, TABLESIZE(vms_keys_table));
}

static ANY_KEYS vi_keys_table[] = {
    { 'h', LYK_PREV_DOC,  0 },
    { 'j', LYK_NEXT_LINK, 0 },
    { 'k', LYK_PREV_LINK, 0 },
    { 'l', LYK_ACTIVATE,  0 },
};

static BOOLEAN did_vi_keys;

PUBLIC void set_vi_keys NOARGS
{
    set_any_keys(vi_keys_table, TABLESIZE(vi_keys_table));
    did_vi_keys = TRUE;
}

PUBLIC void reset_vi_keys NOARGS
{
    if (did_vi_keys) {
	reset_any_keys(vi_keys_table, TABLESIZE(vi_keys_table));
	did_vi_keys = FALSE;
    }
}

static ANY_KEYS emacs_keys_table[] = {
    { 2,  LYK_PREV_DOC,  0 },	/* ^B */
    { 14, LYK_NEXT_LINK, 0 },	/* ^N */
    { 16, LYK_PREV_LINK, 0 },	/* ^P */
    { 6,  LYK_ACTIVATE,  0 },	/* ^F */
};

static BOOLEAN did_emacs_keys;

PUBLIC void set_emacs_keys NOARGS
{
    set_any_keys(emacs_keys_table, TABLESIZE(emacs_keys_table));
    did_emacs_keys = TRUE;
}

PUBLIC void reset_emacs_keys NOARGS
{
    if (did_emacs_keys) {
	reset_any_keys(emacs_keys_table, TABLESIZE(emacs_keys_table));
	did_emacs_keys = FALSE;
    }
}

/*
 * Map numbers to functions as labeled on the IBM Enhanced keypad, and save
 * their original mapping for reset_numbers_as_arrows().  - FM
 */
static ANY_KEYS number_keys_table[] = {
    { '1', LYK_END,        0 },
    { '2', LYK_NEXT_LINK,  0 },
    { '3', LYK_NEXT_PAGE,  0 },
    { '4', LYK_PREV_DOC,   0 },
    { '5', LYK_DO_NOTHING, 0 },
    { '6', LYK_ACTIVATE,   0 },
    { '7', LYK_HOME,       0 },
    { '8', LYK_PREV_LINK,  0 },
    { '9', LYK_PREV_PAGE,  0 },
};

static BOOLEAN did_number_keys;

PUBLIC void set_numbers_as_arrows NOARGS
{
    set_any_keys(number_keys_table, TABLESIZE(number_keys_table));
    did_number_keys = TRUE;
}

PUBLIC void reset_numbers_as_arrows NOARGS
{
    if (did_number_keys) {
	reset_any_keys(number_keys_table, TABLESIZE(number_keys_table));
	did_number_keys = FALSE;
    }
}

PUBLIC char *key_for_func ARGS1 (
	int,	func)
{
    static char *buf;
    int i;
    char *formatted;

    if ((i = LYReverseKeymap(func)) >= 0) {
	formatted = LYKeycodeToString(i, TRUE);
	StrAllocCopy(buf, formatted != 0 ? formatted : "?");
    } else if (buf == 0) {
	StrAllocCopy(buf, "");
    }
    return buf;
}

/*
 *  Given one or two keys as lynxkeycodes, returns an allocated string
 *  representing the key(s) suitable for statusline messages, or NULL
 *  if no valid lynxkeycode is passed in (i.e., lkc_first < 0 or some other
 *  failure).  The caller must free the string. - kw
 */
PUBLIC char *fmt_keys ARGS2(
    int,	lkc_first,
    int,	lkc_second)
{
    char *buf = NULL;
    BOOLEAN quotes = FALSE;
    char *fmt_first;
    char *fmt_second;

    if (lkc_first < 0)
	return NULL;
    fmt_first = LYKeycodeToString(lkc_first, TRUE);
    if (fmt_first && strlen(fmt_first) == 1 && *fmt_first != '\'') {
	quotes = TRUE;
    }
    if (quotes) {
	if (lkc_second < 0) {
	    HTSprintf0(&buf, "'%s'", fmt_first);
	    return buf;
	} else {
	    HTSprintf0(&buf, "'%s", fmt_first);
	}
    } else {
	StrAllocCopy(buf, fmt_first);
    }
    if (lkc_second >= 0) {
	fmt_second = LYKeycodeToString(lkc_second, TRUE);
	if (!fmt_second) {
	    FREE(buf);
	    return NULL;
	}
	HTSprintf(&buf, "%s%s%s",
		  ((strlen(fmt_second) > 2 && *fmt_second != '<') ||
		   (strlen(buf) > 2 && buf[strlen(buf)-1] != '>')) ? " " : "",
		  fmt_second, quotes ? "'" : "");
    }
    return buf;
}

/*
 *  This function returns the (int)ch mapped to the
 *  LYK_foo value passed to it as an argument.  It is like
 *  LYReverseKeymap, only the order of search is different;
 *  e.g., small ASCII letters will be returned in preference to
 *  capital ones.  Cf. LYKeyForEditAction, LYEditKeyForAction in
 *  LYEditmap.c which use the same order to find a best key.
 *  In addition, this function takes the dired override map into
 *  account while LYReverseKeymap doesn't.
 *  The caller must free the returned string. - kw
 */
#define FIRST_I 97
#define NEXT_I(i,imax) ((i==122) ? 32 : (i==96) ? 123 : (i==126) ? 0 :\
			(i==31) ? 256 : (i==imax) ? 127 :\
			(i==255) ? (-1) :i+1)
PRIVATE int best_reverse_keymap ARGS1(
	int,	lac)
{
    int i, c;

    for (i = FIRST_I; i >= 0; i = NEXT_I(i,KEYMAP_SIZE-2)) {
#ifdef NOT_ASCII
	if (i < 256) {
	    c = FROMASCII(i);
	} else
#endif
	    c = i;
#if defined(DIRED_SUPPORT) && defined(OK_OVERRIDE)
	if (lynx_edit_mode && !no_dired_support && lac &&
	    LKC_TO_LAC(key_override,c) == lac)
	    return c;
#endif /* DIRED_SUPPORT && OK_OVERRIDE */
	if (LKC_TO_LAC(keymap,c) == lac) {
	    return c;
	}
    }

    return(-1);
}

/*
 *  This function returns a string representing a key mapped
 *  to a LYK_foo function, or NULL if not found.  The string
 *  may represent a pair of keys.  if context_code is FOR_INPUT,
 *  an appropriate binding for use while in the (forms) line editor
 *  is sought.  - kw
 */
PUBLIC char* key_for_func_ext ARGS2(
    int,	lac,
    int,	context_code)
{
    int lkc, modkey = -1;

    if (context_code == FOR_INPUT) {
	lkc = LYEditKeyForAction(lac, &modkey);
	if (lkc >= 0) {
	    if (lkc & (LKC_MOD1|LKC_MOD2|LKC_MOD3)) {
		return fmt_keys(modkey, lkc & ~(LKC_MOD1|LKC_MOD2|LKC_MOD3));
	    } else {
		return fmt_keys(lkc, -1);
	    }
	}
    }
    lkc = best_reverse_keymap(lac);
    if (lkc < 0)
	return NULL;
    if (context_code == FOR_INPUT) {
	modkey = LYKeyForEditAction(LYE_LKCMD);
	if (modkey < 0)
	    return NULL;
	return fmt_keys(modkey, lkc);
    } else {
	return fmt_keys(lkc, -1);
    }
}

/*
 *  This function returns TRUE if the ch is non-alphanumeric
 *  and maps to KeyName (LYK_foo in the keymap[] array). - FM
 */
PUBLIC BOOL LYisNonAlnumKeyname ARGS2(
	int,	ch,
	int,	KeyName)
{
    if (ch < 0 || ch >= KEYMAP_SIZE)
	return (FALSE);
    if (ch > 0
     && strchr("0123456789\
ABCDEFGHIJKLMNOPQRSTUVWXYZ\
abcdefghijklmnopqrstuvwxyz", ch) != NULL)
	return (FALSE);

    return (BOOL) (keymap[ch+1] == KeyName);
}

/*
 *  This function returns the (int)ch mapped to the
 *  LYK_foo value passed to it as an argument. - FM
 */
PUBLIC int LYReverseKeymap ARGS1(
	int,	KeyName)
{
    int i;

    for (i = 1; i < KEYMAP_SIZE; i++) {
	if (keymap[i] == KeyName) {
	    return(i - 1);
	}
    }

    return(-1);
}

#ifdef EXP_KEYBOARD_LAYOUT
PUBLIC int LYSetKbLayout ARGS1(
	char *,	layout_id)
{
    int i;

    for (i = 0; i < (int) TABLESIZE(LYKbLayoutNames) - 1; i++) {
	if (!strcmp(LYKbLayoutNames[i], layout_id)) {
	    current_layout = i;
	    return (-1);
	}
    }

    return 0;
}
#endif
