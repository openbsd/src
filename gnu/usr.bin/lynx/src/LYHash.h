#if !defined(_LYHASH_H_)
#define _LYHASH_H_ 1

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
#ifdef NOT_USED
extern int hash_table[CSHASHSIZE]; /* 32K should be big enough */
#endif

extern int	s_alink, s_a, s_status,
		s_label, s_value, s_high,
		s_normal, s_alert, s_title;
#define CACHEW 128
#define CACHEH 64

extern unsigned cached_styles[CACHEH][CACHEW];

#endif /* _LYHASH_H_ */
