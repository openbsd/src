/*	$Id: kpropd.c,v 1.3 1996/09/16 18:48:56 millert Exp $	*/

/*-
 * Copyright 1987 by the Massachusetts Institute of Technology.
 *
 * For copying and distribution information,
 * please see the file <mit-copyright.h>.
 */

/*
 * This program is run on slave servers, to catch updates "pushed"
 * from the master kerberos server in a realm.
 */

#include <slav_locl.h>
#include <kprop.h>
#include <sys/param.h>

static char *kdb_util_path = "kdb_util";

static char kprop_version[KPROP_PROT_VERSION_LEN] = KPROP_PROT_VERSION;

int     debug = 0;

char   *logfile = K_LOGFIL;

char    errmsg[256];
int     pause_int = 300;	/* 5 minutes in seconds */

static char    buf[KPROP_BUFSIZ+64 /* leave room for private msg overhead */];

static void 
SlowDeath(void)
{
  klog(L_KRB_PERR, "kpropd will pause before dying so as not to loop init");
  sleep(pause_int);
  klog(L_KRB_PERR, "AAAAAHHHHhhhh....");
  exit(1);
}

static void
usage(void)
{
  fprintf(stderr, "\nUsage: kpropd [-r realm] [-s srvtab] [-l logfile] fname\n\n");
  SlowDeath();
}

static void
recv_auth (int in, int out, int private, struct sockaddr_in *remote, struct sockaddr_in *local, AUTH_DAT *ad)
{
  u_long length;
  long kerror;
  int n;
  MSG_DAT msg_data;
  des_key_schedule session_sched;

  if (private)
#ifdef NOENCRYPTION
    bzero((char *)session_sched, sizeof(session_sched));
#else
  if (des_key_sched (&ad->session, session_sched)) {
    klog (L_KRB_PERR, "kpropd: can't make key schedule");
    SlowDeath();
  }
#endif

  while (1) {
    n = krb_net_read (in, (char *)&length, sizeof length);
    if (n == 0) break;
    if (n < 0) {
      snprintf (errmsg, sizeof(errmsg), "kpropd: read: %s", strerror(errno));
      klog (L_KRB_PERR, errmsg);
      SlowDeath();
    }
    length = ntohl (length);
    if (length > sizeof buf) {
      snprintf (errmsg, sizeof(errmsg),
		"kpropd: read length %ld, bigger than buf %d",
	        length, (int)(sizeof(buf)));
      klog (L_KRB_PERR, errmsg);
      SlowDeath();
    }
    n = krb_net_read(in, buf, length);
    if (n < 0) {
      snprintf(errmsg, sizeof(errmsg), "kpropd: read: %s", strerror(errno));
      klog(L_KRB_PERR, errmsg);
      SlowDeath();
    }
    if (private)
      kerror = krb_rd_priv (buf, n, session_sched, &ad->session, 
			    remote, local, &msg_data);
    else
      kerror = krb_rd_safe (buf, n, &ad->session,
			    remote, local, &msg_data);
    if (kerror != KSUCCESS) {
      snprintf (errmsg, sizeof(errmsg), "kpropd: %s: %s",
	       private ? "krb_rd_priv" : "krb_rd_safe",
	       krb_err_txt[kerror]);
      klog (L_KRB_PERR, errmsg);
      SlowDeath();
    }
    if (write(out, msg_data.app_data, msg_data.app_length) != 
	msg_data.app_length) {
      snprintf(errmsg, sizeof(errmsg), "kpropd: write: %s", strerror(errno));
      klog(L_KRB_PERR, errmsg);
      SlowDeath();
    }
  }
}

static void
recv_clear (int in, int out)
{
  int n;

  while (1) {
    n = read (in, buf, sizeof buf);
    if (n == 0) break;
    if (n < 0) {
      snprintf (errmsg, sizeof(errmsg), "kpropd: read: %s", strerror(errno));
      klog (L_KRB_PERR, errmsg);
      SlowDeath();
    }
    if (write(out, buf, n) != n) {
      snprintf(errmsg, sizeof(errmsg), "kpropd: write: %s", strerror(errno));
      klog(L_KRB_PERR, errmsg);
      SlowDeath();
    }
  }
}

int
main(int argc, char **argv)
{
  struct sockaddr_in from;
  struct sockaddr_in sin;
  struct servent *sp;
  int     s, s2, fd, n, fdlock;
  int     from_len;
  char    local_file[256];
  char    local_temp[256];
  struct hostent *hp;
  char    *dot, admin[MAXHOSTNAMELEN];
  char    hostname[MAXHOSTNAMELEN];
  char    from_str[128];
  long    kerror;
  AUTH_DAT auth_dat;
  KTEXT_ST ticket;
  char my_instance[INST_SZ];
  char my_realm[REALM_SZ];
  char cmd[1024];
  short net_transfer_mode, transfer_mode;
  des_key_schedule session_sched;
  char version[9];
  int c;
  extern char *optarg;
  extern int optind;
  int rflag = 0;
  char *srvtab = "";
  char *local_db = DBM_FILE;

  if (argv[argc - 1][0] == 'k' && isdigit(argv[argc - 1][1])) {
    argc--;			/* ttys file hack */
  }
  while ((c = getopt(argc, argv, "r:s:d:l:p:P:")) != EOF) {
    switch(c) {
    case 'r':
      rflag++;
      strcpy(my_realm, optarg);
      break;
    case 's':
      srvtab = optarg;
      break;
    case 'd':
      local_db = optarg;
      break;
    case 'l':
      logfile = optarg;
      break;	    
    case 'p':
    case 'P':
      kdb_util_path = optarg;
      break;
    default:
      usage();
      break;
    }
  }
  if (optind != argc-1)
    usage();

  kset_logfile(logfile);

  klog(L_KRB_PERR, "\n\n***** kpropd started *****");

  strcpy(local_file, argv[optind]);
  strcat(strcpy(local_temp, argv[optind]), ".tmp");

  if ((sp = getservbyname("krb_prop", "tcp")) == NULL) {
    klog(L_KRB_PERR, "kpropd: tcp/krb_prop: unknown service.");
    SlowDeath();
  }
  bzero((char *)&sin, sizeof(sin));
  sin.sin_port = sp->s_port;
  sin.sin_family = AF_INET;

  if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    snprintf(errmsg, sizeof(errmsg), "kpropd: socket: %s", strerror(errno));
    klog(L_KRB_PERR, errmsg);
    SlowDeath();
  }
  if (bind(s, (struct sockaddr*)&sin, sizeof sin) < 0) {
    snprintf(errmsg, sizeof(errmsg), "kpropd: bind: %s", strerror(errno));
    klog(L_KRB_PERR, errmsg);
    SlowDeath();
  }
 
  if (!rflag) {
    kerror = krb_get_lrealm(my_realm,1);
    if (kerror != KSUCCESS) {
      snprintf(errmsg, sizeof(errmsg), "kpropd: Can't get local realm. %s",
	       krb_err_txt[kerror]);
      klog (L_KRB_PERR, errmsg);
      SlowDeath();
    }
  }
    
  /* Responder uses KPROP_SERVICE_NAME.'hostname' and requestor always
   uses KPROP_SERVICE_NAME.KRB_MASTER (rcmd.kerberos) */
  strcpy(my_instance, "*");
    
  klog(L_KRB_PERR, "Established socket");

  listen(s, 5);
  for (;;) {
    from_len = sizeof from;
    if ((s2 = accept(s, (struct sockaddr *) &from, &from_len)) < 0) {
      snprintf(errmsg, sizeof(errmsg), "kpropd: accept: %s", strerror(errno));
      klog(L_KRB_PERR, errmsg);
      continue;
    }
    strcpy(from_str, inet_ntoa(from.sin_addr));

    if ((hp = gethostbyaddr((char*)&(from.sin_addr.s_addr), from_len, AF_INET)) == NULL) {
      strcpy(hostname, "UNKNOWN");
    } else {
      strcpy(hostname, hp->h_name);
    }

    snprintf(errmsg, sizeof(errmsg), "Connection from %s, %s", hostname,
	     from_str);
    klog(L_KRB_PERR, errmsg);

    /* for krb_rd_{priv, safe} */
    n = sizeof sin;
    if (getsockname (s2, (struct sockaddr *) &sin, &n) != 0) {
      fprintf (stderr, "kpropd: can't get socketname.\n");
      perror ("getsockname");
      SlowDeath();
    }
    if (n != sizeof (sin)) {
      fprintf (stderr, "kpropd: can't get socketname. len");
      SlowDeath();
    }

    if ((fdlock = open(local_temp, O_WRONLY | O_CREAT, 0600)) < 0) {
      snprintf(errmsg, sizeof(errmsg), "kpropd: open: %s", strerror(errno));
      klog(L_KRB_PERR, errmsg);
      SlowDeath();
    }
    if (flock(fdlock, LOCK_EX | LOCK_NB)) {
      snprintf(errmsg, sizeof(errmsg), "kpropd: flock: %s", strerror(errno));
      klog(L_KRB_PERR, errmsg);
      SlowDeath();
    }
    if ((fd = creat(local_temp, 0600)) < 0) {
      snprintf(errmsg, sizeof(errmsg), "kpropd: creat: %s", strerror(errno));
      klog(L_KRB_PERR, errmsg);
      SlowDeath();
    }
    if ((n = read (s2, buf, sizeof (kprop_version)))
	!= sizeof (kprop_version)) {
      klog (L_KRB_PERR, "kpropd: can't read kprop protocol version str.");
      SlowDeath();
    }
    if (strncmp (buf, kprop_version, sizeof (kprop_version))
	!= 0) {
      snprintf (errmsg, sizeof(errmsg), "kpropd: unsupported version %s", buf);
      klog (L_KRB_PERR, errmsg);
      SlowDeath();
    }

    if ((n = read (s2, &net_transfer_mode, sizeof (net_transfer_mode)))
	!= sizeof (net_transfer_mode)) {
      klog (L_KRB_PERR, "kpropd: can't read transfer mode.");
      SlowDeath();
    }
    transfer_mode = ntohs (net_transfer_mode);
    kerror = krb_recvauth(KOPT_DO_MUTUAL, s2, &ticket,
			  KPROP_SERVICE_NAME,
			  my_instance,
			  &from,
			  &sin,
			  &auth_dat,
			  srvtab,
			  session_sched,
			  version);
    if (kerror != KSUCCESS) {
      snprintf (errmsg, sizeof(errmsg), "kpropd: %s: Calling getkdata", 
	        krb_err_txt[kerror]);
      klog (L_KRB_PERR, errmsg);
      SlowDeath();
    }
	
    snprintf (errmsg, sizeof(errmsg), "kpropd: Connection from %s.%s@%s",
	      auth_dat.pname, auth_dat.pinst, auth_dat.prealm);
    klog (L_KRB_PERR, errmsg);

    /* AUTHORIZATION is done here.  We might want to expand this to
     * read an acl file at some point, but allowing for now
     * KPROP_SERVICE_NAME.KRB_MASTER@local-realm is fine ... */
    if (krb_get_admhst(admin, my_realm, 1) != KSUCCESS) {
      klog (L_KRB_PERR, "Unable to get admin host");
      SlowDeath();
    }
    if ((dot = strchr(admin, '.')) != NULL)
	*dot = '\0';

    if ((strcmp (KPROP_SERVICE_NAME, auth_dat.pname) != 0) ||
	(strcmp (admin, auth_dat.pinst) != 0) ||
	(strcmp (my_realm, auth_dat.prealm) != 0)) {
      klog (L_KRB_PERR, "Authorization denied!");
      SlowDeath();
    }

    switch (transfer_mode) {
    case KPROP_TRANSFER_PRIVATE: 
      recv_auth (s2, fd, 1	/* private */, &from, &sin, &auth_dat);
      break;
    case KPROP_TRANSFER_SAFE: 
      recv_auth (s2, fd, 0	/* safe */, &from, &sin, &auth_dat);
      break;
    case KPROP_TRANSFER_CLEAR: 
      recv_clear (s2, fd);
      break;
    default: 
      snprintf (errmsg, sizeof(errmsg), "kpropd: bad transfer mode %d",
		transfer_mode);
      klog (L_KRB_PERR, errmsg);
      SlowDeath();
    }

    if (transfer_mode != KPROP_TRANSFER_PRIVATE) {
      klog(L_KRB_PERR, "kpropd: non-private transfers not supported\n");
      SlowDeath();
#ifdef doesnt_work_yet
      lseek(fd, (long) 0, L_SET);
      if (auth_dat.checksum != get_data_checksum (fd, session_sched)) {
	klog(L_KRB_PERR, "kpropd: checksum doesn't match");
	SlowDeath();
      }
#endif
    } else

      {
	struct stat st;
	fstat(fd, &st);
	if (st.st_size != auth_dat.checksum) {
	  klog(L_KRB_PERR, "kpropd: length doesn't match");
	  SlowDeath();
	}
      }
    close(fd);
    close(s2);
    klog(L_KRB_PERR, "File received.");

    if (rename(local_temp, local_file) < 0) {
      snprintf(errmsg, sizeof(errmsg), "kpropd: rename: %s", strerror(errno));
      klog(L_KRB_PERR, errmsg);
      SlowDeath();
    }
    klog(L_KRB_PERR, "Temp file renamed to %s", local_file);

    if (flock(fdlock, LOCK_UN)) {
      snprintf(errmsg, sizeof(errmsg), "kpropd: flock (unlock): %s",
	       strerror(errno));
      klog(L_KRB_PERR, errmsg);
      SlowDeath();
    }
    close(fdlock);
    snprintf(cmd, sizeof(cmd), "%s load %s %s\n", kdb_util_path, local_file,
	     local_db);
    if (system (cmd) != 0) {
      klog (L_KRB_PERR, "Couldn't load database");
      SlowDeath();
    }
  }
}

#ifdef doesnt_work_yet
unsigned long get_data_checksum(fd, key_sched)
     int fd;
     des_key_schedule key_sched;
{
  unsigned long cksum = 0;
  unsigned long cbc_cksum();
  int n;
  char buf[BUFSIZ];
  char obuf[8];

  while (n = read(fd, buf, sizeof buf)) {
    if (n < 0) {
      snprintf(errmsg, sizeof(errmsg), "kpropd: read (in checksum test): %s",
	       strerror(errno));
      klog(L_KRB_PERR, errmsg);
      SlowDeath();
    }
#ifndef NOENCRYPTION
    cksum += des_cbc_cksum(buf, obuf, n, key_sched, key_sched);
#endif
  }
  return cksum;
}
#endif
