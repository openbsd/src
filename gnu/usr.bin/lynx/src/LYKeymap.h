#ifndef LYKEYMAP_H
#define LYKEYMAP_H

extern int remap PARAMS((char *key, char *func));
extern void set_vms_keys NOPARAMS;
extern void set_vi_keys NOPARAMS;
extern void reset_vi_keys NOPARAMS;
extern void set_emacs_keys NOPARAMS;
extern void reset_emacs_keys NOPARAMS;
extern void set_numbers_as_arrows NOPARAMS;
extern void reset_numbers_as_arrows NOPARAMS;
extern void print_keymap PARAMS((char **newfile));
extern char *key_for_func PARAMS((int func));
extern BOOLEAN LYisNonAlnumKeyname PARAMS((int ch, int key_name));
extern int LYReverseKeymap PARAMS((int key_name));

extern char keymap[]; /* main keymap matrix */

#if defined(DIRED_SUPPORT) && defined(OK_OVERRIDE)
extern char override[];
#endif

#define CURRENT_KEYMAP_TITLE "Current Key Map"

#define       LYK_1             1
#define       LYK_2             2
#define       LYK_3             3
#define       LYK_4             4
#define       LYK_5             5
#define       LYK_6             6
#define       LYK_7             7
#define       LYK_8             8
#define       LYK_9             9
#define       LYK_SOURCE        10
#define       LYK_RELOAD        11
#define       LYK_PIPE          12
#define       LYK_QUIT          13
#define       LYK_ABORT         14
#define       LYK_NEXT_PAGE     15
#define       LYK_PREV_PAGE     16
#define       LYK_UP_TWO        17
#define       LYK_DOWN_TWO      18
#define       LYK_UP_HALF       19
#define       LYK_DOWN_HALF     20
#define       LYK_REFRESH       21
#define       LYK_HOME          22
#define       LYK_END           23
#define       LYK_PREV_LINK     24
#define       LYK_NEXT_LINK     25
#define       LYK_UP_LINK       26
#define       LYK_DOWN_LINK     27
#define       LYK_RIGHT_LINK    28
#define       LYK_LEFT_LINK     29
#define       LYK_HISTORY       30
#define       LYK_PREV_DOC      31
#define       LYK_ACTIVATE      32
#define       LYK_GOTO          33
#define       LYK_ECGOTO        34
#define       LYK_HELP          35
#define       LYK_INDEX         36
#define       LYK_NOCACHE       37
#define       LYK_INTERRUPT     38
#define       LYK_MAIN_MENU     39
#define       LYK_OPTIONS       40
#define       LYK_INDEX_SEARCH  41
#define       LYK_WHEREIS       42
#define       LYK_NEXT          43
#define       LYK_COMMENT       44
#define       LYK_EDIT          45
#define       LYK_INFO          46
#define       LYK_PRINT         47
#define       LYK_ADD_BOOKMARK  48
#define       LYK_DEL_BOOKMARK  49
#define       LYK_VIEW_BOOKMARK 50
#define       LYK_VLINKS        51
#define       LYK_SHELL         52
#define       LYK_DOWNLOAD      53
#define       LYK_TRACE_TOGGLE  54
#define       LYK_TRACE_LOG     55
#define       LYK_IMAGE_TOGGLE  56
#define       LYK_INLINE_TOGGLE 57
#define       LYK_HEAD          58
#define       LYK_DO_NOTHING    59
#define       LYK_TOGGLE_HELP   60
#define       LYK_JUMP          61
#define       LYK_KEYMAP        62
#define       LYK_LIST          63
#define       LYK_TOOLBAR       64
#define       LYK_HISTORICAL    65
#define       LYK_MINIMAL       66
#define       LYK_SOFT_DQUOTES  67
#define       LYK_RAW_TOGGLE    68
#define       LYK_COOKIE_JAR    69
#define       LYK_F_LINK_NUM    70
#define       LYK_CLEAR_AUTH    71
#define       LYK_SWITCH_DTD    72
#define       LYK_ELGOTO        73

#ifdef USE_EXTERNALS
#define       LYK_EXTERN        74
#if defined(VMS) || defined(DIRED_SUPPORT)
#define       LYK_DIRED_MENU    75
#endif /* VMS || DIRED_SUPPORT */
#else  /* USE_EXTERNALS */
#if defined(VMS) || defined(DIRED_SUPPORT)
#define       LYK_DIRED_MENU    74
#endif /* VMS || DIRED_SUPPORT */
#endif /* !defined(USE_EXTERNALS) */

#ifdef DIRED_SUPPORT
#define       LYK_CREATE        (LYK_DIRED_MENU+1)
#define       LYK_REMOVE        (LYK_DIRED_MENU+2)
#define       LYK_MODIFY        (LYK_DIRED_MENU+3)
#define       LYK_TAG_LINK      (LYK_DIRED_MENU+4)
#define       LYK_UPLOAD        (LYK_DIRED_MENU+5)
#define       LYK_INSTALL       (LYK_DIRED_MENU+6)
#endif /* DIRED_SUPPORT */

#ifdef NOT_USED
#define       LYK_VERSION       81
#define       LYK_FORM_UP       82
#define       LYK_FORM_DOWN     83
#endif /* NOT_USED */

#endif /* LYKEYMAP_H */
