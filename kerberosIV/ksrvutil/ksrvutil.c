/*	$Id: ksrvutil.c,v 1.1.1.1 1995/12/14 06:52:53 tholo Exp $	*/

/*-
 * Copyright (C) 1989 by the Massachusetts Institute of Technology
 *
 * Export of this software from the United States of America is assumed
 * to require a specific license from the United States Government.
 * It is the responsibility of any person or organization contemplating
 * export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

/*
 * list and update contents of srvtab files
 */

/*
 * ksrvutil
 * list and update the contents of srvtab files
 */

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

#include <kadm_locl.h>
#include <sys/param.h>

#ifdef NOENCRYPTION
#define read_long_pw_string placebo_read_pw_string
#else /* NOENCRYPTION */
#define read_long_pw_string des_read_pw_string
#endif /* NOENCRYPTION */

#define SRVTAB_MODE 0600	/* rw------- */
#define PAD "  "
#define VNO_HEADER "Version"
#define VNO_FORMAT "%4d   "
#define KEY_HEADER "       Key       " /* 17 characters long */
#define PRINC_HEADER "  Principal\n"
#define PRINC_FORMAT "%s"

static unsigned short
get_mode(char *filename)
{
    struct stat statbuf;
    unsigned short mode;

    (void) bzero((char *)&statbuf, sizeof(statbuf));
    
    if (stat(filename, &statbuf) < 0) 
	mode = SRVTAB_MODE;
    else
	mode = statbuf.st_mode;

    return(mode);
}

static void
copy_keyfile(char *progname, char *keyfile, char *backup_keyfile)
{
    int keyfile_fd;
    int backup_keyfile_fd;
    int keyfile_mode;
    char buf[BUFSIZ];		/* for copying keyfiles */
    int rcount;			/* for copying keyfiles */
    int try_again;
    
    (void) bzero((char *)buf, sizeof(buf));
    
    do {
	try_again = FALSE;
	if ((keyfile_fd = open(keyfile, O_RDONLY, 0)) < 0) {
	    if (errno != ENOENT) {
		(void)fprintf(stderr, "%s: Unable to read %s: %s\n", progname, 
			      keyfile, strerror(errno));
		exit(1);
	    }
	    else {
		try_again = TRUE;
		if ((keyfile_fd = 
		     open(keyfile, 
			  O_WRONLY | O_TRUNC | O_CREAT, SRVTAB_MODE)) < 0) {
		    (void) fprintf(stderr, "%s: Unable to create %s: %s\n", 
				   progname, keyfile, strerror(errno));
		    exit(1);
		}
		else
		    if (close(keyfile_fd) < 0) {
			(void) fprintf(stderr, "%s: Failure closing %s: %s\n",
				       progname, keyfile, strerror(errno));
			exit(1);
		    }
	    }
	}
    } while(try_again);

    keyfile_mode = get_mode(keyfile);

    if ((backup_keyfile_fd = 
	 open(backup_keyfile, O_WRONLY | O_TRUNC | O_CREAT, 
	      keyfile_mode)) < 0) {
	(void) fprintf(stderr, "%s: Unable to write %s: %s\n", progname, 
		       backup_keyfile, strerror(errno));
	exit(1);
    }
    do {
	if ((rcount = read(keyfile_fd, (char *)buf, sizeof(buf))) < 0) {
	    (void) fprintf(stderr, "%s: Error reading %s: %s\n", progname,
			   keyfile, strerror(errno));
	    exit(1);
	}
	if (rcount && (write(backup_keyfile_fd, buf, rcount) != rcount)) {
	    (void) fprintf(stderr, "%s: Error writing %s: %s\n", progname,
			   backup_keyfile, strerror(errno));
	    exit(1);
	}
    } while (rcount);
    if (close(backup_keyfile_fd) < 0) {
	(void) fprintf(stderr, "%s: Error closing %s: %s\n", progname,
		       backup_keyfile, strerror(errno));
	exit(1);
    }
    if (close(keyfile_fd) < 0) {
	(void) fprintf(stderr, "%s: Error closing %s: %s\n", progname,
		       keyfile, strerror(errno));
	exit(1);
    }
}

static void
leave(char *str, int x)
{
    if (str)
	(void) fprintf(stderr, "%s\n", str);
    (void) dest_tkt();
    exit(x);
}

static void
safe_read_stdin(char *prompt, char *buf, int size)
{
    (void) printf(prompt);
    (void) fflush(stdout);
    (void) bzero(buf, size);
    if (read(0, buf, size - 1) < 0) {
	(void) fprintf(stderr, "Failure reading from stdin: %s\n", 
		       strerror(errno));
	leave((char *)NULL, 1);
    }
    fflush(stdin);
    buf[strlen(buf)-1] = 0;
}	
  

static void
safe_write(char *progname, char *filename, int fd, char *buf, int len)
{
    if (write(fd, buf, len) != len) {
	(void) fprintf(stderr, "%s: Failure writing to %s: %s\n", progname,
		       filename, strerror(errno));
	(void) close(fd);
	leave("In progress srvtab in this file.", 1);
    }
}	

static int
yn(char *string)
{
    char ynbuf[5];

    (void) printf("%s (y,n) [y] ", string);
    for (;;) {
	safe_read_stdin("", ynbuf, sizeof(ynbuf));
	
	if ((ynbuf[0] == 'n') || (ynbuf[0] == 'N'))
	    return(0);
	else if ((ynbuf[0] == 'y') || (ynbuf[0] == 'Y') || (ynbuf[0] == 0))
	    return(1);
	else {
	    (void) printf("Please enter 'y' or 'n': ");
	    fflush(stdout);
	}
    }
}

static void
append_srvtab(char *progname, char *filename, int fd, char *sname, char *sinst, char *srealm, unsigned char key_vno, unsigned char *key)
{
    /* Add one to append null */
    safe_write(progname, filename, fd, sname, strlen(sname) + 1);
    safe_write(progname, filename, fd, sinst, strlen(sinst) + 1);
    safe_write(progname, filename, fd, srealm, strlen(srealm) + 1);
    safe_write(progname, filename, fd, (char *)&key_vno, 1);
    safe_write(progname, filename, fd, (char *)key, sizeof(des_cblock));
    (void) fsync(fd);
}    

static void
print_key(unsigned char *key)
{
    int i;

    for (i = 0; i < 4; i++)
	(void) printf("%02x", key[i]);
    (void) printf(" ");
    for (i = 4; i < 8; i++)
	(void) printf("%02x", key[i]);
}

static void
print_name(char *name, char *inst, char *realm)
{
    (void) printf("%s%s%s%s%s", name, inst[0] ? "." : "", inst,
		  realm[0] ? "@" : "", realm);
}

static int
get_svc_new_key(unsigned char *new_key, char *sname, char *sinst, char *srealm, char *keyfile)
{
    int status = KADM_SUCCESS;

    if (((status = krb_get_svc_in_tkt(sname, sinst, srealm, PWSERV_NAME,
				      KADM_SINST, 1, keyfile)) == KSUCCESS) &&
	((status = kadm_init_link("changepw", KRB_MASTER, srealm)) == 
	 KADM_SUCCESS)) {
#ifdef NOENCRYPTION
	(void) bzero((char *) new_key, sizeof(des_cblock));
	new_key[0] = (unsigned char) 1;
#else /* NOENCRYPTION */
	(void) des_new_random_key((des_cblock*)&new_key);
#endif /* NOENCRYPTION */
	return(KADM_SUCCESS);
    }
    
    return(status);
}

static void
get_key_from_password(des_cblock (*key))
{
    char password[MAX_KPW_LEN];	/* storage for the password */

    if (read_long_pw_string(password, sizeof(password)-1, "Password: ", 1))
	leave("Error reading password.", 1);

#ifdef NOENCRYPTION
    (void) bzero((char *) key, sizeof(des_cblock));
    key[0] = (unsigned char) 1;
#else /* NOENCRYPTION */
    (void) des_string_to_key(password, key);
#endif /* NOENCRYPTION */
    (void) bzero((char *)password, sizeof(password));
}    

static void
usage(void)
{
    (void) fprintf(stderr, "Usage: ksrvutil [-f keyfile] [-i] [-k] ");
    (void) fprintf(stderr, "{list | change | add | get}\n");
    (void) fprintf(stderr, "   -i causes the program to ask for ");
    (void) fprintf(stderr, "confirmation before changing keys.\n");
    (void) fprintf(stderr, "   -k causes the key to printed for list or ");
    (void) fprintf(stderr, "change.\n");
    exit(1);
}

int
main(int argc, char **argv)
{
    char sname[ANAME_SZ];	/* name of service */
    char sinst[INST_SZ];	/* instance of service */
    char srealm[REALM_SZ];	/* realm of service */
    unsigned char key_vno;	/* key version number */
    int status;			/* general purpose error status */
    des_cblock new_key;
    des_cblock old_key;
    char change_tkt[MAXPATHLEN]; /* Ticket to use for key change */
    char keyfile[MAXPATHLEN];	/* Original keyfile */
    char work_keyfile[MAXPATHLEN]; /* Working copy of keyfile */
    char backup_keyfile[MAXPATHLEN]; /* Backup copy of keyfile */
    unsigned short keyfile_mode; /* Protections on keyfile */
    int work_keyfile_fd = -1;	/* Initialize so that */
    int backup_keyfile_fd = -1;	/* compiler doesn't complain */
    char local_realm[REALM_SZ];	/* local kerberos realm */
    int i;
    int interactive = FALSE;
    int list = FALSE;
    int change = FALSE;
    int add = FALSE;
    int get = FALSE;
    int key = FALSE;		/* do we show keys? */
    int arg_entered = FALSE;
    int change_this_key = FALSE;
    char databuf[BUFSIZ];
    int first_printed = FALSE;	/* have we printed the first item? */
    
    (void) bzero((char *)sname, sizeof(sname));
    (void) bzero((char *)sinst, sizeof(sinst));
    (void) bzero((char *)srealm, sizeof(srealm));
    
    (void) bzero((char *)change_tkt, sizeof(change_tkt));
    (void) bzero((char *)keyfile, sizeof(keyfile));
    (void) bzero((char *)work_keyfile, sizeof(work_keyfile));
    (void) bzero((char *)backup_keyfile, sizeof(backup_keyfile));
    (void) bzero((char *)local_realm, sizeof(local_realm));
    
    (void) sprintf(change_tkt, "/tmp/tkt_ksrvutil.%d", (int)getpid());
    krb_set_tkt_string(change_tkt);

    /* This is used only as a default for adding keys */
    if (krb_get_lrealm(local_realm, 1) != KSUCCESS)
	(void) strcpy(local_realm, KRB_REALM);
    
    for (i = 1; i < argc; i++) {
	if (strcmp(argv[i], "-i") == 0) 
	    interactive++;
	else if (strcmp(argv[i], "-k") == 0) 
	    key++;
	else if (strcmp(argv[i], "list") == 0) {
	    if (arg_entered)
		usage();
	    else {
		arg_entered++;
		list++;
	    }
	}
	else if (strcmp(argv[i], "change") == 0) {
	    if (arg_entered)
		usage();
	    else {
		arg_entered++;
		change++;
	    }
	}
	else if (strcmp(argv[i], "add") == 0) {
	    if (arg_entered)
		usage();
	    else {
		arg_entered++;
		add++;
	    }
	}
	else if (strcmp(argv[i], "get") == 0) {
	    if (arg_entered)
		usage();
	    else {
		arg_entered++;
		get++;
	    }
	}
	else if (strcmp(argv[i], "-f") == 0) {
	    if (++i == argc)
		usage();
	    else
		(void) strcpy(keyfile, argv[i]);
	}
	else
	    usage();
    }
    
    if (!arg_entered)
	usage();

    if (!keyfile[0])
	(void) strcpy(keyfile, KEYFILE);
    
    (void) strcpy(work_keyfile, keyfile);
    (void) strcpy(backup_keyfile, keyfile);
    
    if (change || add || get) {
	(void) strcat(work_keyfile, ".work");
	(void) strcat(backup_keyfile, ".old");
	
	copy_keyfile(argv[0], keyfile, backup_keyfile);
    }
    
    if (add || get)
	copy_keyfile(argv[0], backup_keyfile, work_keyfile);

    keyfile_mode = get_mode(keyfile);

    if (change || list) {
	if ((backup_keyfile_fd = open(backup_keyfile, O_RDONLY, 0)) < 0) {
	    (void) fprintf(stderr, "%s: Unable to read %s: %s\n", argv[0],
			   backup_keyfile, strerror(errno));
	    exit(1);
	}
    }

    if (change) {
	if ((work_keyfile_fd = 
	     open(work_keyfile, O_WRONLY | O_CREAT | O_TRUNC, 
		  SRVTAB_MODE)) < 0) {
	    (void) fprintf(stderr, "%s: Unable to write %s: %s\n", argv[0],
			   work_keyfile, strerror(errno));
	    exit(1);
	}
    }
    else if (add || get) {
	if ((work_keyfile_fd =
	     open(work_keyfile, O_APPEND | O_WRONLY, SRVTAB_MODE)) < 0) {
	    (void) fprintf(stderr, "%s: Unable to open %s for append: %s\n",
			   argv[0], work_keyfile, strerror(errno));
	    exit(1);
	}
    }
    
    if (change || list) {
	while ((getst(backup_keyfile_fd, sname, SNAME_SZ) > 0) &&
	       (getst(backup_keyfile_fd, sinst, INST_SZ) > 0) &&
	       (getst(backup_keyfile_fd, srealm, REALM_SZ) > 0) &&
	       (read(backup_keyfile_fd, &key_vno, 1) > 0) &&
	       (read(backup_keyfile_fd,(char *)old_key,sizeof(old_key)) > 0)) {
	    if (list) {
		if (!first_printed) {
		    (void) printf(VNO_HEADER);
		    (void) printf(PAD);
		    if (key) {
			(void) printf(KEY_HEADER);
			(void) printf(PAD);
		    }
		    (void) printf(PRINC_HEADER);
		    first_printed = 1;
		}
		(void) printf(VNO_FORMAT, key_vno);
		(void) printf(PAD);
		if (key) {
		    print_key(old_key);
		    (void) printf(PAD);
		}
		print_name(sname, sinst, srealm);
		(void) printf("\n");
	    }
	    else if (change) {
		(void) printf("\nPrincipal: ");
		print_name(sname, sinst, srealm);
		(void) printf("; version %d\n", key_vno);
		if (interactive)
		    change_this_key = yn("Change this key?");
		else if (change)
		    change_this_key = 1;
		else
		    change_this_key = 0;
		
		if (change_this_key)
		    (void) printf("Changing to version %d.\n", key_vno + 1);
		else if (change)
		    (void) printf("Not changing this key.\n");
		
		if (change_this_key) {
		    /* Initialize non shared random sequence old key. */
		    des_init_random_number_generator(&old_key);
		    
		    /* 
		     * Pick a new key and determine whether or not
		     * it is safe to change
		     */
		    if ((status = 
			 get_svc_new_key(new_key, sname, sinst, 
					 srealm, keyfile)) == KADM_SUCCESS)
			key_vno++;
		    else {
			(void) bcopy(old_key, new_key, sizeof(new_key));
			(void) fprintf(stderr, "%s: Key NOT changed: %s\n",
				       argv[0], krb_err_txt[status]);
			change_this_key = FALSE;
		    }
		}
		else 
		    (void) bcopy(old_key, new_key, sizeof(new_key));
		append_srvtab(argv[0], work_keyfile, work_keyfile_fd, 
			      sname, sinst, srealm, key_vno, new_key);
		if (key && change_this_key) {
		    (void) printf("Old key: ");
		    print_key(old_key);
		    (void) printf("; new key: ");
		    print_key(new_key);
		    (void) printf("\n");
		}
		if (change_this_key) {
		    if ((status = kadm_change_pw(new_key)) == KADM_SUCCESS) {
			(void) printf("Key changed.\n");
			(void) dest_tkt();
		    }
		    else {
			com_err(argv[0], status, 
				" attempting to change password.");
			(void) dest_tkt();
			/* XXX This knows the format of a keyfile */
			if (lseek(work_keyfile_fd, -9, SEEK_CUR) >= 0) {
			    key_vno--;
			    safe_write(argv[0], work_keyfile,
				       work_keyfile_fd, (char *)&key_vno, 1);
			    safe_write(argv[0], work_keyfile, work_keyfile_fd,
				       (char *)old_key, sizeof(des_cblock));
			    (void) fsync(work_keyfile_fd);
			    (void) fprintf(stderr,"Key NOT changed.\n");
			}
			else {
			    (void)fprintf(stderr, 
					  "%s: Unable to revert keyfile: %s\n",
					  argv[0], strerror(errno));
			    leave("", 1);
			}
		    }
		}
	    }
	    bzero((char *)old_key, sizeof(des_cblock));
	    bzero((char *)new_key, sizeof(des_cblock));
	}
    }
    else if (add) {
	do {
	    do {
		safe_read_stdin("Name: ", databuf, sizeof(databuf));
		(void) strncpy(sname, databuf, sizeof(sname) - 1);
		safe_read_stdin("Instance: ", databuf, sizeof(databuf));
		(void) strncpy(sinst, databuf, sizeof(sinst) - 1);
		safe_read_stdin("Realm: ", databuf, sizeof(databuf));
		(void) strncpy(srealm, databuf, sizeof(srealm) - 1);
		safe_read_stdin("Version number: ", databuf, sizeof(databuf));
		key_vno = atoi(databuf);
		if (!srealm[0])
		    (void) strcpy(srealm, local_realm);
		(void) printf("New principal: ");
		print_name(sname, sinst, srealm);
		(void) printf("; version %d\n", key_vno);
	    } while (!yn("Is this correct?"));
	    get_key_from_password(&new_key);
	    if (key) {
		(void) printf("Key: ");
		print_key(new_key);
		(void) printf("\n");
	    }
	    append_srvtab(argv[0], work_keyfile, work_keyfile_fd, 
			  sname, sinst, srealm, key_vno, new_key);
	    (void) printf("Key successfully added.\n");
	} while (yn("Would you like to add another key?"));
    }
    else if (get) {
        ksrvutil_get();
    }

    if (change || list) 
	if (close(backup_keyfile_fd) < 0) {
	    (void) fprintf(stderr, "%s: Failure closing %s: %s\n",
			   argv[0], backup_keyfile, strerror(errno));
	    (void) fprintf(stderr, "continuing...\n");
	}
    
    if (change || add || get) {
	if (close(work_keyfile_fd) < 0) {
	    (void) fprintf(stderr, "%s: Failure closing %s: %s\n",
			   argv[0], work_keyfile, strerror(errno));
	    exit(1);
	}
	if (rename(work_keyfile, keyfile) < 0) {
	    (void) fprintf(stderr, "%s: Failure renaming %s to %s: %s\n",
			   argv[0], work_keyfile, keyfile, 
			   strerror(errno));
	    exit(1);
	}
	(void) chmod(backup_keyfile, keyfile_mode);
	(void) chmod(keyfile, keyfile_mode);
	(void) printf("Old keyfile in %s.\n", backup_keyfile);
    }

    exit(0);
}

ksrvutil_get()
{
  char sname[ANAME_SZ];		/* name of service */
  char sinst[INST_SZ];		/* instance of service */
  char srealm[REALM_SZ];	/* realm of service */
  char databuf[BUFSIZ];
  char local_realm[REALM_SZ];	/* local kerberos realm */
  char local_hostname[100];

  if (krb_get_lrealm(local_realm, 1) != KSUCCESS)
    strcpy(local_realm, KRB_REALM);
  gethostname(local_hostname, sizeof(local_hostname));
  strcpy(local_hostname, krb_get_phost(local_hostname));
  do {
    do {
      safe_read_stdin("Name [rcmd]: ", databuf, sizeof(databuf));
      if (databuf[0])
	strncpy(sname, databuf, sizeof(sname) - 1);
      else
	strcpy(sname, "rcmd");

      safe_read_stdin("Instance [hostname]: ", databuf, sizeof(databuf));
      if (databuf[0])
	strncpy(sinst, databuf, sizeof(sinst) - 1);
      else
	strcpy(sinst, local_hostname);

      safe_read_stdin("Realm [localrealm]: ", databuf, sizeof(databuf));
      if (databuf[0])
	strncpy(srealm, databuf, sizeof(srealm) - 1);
      else
	strcpy(srealm, local_realm);

      printf("New principal: ");
      print_name(sname, sinst, srealm);
    } while (!yn("Is this correct?"));
    printf("NOT adding anything!!! Key successfully added.\n");
  } while (yn("Would you like to add another key?"));
}
