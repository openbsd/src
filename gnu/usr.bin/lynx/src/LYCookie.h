
#ifndef LYCOOKIES_H
#define LYCOOKIES_H

extern void LYSetCookie PARAMS((
	CONST char *	SetCookie,
	CONST char *	SetCookie2,
	CONST char *	address));
extern char *LYCookie PARAMS((
	CONST char *	hostname,
	CONST char *	partialpath,
	int		port,
	BOOL		secure));

typedef enum {ACCEPT_ALWAYS, REJECT_ALWAYS, QUERY_USER} behaviour;

struct _domain_entry {
    char *	domain;  /* Domain for which these cookies are valid */
    behaviour	bv;
    HTList *	cookie_list;
};
typedef struct _domain_entry domain_entry;

#endif /* LYCOOKIES_H */
