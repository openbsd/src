/*

auth-passwd.c

Author: Tatu Ylonen <ylo@cs.hut.fi>

Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
                   All rights reserved

Created: Sat Mar 18 05:11:38 1995 ylo

Password authentication.  This file contains the functions to check whether
the password is valid for the user.

*/

#include "includes.h"
RCSID("$Id: auth-passwd.c,v 1.2 1999/09/29 18:16:19 dugsong Exp $");

#ifdef HAVE_SCO_ETC_SHADOW
# include <sys/security.h>
# include <sys/audit.h>
# include <prot.h>
#else /* HAVE_SCO_ETC_SHADOW */
#ifdef HAVE_ETC_SHADOW
#include <shadow.h>
#endif /* HAVE_ETC_SHADOW */
#endif /* HAVE_SCO_ETC_SHADOW */
#ifdef HAVE_ETC_SECURITY_PASSWD_ADJUNCT
#include <sys/label.h>
#include <sys/audit.h>
#include <pwdadj.h>
#endif /* HAVE_ETC_SECURITY_PASSWD_ADJUNCT */
#include "packet.h"
#include "ssh.h"
#include "servconf.h"
#include "xmalloc.h"

#ifdef HAVE_SECURID
/* Support for Security Dynamics SecurID card.
   Contributed by Donald McKillican <dmckilli@qc.bell.ca>. */
#define SECURID_USERS "/etc/securid.users"
#include "sdi_athd.h"
#include "sdi_size.h"
#include "sdi_type.h"
#include "sdacmvls.h"
#include "sdconf.h"
union config_record configure;
static int securid_initialized = 0;
#endif /* HAVE_SECURID */

#ifdef KRB4
extern char *ticket;
#endif /* KRB4 */

/* Tries to authenticate the user using password.  Returns true if
   authentication succeeds. */

int auth_password(const char *server_user, const char *password)
{
  extern ServerOptions options;
  extern char *crypt(const char *key, const char *salt);
  struct passwd *pw;
  char *encrypted_password;
  char correct_passwd[200];

  if (*password == '\0' && options.permit_empty_passwd == 0)
  {
      packet_send_debug("Server does not permit empty password login.");
      return 0;
  }

  /* Get the encrypted password for the user. */
  pw = getpwnam(server_user);
  if (!pw)
    return 0;

#if defined(KRB4)
  /* Support for Kerberos v4 authentication - Dug Song <dugsong@UMICH.EDU> */
  if (options.kerberos_authentication)
    {
      AUTH_DAT adata;
      KTEXT_ST tkt;
      struct hostent *hp;
      unsigned long faddr;
      char localhost[MAXHOSTNAMELEN];	/* local host name */
      char phost[INST_SZ];		/* host instance */
      char realm[REALM_SZ];		/* local Kerberos realm */
      int r;
      
      /* Try Kerberos password authentication only for non-root
	 users and only if Kerberos is installed. */
      if (pw->pw_uid != 0 && krb_get_lrealm(realm, 1) == KSUCCESS) {

	/* Set up our ticket file. */
	if (!ssh_tf_init(pw->pw_uid)) {
	  log("Couldn't initialize Kerberos ticket file for %s!",
	      server_user);
	  goto kerberos_auth_failure;
	}
	/* Try to get TGT using our password. */
	r = krb_get_pw_in_tkt((char *)server_user, "", realm, "krbtgt", realm,
			      DEFAULT_TKT_LIFE, (char *)password);
	if (r != INTK_OK) {
	  packet_send_debug("Kerberos V4 password authentication for %s "
			    "failed: %s", server_user, krb_err_txt[r]);
	  goto kerberos_auth_failure;
	}
	/* Successful authentication. */
	chown(ticket, pw->pw_uid, pw->pw_gid);
	
	(void) gethostname(localhost, sizeof(localhost));
	(void) strncpy(phost, (char *)krb_get_phost(localhost), INST_SZ);
	phost[INST_SZ-1] = 0;
	
	/* Now that we have a TGT, try to get a local "rcmd" ticket to
	   ensure that we are not talking to a bogus Kerberos server. */
	r = krb_mk_req(&tkt, KRB4_SERVICE_NAME, phost, realm, 33);

	if (r == KSUCCESS) {
	  if (!(hp = gethostbyname(localhost))) {
	    log("Couldn't get local host address!");
	    goto kerberos_auth_failure;
	  }
	  memmove((void *)&faddr, (void *)hp->h_addr, sizeof(faddr));

	  /* Verify our "rcmd" ticket. */
	  r = krb_rd_req(&tkt, KRB4_SERVICE_NAME, phost, faddr, &adata, "");
	  if (r == RD_AP_UNDEC) {
	    /* Probably didn't have a srvtab on localhost. Allow login. */
	    log("Kerberos V4 TGT for %s unverifiable, no srvtab installed? "
		"krb_rd_req: %s", server_user, krb_err_txt[r]);
	  }
	  else if (r != KSUCCESS) {
	    log("Kerberos V4 %s ticket unverifiable: %s",
		KRB4_SERVICE_NAME, krb_err_txt[r]);
	    goto kerberos_auth_failure;
	  }
	}
	else if (r == KDC_PR_UNKNOWN) {
	  /* Allow login if no rcmd service exists, but log the error. */
	  log("Kerberos V4 TGT for %s unverifiable: %s; %s.%s "
	      "not registered, or srvtab is wrong?", server_user,
	      krb_err_txt[r], KRB4_SERVICE_NAME, phost);
	}
	else {
	  /* TGT is bad, forget it. Possibly spoofed! */
	  packet_send_debug("WARNING: Kerberos V4 TGT possibly spoofed for"
			    "%s: %s", server_user, krb_err_txt[r]);
	  goto kerberos_auth_failure;
	}
	
	/* Authentication succeeded. */
	return 1;
	
      kerberos_auth_failure:
	(void) dest_tkt();
	xfree(ticket);
	ticket = NULL;
	if (!options.kerberos_or_local_passwd ) return 0;
      }
      else {
	/* Logging in as root or no local Kerberos realm. */
	packet_send_debug("Unable to authenticate to Kerberos.");
      }
      /* Fall back to ordinary passwd authentication. */
    }
#endif /* KRB4 */

#ifdef HAVE_SECURID
  /* Support for Security Dynamics SecurId card.
     Contributed by Donald McKillican <dmckilli@qc.bell.ca>. */
  {
    /*
     * the way we decide if this user is a securid user or not is
     * to check to see if they are included in /etc/securid.users
     */
    int found = 0;
    FILE *securid_users = fopen(SECURID_USERS, "r");
    char *c;
    char su_user[257];
    
    if (securid_users)
      {
	while (fgets(su_user, sizeof(su_user), securid_users))
	  {
	    if (c = strchr(su_user, '\n')) 
	      *c = '\0';
	    if (strcmp(su_user, server_user) == 0) 
	      { 
		found = 1; 
		break; 
	      }
	  }
      }
    fclose(securid_users);

    if (found)
      {
	/* The user has a SecurID card. */
	struct SD_CLIENT sd_dat, *sd;
	log("SecurID authentication for %.100s required.", server_user);

	/*
	 * if no pass code has been supplied, fail immediately: passing
	 * a null pass code to sd_check causes a core dump
	 */
	if (*password == '\0') 
	  {
	    log("No pass code given, authentication rejected.");
	    return 0;
	  }

	sd = &sd_dat;
	if (!securid_initialized)
	  {
	    memset(&sd_dat, 0, sizeof(sd_dat));   /* clear struct */
	    creadcfg();		/*  accesses sdconf.rec  */
	    if (sd_init(sd)) 
	      packet_disconnect("Cannot contact securid server.");
	    securid_initialized = 1;
	  }
	return sd_check(password, server_user, sd) == ACM_OK;
      }
  }
  /* If the user has no SecurID card specified, we fall to normal 
     password code. */
#endif /* HAVE_SECURID */

  /* Save the encrypted password. */
  strncpy(correct_passwd, pw->pw_passwd, sizeof(correct_passwd));

#ifdef HAVE_OSF1_C2_SECURITY
    osf1c2_getprpwent(correct_passwd, pw->pw_name, sizeof(correct_passwd));
#else /* HAVE_OSF1_C2_SECURITY */
  /* If we have shadow passwords, lookup the real encrypted password from
     the shadow file, and replace the saved encrypted password with the
     real encrypted password. */
#ifdef HAVE_SCO_ETC_SHADOW
  {
    struct pr_passwd *pr = getprpwnam(pw->pw_name);
    pr = getprpwnam(pw->pw_name);
    if (pr)
      strncpy(correct_passwd, pr->ufld.fd_encrypt, sizeof(correct_passwd));
    endprpwent();
  }
#else /* HAVE_SCO_ETC_SHADOW */
#ifdef HAVE_ETC_SHADOW
  {
    struct spwd *sp = getspnam(pw->pw_name);
    if (sp)
      strncpy(correct_passwd, sp->sp_pwdp, sizeof(correct_passwd));
    endspent();
  }
#else /* HAVE_ETC_SHADOW */
#ifdef HAVE_ETC_SECURITY_PASSWD_ADJUNCT
  {
    struct passwd_adjunct *sp = getpwanam(pw->pw_name);
    if (sp)
      strncpy(correct_passwd, sp->pwa_passwd, sizeof(correct_passwd));
    endpwaent();
  }
#else /* HAVE_ETC_SECURITY_PASSWD_ADJUNCT */
#ifdef HAVE_ETC_SECURITY_PASSWD
  {
    FILE *f;
    char line[1024], looking_for_user[200], *cp;
    int found_user = 0;
    f = fopen("/etc/security/passwd", "r");
    if (f)
      {
	sprintf(looking_for_user, "%.190s:", server_user);
	while (fgets(line, sizeof(line), f))
	  {
	    if (strchr(line, '\n'))
	      *strchr(line, '\n') = 0;
	    if (strcmp(line, looking_for_user) == 0)
	      found_user = 1;
	    else
	      if (line[0] != '\t' && line[0] != ' ')
		found_user = 0;
	      else
		if (found_user)
		  {
		    for (cp = line; *cp == ' ' || *cp == '\t'; cp++)
		      ;
		    if (strncmp(cp, "password = ", strlen("password = ")) == 0)
		      {
			strncpy(correct_passwd, cp + strlen("password = "), 
				sizeof(correct_passwd));
			correct_passwd[sizeof(correct_passwd) - 1] = 0;
			break;
		      }
		  }
	  }
	fclose(f);
      }
  }
#endif /* HAVE_ETC_SECURITY_PASSWD */
#endif /* HAVE_ETC_SECURITY_PASSWD_ADJUNCT */
#endif /* HAVE_ETC_SHADOW */
#endif /* HAVE_SCO_ETC_SHADOW */
#endif /* HAVE_OSF1_C2_SECURITY */

  /* Check for users with no password. */
  if (strcmp(password, "") == 0 && strcmp(correct_passwd, "") == 0)
    {
      packet_send_debug("Login permitted without a password because the account has no password.");
      return 1; /* The user has no password and an empty password was tried. */
    }

  /* Encrypt the candidate password using the proper salt. */
#ifdef HAVE_OSF1_C2_SECURITY
  encrypted_password = (char *)osf1c2crypt(password,
                                   (correct_passwd[0] && correct_passwd[1]) ?
                                   correct_passwd : "xx");
#else /* HAVE_OSF1_C2_SECURITY */
#ifdef HAVE_SCO_ETC_SHADOW
  encrypted_password = bigcrypt(password, 
			     (correct_passwd[0] && correct_passwd[1]) ?
			     correct_passwd : "xx");
#else /* HAVE_SCO_ETC_SHADOW */
  encrypted_password = crypt(password, 
			     (correct_passwd[0] && correct_passwd[1]) ?
			     correct_passwd : "xx");
#endif /* HAVE_SCO_ETC_SHADOW */
#endif /* HAVE_OSF1_C2_SECURITY */

  /* Authentication is accepted if the encrypted passwords are identical. */
  return (strcmp(encrypted_password, correct_passwd) == 0);
}
