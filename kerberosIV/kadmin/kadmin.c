/*	$Id: kadmin.c,v 1.3 1996/09/15 23:30:52 millert Exp $	*/

/* 
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
 * Kerberos database administrator's tool.  
 * 
 * The default behavior of kadmin is if the -m option is given 
 * on the commandline, multiple requests are allowed to be given
 * with one entry of the admin password (until the tickets expire).
 * If you do not want this to be an available option, compile with
 * NO_MULTIPLE defined.
 */

#include <kadm_locl.h>
#include <sys/param.h>

#define BAD_PW 1
#define GOOD_PW 0
#define FUDGE_VALUE 15		/* for ticket expiration time */
#define PE_NO 0
#define PE_YES 1
#define PE_UNSURE 2

/* for get_password, whether it should do the swapping...necessary for
   using vals structure, unnecessary for change_pw requests */
#define DONTSWAP 0
#define SWAP 1

extern ss_request_table admin_cmds;

static char myname[ANAME_SZ];
static char default_realm[REALM_SZ]; /* default kerberos realm */
static char krbrlm[REALM_SZ];	/* current realm being administered */
static int multiple = 0;	/* Allow multiple requests per ticket */

#ifdef NOENCRYPTION
#define read_long_pw_string placebo_read_pw_string
#else
#define read_long_pw_string des_read_pw_string
#endif

static void
get_maxlife(Kadm_vals *vals)
{
    char buff[BUFSIZ];
    time_t life;
    int l;

    do {
	printf("Maximum ticket lifetime?  (%d)  [%s]  ",
 	     vals->max_life, krb_life_to_atime(vals->max_life));
	fflush(stdout);
	if (fgets(buff, sizeof(buff), stdin) == NULL || *buff == '\n') {
	    clearerr(stdin);
	    return;
	}
	life = krb_atime_to_life(buff);
    } while (life <= 0);

    l = strlen(buff);
    if (buff[l-2] == 'm')
	life = krb_time_to_life(0L, life*60);
    if (buff[l-2] == 'h')
	life = krb_time_to_life(0L, life*60*60);

    vals->max_life = life;
    SET_FIELD(KADM_MAXLIFE,vals->fields);
}

static void
get_attr(Kadm_vals *vals)
{
    char buff[BUFSIZ], *out;
    int attr;

    do {
	printf("Attributes?  [0x%.2x]  ", vals->attributes);
	fflush(stdout);
	if (fgets(buff, sizeof(buff), stdin) == NULL || *buff == '\n') {
	    clearerr(stdin);
	    return;
	}
        attr = strtol(buff, &out, 0);
	if (attr == 0 && out == buff)
	  attr = -1;
    } while (attr < 0 || attr > 0xffff);

    vals->attributes = attr;
    SET_FIELD(KADM_ATTR,vals->fields);
}

static void
get_expdate(Kadm_vals *vals)
{
    char buff[BUFSIZ];
    time_t when;
    struct tm edate;

    bzero(&edate, sizeof(edate));
    do {
        printf("Expiration date (enter yyyy-mm-dd) ?  [%.24s]  ",
             asctime(k_localtime(&vals->exp_date)));
        fflush(stdout);
        if (fgets(buff, sizeof(buff), stdin) == NULL || *buff == '\n') {
            clearerr(stdin);
            return;
        }
        if (sscanf(buff, "%d-%d-%d",
                   &edate.tm_year, &edate.tm_mon, &edate.tm_mday) == 3) {
            edate.tm_mon--;     /* January is 0, not 1 */
            edate.tm_hour = 23; /* nearly midnight at the end of the */
            edate.tm_min = 59;  /* specified day */
            when = maketime(&edate, 1);
        }
    } while (when <= 0);

    vals->exp_date = when;
    SET_FIELD(KADM_EXPDATE,vals->fields);
}

static int
princ_exists(char *name, char *instance, char *realm)
{
    int status;

    status = krb_get_pw_in_tkt(name, instance, realm, "krbtgt", realm, 1, "");

    if ((status == KSUCCESS) || (status == INTK_BADPW))
	return(PE_YES);
    else if (status == KDC_PR_UNKNOWN)
	return(PE_NO);
    else
	return(PE_UNSURE);
}

static int
get_password(u_int32_t *low, u_int32_t *high, char *prompt, int byteswap)
{
    char new_passwd[MAX_KPW_LEN];	/* new password */
    des_cblock newkey;

    if (read_long_pw_string(new_passwd, sizeof(new_passwd)-1, prompt, 1))
    	return(BAD_PW);
    if (strlen(new_passwd) == 0) {
    	printf("Using random password.\n");
#ifdef NOENCRYPTION
	bzero((char *) newkey, sizeof(newkey));
#else
	des_new_random_key(&newkey);
#endif
    } else {
#ifdef NOENCRYPTION
      bzero((char *) newkey, sizeof(newkey));
#else
      des_string_to_key(new_passwd, &newkey);
#endif
      bzero(new_passwd, sizeof(new_passwd));
    }

    bcopy((char *) newkey,(char *)low,4);
    bcopy((char *)(((int32_t *) newkey) + 1), (char *)high,4);

    bzero((char *) newkey, sizeof(newkey));

#ifdef NOENCRYPTION
    *low = 1;
#endif

    if (byteswap != DONTSWAP) {
	*low = htonl(*low);
	*high = htonl(*high);
    }
    return(GOOD_PW);
}

static int
get_admin_password(void)
{
    int status;
    char admin_passwd[MAX_KPW_LEN];	/* Admin's password */
    int ticket_life = 1;	/* minimum ticket lifetime */
    CREDENTIALS c;

    if (multiple) {
	/* If admin tickets exist and are valid, just exit. */
	bzero(&c, sizeof(c));
	if (krb_get_cred(PWSERV_NAME, KADM_SINST, krbrlm, &c) == KSUCCESS)
	    /* 
	     * If time is less than lifetime - FUDGE_VALUE after issue date,
	     * tickets will probably last long enough for the next 
	     * transaction.
	     */
	    if (time(0) < (c.issue_date + (5 * 60 * c.lifetime) - FUDGE_VALUE))
		return(KADM_SUCCESS);
	ticket_life = DEFAULT_TKT_LIFE;
    }
    
    if (princ_exists(myname, "admin", krbrlm) != PE_NO) {
	if (read_long_pw_string(admin_passwd, sizeof(admin_passwd)-1,
				"Admin password:", 0)) {
	    fprintf(stderr, "Error reading admin password.\n");
	    goto bad;
	}
	status = krb_get_pw_in_tkt(myname, "admin", krbrlm, PWSERV_NAME, 
				   KADM_SINST, ticket_life, admin_passwd);
	bzero(admin_passwd, sizeof(admin_passwd));

	/* Initialize non shared random sequence from session key. */
	bzero(&c, sizeof(c));
	krb_get_cred(PWSERV_NAME, KADM_SINST, krbrlm, &c);
	des_init_random_number_generator(&c.session);
    }
    else
	status = KDC_PR_UNKNOWN;

    switch(status) {
    case GT_PW_OK:
	return(GOOD_PW);
    case KDC_PR_UNKNOWN:
	printf("Principal %s.admin@%s does not exist.\n", myname, krbrlm);
	goto bad;
    case GT_PW_BADPW:
	printf("Incorrect admin password.\n");
	goto bad;
    default:
	com_err("kadmin", status+krb_err_base,
		"while getting password tickets");
	goto bad;
    }
    
 bad:
    bzero(admin_passwd, sizeof(admin_passwd));
    (void) dest_tkt();
    return(BAD_PW);
}

static void
usage(void)
{
    fprintf(stderr, "Usage: kadmin [-u admin_name] [-r default_realm]");
    fprintf(stderr, " [-m]");
    fprintf(stderr, "\n");
    fprintf(stderr, "   -m allows multiple admin requests to be ");
    fprintf(stderr, "serviced with one entry of admin\n");
    fprintf(stderr, "   password.\n");
    exit(1);
}

/* GLOBAL */
void
clean_up(void)
{
    (void) dest_tkt();
    return;
}

/* GLOBAL */
void 
quit(void)
{
    printf("Cleaning up and exiting.\n");
    clean_up();
    exit(0);
}

static int inited = 0;

static void
do_init(int argc, char **argv)
{
    char *dot, admin[MAXHOSTNAMELEN];
    struct passwd *pw;
    int c;
#define OPTION_STRING "u:r:m"
    
    bzero(myname, sizeof(myname));

    if (!inited) {
	/* 
	 * This is only as a default/initial realm; we don't care 
	 * about failure.
	 */
	if (krb_get_lrealm(default_realm, 1) != KSUCCESS) {
	    fprintf(stderr,
		    "Could not determine local realm name.\n");
	}

	c = krb_get_admhst(admin, default_realm, 1);
	if (c == KSUCCESS && (dot = strchr(admin, '.')) != NULL)
	    *dot = '\0';

	/* 
	 * If we can reach the local realm, initialize to it.  Otherwise,
	 * don't initialize.
	 */
	if (c == KSUCCESS &&
	    kadm_init_link(PWSERV_NAME, admin, default_realm) != KADM_SUCCESS)
	    bzero(krbrlm, sizeof(krbrlm));
	else
	    strcpy(krbrlm, default_realm);

	while ((c = getopt(argc, argv, OPTION_STRING)) != EOF) 
	    switch (c) {
	      case 'u':
		strncpy(myname, optarg, sizeof(myname) - 1);
		break;
	      case 'r':
		bzero(default_realm, sizeof(default_realm));
		strncpy(default_realm, optarg, sizeof(default_realm) - 1);
		break;
	      case 'm':
		multiple++;
		break;
	      default:
		usage();
		break;
	    }
	if (optind < argc)
	    usage();
	if (!myname[0]) {
	    pw = getpwuid((int) getuid());
	    if (!pw) {
		fprintf(stderr,
			"You aren't in the password file.  Who are you?\n");
		exit(1);
	    }
	    (void) strcpy(myname, pw->pw_name);
	}
	inited = 1;
    }
}

int
main(int argc, char **argv)
{
    int     sci_idx;
    int     code;
    char tktstring[MAXPATHLEN];

    sci_idx = ss_create_invocation("admin", "2.0", (char *) NULL,
				   &admin_cmds, &code);
    if (code) {
	ss_perror(sci_idx, code, "creating invocation");
	exit(1);
    }
    (void) snprintf(tktstring, sizeof(tktstring), "/tmp/tkt_adm_%d",
	(int)getpid());
    krb_set_tkt_string(tktstring);

    do_init(argc, argv);

    printf("Welcome to the Kerberos Administration Program, version 2\n");
    printf("Type \"help\" if you need it.\n");
    code = ss_listen(sci_idx);
    printf("\n");
    quit();
    exit(0);
}

static int
setvals(Kadm_vals *vals, char *string)
{
    char *dot, admin[MAXHOSTNAMELEN];
    char realm[REALM_SZ];
    int status = KADM_SUCCESS;

    bzero(vals, sizeof(*vals));
    bzero(realm, sizeof(realm));

    SET_FIELD(KADM_NAME,vals->fields);
    SET_FIELD(KADM_INST,vals->fields);
    if ((status = kname_parse(vals->name, vals->instance, realm, string))) {
	printf("kerberos error: %s\n", krb_err_txt[status]);
	return status;
    }
    if (!realm[0])
	strcpy(realm, default_realm);
    if (strcmp(realm, krbrlm)) {
	if ((status = krb_get_admhst(admin, realm, 1)) != KSUCCESS)
            return(status);
	if (status == KSUCCESS && (dot = strchr(admin, '.')) != NULL)
	    *dot = '\0';
	strcpy(krbrlm, realm);
	if ((status = kadm_init_link(PWSERV_NAME, admin, krbrlm)) 
	    != KADM_SUCCESS)
	    printf("kadm error for realm %s: %s\n", 
		   krbrlm, error_message(status));
    }
    if (status) 
	return 1;
    else
	return KADM_SUCCESS;
}    

void 
change_password(int argc, char **argv)
{
    Kadm_vals old, new;
    int status;
    char pw_prompt[BUFSIZ];

    if (argc != 2) {
	printf("Usage: change_password loginname\n");
	return;
    }

    if (setvals(&old, argv[1]) != KADM_SUCCESS)
	return;

    new = old;

    SET_FIELD(KADM_DESKEY,new.fields);

    if (princ_exists(old.name, old.instance, krbrlm) != PE_NO) {
	/* get the admin's password */
        if (get_admin_password() != GOOD_PW)
	    return;

	/* get the new password */
	(void) snprintf(pw_prompt, sizeof(pw_prompt), "New password for %s:",
	    argv[1]);
	
	if (get_password(&new.key_low, &new.key_high,
			 pw_prompt, SWAP) == GOOD_PW) {
	    status = kadm_mod(&old, &new);
	    if (status == KADM_SUCCESS) {
		printf("Password changed for %s.\n", argv[1]);
	    } else {
		printf("kadmin: %s\nwhile changing password for %s",
		       error_message(status), argv[1]);
	    }
	} else
	    printf("Error reading password; password unchanged\n");
	bzero((char *)&new, sizeof(new));
	if (!multiple)
	    clean_up();
    }
    else 
	printf("kadmin: Principal does not exist.\n");
    return;
}

/*ARGSUSED*/
void 
change_admin_password(int argc, char **argv)
{
    des_cblock newkey;
    u_int32_t low, high;
    int status;
    char prompt_pw[BUFSIZ];

    if (argc != 1) {
	printf("Usage: change_admin_password\n");
	return;
    }
    /* get the admin's password */
    if (get_admin_password() != GOOD_PW)
	return;

    (void) snprintf(prompt_pw, sizeof(prompt_pw), "New password for %s.admin:",
	myname);
    if (get_password(&low, &high, prompt_pw, DONTSWAP) == GOOD_PW) {
	bcopy((char *)&low,(char *) newkey,4);
	bcopy((char *)&high, (char *)(((int32_t *) newkey) + 1),4);
	low = high = 0L;
	if ((status = kadm_change_pw(newkey)) == KADM_SUCCESS)
	    printf("Admin password changed\n");
	else
	    printf("kadm error: %s\n",error_message(status));
	bzero((char *)newkey, sizeof(newkey));
    } else
	printf("Error reading password; password unchanged\n");
    if (!multiple)
	clean_up();
    return;
}

void 
add_new_key(int argc, char **argv)
{
    Kadm_vals new;
    char pw_prompt[BUFSIZ];
    int status;

    if (argc != 2) {
	printf("Usage: add_new_key user_name.\n");
	return;
    }
    if (setvals(&new, argv[1]) != KADM_SUCCESS)
	return;

    SET_FIELD(KADM_DESKEY,new.fields);

    if (princ_exists(new.name, new.instance, krbrlm) != PE_YES) {
	/* get the admin's password */
	if (get_admin_password() != GOOD_PW)
	    return;
	
	/* This is the default maximum lifetime for new principals. */
	if (krb_life_to_time(0, 162) >= 24*60*60)
	  new.max_life = 162;     /* ca 100 hours */
	else
	  new.max_life = 255;     /* ca 21 hours (maximum) */
	new.exp_date = time(0) + 2*(365*24*60*60); /* + ca 2 years */
	new.attributes = 0;
	get_maxlife(&new);
	get_attr(&new);
	get_expdate(&new);

	/* get the new password */
	(void) snprintf(pw_prompt, sizeof(pw_prompt), "Password for %s:",
	    argv[1]);
	
	if (get_password(&new.key_low, &new.key_high,
			 pw_prompt, SWAP) == GOOD_PW) {
	    status = kadm_add(&new);
	    if (status == KADM_SUCCESS) {
		printf("%s added to database.\n", argv[1]);
	    } else {
		printf("kadm error: %s\n",error_message(status));
	    }
	} else
	    printf("Error reading password; %s not added\n",argv[1]);
	bzero((char *)&new, sizeof(new));
	if (!multiple)
	    clean_up();
    }
    else
	printf("kadmin: Principal already exists.\n");
    return;
}

void 
get_entry(int argc, char **argv)
{
    int status;
    u_char fields[4];
    Kadm_vals vals;

    if (argc != 2) {
	printf("Usage: get_entry username\n");
	return;
    }

    bzero(fields, sizeof(fields));

    SET_FIELD(KADM_NAME,fields);
    SET_FIELD(KADM_INST,fields);
    SET_FIELD(KADM_EXPDATE,fields);
    SET_FIELD(KADM_ATTR,fields);
    SET_FIELD(KADM_MAXLIFE,fields);

    if (setvals(&vals, argv[1]) != KADM_SUCCESS)
	return;


    if (princ_exists(vals.name, vals.instance, krbrlm) != PE_NO) {
	/* get the admin's password */
	if (get_admin_password() != GOOD_PW)
	    return;
	
	if ((status = kadm_get(&vals, fields)) == KADM_SUCCESS)
	    prin_vals(&vals);
	else
	    printf("kadm error: %s\n",error_message(status));
	
	if (!multiple)
	    clean_up();
    }
    else
	printf("kadmin: Principal does not exist.\n");
    return;
}

void 
mod_entry(int argc, char **argv)
{
    int status;
    u_char fields[4];
    Kadm_vals ovals, nvals;

    if (argc != 2) {
	printf("Usage: mod_entry username\n");
	return;
    }

    bzero(fields, sizeof(fields));

    SET_FIELD(KADM_NAME,fields);
    SET_FIELD(KADM_INST,fields);
    SET_FIELD(KADM_EXPDATE,fields);
    SET_FIELD(KADM_ATTR,fields);
    SET_FIELD(KADM_MAXLIFE,fields);

    if (setvals(&ovals, argv[1]) != KADM_SUCCESS)
	return;

    nvals = ovals;

    if (princ_exists(ovals.name, ovals.instance, krbrlm) == PE_NO) {
	printf("kadmin: Principal does not exist.\n");
	return;
    }

    /* get the admin's password */
    if (get_admin_password() != GOOD_PW)
	return;
	
    if ((status = kadm_get(&ovals, fields)) != KADM_SUCCESS) {
	printf("[ unable to retrieve current settings: %s ]\n",
	    error_message(status));
	nvals.max_life = DEFAULT_TKT_LIFE;
	nvals.exp_date = 0;
	nvals.attributes = 0;
    } else {
	nvals.max_life = ovals.max_life;
	nvals.exp_date = ovals.exp_date;
	nvals.attributes = ovals.attributes;
    }

    get_maxlife(&nvals);
    get_attr(&nvals);
    get_expdate(&nvals);
    
    if (IS_FIELD(KADM_MAXLIFE, nvals.fields) || IS_FIELD(KADM_ATTR, nvals.fields) || IS_FIELD(KADM_EXPDATE, nvals.fields)) {
	if ((status = kadm_mod(&ovals, &nvals)) != KADM_SUCCESS) {
	    printf("kadm error: %s\n",error_message(status));
	    goto out;
	}
	if ((status = kadm_get(&ovals, fields)) != KADM_SUCCESS) {
	    printf("kadm error: %s\n",error_message(status));
	    goto out;
	}
    }
    prin_vals(&ovals);

out:
    if (!multiple)
	clean_up();
    return;
}

void 
help(int argc, char **argv)
{
    if (argc == 1) {
	printf("Welcome to the Kerberos administration program.");
	printf("Type \"?\" to get\n");
	printf("a list of requests that are available. You can");
	printf(" get help on each of\n");
	printf("the commands by typing \"help command_name\".");
	printf(" Some functions of this\n");
	printf("program will require an \"admin\" password");
	printf(" from you. This is a password\n");
	printf("private to you, that is used to authenticate");
	printf(" requests from this\n");
	printf("program. You can change this password with");
	printf(" the \"change_admin_password\"\n");
	printf("(or short form \"cap\") command. Good Luck!    \n");
    } else if (!strcmp(argv[1], "change_password") ||
	       !strcmp(argv[1], "cpw")) {
	printf("Usage: change_password user_name.\n");
	printf("\n");
	printf("user_name is the name of the user whose password");
	printf(" you wish to change. \n");
	printf("His/her password is changed in the kerberos database\n");
	printf("When this command is issued, first the \"Admin\"");
	printf(" password will be prompted\n");
	printf("for and if correct the user's new password will");
	printf(" be prompted for (twice with\n");
	printf("appropriate comparison). Note: No minimum password");
	printf(" length restrictions apply, but\n");
	printf("longer passwords are more secure.\n");
    } else if (!strcmp(argv[1], "change_admin_password") ||
	       !strcmp(argv[1], "cap")) {
	printf("Usage: change_admin_password.\n");
	printf("\n");
	printf("This command takes no arguments and is used");
	printf(" to change your private\n");
	printf("\"Admin\" password. It will first prompt for");
	printf(" the (current) \"Admin\"\n");
	printf("password and then ask for the new password");
	printf(" by prompting:\n");
	printf("\n");
	printf("New password for <Your User Name>.admin:\n");
	printf("\n");
	printf("Enter the new admin password that you desire");
	printf(" (it will be asked for\n");
	printf("twice to avoid errors).\n");
    } else if (!strcmp(argv[1], "add_new_key") ||
	       !strcmp(argv[1], "ank")) {
	printf("Usage: add_new_key user_name.\n");
	printf("\n");
	printf("user_name is the name of a new user to put");
	printf(" in the kerberos database. Your\n");
	printf("\"Admin\" password and the user's password");
	printf(" are prompted for. The user's\n");
	printf("password will be asked for");
	printf(" twice to avoid errors.\n");
	printf("You are also prompted for the default ticket");
	printf(" lifetime, attributes\n");
	printf("and expiration date (see the 'mod_entry' command).\n");
    } else if (!strcmp(argv[1], "get_entry") ||
	       !strcmp(argv[1], "get")) {
	printf("Usage: get_entry user_name.\n");
	printf("\n");
	printf("user_name is the name of a user whose");
	printf(" entry you wish to review.  Your\n");
	printf("\"Admin\" password is prompted for. ");
	printf(" The key field is not filled in, for\n");
	printf("security reasons.\n");
    } else if (!strcmp(argv[1], "mod_entry") ||
	       !strcmp(argv[1], "mod")) {
	printf("Usage: mod_entry user_name.\n");
	printf("\n");
	printf("user_name is the name of a user whose");
	printf(" entry you wish to modify.  Your\n");
	printf("\"Admin\" password is prompted for.");
	printf(" You will also be prompted for the new\n");
	printf("default ticket lifetime, attributes");
	printf(" and expiration date. Attributes may\n");
	printf("be entered in decimal by default,");
	printf(" octal if begun with '0', or hexadecimal\n");
	printf("if begun with '0x'. End the lifetime");
	printf(" with 'm' to specify minutes, 'h'\n");
	printf("to specify hours.\n");
    } else if (!strcmp(argv[1], "destroy_tickets") ||
	       !strcmp(argv[1], "dest")) {
	printf("Usage: destroy_tickets\n");
	printf("\n");
	printf("Destroy your admin tickets.  This will");
	printf(" cause you to be prompted for your\n");
	printf("admin password on your next request.\n");
    } else if (!strcmp(argv[1], "list_requests") ||
	       !strcmp(argv[1], "lr") ||
	       !strcmp(argv[1], "?")) {
	printf("Usage: list_requests\n");
	printf("\n");
	printf("This command lists what other commands are");
	printf(" currently available.\n");
    } else if (!strcmp(argv[1], "exit") ||
	       !strcmp(argv[1], "quit") ||
	       !strcmp(argv[1], "q")) {
	printf("Usage: quit\n");
	printf("\n");
	printf("This command exits this program.\n");
    } else {
	printf("Sorry there is no such command as %s.", argv[1]);
	printf(" Type \"help\" for more information.    \n");
    }
    return;
}
#if 0
static void
go_home(str,x)
char *str;
int x;
{
    fprintf(stderr, "%s: %s\n", str, error_message(x));
    clean_up();
    exit(1);
}
#endif
