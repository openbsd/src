/*	$OpenBSD: rusers_proc.c,v 1.12 2001/11/18 23:39:18 deraadt Exp $	*/

/*-
 *  Copyright (c) 1993 John Brezak
 *  All rights reserved.
 * 
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR `AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef lint
static char rcsid[] = "$OpenBSD: rusers_proc.c,v 1.12 2001/11/18 23:39:18 deraadt Exp $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <utmp.h>
#include <stdio.h>
#include <syslog.h>
#include <string.h>
#include <rpc/rpc.h>
#include <rpcsvc/rusers.h>	/* New version */
#include <rpcsvc/rnusers.h>	/* Old version */

#define	IGNOREUSER	"sleeper"

#ifndef _PATH_UTMP
#define _PATH_UTMP "/etc/utmp"
#endif

#ifndef _PATH_DEV
#define _PATH_DEV "/dev"
#endif

#ifndef UT_LINESIZE
#define UT_LINESIZE sizeof(((struct utmp *)0)->ut_line)
#endif
#ifndef UT_NAMESIZE
#define UT_NAMESIZE sizeof(((struct utmp *)0)->ut_name)
#endif
#ifndef UT_HOSTSIZE
#define UT_HOSTSIZE sizeof(((struct utmp *)0)->ut_host)
#endif

typedef char ut_line_t[UT_LINESIZE+1];
typedef char ut_name_t[UT_NAMESIZE+1];
typedef char ut_host_t[UT_HOSTSIZE+1];

struct rusers_utmp utmps[MAXUSERS];
struct utmpidle *utmp_idlep[MAXUSERS];
struct utmpidle utmp_idle[MAXUSERS];
struct ru_utmp *ru_utmpp[MAXUSERS];
struct ru_utmp ru_utmp[MAXUSERS];
ut_line_t line[MAXUSERS];
ut_name_t name[MAXUSERS];
ut_host_t host[MAXUSERS];

extern int from_inetd;

FILE *ufp;

static u_int
getidle(tty, display)
	char *tty, *display;
{
	struct stat st;
	char devname[PATH_MAX];
	time_t now;
	u_long idle;
	
	/*
	 * If this is an X terminal or console, then try the
	 * XIdle extension
	 */
	idle = 0;
	if (*tty == 'X') {
		u_long kbd_idle, mouse_idle;
#if !defined(__i386__)
		kbd_idle = getidle("kbd", NULL);
#else
		/*
		 * XXX Icky i386 console hack.
		 */
		kbd_idle = getidle("vga", NULL);
#endif
		mouse_idle = getidle("mouse", NULL);
		idle = (kbd_idle < mouse_idle) ? kbd_idle : mouse_idle;
	} else {
		sprintf(devname, "%s/%s", _PATH_DEV, tty);
		if (stat(devname, &st) < 0) {
#ifdef DEBUG
			printf("%s: %m\n", devname);
#endif
			return (0);
		}
		time(&now);
#ifdef DEBUG
		printf("%s: now=%d atime=%d\n", devname, now, st.st_atime);
#endif
		idle = now - st.st_atime;
		idle = (idle + 30) / 60; /* secs->mins */
	}
	if (idle < 0)
		idle = 0;

	return (idle);
}
	
int *
rusers_num_svc(arg, rqstp)
	void *arg;
	struct svc_req *rqstp;
{
	static int num_users = 0;
	struct utmp usr;

	ufp = fopen(_PATH_UTMP, "r");
	if (!ufp) {
		syslog(LOG_ERR, "%m");
		return (0);
	}

	/* only entries with both name and line fields */
	while (fread((char *)&usr, sizeof(usr), 1, ufp) == 1)
		if (*usr.ut_name && *usr.ut_line &&
		    strncmp(usr.ut_name, IGNOREUSER,
			    sizeof(usr.ut_name))
#ifdef USER_PROCESS
		    && usr.ut_type == USER_PROCESS
#endif
		    ) {
			num_users++;
		}

	fclose(ufp);
	return (&num_users);
}

static utmp_array *
do_names_3(int all)
{
	static utmp_array ut;
	struct utmp usr;
	int nusers = 0;
	
	bzero((char *)&ut, sizeof(ut));
	ut.utmp_array_val = &utmps[0];
	
	ufp = fopen(_PATH_UTMP, "r");
	if (!ufp) {
		syslog(LOG_ERR, "%m");
		return (NULL);
	}

	/* only entries with both name and line fields */
	while (fread((char *)&usr, sizeof(usr), 1, ufp) == 1 &&
	       nusers < MAXUSERS)
		if (*usr.ut_name && *usr.ut_line &&
		    strncmp(usr.ut_name, IGNOREUSER,
			    sizeof(usr.ut_name))
#ifdef USER_PROCESS
		    && usr.ut_type == USER_PROCESS
#endif
		    ) {
			utmps[nusers].ut_type = RUSERS_USER_PROCESS;
			utmps[nusers].ut_time =
				usr.ut_time;
			utmps[nusers].ut_idle =
				getidle(usr.ut_line, usr.ut_host);
			utmps[nusers].ut_line = line[nusers];
			memset(line[nusers], 0, sizeof(line[nusers]));
			strlcpy(line[nusers], usr.ut_line, sizeof(line[nusers]));
			utmps[nusers].ut_user = name[nusers];
			memset(name[nusers], 0, sizeof(name[nusers]));
			strlcpy(name[nusers], usr.ut_name, sizeof(name[nusers]));
			utmps[nusers].ut_host = host[nusers];
			memset(host[nusers], 0, sizeof(host[nusers]));
			strlcpy(host[nusers], usr.ut_host, sizeof(host[nusers]));
			nusers++;
		}
	ut.utmp_array_len = nusers;

	fclose(ufp);
	return (&ut);
}

utmp_array *
rusersproc_names_3_svc(arg, rqstp)
	void *arg;
	struct svc_req *rqstp;
{
	return (do_names_3(0));
}

utmp_array *
rusersproc_allnames_3_svc(arg, rqstp)
	void *arg;
	struct svc_req *rqstp;
{
	return (do_names_3(1));
}

static struct utmpidlearr *
do_names_2(int all)
{
	static struct utmpidlearr ut;
	struct utmp usr;
	int nusers = 0;
	
	bzero((char *)&ut, sizeof(ut));
	ut.uia_arr = utmp_idlep;
	ut.uia_cnt = 0;
	
	ufp = fopen(_PATH_UTMP, "r");
	if (!ufp) {
		syslog(LOG_ERR, "%m");
		return (NULL);
	}

	/* only entries with both name and line fields */
	while (fread((char *)&usr, sizeof(usr), 1, ufp) == 1 &&
	       nusers < MAXUSERS)
		if (*usr.ut_name && *usr.ut_line &&
		    strncmp(usr.ut_name, IGNOREUSER,
			    sizeof(usr.ut_name))
#ifdef USER_PROCESS
		    && usr.ut_type == USER_PROCESS
#endif
		    ) {
			utmp_idlep[nusers] = &utmp_idle[nusers];
			utmp_idle[nusers].ui_utmp.ut_time =
				usr.ut_time;
			utmp_idle[nusers].ui_idle =
				getidle(usr.ut_line, usr.ut_host);
			utmp_idle[nusers].ui_utmp.ut_line = line[nusers];
			memset(line[nusers], 0, sizeof(line[nusers]));
			strlcpy(line[nusers], usr.ut_line, sizeof(line[nusers]));
			utmp_idle[nusers].ui_utmp.ut_name = name[nusers];
			memset(name[nusers], 0, sizeof(name[nusers]));
			strlcpy(name[nusers], usr.ut_name, sizeof(name[nusers]));
			utmp_idle[nusers].ui_utmp.ut_host = host[nusers];
			memset(host[nusers], 0, sizeof(host[nusers]));
			strlcpy(host[nusers], usr.ut_host, sizeof(host[nusers]));
			nusers++;
		}

	ut.uia_cnt = nusers;
	fclose(ufp);
	return (&ut);
}

struct utmpidlearr *
rusersproc_names_2_svc(arg, rqstp)
	void *arg;
	struct svc_req *rqstp;
{
	return (do_names_2(0));
}

struct utmpidlearr *
rusersproc_allnames_2_svc(arg, rqstp)
	void *arg;
	struct svc_req *rqstp;
{
	return (do_names_2(1));
}

static struct utmparr *
do_names_1(int all)
{
	static struct utmparr ut;
	struct utmp usr;
	int nusers = 0;
	
	bzero((char *)&ut, sizeof(ut));
	ut.uta_arr = ru_utmpp;
	ut.uta_cnt = 0;
	
	ufp = fopen(_PATH_UTMP, "r");
	if (!ufp) {
		syslog(LOG_ERR, "%m");
		return (NULL);
	}

	/* only entries with both name and line fields */
	while (fread((char *)&usr, sizeof(usr), 1, ufp) == 1 &&
	       nusers < MAXUSERS)
		if (*usr.ut_name && *usr.ut_line &&
		    strncmp(usr.ut_name, IGNOREUSER,
			    sizeof(usr.ut_name))
#ifdef USER_PROCESS
		    && usr.ut_type == USER_PROCESS
#endif
		    ) {
			ru_utmpp[nusers] = &ru_utmp[nusers];
			ru_utmp[nusers].ut_time = usr.ut_time;
			ru_utmp[nusers].ut_line = line[nusers];
			strlcpy(line[nusers], usr.ut_line, sizeof(line[nusers]));
			ru_utmp[nusers].ut_name = name[nusers];
			strlcpy(name[nusers], usr.ut_name, sizeof(name[nusers]));
			ru_utmp[nusers].ut_host = host[nusers];
			strlcpy(host[nusers], usr.ut_host, sizeof(host[nusers]));
			nusers++;
		}

	ut.uta_cnt = nusers;
	fclose(ufp);
	return (&ut);
}

struct utmparr *
rusersproc_names_1_svc(arg, rqstp)
	void *arg;
	struct svc_req *rqstp;
{
	return (do_names_1(0));
}

struct utmparr *
rusersproc_allnames_1_svc(arg, rqstp)
	void *arg;
	struct svc_req *rqstp;
{
	return (do_names_1(1));
}

void
rusers_service(rqstp, transp)
	struct svc_req *rqstp;
	SVCXPRT *transp;
{
	union {
		int fill;
	} argument;
	char *result;
	xdrproc_t xdr_argument, xdr_result;
	char *(*local) __P((void *, struct svc_req *));

	switch (rqstp->rq_proc) {
	case NULLPROC:
		(void)svc_sendreply(transp, xdr_void, (char *)NULL);
		goto leave;

	case RUSERSPROC_NUM:
		xdr_argument = (xdrproc_t)xdr_void;
		xdr_result = (xdrproc_t)xdr_int;
		switch (rqstp->rq_vers) {
		case RUSERSVERS_3:
		case RUSERSVERS_IDLE:
		case RUSERSVERS_ORIG:
			local = (char *(*) __P((void *, struct svc_req *)))
					rusers_num_svc;
			break;
		default:
			svcerr_progvers(transp, RUSERSVERS_IDLE, RUSERSVERS_3);
			goto leave;
			/*NOTREACHED*/
		}
		break;

	case RUSERSPROC_NAMES:
		xdr_argument = (xdrproc_t)xdr_void;
		xdr_result = (xdrproc_t)xdr_utmp_array;
		switch (rqstp->rq_vers) {
		case RUSERSVERS_3:
			local = (char *(*) __P((void *, struct svc_req *)))
					rusersproc_names_3_svc;
			break;

		case RUSERSVERS_IDLE:
			xdr_result = (xdrproc_t)xdr_utmpidlearr;
			local = (char *(*) __P((void *, struct svc_req *)))
					rusersproc_names_2_svc;
			break;

		case RUSERSVERS_ORIG:
			xdr_result = (xdrproc_t)xdr_utmpidlearr;
			local = (char *(*) __P((void *, struct svc_req *)))
					rusersproc_names_1_svc;
			break;

		default:
			svcerr_progvers(transp, RUSERSVERS_IDLE, RUSERSVERS_3);
			goto leave;
			/*NOTREACHED*/
		}
		break;

	case RUSERSPROC_ALLNAMES:
		xdr_argument = (xdrproc_t)xdr_void;
		xdr_result = (xdrproc_t)xdr_utmp_array;
		switch (rqstp->rq_vers) {
		case RUSERSVERS_3:
			local = (char *(*) __P((void *, struct svc_req *)))
					rusersproc_allnames_3_svc;
			break;

		case RUSERSVERS_IDLE:
			xdr_result = (xdrproc_t)xdr_utmpidlearr;
			local = (char *(*) __P((void *, struct svc_req *)))
					rusersproc_allnames_2_svc;
			break;

		case RUSERSVERS_ORIG:
			xdr_result = (xdrproc_t)xdr_utmpidlearr;
			local = (char *(*) __P((void *, struct svc_req *)))
					rusersproc_allnames_1_svc;
			break;

		default:
			svcerr_progvers(transp, RUSERSVERS_IDLE, RUSERSVERS_3);
			goto leave;
			/*NOTREACHED*/
		}
		break;

	default:
		svcerr_noproc(transp);
		goto leave;
	}
	bzero((char *)&argument, sizeof(argument));
	if (!svc_getargs(transp, xdr_argument, (caddr_t)&argument)) {
		svcerr_decode(transp);
		goto leave;
	}
	result = (*local)(&argument, rqstp);
	if (result != NULL && !svc_sendreply(transp, xdr_result, result)) {
		svcerr_systemerr(transp);
	}
	if (!svc_freeargs(transp, xdr_argument, (caddr_t)&argument)) {
		syslog(LOG_ERR, "unable to free arguments");
		exit(1);
	}
leave:
	if (from_inetd)
		exit(0);
}
