/*	$OpenBSD: paths.h,v 1.2 2001/08/19 19:16:12 ericj Exp $	*/
/*	$NetBSD: paths.h,v 1.1 1995/08/14 19:50:09 gwr Exp $	*/

#define SPOOLDIR        "/export/pcnfs"
#define LPRDIR		"/usr/bin"
#define LPCDIR		"/usr/sbin"

pr_list printers;
pr_queue queue;

/* pcnfsd_misc.c */
void scramble __P((char *, char *));
void wlogin __P((char *, struct svc_req *));
struct passwd *get_password __P((char *));

/* pcnfsd_print.c */
void *grab __P((int));
FILE *su_popen __P((char *, char *, int));
int su_pclose __P((FILE *));
int build_pr_list __P((void));
pirstat build_pr_queue __P((printername, username, int, int *, int *));
psrstat pr_start2 __P((char *, char *, char *, char *, char *, char **));
pcrstat pr_cancel __P((char *, char *, char *));
pirstat get_pr_status __P((printername, bool_t *, bool_t *, int *, bool_t *,
			char *));
