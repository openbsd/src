/*	$OpenBSD: login_krb5.c,v 1.7 2001/06/25 15:49:21 hin Exp $	*/

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

#include <sys/types.h>
#include <sys/signal.h>
#include <sys/resource.h>
#include <sys/param.h>

#include <syslog.h>
#include <stdlib.h>
#include <unistd.h>
#include <login_cap.h>
#include <bsd_auth.h>
#include <stdio.h>
#include <string.h>
#include <pwd.h>
#include <err.h>
#include <util.h>

#include <kerberosV/krb5.h>
#ifdef KRB4
#include <kerberosIV/krb.h>
#endif

#define MODE_LOGIN 0
#define MODE_CHALLENGE 1
#define MODE_RESPONSE 2

#define AUTH_OK 0
#define AUTH_FAILED -1

FILE *back = NULL;

void
krb5_syslog(krb5_context context, int level, krb5_error_code code, char *fmt, ...) 
{
    va_list ap;
    char buf[256];
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    syslog(level, "%s: %s", buf, krb5_get_err_text(context, code));
}

int
krb5_login(char *username, char *password)
{
	int return_code = AUTH_FAILED;
	char *instance, *tmp_name;
	krb5_error_code ret;
	krb5_context context;
	krb5_ccache ccache;
	krb5_principal princ;

	if(username == NULL || password == NULL)
		return AUTH_FAILED;

	if(strcmp(username, "root") == 0)
		return AUTH_FAILED;

	ret = krb5_init_context(&context);
	if(ret != 0) {
		krb5_syslog(context, LOG_ERR, ret, "krb5_init_context");
		exit(1);
	}

	ret = krb5_cc_gen_new(context, &krb5_mcc_ops, &ccache);
	if(ret != 0) {
		krb5_syslog(context, LOG_ERR, ret, "krb5_cc_gen_new");
		exit(1);
	}

	ret = krb5_parse_name(context, username, &princ);
	if(ret != 0) {
		krb5_syslog(context, LOG_ERR, ret, "krb5_parse_name");
		exit(1);
	}

	instance = strchr(username, '/');
	if(instance != NULL) {
		*instance++ = '\0';
	} else
		instance = "";

	krb5_unparse_name(context, princ, &tmp_name);

	ret = krb5_verify_user_lrealm(context, princ, ccache, 
				      password,
				      1,	/* verify with keytab */
				      NULL);

	switch(ret) {
	case 0: {
		krb5_ccache ccache_store;
		struct passwd *pwd;
		int get_krb4_ticket = 0;
		char krb4_ticket_file[MAXPATHLEN];
		char cc_file[MAXPATHLEN];

		/*
		 * The only instance a user should be allowed to login with
		 * is "root".
		 */
		if((strcmp(instance, "root") == 0)) {
			if(krb5_kuserok(context, princ, "root"))
				fprintf(back, BI_AUTH " root\n");
			else {
				fprintf(back, BI_REJECT "\n");
				exit(0);
			}
		} else if(strlen(instance) != 0) {
			fprintf(back, BI_REJECT "\n");
			exit(0);
		}

		pwd = getpwnam(username);
		if(pwd == NULL) {
			krb5_syslog(context, LOG_ERR, ret,
				    "%s: no such user", username);
		}
		snprintf(cc_file, sizeof(cc_file), "FILE:/tmp/krb5cc_%d",
			 pwd->pw_uid);

		ret = krb5_cc_resolve(context, cc_file, &ccache_store);
		if(ret != 0) {
			krb5_syslog(context, LOG_ERR, ret, "krb5_cc_gen_new");
			exit(1);
		}

		ret = krb5_cc_copy_cache(context, ccache, ccache_store);
		if(ret != 0) {
			krb5_syslog(context, LOG_ERR, ret,
				    "krb5_cc_copy_cache");
		}

#ifdef KRB4
		get_krb4_ticket =
			krb5_config_get_bool_default (context, NULL,
						      get_krb4_ticket,
						      "libdefaults",
						      "krb4_get_tickets",
						      NULL);
#if 1
		if(get_krb4_ticket) {
			CREDENTIALS c;
			krb5_creds cred;
			krb5_cc_cursor cursor;

			ret = krb5_cc_start_seq_get(context, ccache, &cursor);
			if(ret != 0) {
				krb5_syslog(context, LOG_ERR, ret,
					    "start seq");
				exit(1);
			}

			ret = krb5_cc_next_cred(context, ccache, &cursor,
						&cred);
			if(ret != 0) {
				krb5_syslog(context, LOG_ERR, ret,
					    "next cred");
				exit(1);
			}

			ret = krb5_cc_end_seq_get(context, ccache, &cursor);
			if(ret != 0) {
				krb5_syslog(context, LOG_ERR, ret, "end seq");
				exit(1);
			}
			
			ret = krb524_convert_creds_kdc(context, ccache, &cred,
						       &c);
			if(ret != 0) {
				krb5_syslog(context, LOG_ERR, ret, "convert");
			} else {
				snprintf(krb4_ticket_file,
					 sizeof(krb4_ticket_file),
					 "%s%d", TKT_ROOT, pwd->pw_uid);
				krb_set_tkt_string(krb4_ticket_file);
				tf_setup(&c, c.pname, c.pinst);
				chown(krb4_ticket_file,
				      pwd->pw_uid, pwd->pw_gid);
			}
		}
#endif
#endif

		if(strcmp(instance, "root") == 0) {
		} else {
			/* Need to chown the ticket file */
			chown(krb5_cc_get_name(context, ccache_store),
			      pwd->pw_uid, pwd->pw_gid);
		}

		fprintf(back, BI_AUTH "\n");
		
		fprintf(back, BI_SETENV " KRB5CCNAME %s:%s\n",
			krb5_cc_get_type(context, ccache_store),
			krb5_cc_get_name(context, ccache_store));
#ifdef KRB4
		if(get_krb4_ticket)
			fprintf(back, BI_SETENV " KRBTKFILE %s\n",
				krb4_ticket_file);
#endif

		return_code = AUTH_OK;
		break;
	}
	case KRB5KRB_AP_ERR_MODIFIED:
		/* XXX syslog here? */
	case KRB5KRB_AP_ERR_BAD_INTEGRITY:
		break;
	default:
		krb5_syslog(context, LOG_ERR, ret, "verify");
		break;
	}

	krb5_free_context(context);
	krb5_free_principal(context, princ);
	krb5_cc_close(context, ccache);

	return return_code;
}

#ifdef PASSWD
int
pwd_login(char *username, char *password)
{
	struct passwd *pwd;
	char *salt;

	pwd = getpwnam(username);

	/* Check for empty password */
	if((pwd != NULL) && (*pwd->pw_passwd == '\0')) {
		fprintf(back, BI_AUTH "\n");
		return AUTH_OK;
	}

	if(pwd)
		salt = pwd->pw_passwd;
	else
		salt = "xx";

	setpriority(PRIO_PROCESS, 0, -4);

	salt = crypt(password, salt);
	memset(password, 0, strlen(password));
	if (!pwd || strcmp(salt, pwd->pw_passwd) != 0)
		return AUTH_FAILED;

	if(login_check_expire(back, pwd, NULL, 0))
		return AUTH_FAILED;

	fprintf(back, BI_AUTH "\n");

	return AUTH_OK;
}
#endif PASSWD

int
main(int argc, char **argv)
{
	int opt, mode = 0, ret;
	char *username, *password = NULL;
	char response[1024];

	signal(SIGQUIT, SIG_IGN);
	signal(SIGINT, SIG_IGN);
	setpriority(PRIO_PROCESS, 0, 0);

	openlog(NULL, LOG_ODELAY, LOG_AUTH);

	while((opt = getopt(argc, argv, "ds:v:")) != -1) {
		switch(opt) {
		case 'd':
			back = stdout;
			break;
		case 's':	/* service */
			if(strcmp(optarg, "login") == 0)
				mode = MODE_LOGIN;
			else if(strcmp(optarg, "challenge") == 0)
				mode = MODE_CHALLENGE;
			else if(strcmp(optarg, "response") == 0)
				mode = MODE_RESPONSE;
			else {
				syslog(LOG_ERR, "%s: invalid service", optarg);
				exit(1);
			}
			break;
		case 'v':
			/* silently ignore -v options */
			break;
		default:
			syslog(LOG_ERR, "usage error1");
			exit(1);
		}
	}
	switch(argc - optind) {
	case 2:
		/* class = argv[optind + 1]; */
	case 1:
		username = argv[optind];
		break;
	default:
		syslog(LOG_ERR, "usage error2");
		exit(1);
	}

	if(back == NULL && (back = fdopen(3, "r+")) == NULL) {
		syslog(LOG_ERR, "reopening back channel: %m");
		exit(1);
	}

	/*
	 * Read password, either as from the terminal or if the
	 * response mode is active from the caller program.
	 *
	 * XXX  This is completely ungrokkable, and should be rewritten.
	 */
	if(mode == MODE_RESPONSE) {
		int count;
		mode = 0;
		count = -1;
		while(++count < sizeof(response) &&
		      read(3, &response[count], 1) == 1) {
			if(response[count] == '\0' && ++mode == 2)
				break;
			if(response[count] == '\0' && mode == 1) {
				password = response + count + 1;
			}
		}
		if(mode < 2) {
			syslog(LOG_ERR, "protocol error on back channel");
			exit(1);
		}
	} else
		password = getpass("Password:");

	ret = krb5_login(username, password);
#ifdef PASSWD
	if(ret != AUTH_OK)
		ret = pwd_login(username, password);
#endif
	memset(password, 0, strlen(password));
	if(ret != AUTH_OK)
		fprintf(back, BI_REJECT "\n");

	closelog();

	return 0;
}
