#if !defined(_LYHASH_H_)
#define _LYHASH_H_ 1

#ifndef HTUTILS_H
#include <HTUtils.h>
#endif

struct _hashbucket {
	char *name; /* name of this item */
	int	code; /* code of this item */
	int color; /* color highlighting to be done */
	int mono; /* mono highlighting to be done */
	int cattr; /* attributes to go with the color */
	struct _hashbucket *next; /* next item */
};

typedef struct _hashbucket bucket;

#if !defined(CSHASHSIZE)
#define CSHASHSIZE 8193
#endif

#define NOSTYLE -1

extern bucket hashStyles[CSHASHSIZE];
extern int hash_code PARAMS((CONST char* string));
extern bucket special_bucket;/*it's used when OMIT_SCN_KEEPING is 1 in HTML.c
    and LYCurses.c. */
extern bucket nostyle_bucket;/*initialized properly - to be used in CTRACE when
            NOSTYLE is passed as 'style' to curses_w_style */

extern int hash_code_lowercase_on_fly PARAMS((CONST char* string));
extern int hash_code_aggregate_char PARAMS((char c,int hash));
extern int hash_code_aggregate_lower_str PARAMS((CONST char* c, int hash_was));

extern int	s_a;
extern int	s_aedit;
extern int	s_aedit_arr;
extern int	s_aedit_pad;
extern int	s_aedit_sel;
extern int	s_alert;
extern int	s_alink;
extern int	s_curedit;
extern int	s_forw_backw;
extern int	s_normal;
extern int	s_prompt_edit;
extern int	s_prompt_edit_arr;
extern int	s_prompt_edit_pad;
extern int	s_prompt_sel;
extern int	s_status;
extern int	s_title;
extern int	s_whereis;
extern int	s_menu_frame;
extern int	s_menu_bg;
extern int	s_menu_number;
extern int	s_menu_entry;
extern int	s_menu_active;
extern int	s_menu_sb;

#ifdef USE_SCROLLBAR
extern int	s_sb_aa;
extern int	s_sb_bar;
extern int	s_sb_bg;
extern int	s_sb_naa;
#endif

#define CACHEW 128
#define CACHEH 64

extern unsigned cached_styles[CACHEH][CACHEW];

#endif /* _LYHASH_H_ */
