/*	$OpenBSD: paths.h,v 1.5 2003/02/15 12:19:27 deraadt Exp $	*/
/*	$NetBSD: paths.h,v 1.1 1995/08/14 19:50:09 gwr Exp $	*/

/*
 *=====================================================================
 * Copyright (c) 1986,1987,1988,1989,1990,1991 by Sun Microsystems, Inc.
 *	@(#)pcnfsd_cache.c	1.1	9/3/91
 *
 * pcnfsd is copyrighted software, but is freely licensed. This
 * means that you are free to redistribute it, modify it, ship it
 * in binary with your system, whatever, provided:
 *
 * - you leave the Sun copyright notice in the source code
 * - you make clear what changes you have introduced and do
 *   not represent them as being supported by Sun.
 *
 * If you make changes to this software, we ask that you do so in
 * a way which allows you to build either the "standard" version or
 * your custom version from a single source file. Test it, lint
 * it (it won't lint 100%, very little does, and there are bugs in
 * some versions of lint :-), and send it back to Sun via email
 * so that we can roll it into the source base and redistribute
 * it. We'll try to make sure your contributions are acknowledged
 * in the source, but after all these years it's getting hard to
 * remember who did what.
 *=====================================================================
 */

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
