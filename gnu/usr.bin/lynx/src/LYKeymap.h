#ifndef LYKEYMAP_H
#define LYKEYMAP_H

#ifndef HTUTILS_H
#include <HTUtils.h>
#endif

extern BOOLEAN LYisNonAlnumKeyname PARAMS((int ch, int key_name));
extern char *key_for_func PARAMS((int func));
extern int LYReverseKeymap PARAMS((int key_name));
extern int lookup_keymap PARAMS((int code));
extern int remap PARAMS((char *key, char *func));
extern void print_keymap PARAMS((char **newfile));
extern void reset_emacs_keys NOPARAMS;
extern void reset_numbers_as_arrows NOPARAMS;
extern void reset_vi_keys NOPARAMS;
extern void set_emacs_keys NOPARAMS;
extern void set_numbers_as_arrows NOPARAMS;
extern void set_vi_keys NOPARAMS;
extern void set_vms_keys NOPARAMS;

/* We only use unsigned keycodes; if there's a problem matching with enum
 * (which is supposed to be 'int'), that would be okay, but not as clean
 * for type-checking.
 */
typedef unsigned short LYKeymap_t;

#define KEYMAP_SIZE 661
extern LYKeymap_t keymap[KEYMAP_SIZE]; /* main keymap matrix */

#ifdef EXP_KEYBOARD_LAYOUT
extern int current_layout;
extern LYKeymap_t * LYKbLayouts[];
extern char * LYKbLayoutNames[];
extern int LYSetKbLayout PARAMS((char *layout_id));
#endif

#if defined(DIRED_SUPPORT) && defined(OK_OVERRIDE)
extern LYKeymap_t key_override[];
#endif

/* The order of this enum must match the 'revmap[]' array in LYKeymap.c */
typedef enum {
    LYK_UNKNOWN=0
  , LYK_1
  , LYK_2
  , LYK_3
  , LYK_4
  , LYK_5
  , LYK_6
  , LYK_7
  , LYK_8
  , LYK_9
  , LYK_SOURCE
  , LYK_RELOAD
  , LYK_PIPE
  , LYK_QUIT
  , LYK_ABORT
  , LYK_NEXT_PAGE
  , LYK_PREV_PAGE
  , LYK_UP_TWO
  , LYK_DOWN_TWO
  , LYK_UP_HALF
  , LYK_DOWN_HALF
  , LYK_REFRESH
  , LYK_HOME
  , LYK_END
  , LYK_PREV_LINK
  , LYK_NEXT_LINK
  , LYK_FASTBACKW_LINK
  , LYK_FASTFORW_LINK
  , LYK_UP_LINK
  , LYK_DOWN_LINK
  , LYK_RIGHT_LINK
  , LYK_LEFT_LINK
  , LYK_HISTORY
  , LYK_PREV_DOC
  , LYK_ACTIVATE
  , LYK_GOTO
  , LYK_ECGOTO
  , LYK_HELP
  , LYK_INDEX
  , LYK_NOCACHE
  , LYK_INTERRUPT
  , LYK_MAIN_MENU
  , LYK_OPTIONS
  , LYK_INDEX_SEARCH
  , LYK_WHEREIS
  , LYK_NEXT
  , LYK_COMMENT
  , LYK_EDIT
  , LYK_INFO
  , LYK_PRINT
  , LYK_ADD_BOOKMARK
  , LYK_DEL_BOOKMARK
  , LYK_VIEW_BOOKMARK
  , LYK_VLINKS
  , LYK_SHELL
  , LYK_DOWNLOAD
  , LYK_TRACE_TOGGLE
  , LYK_TRACE_LOG
  , LYK_IMAGE_TOGGLE
  , LYK_INLINE_TOGGLE
  , LYK_HEAD
  , LYK_DO_NOTHING
  , LYK_TOGGLE_HELP
  , LYK_JUMP
  , LYK_KEYMAP
  , LYK_LIST
  , LYK_TOOLBAR
  , LYK_HISTORICAL
  , LYK_MINIMAL
  , LYK_SOFT_DQUOTES
  , LYK_RAW_TOGGLE
  , LYK_COOKIE_JAR
  , LYK_F_LINK_NUM
  , LYK_CLEAR_AUTH
  , LYK_SWITCH_DTD
  , LYK_ELGOTO
  , LYK_CHANGE_LINK
  , LYK_EDIT_TEXTAREA
  , LYK_GROW_TEXTAREA
  , LYK_INSERT_FILE

#ifdef EXP_ADDRLIST_PAGE
  , LYK_ADDRLIST
#else
#define LYK_ADDRLIST      LYK_ADD_BOOKMARK
#endif

#ifdef USE_EXTERNALS
  , LYK_EXTERN
#endif /* !defined(USE_EXTERNALS) */

#if defined(VMS) || defined(DIRED_SUPPORT)
  , LYK_DIRED_MENU
#else
#define LYK_DIRED_MENU    LYK_UNKNOWN
#endif /* VMS || DIRED_SUPPORT */

#ifdef DIRED_SUPPORT
  , LYK_CREATE
  , LYK_REMOVE
  , LYK_MODIFY
  , LYK_TAG_LINK
  , LYK_UPLOAD
  , LYK_INSTALL
#else
#define LYK_TAG_LINK      LYK_UNKNOWN
#endif /* DIRED_SUPPORT */

} LYKeymapCodes;


#endif /* LYKEYMAP_H */
