/*	$OpenBSD: klogin.c,v 1.1 2000/12/12 02:31:38 millert Exp $	*/

/*-
 * Copyright (c) 1990, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
#if 0
static char sccsid[] = "@(#)klogin.c	8.3 (Berkeley) 4/2/94";
#endif
static char rcsid[] = "$OpenBSD: klogin.c,v 1.1 2000/12/12 02:31:38 millert Exp $";
#endif /* not lint */

#ifdef KERBEROS
#include <sys/param.h>
#include <sys/syslog.h>
#include <des.h>
#include <kerberosIV/krb.h>
#include <kerberosIV/kafs.h>

#include <err.h>
#include <netdb.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define	INITIAL_TICKET	"krbtgt"
#define	VERIFY_SERVICE	"rcmd"

extern int notickets;
extern int always_use_klogin;
extern char *krbtkfile_env;
static char realm[REALM_SZ];

int koktologin __P((char *, char *, char *));

/*
 * Attempt to log the user in using Kerberos authentication
 *
 * return 0 on success (will be logged in)
 *	  1 if Kerberos failed (try local password in login)
 */
int
klogin(pw, instance, localhost, password)
	struct passwd *pw;
	char *instance, *localhost, *password;
{
	int kerror, fd;
	AUTH_DAT authdata;
	KTEXT_ST ticket;
	struct hostent *hp;
	unsigned long faddr;
	char savehost[MAXHOSTNAMELEN];
	char tkt_location[MAXPATHLEN];
	char *tty;
	char *user = strcmp(instance, "root") == 0 ? instance : pw->pw_name;
	char *krb_get_phost();

	/*
	 * Root logins don't use Kerberos (or at least shouldn't be
	 * sending kerberos passwords around in cleartext), so don't
	 * allow any root logins here (keeping in mind that we only
	 * get here with a password).
	 *
	 * If we have a realm, try getting a ticket-granting ticket
	 * and using it to authenticate.  Otherwise, return
	 * failure so that we can try the normal passwd file
	 * for a password.  If that's ok, log the user in
	 * without issuing any tickets.
	 */
	if (pw->pw_uid == 0 ||
	    (realm[0] == '\0' && krb_get_lrealm(realm, 0) != KSUCCESS))
		return (1);

	tty = ttyname(STDIN_FILENO);
	if (tty == NULL || *tty == '\0')
		tty = "??";
	else if ((tty = strrchr(tty, '/')))
		tty++;

	/*
	 * get TGT for local realm
	 * tickets are stored in a file named TKT_ROOT plus uid plus tty
	 * except for user.root tickets.
	 */

	if (strcmp(instance, "root") != 0)
		snprintf(tkt_location, sizeof(tkt_location), "%s%d.%s",
			TKT_ROOT, pw->pw_uid, tty);
	else {
		snprintf(tkt_location, sizeof(tkt_location), "%s_root_%d.%s",
			TKT_ROOT, pw->pw_uid, tty);
		krbtkfile_env = tkt_location;
	}
	(void)krb_set_tkt_string(tkt_location);

	/*
	 * Set real as well as effective ID to 0 for the moment,
	 * to make the kerberos library do the right thing.
	 */
	if (setuid(0) < 0) {
		warnx("setuid");
		return (1);
	}
	kerror = krb_get_pw_in_tkt(pw->pw_name, instance,
		    realm, INITIAL_TICKET, realm, DEFAULT_TKT_LIFE, password);
	/*
	 * If we got a TGT, get a local "rcmd" ticket and check it so as to
	 * ensure that we are not talking to a bogus Kerberos server.
	 *
	 * There are 2 cases where we still allow a login:
	 *	1: the VERIFY_SERVICE doesn't exist in the KDC
	 *	2: local host has no srvtab, as (hopefully) indicated by a
	 *	   return value of RD_AP_UNDEC from krb_rd_req().
	 */
	if (kerror != INTK_OK) {
		if (kerror != INTK_BADPW && kerror != KDC_PR_UNKNOWN) {
			syslog(LOG_ERR, "Kerberos intkt error: %s",
			    krb_err_txt[kerror]);
			dest_tkt();
		}
		return (1);
	}

	/*
	 * Set the owner of the ticket file to root but bail if someone
	 * has nefariously swapped a link in place of the file.
	 */
	fd = open(TKT_FILE, O_RDWR|O_NOFOLLOW, 0);
	if (fd == -1) {
		syslog(LOG_ERR, "unable to open ticket file: %m");
		dest_tkt();
		return (1);
	}
	if (fchown(fd, pw->pw_uid, pw->pw_gid) < 0)
		syslog(LOG_ERR, "fchown tkfile (%s): %m", TKT_FILE);
	close(fd);

	(void)strlcpy(savehost, krb_get_phost(localhost), sizeof(savehost));

	/*
	 * if the "VERIFY_SERVICE" doesn't exist in the KDC for this host,
	 * still allow login with tickets, but log the error condition.
	 */

	kerror = krb_mk_req(&ticket, VERIFY_SERVICE, savehost, realm, 33);
	if (kerror == KDC_PR_UNKNOWN) {
		syslog(LOG_NOTICE,
    		    "warning: TGT not verified (%s); %s.%s not registered, or srvtab is wrong?",
		    krb_err_txt[kerror], VERIFY_SERVICE, savehost);
		if (koktologin(pw->pw_name, instance, user))
			return (1);
		notickets = 0;
		/*
		 * but for security, don't allow root instances in under
		 * this condition!
		 */
		if (strcmp(instance, "root") == 0) {
		  syslog(LOG_ERR, "Kerberos %s root instance login refused",
			 pw->pw_name);
		  dest_tkt();
		  return (1);
		}
		/* Otherwise, leave ticket around, but make sure
		 * password matches the Unix password. */
		return (1);
	}

	if (kerror != KSUCCESS) {
		warnx("unable to use TGT: (%s)", krb_err_txt[kerror]);
		syslog(LOG_NOTICE, "unable to use TGT: (%s)",
		    krb_err_txt[kerror]);
		dest_tkt();
		return (1);
	}

	if (!(hp = gethostbyname(localhost))) {
		syslog(LOG_ERR, "couldn't get local host address");
		dest_tkt();
		return (1);
	}

	memmove((void *)&faddr, (void *)hp->h_addr, sizeof(faddr));

	kerror = krb_rd_req(&ticket, VERIFY_SERVICE, savehost, faddr,
	    &authdata, "");

	if (kerror == KSUCCESS) {
		if (koktologin(pw->pw_name, instance, user))
			return (1);

		notickets = 0;
		return (0);
	}

	/* undecipherable: probably didn't have a srvtab on the local host */
	if (kerror == RD_AP_UNDEC) {
		syslog(LOG_NOTICE, "krb_rd_req: (%s)", krb_err_txt[kerror]);
		dest_tkt();
		return (1);
	}
	/* failed for some other reason */
	warnx("unable to verify %s ticket: (%s)", VERIFY_SERVICE,
	    krb_err_txt[kerror]);
	syslog(LOG_NOTICE, "couldn't verify %s ticket: %s", VERIFY_SERVICE,
	    krb_err_txt[kerror]);
	dest_tkt();
	return (1);
}

int
koktologin(name, instance, user)
	char *name, *instance, *user;
{
	AUTH_DAT *kdata;
	AUTH_DAT kdata_st;

	if (realm[0] == '\0' && krb_get_lrealm(realm, 0) != KSUCCESS)
		return(1);

	if (always_use_klogin == 0 && *instance == '\0')
		return (0);

	kdata = &kdata_st;
	memset((char *)kdata, 0, sizeof(*kdata));
	(void)strlcpy(kdata->pname, name, sizeof(kdata->pname));
	(void)strlcpy(kdata->pinst, instance, sizeof(kdata->pinst));
	(void)strlcpy(kdata->prealm, realm, sizeof(kdata->prealm));
	if (kuserok(kdata, user)) {
		warnx("not in %s's ACL", user);
		dest_tkt();
		return (1);
	}
	return (0);
}

int
krb_configured()
{

	if (realm[0] != '\0')
		return(KSUCCESS);
	else
		return (krb_get_lrealm(realm, 0));
}
#endif
