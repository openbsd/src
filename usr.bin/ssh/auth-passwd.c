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
RCSID("$Id: auth-passwd.c,v 1.5 1999/09/30 04:30:03 deraadt Exp $");

#include "packet.h"
#include "ssh.h"
#include "servconf.h"
#include "xmalloc.h"

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
  char *saved_pw_name, *saved_pw_passwd;

  if (*password == '\0' && options.permit_empty_passwd == 0)
  {
      packet_send_debug("Server does not permit empty password login.");
      return 0;
  }

  /* Get the encrypted password for the user. */
  pw = getpwnam(server_user);
  if (!pw)
    return 0;

  saved_pw_name = xstrdup(pw->pw_name);
  saved_pw_passwd = xstrdup(pw->pw_passwd);
  
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
	(void) strlcpy(phost, (char *)krb_get_phost(localhost), INST_SZ);
	
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

  /* Save the encrypted password. */
  strlcpy(correct_passwd, saved_pw_passwd, sizeof(correct_passwd));

  /* Check for users with no password. */
  if (strcmp(password, "") == 0 && strcmp(correct_passwd, "") == 0)
    {
      packet_send_debug("Login permitted without a password because the account has no password.");
      return 1; /* The user has no password and an empty password was tried. */
    }

  xfree(saved_pw_name);
  xfree(saved_pw_passwd);
  
  /* Encrypt the candidate password using the proper salt. */
  encrypted_password = crypt(password, 
			     (correct_passwd[0] && correct_passwd[1]) ?
			     correct_passwd : "xx");

  /* Authentication is accepted if the encrypted passwords are identical. */
  return (strcmp(encrypted_password, correct_passwd) == 0);
}
