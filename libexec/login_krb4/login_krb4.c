/*	$OpenBSD: login_krb4.c,v 1.5 2002/09/06 18:45:06 deraadt Exp $	*/

/*-
 * Copyright (c) 2001 Hans Insulander <hin@openbsd.org>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "common.h"
#include <fcntl.h>

#include <kerberosIV/krb.h>

int
krb4_login(char *username, char *password, char *invokinguser, int new_tickets)
{
	char realm[REALM_SZ];
	char tkfile[MAXPATHLEN];
	char *instance, *targetuser;
	struct passwd *pwd;
	int ret, fd;

	/* Check if we can open the srvtab file */
	if ((fd = open(KEYFILE, O_RDONLY, 0400)) < 0)
		return (AUTH_FAILED);
	close(fd);

	pwd = getpwnam(username);
	tkfile[0] = '\0';

	targetuser = username;
	if (krb_get_lrealm(realm, 1))
		syslog(LOG_INFO, "krb_get_lrealm failed");

	if (new_tickets) {
		snprintf(tkfile, sizeof(tkfile), "%s%d", TKT_ROOT,
		    pwd ? pwd->pw_uid : getuid());
		krb_set_tkt_string(tkfile);
		unlink(tkfile);
	}

	if (strcmp(username, "root") == 0) {
		instance = "root";
		username = invokinguser;
	} else
		instance = "";

	/*
	 * This kludge is needed because the krb library checks if it seems
	 * to be running as a setuid program, due to problems with setuid
	 * programs and environment variables.
	 *
	 * But in this case it's okay, because the login scripts are called
	 * with a clean environment.
	 */
	setuid(geteuid());
	ret = krb_verify_user(username, instance , realm, password, 1, "rcmd");

	if (new_tickets && pwd)
		chown(tkfile, pwd->pw_uid, pwd->pw_gid);

	if (ret == KSUCCESS &&
	    krb_kuserok(username, instance, realm, targetuser) == 0) {
		fprintf(back, BI_AUTH "\n");
		if (strlen(tkfile) > 0)
			fprintf(back, BI_SETENV " KRBTKFILE %s\n", tkfile);
		return (AUTH_OK);
	}
	unlink(tkfile);
	return (AUTH_FAILED);
}
