/*	$OpenBSD: defs.h,v 1.2 1997/03/12 10:41:52 downsj Exp $	*/

/*
** Declaration of functions.
**
**	@(#)defs.h              e07@nikhef.nl (Eric Wassenaar) 961113
*/

/*
** Internal modules of the host utility
** ------------------------------------
*/
	/* main.c */

int main		PROTO((int, char **));
void set_defaults	PROTO((char *, int, char **));
int process_argv	PROTO((int, char **));
int process_file	PROTO((FILE *));
int process_name	PROTO((char *));
int execute_name	PROTO((char *));
bool execute		PROTO((char *, ipaddr_t));
bool host_query		PROTO((char *, ipaddr_t));
char *myhostname	PROTO((void));
void set_server		PROTO((char *));
void set_logfile	PROTO((char *));
void fatal		PROTO((char *, ...));
void errmsg		PROTO((char *, ...));

	/* info.c */

bool get_hostinfo	PROTO((char *, bool));
bool get_domaininfo	PROTO((char *, char *));
int get_info		PROTO((querybuf *, char *, int, int));
bool print_info		PROTO((querybuf *, int, char *, int, int, bool));
void print_data		PROTO((char *, ...));
u_char *print_rrec	PROTO((char *, int, int, u_char *, u_char *, u_char *, bool));
u_char *skip_qrec	PROTO((char *, int, int, u_char *, u_char *, u_char *));
bool get_recursive	PROTO((char **));

	/* list.c */

bool list_zone		PROTO((char *));
bool find_servers	PROTO((char *));
bool get_servers	PROTO((char *));
bool get_nsinfo		PROTO((querybuf *, int, char *));
void sort_servers	PROTO((void));
bool skip_transfer	PROTO((char *));
void do_check		PROTO((char *));
bool do_transfer	PROTO((char *));
bool transfer_zone	PROTO((char *, struct in_addr, char *));
bool get_zone		PROTO((char *, struct in_addr, char *));
void update_zone	PROTO((char *));
bool get_mxrec		PROTO((char *));
char *get_primary	PROTO((char *));
bool check_zone		PROTO((char *));
bool get_soainfo	PROTO((querybuf *, int, char *));
void check_soa		PROTO((querybuf *, char *));
bool check_dupl		PROTO((ipaddr_t));
bool check_ttl		PROTO((char *, int, int, int));
void clear_ttltab	PROTO((void));
int host_index		PROTO((char *, bool));
void clear_hosttab	PROTO((void));
int zone_index		PROTO((char *, bool));
void clear_zonetab	PROTO((void));
int check_canon		PROTO((char *));

	/* addr.c */

bool check_addr		PROTO((char *));
bool check_name		PROTO((ipaddr_t));

	/* geth.c */

struct hostent *geth_byname	PROTO((CONST char *));
struct hostent *geth_byaddr	PROTO((CONST char *, int, int));

	/* util.c */

int parse_type		PROTO((char *));
int parse_class		PROTO((char *));
char *in_addr_arpa	PROTO((char *));
char *nsap_int		PROTO((char *));
void print_host		PROTO((char *, struct hostent *));
void show_res		PROTO((void));
void print_statistics	PROTO((char *, int, int));
void clear_statistics	PROTO((void));
void show_types		PROTO((char *, int, int));
void ns_error		PROTO((char *, int, int, char *));
char *decode_error	PROTO((int));
void print_status	PROTO((querybuf *, int));
void pr_error		PROTO((char *, ...));
void pr_warning		PROTO((char *, ...));
bool want_type		PROTO((int, int));
bool want_class		PROTO((int, int));
bool indomain		PROTO((char *, char *, bool));
bool samedomain		PROTO((char *, char *, bool));
bool gluerecord		PROTO((char *, char *, char **, int));
int matchlabels		PROTO((char *, char *));
char *pr_domain		PROTO((char *, bool));
char *pr_dotname	PROTO((char *));
char *pr_nsap		PROTO((char *));
char *pr_type		PROTO((int));
char *pr_class		PROTO((int));
int expand_name		PROTO((char *, int, u_char *, u_char *, u_char *, char *));
int check_size		PROTO((char *, int, u_char *, u_char *, u_char *, int));
bool valid_name		PROTO((char *, bool, bool, bool));
int canonical		PROTO((char *));
char *mapreverse	PROTO((char *, struct in_addr));
int compare_name	PROTO((const ptr_t *, const ptr_t *));

	/* misc.c */

ptr_t *xalloc		PROTO((ptr_t *, siz_t));
char *itoa		PROTO((int));
char *utoa		PROTO((int));
char *xtoa		PROTO((int));
char *stoa		PROTO((u_char *, int, bool));
char *base_ntoa		PROTO((u_char *, int));
char *nsap_ntoa		PROTO((u_char *, int));
char *ipng_ntoa		PROTO((u_char *));
char *pr_date		PROTO((int));
char *pr_time		PROTO((int, bool));
char *pr_spherical	PROTO((int, char *, char *));
char *pr_vertical	PROTO((int, char *, char *));
char *pr_precision	PROTO((int));

	/* send.c */

#ifdef HOST_RES_SEND
int res_send		PROTO((CONST qbuf_t *, int, qbuf_t *, int));
#endif /*HOST_RES_SEND*/
int _res_connect	PROTO((int, struct sockaddr_in *, int));
int _res_write		PROTO((int, struct sockaddr_in *, char *, char *, int));
int _res_read		PROTO((int, struct sockaddr_in *, char *, char *, int));
void _res_perror	PROTO((struct sockaddr_in *, char *, char *));

/*
** External library functions
** --------------------------
*/
	/* extern */

#if 0
ipaddr_t inet_addr	PROTO((CONST char *));
char *inet_ntoa		PROTO((struct in_addr));
char *hostalias		PROTO((CONST char *));
#endif

	/* avoid <strings.h> */

#if !defined(index)

char *index		PROTO((const char *, int));
char *rindex		PROTO((const char *, int));

#endif

	/* <string.h> */

#if !defined(NO_STRING_H)
#include <string.h>
#else

char *strcpy		PROTO((char *, const char *));
char *strncpy		PROTO((char *, const char *, siz_t));

#endif

	/* <stdlib.h> */

#if defined(__STDC__) && !defined(apollo)
#include <stdlib.h>
#else

char *getenv		PROTO((const char *));
ptr_t *malloc		PROTO((siz_t));
ptr_t *realloc		PROTO((ptr_t *, siz_t));
free_t free		PROTO((ptr_t *));
void exit		PROTO((int));
void qsort		PROTO((ptr_t *, siz_t, siz_t, int (*)(const ptr_t *, const ptr_t *)));

#endif

	/* <unistd.h> */

#if defined(__STDC__) && !defined(apollo)
#include <unistd.h>
#endif
