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
#ifdef NOT_USED
#define CSHASHSIZE 32768
#else
#define CSHASHSIZE 8193
#endif
#endif

#define NOSTYLE -1

extern bucket hashStyles[CSHASHSIZE];
extern int hash_code PARAMS((char* string));
extern bucket special_bucket;/*it's used when OMIT_SCN_KEEPING is 1 in HTML.c
    and LYCurses.c. */
extern bucket nostyle_bucket;/*initialized properly - to be used in CTRACE when
            NOSTYLE is passed as 'style' to curses_w_style */    

extern int hash_code_lowercase_on_fly PARAMS((char* string));
extern int hash_code_aggregate_char PARAMS((char c,int hash));
extern int hash_code_aggregate_lower_str  PARAMS((char* c,int hash_was));


#ifdef NOT_USED
extern int hash_table[CSHASHSIZE]; /* 32K should be big enough */
#endif

extern int	s_alink, s_a, s_status,
		s_label, s_value, s_high,
		s_normal, s_alert, s_title,
		s_whereis;
#define CACHEW 128
#define CACHEH 64

extern unsigned cached_styles[CACHEH][CACHEW];

#endif /* _LYHASH_H_ */
