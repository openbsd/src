#ifndef LYCOOKIES_H
#define LYCOOKIES_H

#ifndef HTUTILS_H
#include <HTUtils.h>
#endif

#include <HTList.h>

typedef enum {ACCEPT_ALWAYS, REJECT_ALWAYS, QUERY_USER} behaviour_t;
typedef enum {INVCHECK_QUERY,
	      INVCHECK_STRICT,
	      INVCHECK_LOOSE} invcheck_behaviour_t;
typedef enum {FLAG_ACCEPT_ALWAYS,
	      FLAG_REJECT_ALWAYS,
	      FLAG_QUERY_USER,
	      FLAG_FROM_FILE,
	      FLAG_INVCHECK_QUERY,
	      FLAG_INVCHECK_STRICT,
	      FLAG_INVCHECK_LOOSE} cookie_domain_flags;

struct _domain_entry {
    char *	domain;  /* Domain for which these cookies are valid */
    behaviour_t	bv;
    invcheck_behaviour_t	invcheck_bv;
    HTList *	cookie_list;
};
typedef struct _domain_entry domain_entry;

extern void LYSetCookie PARAMS((
	CONST char *	SetCookie,
	CONST char *	SetCookie2,
	CONST char *	address));
extern char *LYCookie PARAMS((
	char *		hostname,
	char *		partialpath,
	int		port,
	BOOL		secure));
extern void LYStoreCookies PARAMS((
	char *		cookie_file));
extern void LYLoadCookies PARAMS((
	char * 		cookie_file));
extern void cookie_domain_flag_set PARAMS((
	char * 		domainstr,
	int 		flag));
extern void LYConfigCookies NOPARAMS;

#endif /* LYCOOKIES_H */
