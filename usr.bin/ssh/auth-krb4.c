/*

   auth-kerberos.c

   Hacked together by Dug Song <dugsong@umich.edu>.

   Kerberos authentication and ticket-passing routines.

*/

#include "includes.h"
#include "packet.h"
#include "xmalloc.h"
#include "ssh.h"

#ifdef KRB4
#include <sys/param.h>
#include <krb.h>

int ssh_tf_init(uid_t uid)
{
  extern char *ticket;
  char *tkt_root = TKT_ROOT;
  struct stat st;
  int fd;

  /* Set unique ticket string manually since we're still root. */
  ticket = xmalloc(MAXPATHLEN);
#ifdef AFS
  if (lstat("/ticket", &st) != -1)
    tkt_root = "/ticket/";
#endif /* AFS */
  sprintf(ticket, "%.100s%d_%d", tkt_root, uid, getpid());
  (void) krb_set_tkt_string(ticket);

  /* Make sure we own this ticket file, and we created it. */
  if (lstat(ticket, &st) < 0 && errno == ENOENT) {
    /* good, no ticket file exists. create it. */
    if ((fd = open(ticket, O_RDWR|O_CREAT|O_EXCL, 0600)) != -1) {
      close(fd);
      return 1;
    }
  }
  else {
    /* file exists. make sure server_user owns it (e.g. just passed ticket),
       and that it isn't a symlink, and that it is mode 600. */
    if (st.st_mode == (S_IFREG|S_IRUSR|S_IWUSR) && st.st_uid == uid)
      return 1;
  }
  /* Failure. */
  log("WARNING: bad ticket file %.100s", ticket);
  return 0;
}

int auth_krb4(const char *server_user, KTEXT auth, char **client)
{
  AUTH_DAT adat   = { 0 };
  KTEXT_ST reply;
  char instance[INST_SZ];
  int r, s;
  u_long cksum;
  Key_schedule schedule;
  struct sockaddr_in local, foreign;
  
  s = packet_get_connection_in();
  
  r = sizeof(local);
  memset(&local, 0, sizeof(local));
  if (getsockname(s, (struct sockaddr *) &local, &r) < 0)
    debug("getsockname failed: %.100s", strerror(errno));
  r = sizeof(foreign);
  memset(&foreign, 0, sizeof(foreign));
  if (getpeername(s, (struct sockaddr *)&foreign, &r) < 0)
    debug("getpeername failed: %.100s", strerror(errno));
  
  instance[0] = '*'; instance[1] = 0;
  
  /* Get the encrypted request, challenge, and session key. */
  r = krb_rd_req(auth, KRB4_SERVICE_NAME, instance, 0, &adat, "");
  if (r != KSUCCESS) {
    packet_send_debug("Kerberos V4 krb_rd_req: %.100s", krb_err_txt[r]);
    return 0;
  }
  des_key_sched((des_cblock *)adat.session, schedule);
  
  *client = xmalloc(MAX_K_NAME_SZ);
  sprintf(*client, "%.100s%.100s%.100s@%.100s", adat.pname, *adat.pinst ? "." : "",
		 adat.pinst, adat.prealm);

  /* Check ~/.klogin authorization now. */
  if (kuserok(&adat, (char *)server_user) != KSUCCESS) {
    packet_send_debug("Kerberos V4 .klogin authorization failed!");
    log("Kerberos V4 .klogin authorization failed for %.100s to account %.100s",
	*client, server_user);
    return 0;
  }
  /* Increment the checksum, and return it encrypted with the session key. */
  cksum = adat.checksum + 1;
  cksum = htonl(cksum);
  
  /* If we can't successfully encrypt the checksum, we send back an empty
     message, admitting our failure. */
  if ((r = krb_mk_priv((u_char *)&cksum, reply.dat, sizeof(cksum)+1,
		       schedule, &adat.session, &local, &foreign)) < 0) {
    packet_send_debug("Kerberos V4 mk_priv: (%d) %.100s", r, krb_err_txt[r]);
    reply.dat[0] = 0;
    reply.length = 0;
  }
  else
    reply.length = r;
  
  /* Clear session key. */
  memset(&adat.session, 0, sizeof(&adat.session));
  
  packet_start(SSH_SMSG_AUTH_KERBEROS_RESPONSE);
  packet_put_string((char *) reply.dat, reply.length);
  packet_send();
  packet_write_wait();
  return 1;
}
#endif /* KRB4 */

#ifdef AFS
#include <kafs.h>


#ifdef KERBEROS_TGT_PASSING

int auth_kerberos_tgt(struct passwd *pw, const char *string)
{
  CREDENTIALS creds;
  extern char *ticket;
  int r;
  
  if (!radix_to_creds(string, &creds)) {
    log("Protocol error decoding Kerberos V4 tgt");
    packet_send_debug("Protocol error decoding Kerberos V4 tgt");
    goto auth_kerberos_tgt_failure;
  }
  if (strncmp(creds.service, "", 1) == 0) /* backward compatibility */
    strcpy(creds.service, "krbtgt");
  
  if (strcmp(creds.service, "krbtgt")) {
    log("Kerberos V4 tgt (%.100s%.100s%.100s@%.100s) rejected for uid %d",
	creds.pname, creds.pinst[0] ? "." : "", creds.pinst, creds.realm,
	pw->pw_uid);
    packet_send_debug("Kerberos V4 tgt (%.100s%.100s%.100s@%.100s) rejected for uid %d",
		      creds.pname, creds.pinst[0] ? "." : "", creds.pinst,
		      creds.realm, pw->pw_uid);
    goto auth_kerberos_tgt_failure;
  }
  if (!ssh_tf_init(pw->pw_uid) ||
      (r = in_tkt(creds.pname, creds.pinst)) ||
      (r = save_credentials(creds.service,creds.instance,creds.realm,
			    creds.session,creds.lifetime,creds.kvno,
			    &creds.ticket_st,creds.issue_date))) {
    xfree(ticket);
    ticket = NULL;
    packet_send_debug("Kerberos V4 tgt refused: couldn't save credentials");
    goto auth_kerberos_tgt_failure;
  }
  /* Successful authentication, passed all checks. */
  chown(ticket, pw->pw_uid, pw->pw_gid);
  packet_send_debug("Kerberos V4 ticket accepted (%.100s.%.100s@%.100s, %.100s%.100s%.100s@%.100s)",
		    creds.service, creds.instance, creds.realm,
		    creds.pname, creds.pinst[0] ? "." : "",
		    creds.pinst, creds.realm);
  
  packet_start(SSH_SMSG_SUCCESS);
  packet_send();
  packet_write_wait();
  return 1;

auth_kerberos_tgt_failure:
  memset(&creds, 0, sizeof(creds));
  packet_start(SSH_SMSG_FAILURE);
  packet_send();
  packet_write_wait();
  return 0;
}
#endif /* KERBEROS_TGT_PASSING */

int auth_afs_token(char *server_user, uid_t uid, const char *string)
{
  CREDENTIALS creds;

  if (!radix_to_creds(string, &creds)) {
    log("Protocol error decoding AFS token");
    packet_send_debug("Protocol error decoding AFS token");
    packet_start(SSH_SMSG_FAILURE);
    packet_send();
    packet_write_wait();
    return 0;
  }
  if (strncmp(creds.service, "", 1) == 0) /* backward compatibility */
    strcpy(creds.service, "afs");
  
  if (strncmp(creds.pname, "AFS ID ", 7) == 0)
    uid = atoi(creds.pname + 7);
  
  if (kafs_settoken(creds.realm, uid, &creds)) {
    log("AFS token (%.100s@%.100s) rejected for uid %d",
	creds.pname, creds.realm, uid);
    packet_send_debug("AFS token (%.100s@%.100s) rejected for uid %d", creds.pname,
		      creds.realm, uid);
    packet_start(SSH_SMSG_FAILURE);
    packet_send();
    packet_write_wait();
    return 0;
  }
  packet_send_debug("AFS token accepted (%.100s@%.100s, %.100s@%.100s)", creds.service,
		    creds.realm, creds.pname, creds.realm);
  packet_start(SSH_SMSG_SUCCESS);
  packet_send();
  packet_write_wait();
  return 1;
}
#endif /* AFS */
