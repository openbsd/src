/*	$OpenBSD: paths.h,v 1.4 2002/02/17 19:42:39 millert Exp $	*/
/*	$NetBSD: paths.h,v 1.1 1995/08/14 19:50:09 gwr Exp $	*/

#define SPOOLDIR        "/export/pcnfs"
#define LPRDIR		"/usr/bin"
#define LPCDIR		"/usr/sbin"

pr_list printers;
pr_queue queue;

/* pcnfsd_misc.c */
void scramble(char *, char *);
void wlogin(char *, struct svc_req *);
struct passwd *get_password(char *);

/* pcnfsd_print.c */
void *grab(int);
FILE *su_popen(char *, char *, int);
int su_pclose(FILE *);
int build_pr_list(void);
pirstat build_pr_queue(printername, username, int, int *, int *);
psrstat pr_start2(char *, char *, char *, char *, char *, char **);
pcrstat pr_cancel(char *, char *, char *);
pirstat get_pr_status(printername, bool_t *, bool_t *, int *, bool_t *, char *);
